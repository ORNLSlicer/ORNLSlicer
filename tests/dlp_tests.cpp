#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtEndian>
#include <limits>

#include "dlp/dlp_build.h"
#include "test_utils.h"
#include "units/unit.h"

namespace {
using namespace ORNL;

Polygon rectangle(double left, double bottom, double right, double top) {
    return Polygon {Point(left * 1024, bottom), Point(right * 1024, bottom), Point(right * 1024, top),
                    Point(left * 1024, top)};
}

DlpSettings settings(int bit_depth = 1) {
    DlpSettings result;
    result.x_min            = -2048;
    result.x_max            = 8192;
    result.y_min            = 10;
    result.y_max            = 14;
    result.z_min            = 0;
    result.z_max            = 2000;
    result.layer_height     = 1000;
    result.projection_width = 4096;
    result.lane_pixel_width = 4096;
    result.bit_depth        = bit_depth;
    return result;
}

bool white(const QImage& image, int x, int y) {
    return qGray(image.pixel(x, y)) == 255;
}

bool testRaster() {
    bool passed = true;
    DlpBuild build;
    QString error;
    passed &= Testing::expect(DlpRasterizer::initialize(build, settings(), &error), error.toStdString());
    passed &= Testing::expect(build.image_width == 10240 && build.image_height == 4 && build.lane_count == 3 &&
                                  build.pixel_pitch == 1 && build.lane_image_height == 2160 &&
                                  build.lane_starts == QVector<int>({0, 4096, 6144}),
                              "DLP layout must use printer spans, square pixels, and rounded-up lanes");
    PolygonList shell {rectangle(-2, 10, 4, 14), rectangle(0, 11, 2, 13), rectangle(6, 10, 8, 11)};
    PolygonList overlap {rectangle(1, 11, 3, 13)};
    QImage image = DlpRasterizer::rasterize({shell, overlap}, build);
    passed &= Testing::expect(!image.isNull(), "DLP raster allocation failed");
    if (image.isNull()) return false;
    passed &= Testing::expect(white(image, 0, 0) && !white(image, 6 * 1024, 0),
                              "DLP X coordinates must subtract the nonzero printer minimum");
    passed &= Testing::expect(!white(image, 2 * 1024, 1) && white(image, 3 * 1024, 1),
                              "DLP holes must remain black, with overlapping mesh geometry unioned");
    passed &= Testing::expect(white(image, 8 * 1024, 3) && !white(image, 8 * 1024, 0),
                              "DLP disjoint geometry must retain +Y-up image orientation");
    passed &= Testing::expect(white(image, 4095, 0) && white(image, 4096, 0),
                              "DLP geometry must remain filled on both sides of a lane seam");

    auto exact  = settings();
    exact.x_max = 10240;
    passed &= Testing::expect(DlpRasterizer::initialize(build, exact, &error) && build.lane_count == 3 &&
                                  build.image_width == 12288 && build.lane_starts == QVector<int>({0, 4096, 8192}),
                              "An exact physical lane multiple must not create an extra lane");
    auto fractional = settings();
    for (double remainder : {0.2, 0.8}) {
        fractional.x_max = 8192 + remainder;
        passed &=
            Testing::expect(!DlpRasterizer::initialize(build, fractional, &error) && error.contains("whole number"),
                            "Shifted lanes must reject fractional-pixel X spans on either side of half a pixel");
    }
    fractional.x_min = 0;
    fractional.x_max = 9.2;
    passed &= Testing::expect(DlpRasterizer::initialize(build, fractional, &error), error.toStdString());
    image = DlpRasterizer::rasterize({PolygonList {rectangle(-20, 0, 20, 20)}}, build);
    passed &= Testing::expect(white(image, 8, 0) && !white(image, 9, 0),
                              "Pixel centers outside fractional printer bounds must stay black");
    return passed;
}

bool testSlicingCancellation() {
    DlpBuild build;
    QString error;
    if (!Testing::expect(DlpRasterizer::initialize(build, settings(), &error), error.toStdString())) return false;
    const QVector<PolygonList> sections {PolygonList {rectangle(-20, 0, 20, 20)}};
    int checks  = 0;
    bool passed = Testing::expect(DlpRasterizer::rasterize(sections, build, [&] { return ++checks >= 3; }).isNull() &&
                                      checks == 3 && build.layers.isEmpty(),
                                  "Cancellation during raster rows must discard the incomplete image");
    const QImage image = DlpRasterizer::rasterize(sections, build);
    if (!Testing::expect(!image.isNull(), "Could not create the cancellation fixture image")) return false;

    checks                   = 0;
    const QString first_full = build.storage->filePath("Layer 1/Layer_1_Full.bmp");
    passed &= Testing::expect(!DlpRasterizer::writeLayer(build, 1, 500, image, &error, [&] { return ++checks >= 5; }) &&
                                  checks == 5 && error == "DLP slicing canceled." && build.layers.isEmpty() &&
                                  !QFileInfo::exists(first_full),
                              "Cancellation during 1-bit encoding must not save or publish an incomplete layer");

    checks                    = 0;
    const QString second_full = build.storage->filePath("Layer 2/Layer_2_Full.bmp");
    const QString second_lane = build.storage->filePath("Layer 2/Layer2_1.bmp");
    passed &=
        Testing::expect(!DlpRasterizer::writeLayer(build, 2, 1500, image, &error,
                                                   [&] { return QFileInfo::exists(second_full) && ++checks >= 4; }) &&
                            checks == 4 && error == "DLP slicing canceled." && build.layers.isEmpty() &&
                            QFileInfo::exists(second_full) && !QFileInfo::exists(second_lane),
                        "Cancellation while copying a lane must retain no completed layer or incomplete lane image");

    const QString final_lane = build.storage->filePath(QString("Layer 3/Layer3_%1.bmp").arg(build.lane_count));
    passed &= Testing::expect(
        !DlpRasterizer::writeLayer(build, 3, 2500, image, &error, [&] { return QFileInfo::exists(final_lane); }) &&
            error == "DLP slicing canceled." && QFileInfo::exists(final_lane) && build.layers.isEmpty(),
        "Cancellation after the last image is written must still prevent layer publication");
    passed &= Testing::expect(DlpRasterizer::writeLayer(build, 3, 2500, image, &error) && build.layers.size() == 1,
                              "A canceled layer must remain retryable without publishing duplicate layers");
    return passed;
}

bool testExport(int bit_depth) {
    bool passed = true;
    DlpBuild build;
    QString error;
    passed &= Testing::expect(DlpRasterizer::initialize(build, settings(bit_depth), &error), error.toStdString());
    const QImage image =
        DlpRasterizer::rasterize({PolygonList {rectangle(-2, 10, -1, 14), rectangle(7, 10, 8, 14)}}, build);
    passed &= Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, image, &error), error.toStdString());
    const QImage empty = DlpRasterizer::rasterize({}, build);
    passed &= Testing::expect(DlpRasterizer::writeLayer(build, 2, 1500, empty, &error), error.toStdString());
    if (build.layers.size() != 2 || build.layers[0].lanes.size() != 3) return false;
    const auto& layer = build.layers[0];
    passed &= Testing::expect(!layer.empty && !layer.lanes[0].empty && layer.lanes[1].empty && !layer.lanes[2].empty,
                              "DLP emptiness must follow actual full and lane exposure pixels");
    passed &= Testing::expect(build.layers[1].empty && build.layers[1].lanes[0].empty &&
                                  build.layers[1].lanes[1].empty && build.layers[1].lanes[2].empty,
                              "DLP empty layers must retain all empty lane images");
    for (int lane = 0; lane < 3; ++lane) {
        QFile file(layer.lanes[lane].image_path);
        passed &= Testing::expect(file.open(QIODevice::ReadOnly), "Could not inspect DLP BMP header");
        const QByteArray header = file.read(30);
        passed &= Testing::expect(header.size() == 30 && header.startsWith("BM"), "DLP output must be a BMP file");
        if (header.size() != 30) return false;
        passed &= Testing::expect(qFromLittleEndian<quint16>(header.constData() + 28) == bit_depth,
                                  "DLP BMP must use the selected encoded bit depth");
        const QImage saved(layer.lanes[lane].image_path);
        passed &= Testing::expect(saved.width() == 4096 && saved.height() == 2160,
                                  "Every DLP lane image must have identical full projector dimensions");
        bool matches = true;
        for (int y = 0; y < saved.height() && matches; ++y) {
            for (int x = 0; x < saved.width(); ++x) {
                const int full_x = build.lane_starts[lane] + x;
                const bool exposed =
                    y < image.height() && full_x >= lane * 4096 && full_x < image.width() && white(image, full_x, y);
                if (white(saved, x, y) != exposed) {
                    matches = false;
                    break;
                }
            }
        }
        passed &= Testing::expect(matches,
                                  "Lane images must preserve geometry, mask final left overlap, and black-pad below");
    }

    QTemporaryDir destination;
    const QString output = destination.filePath("Hex Build v1.0");
    passed &= Testing::expect(DlpExporter::exportBuild(build, output, &error), error.toStdString());
    passed &= Testing::expect(QFile::exists(output + "/Layer 1/Layer_1_Full.bmp") &&
                                  QFile::exists(output + "/Layer 1/Layer1_3.bmp") &&
                                  QFile::exists(output + "/Layer 2/Layer2_1.bmp"),
                              "DLP export must preserve the requested layer directory and image names");
    QFile csv(output + "/build.csv");
    passed &= Testing::expect(csv.open(QIODevice::ReadOnly), "Could not read DLP build.csv");
    const QByteArray content = csv.readAll();
    const bool csv_valid =
        content.startsWith("layer,z_mm,lane,file,empty,x_min_mm,y_max_mm,pixel_pitch_mm,width_px,height_px\n") &&
        content.contains("1,0.5,0,Layer 1/Layer_1_Full.bmp,0,-2.048,0.014,0.001,10240,4\n") &&
        content.contains("1,0.5,2,Layer 1/Layer1_2.bmp,1,2.048,0.014,0.001,4096,2160\n") &&
        content.contains("2,1.5,3,Layer 2/Layer2_3.bmp,1,4.096,0.014,0.001,4096,2160\n") && content.count('\n') == 9;
    passed &= Testing::expect(
        csv_valid, "DLP CSV must list every image, emptiness, physical placement, pitch, and actual pixel dimensions");
    if (!csv_valid) std::cerr << "Actual DLP CSV (" << bit_depth << "-bit):\n" << content.toStdString();
    passed &= Testing::expect(!DlpExporter::exportBuild(build, output, &error),
                              "DLP export must reject nonempty output directories");
    QFile unchanged(output + "/build.csv");
    passed &= Testing::expect(unchanged.open(QIODevice::ReadOnly) && unchanged.readAll() == content,
                              "Rejected DLP exports must leave existing files untouched");
    QFile existing_file(destination.filePath("existing.txt"));
    if (!Testing::expect(existing_file.open(QIODevice::WriteOnly) && existing_file.write("keep") == 4,
                         "Could not create the existing-file export fixture"))
        return false;
    existing_file.close();
    passed &= Testing::expect(!DlpExporter::exportBuild(build, existing_file.fileName(), &error),
                              "DLP export must reject an existing file entered as a folder name");
    passed &= Testing::expect(existing_file.open(QIODevice::ReadOnly) && existing_file.readAll() == "keep",
                              "An existing file must remain unchanged when used as the export destination");
    return passed;
}

