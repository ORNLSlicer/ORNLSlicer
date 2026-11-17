#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "dlp/dlp_build.h"
#include "dlp/dlp_storage.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "threading/slicers/dlp_slicer.h"
#include "threading/slicers/image_slicer.h"
#include "utilities/constants.h"

namespace {
using ORNL::Testing::expect;
using ORNL::Testing::near;

class TestImageSlicer : public ORNL::ImageSlicer {
   public:
    using ImageSlicer::ImageSlicer;
    using ImageSlicer::preProcess;
};

void addBox(const QString& name, ORNL::MeshType type, QVector3D minimum, QVector3D size) {
    auto mesh =
        QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(size.x(), size.y(), size.z()));
    QMatrix4x4 transform;
    transform.translate(minimum);
    mesh->setTransformation(transform);
    mesh->setType(type);
    mesh->setName(name);
    ORNL::CSM->addPart(QSharedPointer<ORNL::Part>::create(mesh, name + ".stl", type), false);
}

void setBounds(double x_min, double x_max, double y_min, double y_max, double z_min, double z_max) {
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::Dimensions::kXMin, x_min);
    settings->setSetting(ORNL::PRS::Dimensions::kXMax, x_max);
    settings->setSetting(ORNL::PRS::Dimensions::kYMin, y_min);
    settings->setSetting(ORNL::PRS::Dimensions::kYMax, y_max);
    settings->setSetting(ORNL::PRS::Dimensions::kZMin, z_min);
    settings->setSetting(ORNL::PRS::Dimensions::kZMax, z_max);
}

struct SliceResult {
    QSharedPointer<const ORNL::DlpBuild> build;
    int completed      = 0;
    int started_layers = 0;
    QString error;
};

SliceResult sliceDlp(const QString& output, bool cancel = false, bool cancel_during = false) {
    SliceResult result;
    QObject callbacks;
    ORNL::DlpSlicer slicer(output);
    QObject::connect(&slicer, &ORNL::AbstractSlicingThread::sliceComplete, &callbacks, [&] { ++result.completed; });
    QObject::connect(&slicer, &ORNL::AbstractSlicingThread::sliceFailed, &callbacks,
                     [&](const QString& message) { result.error = message; });
    QObject::connect(&slicer, &ORNL::AbstractSlicingThread::statusMessage, &callbacks, [&](const QString& message) {
        if (!message.startsWith("Slicing DLP layer ")) return;
        ++result.started_layers;
        if (cancel_during) slicer.setCancel();
    });
    if (cancel) slicer.setCancel();
    slicer.doSlice();
    result.build = slicer.build();
    return result;
}

