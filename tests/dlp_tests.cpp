#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
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
    const QString output = destination.filePath("build");
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
    return passed;
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
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    bool passed = testRaster();
    passed &= testExport(1);
    passed &= testExport(8);
    passed &= testLaneCoverage();
    passed &= testFormats();
    passed &= testInvalid();
    passed &= testImageAllocationLimit();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