bool testGrayscaleEncoding() {
    bool passed = true;
    const uchar levels[] {0, 1, 63, 127, 128, 192, 254, 255, 128};
    for (int bit_depth : {1, 8}) {
        auto grayscale  = settings(bit_depth);
        grayscale.x_min = 0;
        grayscale.x_max = 9;
        grayscale.y_max = 11;
        DlpBuild build;
        QString error;
        if (!Testing::expect(DlpRasterizer::initialize(build, grayscale, &error), error.toStdString())) return false;
        QImage input(9, 1, QImage::Format_Grayscale8);
        for (int x = 0; x < 9; ++x) input.scanLine(0)[x] = levels[x];
        if (!Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, input, &error), error.toStdString()))
            return false;
        const QImage saved(build.layers[0].image_path);
        if (!Testing::expect(saved.size() == QSize(9, 1), "Could not decode the grayscale BMP fixture")) return false;
        bool pixels_match = true;
        for (int x = 0; x < 9; ++x) {
            const int expected = bit_depth == 1 ? (levels[x] >= 128 ? 255 : 0) : levels[x];
            pixels_match &= qGray(saved.pixel(x, 0)) == expected;
        }
        passed &= Testing::expect(
            pixels_match, "BMP encoding must threshold 1-bit pixels at 128 and preserve 8-bit grayscale values");
        QFile file(build.layers[0].image_path);
        if (!Testing::expect(file.open(QIODevice::ReadOnly), "Could not inspect the grayscale BMP encoding"))
            return false;
        const QByteArray encoded = file.readAll();
        if (!Testing::expect(encoded.size() >= 54, "BMP encoding must include a complete header")) return false;
        const quint32 pixels  = qFromLittleEndian<quint32>(encoded.constData() + 10);
        const quint32 palette = 14 + qFromLittleEndian<quint32>(encoded.constData() + 14);
        const int colors      = bit_depth == 1 ? 2 : 256;
        if (!Testing::expect(palette + colors * 4 <= pixels && pixels + 2 <= encoded.size(),
                             "BMP encoding must include its palette and active pixel bytes"))
            return false;
        bool palette_matches = true;
        for (int i = 0; i < colors; ++i) {
            const int gray = bit_depth == 1 ? i * 255 : i;
            for (int channel = 0; channel < 3; ++channel)
                palette_matches &= static_cast<uchar>(encoded[palette + i * 4 + channel]) == gray;
        }
        passed &= Testing::expect(palette_matches,
                                  "BMP palettes must encode black at index zero and the full grayscale range");
        if (bit_depth == 1)
            passed &= Testing::expect(
                static_cast<uchar>(encoded[pixels]) == 0x0f && (static_cast<uchar>(encoded[pixels + 1]) & 0x80) == 0x80,
                "1-bit BMP pixels must be packed most-significant-bit first across byte boundaries");
    }
    return passed;
}