bool sharedLayerGrid(const QString& output) {
    ORNL::CSM->clearParts();
    setBounds(-4000, 6000, 1000, 7000, 2000, 10000);
    addBox("base", ORNL::MeshType::kBuild, {-3000, 2000, 2000}, {2000, 2000, 2000});
    addBox("elevated", ORNL::MeshType::kBuild, {1000, 4000, 6000}, {2000, 2000, 2000});
    addBox("imported_support", ORNL::MeshType::kSupport, {4000, 2000, 2000}, {1000, 1000, 1000});

    const auto settings = ORNL::GSM->getGlobal();
    settings->remove(ORNL::ES::DlpSlicer::kInterlacedStitching);
    settings->remove(ORNL::ES::DlpSlicer::kStitchOverlap);
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 2048000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 0);

    ORNL::DlpPreflight summary;
    QString preflight_error;
    if (!expect(ORNL::DlpSlicer::preflight(summary, &preflight_error) && preflight_error.isEmpty() &&
                    !summary.layout.storage && summary.layout.layers.isEmpty() && summary.warnings.isEmpty() &&
                    summary.layer_count == 6 && summary.file_count == 13 && summary.layout.image_width == 20 &&
                    summary.layout.image_height == 12 && summary.layout.lane_image_height == 2160 &&
                    near(summary.layout.pixel_pitch, 500) && summary.working_bytes > 0 && summary.output_bytes > 0,
                "DLP preflight must predict the complete layout and resources without temporary images."))
        return false;

    const auto result = sliceDlp(output);
    if (!expect(result.completed == 1 && result.error.isEmpty() && result.build,
                "DLP slicing must complete successfully for build and imported support meshes."))
        return false;
    const auto& build = *result.build;
    if (!expect(build.layers.size() == 6 && build.image_width == 20 && build.image_height == 12,
                "DLP slicing must use the printer bounds and one shared bed-relative layer grid."))
        return false;
    if (!expect(!build.settings.interlaced_stitching && build.stitch_overlap_pixels == 0 &&
                    build.stitch_band_starts.isEmpty(),
                "Projects without stitching settings must retain the original disabled lane layout."))
        return false;

    double actual_bytes = 0;
    for (const auto& layer : build.layers) {
        actual_bytes += QFileInfo(layer.image_path).size();
        for (const auto& lane : layer.lanes) actual_bytes += QFileInfo(lane.image_path).size();
    }
    bool passed = expect(near(summary.output_bytes, actual_bytes),
                         "Preflight BMP storage estimates must match every actual padded 1-bit image file.");
    for (int i = 0; i < build.layers.size(); ++i) {
        const auto& layer = build.layers[i];
        passed &= expect(layer.number == i + 1 && near(layer.z, 2500 + i * 1000),
                         "Layer samples must start at Z minimum plus half a layer, then advance by a full layer.");
        const QImage image(layer.image_path);
        if (!expect(!image.isNull(), "Completed DLP layer images must survive destruction of the slicer."))
            return false;
        passed &= expect((qGray(image.pixel(3, 8)) > 0) == (i < 2),
                         "A bed-level part must occupy only its shared-grid layers at the correct world coordinates.");
        passed &= expect((qGray(image.pixel(11, 4)) > 0) == (i >= 4),
                         "An elevated part must retain its absolute Z position and printer-relative XY position.");
        passed &= expect((qGray(image.pixel(16, 9)) > 0) == (i == 0),
                         "Imported support meshes must appear in the DLP exposure mask.");
        passed &= expect(layer.empty == (i == 2 || i == 3),
                         "Empty layers between independently elevated objects must remain in the build.");
    }

    settings->setSetting(ORNL::PRS::Dimensions::kZMax, 7000.0);
    const auto clipped = sliceDlp(output);
    passed &= expect(clipped.completed == 1 && clipped.error.isEmpty() && clipped.build &&
                         clipped.build->layers.size() == 5 && near(clipped.build->layers.last().z, 6500),
                     "DLP cross-sections must stop at the configured printer Z maximum.");
    passed &= expect(ORNL::DlpSlicer::preflight(summary, &preflight_error) && summary.layer_count == 5 &&
                         summary.warnings.join(' ').contains("elevated"),
                     "Preflight must predict Z-clipped layer counts and identify the affected part.");

    const auto cancelled = sliceDlp(output, true);
    passed &= expect(cancelled.completed == 0 && !cancelled.error.isEmpty() && !cancelled.build,
                     "Cancelled DLP slicing must report failure without publishing a partial or successful build.");
    const auto active_cancelled = sliceDlp(output, false, true);
    passed &= expect(active_cancelled.started_layers == 1 && active_cancelled.completed == 0 &&
                         active_cancelled.error == "DLP slicing canceled." && !active_cancelled.build,
                     "Cancellation after a layer starts must stop that layer and never publish a completed build.");

    const QByteArray before = [&] {
        QFile file(build.layers.first().image_path);
        file.open(QIODevice::ReadOnly);
        return file.readAll();
    }();
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 1000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 2);
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 1);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kImage));
    QString error;
    QTemporaryDir exported;
    if (!expect(exported.isValid() && ORNL::DlpExporter::exportBuild(build, exported.path() + "/build", &error),
                "A completed DLP build must remain exportable after settings and slicing mode change."))
        return false;
    QFile copied(exported.path() + "/build/Layer 1/Layer_1_Full.bmp");
    passed &= expect(copied.open(QIODevice::ReadOnly) && !before.isEmpty() && copied.readAll() == before &&
                         near(build.settings.projection_width, 2048000) && build.settings.lane_pixel_width == 4096 &&
                         build.settings.bit_depth == 1 && build.layers.size() == 6,
                     "Export must preserve the completed DLP result rather than reinterpret current settings.");
    return passed;
}

