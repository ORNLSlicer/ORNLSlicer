#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMap>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>

#include "dlp/dlp_build.h"
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
    int completed = 0;
    QString error;
};

SliceResult sliceDlp(const QString& output, bool cancel = false) {
    SliceResult result;
    QObject callbacks;
    ORNL::DlpSlicer slicer(output);
    QObject::connect(&slicer, &ORNL::AbstractSlicingThread::sliceComplete, &callbacks, [&] { ++result.completed; });
    QObject::connect(&slicer, &ORNL::AbstractSlicingThread::sliceFailed, &callbacks,
                     [&](const QString& message) { result.error = message; });
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
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kDlp));
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 4000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 8);
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 0);

    const auto result = sliceDlp(output);
    if (!expect(result.completed == 1 && result.error.isEmpty() && result.build,
                "DLP slicing must complete successfully for build and imported support meshes."))
        return false;
    const auto& build = *result.build;
    if (!expect(build.layers.size() == 6 && build.image_width == 20 && build.image_height == 12,
                "DLP slicing must use the printer bounds and one shared bed-relative layer grid."))
        return false;

    bool passed = true;
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

    const auto cancelled = sliceDlp(output, true);
    passed &= expect(cancelled.completed == 0 && !cancelled.error.isEmpty() && !cancelled.build,
                     "Cancelled DLP slicing must report failure without publishing a partial or successful build.");

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
                         near(build.settings.projection_width, 4000) && build.settings.lane_pixel_width == 8 &&
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
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 4000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 8);
    settings->setSetting(ORNL::ES::DlpSlicer::kBitDepth, 0);
    settings->setSetting(ORNL::PRS::Dimensions::kWMin, -4000.0);
    settings->setSetting(ORNL::PRS::Dimensions::kWMax, 0.0);
    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, false);

    const auto without_w = sliceDlp(output);
    bool passed = expect(without_w.completed == 1 && without_w.error.isEmpty() && without_w.build &&
                             near(without_w.build->settings.z_min, 0) && near(without_w.build->settings.z_max, 2000) &&
                             without_w.build->layers.size() == 2,
                         "Negative machine Z limits must map to the positive build volume without W travel.");
    if (without_w.build) {
        for (int i = 0; i < without_w.build->layers.size(); ++i) {
            passed &= expect(near(without_w.build->layers[i].z, 500 + i * 1000) && without_w.build->layers[i].empty,
                             "Normalized machine Z must retain empty positive build layers below elevated geometry.");
        }
    }

    settings->setSetting(ORNL::PRS::Dimensions::kEnableW, true);
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
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 4000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 8);
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
    settings->setSetting(ORNL::ES::DlpSlicer::kProjectionWidth, 4000.0);
    settings->setSetting(ORNL::ES::DlpSlicer::kPixelWidth, 8);
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
    passed &= preservesImageSlicer(directory.path());
    passed &= sessionRoutesDlpResults(directory.path());
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