bool testExportRecovery() {
    DlpBuild build;
    QString error;
    if (!Testing::expect(DlpRasterizer::initialize(build, settings(8), &error), error.toStdString())) return false;
    const QImage image = DlpRasterizer::rasterize({}, build);
    if (!Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, image, &error), error.toStdString())) return false;

    QTemporaryDir directory;
    if (!Testing::expect(directory.isValid(), "Could not create the export-recovery fixture")) return false;
    const QString destination = directory.filePath("Retry Build");
    const QString source      = build.layers[0].lanes[1].image_path;
    const QString backup      = source + ".saved";
    if (!Testing::expect(QFile::rename(source, backup), "Could not hide a cached image for the failed-copy fixture"))
        return false;
    bool canceled = true;
    bool passed =
        Testing::expect(!DlpExporter::exportBuild(build, destination, &error, {}, &canceled) && !canceled &&
                            error.contains("image") && !QFileInfo::exists(destination) &&
                            QDir(directory.path())
                                .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)
                                .isEmpty(),
                        "A failed copy must remove staging files and leave the final folder absent");
    if (!Testing::expect(QFile::rename(backup, source), "Could not restore the cached image after a copy failure"))
        return false;
    qint64 previous = -1, expected_total = 0;
    bool progress_valid = true;
    for (const auto& layer : build.layers) {
        expected_total += QFileInfo(layer.image_path).size();
        for (const auto& lane : layer.lanes) expected_total += QFileInfo(lane.image_path).size();
    }
    passed &= Testing::expect(
        DlpExporter::exportBuild(
            build, destination, &error,
            [&](qint64 completed, qint64 total) {
                progress_valid &= completed >= previous && completed <= total && total == expected_total &&
                                  !QFileInfo::exists(destination);
                previous = completed;
                return true;
            },
            &canceled) &&
            !canceled && progress_valid && previous == expected_total && QFile::exists(destination + "/build.csv"),
        "A failed export must be retryable with monotonic byte progress and publish only the finished build");
    QFile cached(source), exported(destination + "/Layer 1/" + QFileInfo(source).fileName());
    passed &= Testing::expect(cached.open(QIODevice::ReadOnly) && exported.open(QIODevice::ReadOnly) &&
                                  cached.readAll() == exported.readAll(),
                              "Chunked export must preserve every byte of a BMP larger than the copy buffer");

    const QString empty_destination = directory.filePath("Empty Build");
    if (!Testing::expect(QDir().mkpath(empty_destination), "Could not create an existing empty export folder"))
        return false;
    qint64 canceled_at = 0;
    passed &= Testing::expect(
        !DlpExporter::exportBuild(
            build, empty_destination, &error,
            [&](qint64 completed, qint64 total) {
                canceled_at = completed;
                return completed < total / 2;
            },
            &canceled) &&
            canceled && canceled_at > 0 && canceled_at < expected_total && QFileInfo(empty_destination).isDir() &&
            QDir(empty_destination)
                .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)
                .isEmpty() &&
            QDir(directory.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden).size() == 2,
        "Canceling a partial image copy must remove staging files and preserve the existing empty folder");
    passed &= Testing::expect(
        DlpExporter::exportBuild(build, empty_destination, &error) && QFile::exists(empty_destination + "/build.csv"),
        "A canceled export must be retryable in the same existing empty folder");

    const QString raced_destination = directory.filePath("Raced Build");
    bool wrote_blocker              = false;
    passed &= Testing::expect(!DlpExporter::exportBuild(build, raced_destination, &error,
                                                        [&](qint64 completed, qint64 total) {
                                                            if (completed == total && !wrote_blocker) {
                                                                QDir().mkpath(raced_destination);
                                                                QFile blocker(raced_destination + "/keep.txt");
                                                                wrote_blocker = blocker.open(QIODevice::WriteOnly) &&
                                                                                blocker.write("keep") == 4;
                                                            }
                                                            return true;
                                                        }) &&
                                  wrote_blocker && !QFile::exists(raced_destination + "/build.csv"),
                              "Publishing must reject a destination populated while the export was being staged");
    QFile blocker(raced_destination + "/keep.txt");
    passed &= Testing::expect(blocker.open(QIODevice::ReadOnly) && blocker.readAll() == "keep",
                              "A destination changed during export must retain the newly created user file");
    bool root_progress = false;
    passed &=
        Testing::expect(!DlpExporter::exportBuild(build, QDir::rootPath(), &error,
                                                  [&](qint64, qint64) {
                                                      root_progress = true;
                                                      return false;
                                                  }) &&
                            !root_progress && error.contains("filesystem root") && error.contains("subfolder"),
                        "A filesystem root must be rejected before copying with guidance to create a build subfolder");
    return passed;
}