bool machineZCoordinates(const QString& output) {
    ORNL::CSM->clearParts();
    setBounds(0, 8000, 0, 8000, -2000, 0);
    addBox("positive_build", ORNL::MeshType::kBuild, {2000, 2000, 3000}, {2000, 2000, 2000});
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 2048000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 0);
    settings->setSetting(ORNL::PRS::Dimensions::kWMin, -4000.0);
    settings->setSetting(ORNL::PRS::Dimensions::kWMax, 0.0);
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);

    const auto without_w = sliceDlp(output);
    bool passed = expect(without_w.completed == 1 && without_w.error.isEmpty() && without_w.build &&
                             near(without_w.build->settings.z_min, 0) && near(without_w.build->settings.z_max, 2000) &&
                             without_w.build->layers.size() == 2,
                         "Negative machine Z limits must map to the positive build volume without W travel.");
    ORNL::DlpPreflight summary;
    QString error;
    passed &= expect(ORNL::DlpSlicer::preflight(summary, &error) && summary.layer_count == 2 &&
                         summary.warnings.join(' ').contains("clipped") &&
                         summary.warnings.join(' ').contains("entirely black"),
                     "Preflight must warn when normalized Z travel cannot reach any model bounds.");
    if (without_w.build) {
        for (int i = 0; i < without_w.build->layers.size(); ++i) {
            passed &= expect(near(without_w.build->layers[i].z, 500 + i * 1000) && without_w.build->layers[i].empty,
                             "Normalized machine Z must retain empty positive build layers below elevated geometry.");
        }
    }

    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, true);
    passed &= expect(ORNL::DlpSlicer::preflight(summary, &error) && summary.layer_count == 5 &&
                         summary.warnings.isEmpty() && near(summary.layout.settings.z_max, 6000),
                     "Preflight must include W travel and avoid false clipping warnings for normalized machine Z.");
    const auto with_w = sliceDlp(output);
    passed &= expect(with_w.completed == 1 && with_w.error.isEmpty() && with_w.build &&
                         near(with_w.build->settings.z_min, 0) && near(with_w.build->settings.z_max, 6000) &&
                         with_w.build->layers.size() == 5,
                     "Enabled W travel must extend the normalized DLP build volume to reach visible geometry.");
    if (with_w.build) {
        for (int i = 0; i < with_w.build->layers.size(); ++i) {
            const auto& layer = with_w.build->layers[i];
            const QImage image(layer.image_path);
            passed &= expect(!image.isNull() && near(layer.z, 500 + i * 1000) && layer.empty == (i < 3),
                             "DLP layers must follow one positive build-coordinate grid with W travel.");
            if (!image.isNull())
                passed &= expect((qGray(image.pixel(5, 9)) > 0) == (i >= 3),
                                 "An elevated model visible in the build volume must produce exposed DLP pixels.");
        }
    }
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    return passed;
}

bool preflightBoundsWarnings() {
    ORNL::CSM->clearParts();
    setBounds(0, 8000, 0, 8000, 0, 4000);
    addBox("outside_xy", ORNL::MeshType::kBuild, {9000, 1000, 0}, {1000, 1000, 1000});
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 2048000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 0);
    ORNL::DlpPreflight summary;
    QString error;
    bool passed = expect(ORNL::DlpSlicer::preflight(summary, &error) && summary.layer_count == 1 &&
                             summary.warnings.join(' ').contains("outside_xy") &&
                             summary.warnings.join(' ').contains("entirely black"),
                         "Preflight must warn about a model entirely outside the printable XY area.");
    settings->setSetting(ORNL::PRS::Dimensions::kXMax, 12000.0);
    passed &= expect(ORNL::DlpSlicer::preflight(summary, &error) && summary.warnings.isEmpty(),
                     "Expanding the printer to contain geometry must clear clipping and empty-output warnings.");
    return passed;
}

