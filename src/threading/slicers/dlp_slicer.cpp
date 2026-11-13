#include "threading/slicers/dlp_slicer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "cross_section/cross_section.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/open_mesh.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
DlpSlicer::DlpSlicer(QString outputLocation) : TraditionalAST(outputLocation, true) {}

void DlpSlicer::preProcess(nlohmann::json opt_data) {
    m_build.clear();
    const auto global = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
    if (global->setting<BuildVolumeType>(PRS::Dimensions::kBuildVolumeType) != BuildVolumeType::kRectangular) {
        failPreProcess("DLP slicing requires a rectangular printer build volume.");
        return;
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

    auto build = QSharedPointer<DlpBuild>::create();
    QString error;
    if (!DlpRasterizer::initialize(*build, settings, &error)) {
        failPreProcess(error);
        return;
    }

    QVector<QSharedPointer<MeshBase>> meshes;
    double highest_z = settings.z_min;
    for (const auto& part : CSM->parts()) {
        if (part->getMeshType() != MeshType::kBuild && part->getMeshType() != MeshType::kSupport) continue;
        part->clearSteps();
        for (const auto& original : part->meshes()) {
            QSharedPointer<MeshBase> mesh;
            if (const auto* closed = dynamic_cast<ClosedMesh*>(original.get()))
                mesh = QSharedPointer<ClosedMesh>::create(*closed);
            else if (const auto* open = dynamic_cast<OpenMesh*>(original.get()))
                mesh = QSharedPointer<OpenMesh>::create(*open);
            if (!mesh) continue;
            highest_z = std::max(highest_z, double(mesh->max().z()));
            meshes.append(mesh);
        }
    }

    const double first_z         = settings.z_min + (settings.layer_height / 2.0);
    const double highest_slice_z = std::min(highest_z, settings.z_max);
    const double count           = std::floor((highest_slice_z - first_z) / settings.layer_height) + 1.0;
    if (count <= 0 || meshes.isEmpty()) {
        failPreProcess("No DLP slice planes intersect the build within the printer bounds.");
        return;
    }
    if (!std::isfinite(count) || count > std::numeric_limits<int>::max()) {
        failPreProcess("DLP layer count exceeds the supported range.");
        return;
    }

    for (int i = 0; i < static_cast<int>(count); ++i) {
        if (shouldCancel()) {
            failPreProcess("DLP slicing canceled.");
            return;
        }
        const double z = first_z + (i * settings.layer_height);
        if (z >= settings.z_max) break;
        Plane plane(Point(0, 0, z), QVector3D(0, 0, 1));
        QVector<PolygonList> sections;
        for (const auto& mesh : meshes) {
            if (z < mesh->min().z() || z > mesh->max().z()) continue;
            Point shift;
            QVector3D normal;
            const auto section = CrossSection::doCrossSection(mesh, plane, shift, normal, global);
            if (!section.isEmpty()) sections.append(section);
        }
        const QImage image = DlpRasterizer::rasterize(sections, *build);
        if (image.isNull()) {
            failPreProcess("Could not allocate the DLP layer image. Reduce the image resolution.");
            return;
        }
        if (!DlpRasterizer::writeLayer(*build, i + 1, z, image, &error)) {
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