bool testExportPermissions() {
#ifdef Q_OS_UNIX
    DlpBuild build;
    QString error;
    if (!Testing::expect(DlpRasterizer::initialize(build, settings(), &error), error.toStdString())) return false;
    const QImage image = DlpRasterizer::rasterize({}, build);
    if (!Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, image, &error), error.toStdString())) return false;
    QTemporaryDir directory;
    if (!Testing::expect(directory.isValid() && QDir(directory.path()).mkdir("Normal Folder"),
                         "Could not create the export-permissions fixture"))
        return false;

    const auto normal_permissions = QFileInfo(directory.filePath("Normal Folder")).permissions();
    const QString fresh           = directory.filePath("New Build");
    bool passed                   = Testing::expect(
        DlpExporter::exportBuild(build, fresh, &error) && QFileInfo(fresh).permissions() == normal_permissions,
        "New export folders must use normal directory permissions rather than private staging permissions");

    const QString existing = directory.filePath("Existing Build");
    const auto requested = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup;
    if (!Testing::expect(QDir().mkdir(existing) && QFile::setPermissions(existing, requested),
                         "Could not set custom permissions on the empty export folder"))
        return false;
    const auto existing_permissions = QFileInfo(existing).permissions();
    bool canceled                   = false;
    passed &=
        Testing::expect(!DlpExporter::exportBuild(
                            build, existing, &error, [](qint64, qint64) { return false; }, &canceled) &&
                            canceled && QFileInfo(existing).permissions() == existing_permissions &&
                            QDir(existing).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty(),
                        "Canceling export must leave the existing empty folder and its custom permissions intact");
    passed &= Testing::expect(DlpExporter::exportBuild(build, existing, &error) &&
                                  QFileInfo(existing).permissions() == existing_permissions &&
                                  QFile::exists(existing + "/build.csv"),
                              "Publishing into an existing empty folder must preserve its custom permissions");
    const QString changed = directory.filePath("Changed Build");
    if (!Testing::expect(QDir().mkdir(changed) && QFile::setPermissions(changed, requested),
                         "Could not create the late permission-change fixture"))
        return false;
    bool changed_permissions = false;
    QFile::Permissions latest_permissions;
    passed &= Testing::expect(DlpExporter::exportBuild(build, changed, &error,
                                                       [&](qint64 completed, qint64 total) {
                                                           if (completed == total && !changed_permissions) {
                                                               changed_permissions = QFile::setPermissions(
                                                                   changed, QFile::ReadOwner | QFile::WriteOwner |
                                                                                QFile::ExeOwner | QFile::ExeGroup);
                                                               latest_permissions = QFileInfo(changed).permissions();
                                                           }
                                                           return true;
                                                       }) &&
                                  changed_permissions && QFileInfo(changed).permissions() == latest_permissions,
                              "Publication must preserve permissions changed while the export was being staged");
    return passed;