bool insufficientTemporarySpace(const QString& output) {
    QString temporary_root, error;
    if (!expect(ORNL::DlpStorage::temporaryRoot(temporary_root, &error), error.toStdString())) return false;
    QStorageInfo storage(temporary_root);
    storage.refresh();
    if (!expect(storage.isValid() && storage.isReady() && storage.bytesAvailable() >= 0,
                "Could not inspect temporary-volume capacity for the disk-space regression."))
        return false;

    // This layout has over 8 GiB of padded BMPs per layer, without allocating any raster images.
    // Even the largest signed 64-bit capacity can therefore be exceeded within the supported layer count.
    constexpr double width           = 4096.0 * 8192;
    constexpr double bytes_per_layer = 62 + width / 8 + 8192.0 * (62 + 512.0 * 2160);
    const double height              = std::floor(storage.bytesAvailable() / bytes_per_layer) + 1024;
    ORNL::CSM->clearParts();
    setBounds(0, width, 0, 1, 0, height);
    addBox("exceeds_temporary_capacity", ORNL::MeshType::kBuild, {0, 0, 0}, {1, 1, float(height)});
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 4096.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 0);

    const auto images_before = QDir(temporary_root).entryList({"ornlslicer-dlp-*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    ORNL::DlpPreflight summary;
    if (!expect(!ORNL::DlpSlicer::preflight(summary, &error) && error.contains("space", Qt::CaseInsensitive) &&
                    summary.output_bytes > storage.bytesAvailable() && !summary.layout.storage &&
                    summary.layout.layers.isEmpty(),
                "Preflight must reject a cache larger than available disk space before creating output."))
        return false;  // Never attempt this deliberately oversized build if the preflight guard regresses.
    const auto result = sliceDlp(output);
    return expect(
        result.completed == 0 && result.started_layers == 0 && !result.build &&
            result.error.contains("space", Qt::CaseInsensitive) &&
            QDir(temporary_root).entryList({"ornlslicer-dlp-*"}, QDir::Dirs | QDir::NoDotAndDotDot) == images_before,
        "The slicing worker must recheck capacity and fail before images or temporary build directories.");
}

bool projectorFormats(const QString& output) {
    ORNL::CSM->clearParts();
    setBounds(0, 409600, 0, 108000, 0, 1000);
    addBox("format_probe", ORNL::MeshType::kBuild, {1000, 1000, 0}, {1000, 1000, 1000});
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 204800.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 1);
    ORNL::DlpPreflight summary;
    QString preflight_error;
    if (!expect(ORNL::DlpSlicer::preflight(summary, &preflight_error) && summary.layer_count == 1 &&
                    summary.file_count == 4,
                "Static preflight must count the full image, every lane image, and build.csv."))
        return false;
    const auto static_result = sliceDlp(output);
    if (!expect(static_result.completed == 1 && static_result.error.isEmpty() && static_result.build &&
                    static_result.build->layers.size() == 1,
                "A 108 mm static build must fit in one 4096 by 2160 projector frame."))
        return false;
    const auto& static_build = *static_result.build;
    double actual_bytes      = QFileInfo(static_build.layers.first().image_path).size();
    for (const auto& lane : static_build.layers.first().lanes) actual_bytes += QFileInfo(lane.image_path).size();
    bool passed = expect(near(summary.output_bytes, actual_bytes),
                         "Preflight BMP storage estimates must match the actual padded 8-bit image files.");
    passed &= expect(static_build.image_width == 8192 && static_build.image_height == 2160 &&
                         static_build.lane_image_height == 2160 && static_build.lane_count == 2 &&
                         near(static_build.pixel_pitch, 50) && static_build.settings.bit_depth == 8,
                     "Static slicing must snapshot the selected depth and physical projector resolution.");
    const QImage static_lane(static_build.layers[0].lanes[0].image_path);
    passed &= expect(static_lane.size() == QSize(4096, 2160) && qGray(static_lane.pixel(20, 2130)) == 255,
                     "Static lane BMPs must use one full frame and preserve physical geometry placement.");

    settings->setSetting(ORNL::PRS::Dimensions::kYMax, 108050.0);
    const auto too_deep = sliceDlp(output);
    passed &= expect(too_deep.completed == 0 && !too_deep.error.isEmpty() && !too_deep.build,
                     "Static slicing must reject build depth beyond one frame instead of clipping or tiling it.");

    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 8);
    const auto invalid_width = sliceDlp(output);
    passed &= expect(invalid_width.completed == 0 && !invalid_width.error.isEmpty() && !invalid_width.build,
                     "Static slicing must reject projector widths other than 4096 pixels.");

    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kScrolling));
    settings->setSetting(ORNL::PRS::Dimensions::kYMax, 108850.0);
    const auto scrolling = sliceDlp(output);
    if (!expect(scrolling.completed == 1 && scrolling.error.isEmpty() && scrolling.build &&
                    scrolling.build->layers.size() == 1,
                "Scrolling slicing must support build depths above a static frame."))
        return false;
    const auto& scroll_build = *scrolling.build;
    passed &= expect(scroll_build.image_height == 2177 && scroll_build.lane_image_height == 4352 &&
                         scroll_build.settings.bit_depth == 1,
                     "Scrolling must round lane heights to 2176-row blocks and override an 8-bit selection.");
    const QString lane_path = scroll_build.layers[0].lanes[0].image_path;
    QFile lane_file(lane_path);
    passed &= expect(lane_file.open(QIODevice::ReadOnly), "Could not inspect scrolling BMP encoding.");
    const QByteArray header = lane_file.read(30);
    passed &= expect(header.size() == 30 && qFromLittleEndian<quint16>(header.constData() + 28) == 1,
                     "Scrolling BMP files must be encoded at 1 bit per pixel.");
    const QImage scroll_lane(lane_path);
    passed &= expect(scroll_lane.size() == QSize(4096, 4352) && qGray(scroll_lane.pixel(20, 2150)) == 255 &&
                         qGray(scroll_lane.pixel(20, 2177)) == 0 && qGray(scroll_lane.pixel(20, 4351)) == 0,
                     "Scrolling lanes must preserve geometry and black-pad below the build to a complete block.");
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 8);
    const auto invalid_scroll_width = sliceDlp(output);
    passed &= expect(
        invalid_scroll_width.completed == 0 && !invalid_scroll_width.error.isEmpty() && !invalid_scroll_width.build,
        "Scrolling slicing must reject projector widths other than 4096 pixels.");
    return passed;
}

