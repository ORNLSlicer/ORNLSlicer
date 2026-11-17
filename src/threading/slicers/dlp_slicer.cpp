#include "threading/slicers/dlp_slicer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "cross_section/cross_section.h"
#include "dlp/dlp_storage.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/open_mesh.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
bool preflightForSettings(const QSharedPointer<SettingsBase>& global, DlpPreflight& result, QString* error) {
    result          = DlpPreflight();
    const auto fail = [&](const QString& message) {
        if (error) *error = message;
        return false;
    };
    if (global->setting<BuildVolumeType>(PRS::Dimensions::kBuildVolumeType) != BuildVolumeType::kRectangular) {
        return fail("DLP slicing requires a rectangular printer build volume.");
    }
    DlpSettings settings;
    settings.x_min = global->setting<double>(PRS::Dimensions::kXMin);
    settings.x_max = global->setting<double>(PRS::Dimensions::kXMax);
    settings.y_min = global->setting<double>(PRS::Dimensions::kYMin);
    settings.y_max = global->setting<double>(PRS::Dimensions::kYMax);
    settings.z_min = global->setting<double>(PRS::Dimensions::kZMin);
    settings.z_max = global->setting<double>(PRS::Dimensions::kZMax);
    // Machine profiles can use negative Z travel; meshes use the positive build volume shown in Part View.
    if (settings.z_min < 0) {
        settings.z_max -= settings.z_min;
        settings.z_min = 0;
    }
    if (global->setting<bool>(PRS::Dimensions::kEnableW))
        settings.z_max +=
            std::abs(global->setting<double>(PRS::Dimensions::kWMax) - global->setting<double>(PRS::Dimensions::kWMin));
    settings.layer_height     = global->setting<double>(PS::Layer::kLayerHeight);
    settings.output_mode      = static_cast<DlpOutputMode>(global->setting<int>(ES::DlpSlicer::kOutputMode));
    settings.projection_width = global->setting<double>(ES::DlpSlicer::kProjectionWidth);
    settings.lane_pixel_width = global->setting<int>(ES::DlpSlicer::kPixelWidth);
    const int bit_depth       = global->setting<int>(ES::DlpSlicer::kBitDepth);
    settings.bit_depth        = bit_depth == 0 ? 1 : bit_depth == 1 ? 8 : 0;

    if (!DlpRasterizer::layout(result.layout, settings, error)) return false;

    double highest_z = settings.z_min;
    bool has_meshes = false, intersects = false;
    QStringList clipped_parts;
    const double first_z = settings.z_min + (settings.layer_height / 2.0);
    const auto outside   = [](double value, double bound, bool lower) {
        const double tolerance =
            std::numeric_limits<float>::epsilon() * std::max({1.0, std::abs(value), std::abs(bound)});
        return lower ? value < bound - tolerance : value > bound + tolerance;
    };
    for (const auto& part : CSM->parts()) {
        if (part->getMeshType() != MeshType::kBuild && part->getMeshType() != MeshType::kSupport) continue;
        bool clipped = false;
        for (const auto& mesh : part->meshes()) {
            if (!dynamic_cast<ClosedMesh*>(mesh.get()) && !dynamic_cast<OpenMesh*>(mesh.get())) continue;
            has_meshes         = true;
            const auto minimum = mesh->min();
            const auto maximum = mesh->max();
            highest_z          = std::max(highest_z, double(maximum.z()));
            clipped |= outside(minimum.x(), settings.x_min, true) || outside(maximum.x(), settings.x_max, false) ||
                       outside(minimum.y(), settings.y_min, true) || outside(maximum.y(), settings.y_max, false) ||
                       outside(minimum.z(), settings.z_min, true) || outside(maximum.z(), settings.z_max, false);
            intersects |= maximum.x() > settings.x_min && minimum.x() < settings.x_max &&
                          maximum.y() > settings.y_min && minimum.y() < settings.y_max && maximum.z() >= first_z &&
                          minimum.z() < settings.z_max;
        }
        if (clipped) clipped_parts.append(part->name());
    }
    const double highest_slice_z = std::min(highest_z, settings.z_max);
    double count                 = std::floor((highest_slice_z - first_z) / settings.layer_height) + 1.0;
    if (!std::isfinite(count) || count > std::numeric_limits<int>::max())
        return fail("DLP layer count exceeds the supported range.");
    if (count > 0 && first_z + ((count - 1) * settings.layer_height) >= settings.z_max) --count;
    if (count <= 0 || !has_meshes) return fail("No DLP slice planes intersect the build within the printer bounds.");

    result.layer_count        = static_cast<int>(count);
    result.file_count         = qint64(result.layer_count) * (qint64(result.layout.lane_count) + 1) + 1;
    const auto& layout        = result.layout;
    const double depth        = layout.settings.bit_depth;
    const double full_gray    = std::ceil(layout.image_width / 4.0) * 4.0 * layout.image_height;
    const double lane_gray    = double(settings.lane_pixel_width) * layout.lane_image_height;
    const double full_encoded = std::ceil(layout.image_width * depth / 32.0) * 4.0 * layout.image_height;
    const double lane_encoded = std::ceil(settings.lane_pixel_width * depth / 32.0) * 4.0 * layout.lane_image_height;
    const double header       = 54 + (depth == 1 ? 8 : 1024);
    result.working_bytes      = full_gray + std::max(full_encoded, lane_gray + lane_encoded);
    result.output_bytes = result.layer_count * (header + full_encoded + layout.lane_count * (header + lane_encoded));
    QString temporary_root;
    if (!DlpStorage::temporaryRoot(temporary_root, error) ||
        !DlpStorage::checkSpace(temporary_root, std::ceil(result.output_bytes), error))
        return false;
    if (!clipped_parts.isEmpty())
        result.warnings.append("Geometry outside the printer bounds will be clipped: " + clipped_parts.join(", "));
    if (!intersects)
        result.warnings.append(
            "No model or support bounds intersect the sampled build volume. Output may be entirely black.");
    return true;
}
}  // namespace