#else
    return true;
#endif
}

bool testLaneCoverage() {
    bool passed = true;
    DlpBuild build;
    QString error;
    if (!Testing::expect(DlpRasterizer::initialize(build, settings(), &error), error.toStdString())) return false;
    const QImage image = DlpRasterizer::rasterize({PolygonList {rectangle(-20, 0, 20, 20)}}, build);
    if (!Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, image, &error), error.toStdString())) return false;
    QVector<int> exposures(build.image_width * build.image_height, 0);
    bool padding_black = true;
    for (int i = 0; i < build.lane_count; ++i) {
        const QImage lane(build.layers[0].lanes[i].image_path);
        if (!Testing::expect(lane.size() == QSize(4096, 2160), "Could not load complete projector lane")) return false;
        for (int y = 0; y < lane.height(); ++y) {
            for (int x = 0; x < lane.width(); ++x) {
                if (!white(lane, x, y)) continue;
                const int full_x = build.lane_starts[i] + x;
                if (y < image.height() && full_x < image.width())
                    ++exposures[y * image.width() + full_x];
                else
                    padding_black = false;
            }
        }
    }
    bool exactly_once = true;
    for (int count : exposures) exactly_once &= count == 1;
    passed &= Testing::expect(exactly_once && padding_black,
                              "Shifted projector footprints must expose every full-layer pixel exactly once");
    const QImage previous(build.layers[0].lanes[1].image_path);
    const QImage final(build.layers[0].lanes[2].image_path);
    passed &= Testing::expect(white(previous, 4095, 0) && !white(final, 0, 0) && !white(final, 2047, 0) &&
                                  white(final, 2048, 0) && white(final, 4095, 0),
                              "Only the shifted final lane must mask the overlap; its predecessor stays exposed");

    auto narrow  = settings();
    narrow.x_max = narrow.x_min + 3.2;
    if (!Testing::expect(DlpRasterizer::initialize(build, narrow, &error), error.toStdString())) return false;
    const QImage small = DlpRasterizer::rasterize({PolygonList {rectangle(-20, 0, 20, 20)}}, build);
    if (!Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, small, &error), error.toStdString())) return false;
    const QImage single(build.layers[0].lanes[0].image_path);
    passed &= Testing::expect(build.lane_count == 1 && build.lane_starts == QVector<int>({0}) && white(single, 0, 0) &&
                                  white(single, 2, 0) && !white(single, 3, 0) && !white(single, 4095, 0) &&
                                  !white(single, 0, 2159),
                              "A build narrower than one footprint must stay at minimum X with black right padding");
    return passed;
}