bool interlacedStitching(const QString& output) {
    const auto settings         = ORNL::GSM->getGlobal();
    const auto saved_settings   = settings->json();
    const auto restore_settings = qScopeGuard([&] { settings->json(saved_settings); });
    ORNL::CSM->clearParts();
    setBounds(0, 8129, 0, 128, 0, 2000);
    addBox("stitching_rectangle", ORNL::MeshType::kBuild, {0, 0, 0}, {8129, 128, 2000});
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 4096.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 1);
    QTemporaryDir exported;
    if (!expect(exported.isValid(), "Could not create the frozen stitched-export fixture.")) return false;
    bool passed = true;
    for (auto mode : {ORNL::DlpOutputMode::kStatic, ORNL::DlpOutputMode::kScrolling}) {
        settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(mode));
        settings->setSetting(ORNL::ES::DlpSlicer::kInterlacedStitching, true);
        settings->setSetting(ORNL::ES::DlpSlicer::kStitchOverlap, 64.0);
        ORNL::DlpPreflight summary;
        QString error;
        if (!expect(ORNL::DlpSlicer::preflight(summary, &error) && summary.layer_count == 2 &&
                        summary.layout.lane_count == 3 && summary.file_count == 9 &&
                        summary.layout.stitch_overlap_pixels == 64 &&
                        summary.layout.lane_starts == QVector<int>({0, 4032, 4033}),
                    "Stitched preflight must account for the extra lane and anchored final footprint."))
            return false;
        const auto result = sliceDlp(output);
        if (!expect(result.completed == 1 && result.error.isEmpty() && result.build &&
                        result.build->layers.size() == 2 && result.build->lane_count == 3,
                    "The real DLP slicer must complete interlaced static and scrolling builds."))
            return false;
        const auto& build = *result.build;
        const int depth   = mode == ORNL::DlpOutputMode::kStatic ? 8 : 1;
        passed &= expect(build.settings.interlaced_stitching && near(build.settings.stitch_overlap, 64) &&
                             build.stitch_overlap_pixels == 64 && build.settings.bit_depth == depth &&
                             build.lane_starts == summary.layout.lane_starts && build.stitch_band_starts.size() == 2,
                         "Stitched builds must cache physical placement, pattern settings, and the output depth.");
        for (int layer_index = 0; layer_index < build.layers.size(); ++layer_index) {
            const auto& layer = build.layers[layer_index];
            const QImage full(layer.image_path);
            QVector<QImage> lanes;
            for (const auto& lane : layer.lanes) lanes.append(QImage(lane.image_path));
            if (!expect(
                    !full.isNull() && full.size() == QSize(8129, 128) && lanes.size() == 3 &&
                        std::all_of(lanes.cbegin(), lanes.cend(), [](const QImage& image) { return !image.isNull(); }),
                    "Stitched slicer output must contain readable full and lane images."))
                return false;
            bool once = true;
            for (int y = 0; y < full.height() && once; ++y) {
                for (int x = 0; x < full.width(); ++x) {
                    int exposures = 0;
                    for (int lane = 0; lane < lanes.size(); ++lane) {
                        const int column = x - build.lane_starts[lane];
                        if (column >= 0 && column < lanes[lane].width())
                            exposures += qGray(lanes[lane].pixel(column, y)) > 0;
                    }
                    if (qGray(full.pixel(x, y)) != 255 || exposures != 1) {
                        once = false;
                        break;
                    }
                }
            }
            passed &= expect(
                once,
                "Actual stitched slices must reproduce the model exactly once, including triple footprint overlap.");
            const int probe = build.stitch_band_starts[0] + 32;
            const int first = qGray(lanes[0].pixel(probe, 0));
            passed &= expect(first == (layer_index == 0 ? 0 : 255) && qGray(lanes[0].pixel(probe, 64)) == 255 - first,
                             "Finger ownership must alternate along Y and swap between consecutive real layers.");
            QFile encoded(layer.lanes.first().image_path);
            if (!expect(encoded.open(QIODevice::ReadOnly), "Could not inspect stitched BMP depth.")) return false;
            const auto header = encoded.read(30);
            passed &= expect(header.size() == 30 && qFromLittleEndian<quint16>(header.constData() + 28) == depth &&
                                 qGray(lanes[0].pixel(0, 128)) == 0 &&
                                 qGray(lanes.last().pixel(4095, build.lane_image_height - 1)) == 0,
                             "Interlacing must preserve static/scrolling depth and black frame padding.");
        }

        settings->setSetting(ORNL::ES::DlpSlicer::kInterlacedStitching, false);
        settings->setSetting(ORNL::ES::DlpSlicer::kStitchOverlap, 1000.0);
        const QString destination = exported.filePath(mode == ORNL::DlpOutputMode::kStatic ? "Static" : "Scrolling");
        if (!expect(ORNL::DlpExporter::exportBuild(build, destination, &error), error.toStdString())) return false;
        QFile cached(build.layers.first().lanes[1].image_path);
        QFile copied(destination + "/Layer 1/Layer1_2.bmp");
        QFile csv(destination + "/build.csv");
        if (!expect(
                cached.open(QIODevice::ReadOnly) && copied.open(QIODevice::ReadOnly) && csv.open(QIODevice::ReadOnly),
                "Could not inspect frozen stitched export files."))
            return false;
        const auto metadata = csv.readAll();
        passed &= expect(cached.readAll() == copied.readAll() && build.settings.interlaced_stitching &&
                             near(build.settings.stitch_overlap, 64) &&
                             metadata.contains(",2,Layer 1/Layer1_2.bmp,0,4.032,") &&
                             metadata.contains(",3,Layer 1/Layer1_3.bmp,0,4.033,"),
                         "Changing the checkbox after slicing must not change cached masks or exported lane origins.");
    }
    return passed;
}