DlpSlicer::DlpSlicer(QString outputLocation) : TraditionalAST(outputLocation, true) {}

bool DlpSlicer::preflight(DlpPreflight& result, QString* error) {
    return preflightForSettings(QSharedPointer<SettingsBase>::create(*GSM->getGlobal()), result, error);
}

void DlpSlicer::preProcess(nlohmann::json opt_data) {
    m_build.clear();
    const auto global = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
    DlpPreflight preflight;
    QString error;
    if (!preflightForSettings(global, preflight, &error)) {
        failPreProcess(error);
        return;
    }
    const auto& settings = preflight.layout.settings;
    auto build           = QSharedPointer<DlpBuild>::create();
    if (!DlpRasterizer::initialize(*build, settings, &error)) {
        failPreProcess(error);
        return;
    }

    bool was_canceled   = false;
    const auto canceled = [&] {
        was_canceled = was_canceled || shouldCancel();
        return was_canceled;
    };
    QVector<QSharedPointer<MeshBase>> meshes;
    for (const auto& part : CSM->parts()) {
        if (part->getMeshType() != MeshType::kBuild && part->getMeshType() != MeshType::kSupport) continue;
        part->clearSteps();
        for (const auto& original : part->meshes()) {
            if (canceled()) {
                failPreProcess("DLP slicing canceled.");
                return;
            }
            QSharedPointer<MeshBase> mesh;
            if (const auto* closed = dynamic_cast<ClosedMesh*>(original.get()))
                mesh = QSharedPointer<ClosedMesh>::create(*closed);
            else if (const auto* open = dynamic_cast<OpenMesh*>(original.get()))
                mesh = QSharedPointer<OpenMesh>::create(*open);
            if (mesh) meshes.append(mesh);
        }
    }

    const double first_z = settings.z_min + (settings.layer_height / 2.0);
    const int count      = preflight.layer_count;
    for (int i = 0; i < static_cast<int>(count); ++i) {
        if (canceled()) {
            failPreProcess("DLP slicing canceled.");
            return;
        }
        const double z = first_z + (i * settings.layer_height);
        emit statusMessage(QString("Slicing DLP layer %1 of %2").arg(i + 1).arg(count));
        Plane plane(Point(0, 0, z), QVector3D(0, 0, 1));
        QVector<PolygonList> sections;
        for (const auto& mesh : meshes) {
            if (canceled()) {
                failPreProcess("DLP slicing canceled.");
                return;
            }
            if (z < mesh->min().z() || z > mesh->max().z()) continue;
            Point shift;
            QVector3D normal;
            const auto section = CrossSection::doCrossSection(mesh, plane, shift, normal, global);
            if (!section.isEmpty()) sections.append(section);
        }
        const QImage image = DlpRasterizer::rasterize(sections, *build, canceled);
        if (image.isNull()) {
            failPreProcess(was_canceled ? "DLP slicing canceled."
                                        : "Could not allocate the DLP layer image. Reduce the image resolution.");
            return;
        }
        if (!DlpRasterizer::writeLayer(*build, i + 1, z, image, &error, canceled)) {
            failPreProcess(error);
            return;
        }
        emit statusUpdate(StatusUpdateStepType::kPreProcess, (i + 1) * 100.0 / count);
    }
    if (build->layers.isEmpty()) {
        failPreProcess("No DLP slice planes intersect the build within the printer bounds.");
        return;
    }
    emit statusUpdate(StatusUpdateStepType::kPreProcess, 100);
    m_build = build;
}

void DlpSlicer::postProcess(nlohmann::json opt_data) {
    emit statusUpdate(StatusUpdateStepType::kPostProcess, 100);
}

void DlpSlicer::writeGCode() {}
}  // namespace ORNL