bool testFormats() {
    bool passed                = true;
    auto projector             = settings(8);
    projector.x_min            = 0;
    projector.x_max            = 204800;
    projector.y_min            = 0;
    projector.y_max            = 108000;
    projector.projection_width = 204800;
    DlpBuild build;
    QString error;
    passed &= Testing::expect(DlpRasterizer::initialize(build, projector, &error) && build.image_width == 4096 &&
                                  build.image_height == 2160 && build.lane_image_height == 2160 &&
                                  build.pixel_pitch == 50 && build.settings.bit_depth == 8,
                              "A 4096 by 2160 static image at 0.05 mm pitch must cover 204.8 by 108 mm");
    auto entered = projector;
    Distance distance;
    entered.x_max = distance.from(300, mm);
    entered.y_max = distance.from(108, mm);
    passed &= Testing::expect(DlpRasterizer::initialize(build, entered, &error) && build.image_width == 6000 &&
                                  build.image_height == 2160 && build.lane_starts == QVector<int>({0, 1904}),
                              "GUI distance conversion noise must not add rows or reject an aligned final lane");
    entered.projection_width = distance.from(204.8, mm);
    passed &= Testing::expect(
        DlpRasterizer::initialize(build, entered, &error) && build.image_width == 6000 && build.image_height == 2160,
        "Entering the default projection width in millimeters must preserve its layout");
    projector.y_max = 108050;
    passed &= Testing::expect(!DlpRasterizer::initialize(build, projector, &error) && error.contains("one 4096 x 2160"),
                              "Static output must reject depth beyond one projector frame");
    projector.output_mode = DlpOutputMode::kScrolling;
    projector.y_max       = 108800;
    passed &= Testing::expect(DlpRasterizer::initialize(build, projector, &error) && build.image_height == 2176 &&
                                  build.lane_image_height == 2176 && build.settings.bit_depth == 1,
                              "An exact scrolling block must retain 2176 rows and force 1-bit output");
    projector.y_max = 108850;
    if (!Testing::expect(DlpRasterizer::initialize(build, projector, &error) && build.image_height == 2177 &&
                             build.lane_image_height == 4352 && build.settings.bit_depth == 1,
                         "Scrolling heights must round up to a complete 2176-row block"))
        return false;
    QImage full(build.image_width, build.image_height, QImage::Format_Grayscale8);
    full.fill(255);
    if (!Testing::expect(DlpRasterizer::writeLayer(build, 1, 500, full, &error), error.toStdString())) return false;
    QFile file(build.layers[0].lanes[0].image_path);
    passed &= Testing::expect(file.open(QIODevice::ReadOnly), "Could not inspect scrolling BMP depth");
    const QByteArray header = file.read(30);
    passed &= Testing::expect(header.size() == 30 && qFromLittleEndian<quint16>(header.constData() + 28) == 1,
                              "Scrolling lane BMPs must encode 1-bit pixels regardless of the selected static depth");
    const QImage lane(build.layers[0].lanes[0].image_path);
    passed &= Testing::expect(
        lane.size() == QSize(4096, 4352) && white(lane, 0, 2176) && !white(lane, 0, 2177) && !white(lane, 4095, 4351),
        "Scrolling lanes must preserve all build rows and black-pad only after maximum row");
    return passed;
}