QMap<QString, QByteArray> legacyImages(const QString& directory) {
    QDir().mkpath(directory);
    TestImageSlicer slicer(directory + "/unused.gcode");
    slicer.preProcess();
    QMap<QString, QByteArray> files;
    for (const auto& name : QDir(directory).entryList({"*.png", "idFileLinks.dat"}, QDir::Files, QDir::Name)) {
        QFile file(directory + '/' + name);
        if (file.open(QIODevice::ReadOnly)) files.insert(name, file.readAll());
    }
    return files;
}

bool preservesImageSlicer(const QString& directory) {
    ORNL::CSM->clearParts();
    setBounds(0, 8000, 0, 8000, 0, 5000);
    addBox("legacy_build", ORNL::MeshType::kBuild, {2000, 2000, 0}, {2000, 2000, 3000});
    addBox("legacy_support", ORNL::MeshType::kSupport, {5000, 2000, 0}, {1000, 1000, 1000});
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kImage));
    settings->setSetting(ORNL::PS::Slicing::kImagePixelSizeX, 500.0);
    settings->setSetting(ORNL::PS::Slicing::kImagePixelSizeY, 500.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 0.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalY, 0.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalZ, 1.0);
    const auto saved  = settings->json();
    const auto before = legacyImages(directory + "/image-before");
    if (!expect(before.size() == 4 && !before.value("idFileLinks.dat").isEmpty(),
                "The existing Image slicer fixture must produce three PNG layers and object-ID metadata."))
        return false;
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        if (!it.key().endsWith(".png")) continue;
        if (!expect(it.value().startsWith(QByteArray::fromHex("89504e470d0a1a0a")) && it.value().size() > 25 &&
                        static_cast<unsigned char>(it.value()[24]) == 16,
                    "Existing Image slicing must continue producing 16-bit PNG images."))
            return false;
    }

    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 2048000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 1);
    const auto dlp = sliceDlp(directory + "/unused.gcode");
    if (!expect(dlp.completed == 1 && dlp.error.isEmpty() && dlp.build,
                "The preservation fixture must perform a DLP slice between legacy Image slices."))
        return false;
    settings->json(saved);
    return expect(legacyImages(directory + "/image-after") == before,
                  "Image PNG bytes and object-ID links must be identical before and after using DLP slicing.");
}

bool sessionRoutesDlpResults(const QString& directory) {
    const auto session  = ORNL::CSM;
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 2048000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 4096);
    settings->setSetting(ORNL::ES::DlpSlicer::kOutputMode, static_cast<int>(ORNL::DlpOutputMode::kStatic));
    session->setDefaultGcodeDir(directory);
    int dlp_completions = 0, gcode_completions = 0, failures = 0;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timed_out = false;
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        timed_out = true;
        loop.quit();
    });
    QObject::connect(session.get(), &ORNL::SessionManager::forwardDlpSliceComplete, &loop, [&] {
        ++dlp_completions;
        loop.quit();
    });
    QObject::connect(session.get(), &ORNL::SessionManager::forwardSliceComplete, &loop, [&] {
        ++gcode_completions;
        loop.quit();
    });
    QObject::connect(session.get(), &ORNL::SessionManager::sliceFailed, &loop, [&](const QString&) {
        ++failures;
        loop.quit();
    });
    const auto run = [&] {
        bool started = false;
        timed_out    = false;
        timeout.start(15000);
        QTimer::singleShot(0, &loop, [&] {
            started = session->doSlice();
            if (!started) loop.quit();
        });
        loop.exec();
        timeout.stop();
        return started && !timed_out;
    };
    bool passed =
        expect(run() && dlp_completions == 1 && gcode_completions == 0 && failures == 0 && session->dlpBuild(),
               "Session DLP completion must provide a build without entering G-Code import/export.");
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 0);
    passed &= expect(run() && failures == 1 && dlp_completions == 1 && gcode_completions == 0 && !session->dlpBuild(),
                     "Invalid DLP configuration must report failure and discard the old result without completion.");
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    if (!expect(directory.isValid(), "Could not create a DLP integration test directory.")) return EXIT_FAILURE;
    ORNL::GSM->setConsoleSettings(QSharedPointer<ORNL::SettingsBase>::create());
    bool passed = sharedLayerGrid(directory.path() + "/unused.gcode");
    passed &= machineZCoordinates(directory.path() + "/unused.gcode");
    passed &= preflightBoundsWarnings();
    passed &= insufficientTemporarySpace(directory.path() + "/unused.gcode");
    passed &= projectorFormats(directory.path() + "/unused.gcode");
    passed &= interlacedStitching(directory.path() + "/unused.gcode");
    passed &= preservesImageSlicer(directory.path());
    passed &= sessionRoutesDlpResults(directory.path());
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