bool testInvalid() {
    bool passed = true;
    DlpBuild build;
    QString error;
    auto invalid             = settings();
    invalid.lane_pixel_width = 0;
    passed &= Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject zero pixel width");
    invalid                  = settings();
    invalid.lane_pixel_width = 1024;
    passed &= Testing::expect(!DlpRasterizer::initialize(build, invalid, &error) && error.contains("4096"),
                              "Both projector formats must require 4096 pixels across");
    invalid             = settings();
    invalid.output_mode = static_cast<DlpOutputMode>(2);
    passed &=
        Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject invalid output modes");
    invalid                  = settings();
    invalid.projection_width = -1;
    passed &=
        Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject negative lane width");
    invalid       = settings();
    invalid.x_max = invalid.x_min;
    passed &=
        Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject empty printer bounds");
    invalid              = settings();
    invalid.layer_height = std::numeric_limits<double>::quiet_NaN();
    passed &=
        Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject nonfinite dimensions");
    invalid           = settings();
    invalid.bit_depth = 24;
    passed &=
        Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject unsupported BMP depths");
    invalid                  = settings();
    invalid.projection_width = 1.0e-12;
    passed &=
        Testing::expect(!DlpRasterizer::initialize(build, invalid, &error), "DLP must reject overflowing image sizes");
    return passed;
}

bool testImageAllocationLimit() {
    const int previous_limit = QImageReader::allocationLimit();
    QImageReader::setAllocationLimit(256);
    bool passed              = true;
    auto project             = settings();
    project.x_min            = 0;
    project.x_max            = 6095999.7721910495;
    project.y_min            = 0;
    project.y_max            = 2158999.919317663;
    project.projection_width = 25399.99905079604;
    project.lane_pixel_width = 1024;
    DlpBuild oversized;
    QString error;
    passed &= Testing::expect(
        !DlpRasterizer::initialize(oversized, project, &error) && !oversized.storage && error.contains("4096"),
        "The supplied project's old 1024-pixel format must report the required hardware width");
    project.lane_pixel_width = 4096;
    project.output_mode      = DlpOutputMode::kScrolling;
    passed &= Testing::expect(!DlpRasterizer::initialize(oversized, project, &error) && !oversized.storage,
                              "Oversized projector images must be rejected before allocating storage");
    passed &=
        Testing::expect(error.contains("983040") && error.contains("348160") && error.contains("allocation limit"),
                        "Oversized-image errors must identify actual full-layer dimensions and allocation limit");

    auto small  = settings();
    small.x_min = 0;
    small.x_max = 4096;
    DlpBuild reduced;
    passed &= Testing::expect(DlpRasterizer::initialize(reduced, small, &error) && reduced.image_width == 4096 &&
                                  reduced.image_height == 4 && reduced.lane_count == 1,
                              "Reducing the printer area must allow a practical full-resolution DLP test");

    auto four_lane             = settings();
    four_lane.x_min            = 0;
    four_lane.x_max            = 508000;
    four_lane.y_min            = 0;
    four_lane.y_max            = 381000;
    four_lane.projection_width = 127000;
    four_lane.output_mode      = DlpOutputMode::kScrolling;
    DlpBuild four_lane_build;
    passed &= Testing::expect(
        DlpRasterizer::initialize(four_lane_build, four_lane, &error) && four_lane_build.image_width == 16384 &&
            four_lane_build.image_height == 12288 && four_lane_build.lane_count == 4 &&
            four_lane_build.lane_image_height == 13056,
        "A four-lane build with a 192 MiB grayscale raster must not be rejected by its 768 MiB full-preview estimate");

    auto wide  = four_lane;
    wide.x_max = 2032000;
    DlpBuild oversized_raster;
    passed &= Testing::expect(
        !DlpRasterizer::initialize(oversized_raster, wide, &error) && !oversized_raster.storage,
        "A full grayscale raster exceeding the allocation limit must fail before creating storage even when lanes fit");

    auto tall  = four_lane;
    tall.x_max = 127000;
    tall.y_max = 508000;
    DlpBuild oversized_padded_lane;
    passed &= Testing::expect(
        !DlpRasterizer::initialize(oversized_padded_lane, tall, &error) && !oversized_padded_lane.storage,
        "A 272 MiB padded lane preview must remain rejected when the full grayscale raster is only 64 MiB");

    auto padded_raster        = settings();
    padded_raster.x_min       = 0;
    padded_raster.x_max       = 17623;
    padded_raster.y_min       = 0;
    padded_raster.y_max       = 15232;
    padded_raster.output_mode = DlpOutputMode::kScrolling;
    DlpBuild oversized_stride;
    passed &= Testing::expect(
        !DlpRasterizer::initialize(oversized_stride, padded_raster, &error) && !oversized_stride.storage,
        "Full-raster preflight must include grayscale scanline alignment when pixel count alone fits the allocation "
        "limit");

    QImageReader::setAllocationLimit(32);
    small.x_max = 1;
    DlpBuild oversized_lane;
    passed &= Testing::expect(!DlpRasterizer::initialize(oversized_lane, small, &error) && !oversized_lane.storage &&
                                  error.contains("4096 x 2160"),
                              "Preflight must count a padded static lane even when the full image is tiny");
    small.output_mode = DlpOutputMode::kScrolling;
    passed &= Testing::expect(!DlpRasterizer::initialize(oversized_lane, small, &error) && !oversized_lane.storage &&
                                  error.contains("4096 x 2176"),
                              "Preflight must count padded scrolling blocks before allocating their images");
    QImageReader::setAllocationLimit(previous_limit);
    return passed;
}

bool temporaryRootChild(const QString& root) {
    if (!Testing::expect(QDir::cleanPath(QDir::tempPath()) == root && QFileInfo(root).isDir() &&
                             QFileInfo(root).fileName() == "temp-root",
                         "The temporary-root child must use its isolated test directory"))
        return false;
    if (!Testing::expect(QDir().rmdir(root), "Could not remove the empty test temp root after startup")) return false;

    bool passed = true;
    QString storage_path;
    {
        DlpBuild build;
        QString error;
        const bool initialized = DlpRasterizer::initialize(build, settings(), &error);
        passed &= Testing::expect(initialized, "DLP must recover a deleted temporary root: " + error.toStdString());
        if (initialized) {
            storage_path = build.storage->path();
            passed &= Testing::expect(QFileInfo(storage_path).isDir() && QFileInfo(storage_path).absolutePath() == root,
                                      "DLP output must live under the recovered temporary root");
        }
    }
    passed &= Testing::expect(!storage_path.isEmpty() && !QFileInfo::exists(storage_path),
                              "Releasing a DLP build must remove its temporary child directory");
    if (QFileInfo(root).isDir() &&
        !Testing::expect(QDir().rmdir(root), "DLP storage cleanup must leave an empty temporary root"))
        return false;

    QFile blocker(root);
    if (!Testing::expect(blocker.open(QIODevice::WriteOnly), "Could not create the temporary-root blocker file"))
        return false;
    blocker.close();
    QTemporaryDir probe(root + "/probe-XXXXXX");
    DlpBuild blocked;
    QString error;
    passed &= Testing::expect(!DlpRasterizer::initialize(blocked, settings(), &error) &&
                                  error.contains(QDir::toNativeSeparators(root)) && !probe.isValid() &&
                                  !probe.errorString().isEmpty() && error.contains(probe.errorString()),
                              "An unusable temporary root must report its path and the filesystem failure reason");
    return passed;
}

bool testTemporaryRoot() {
    QTemporaryDir directory(QDir::tempPath() + "/ornlslicer-dlp-temp-test-XXXXXX");
    if (!Testing::expect(directory.isValid(), "Could not create temporary-root fixture directory")) return false;
    const QString root = QDir::cleanPath(directory.filePath("temp-root"));
    if (!Testing::expect(QDir().mkpath(root), "Could not create isolated temporary root")) return false;
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto* name : {"TMPDIR", "TMP", "TEMP"}) environment.insert(QString::fromLatin1(name), root);
    QProcess child;
    child.setProcessEnvironment(environment);
    child.start(QCoreApplication::applicationFilePath(), {"--temporary-root", root});
    if (!child.waitForFinished(30000)) {
        child.kill();
        child.waitForFinished(5000);
        return Testing::expect(false, "Temporary-root child failed: " + child.errorString().toStdString());
    }
    return Testing::expect(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
                           "Temporary-root regression failed:\n" + child.readAllStandardOutput().toStdString() +
                               child.readAllStandardError().toStdString());
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 3 && arguments[1] == "--temporary-root")
        return temporaryRootChild(arguments[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    bool passed = testRaster();
    passed &= testSlicingCancellation();
    passed &= testExport(1);
    passed &= testExport(8);
    passed &= testExportRecovery();
    passed &= testExportPermissions();
    passed &= testGrayscaleEncoding();
    passed &= testLaneCoverage();
    passed &= testFormats();
    passed &= testInvalid();
    passed &= testImageAllocationLimit();
    passed &= testTemporaryRoot();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
