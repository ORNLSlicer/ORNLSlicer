#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>
#include <QtEndian>
#include <limits>

#include "dlp/dlp_build.h"
#include "test_utils.h"

namespace {
using namespace ORNL;

Polygon rectangle(double left, double bottom, double right, double top) {
    return Polygon {Point(left, bottom), Point(right, bottom), Point(right, top), Point(left, top)};
}

DlpSettings settings(int bit_depth = 1) {
    DlpSettings result;
    result.x_min            = -2;
    result.x_max            = 8;
    result.y_min            = 10;
    result.y_max            = 14;
    result.z_min            = 0;
    result.z_max            = 2000;
    result.layer_height     = 1000;
    result.projection_width = 4;
    result.lane_pixel_width = 4;
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
    passed &= Testing::expect(
        build.image_width == 10 && build.image_height == 4 && build.lane_count == 3 && build.pixel_pitch == 1,
        "DLP layout must use printer spans, square pixels, and rounded-up lanes");
    PolygonList shell {rectangle(-2, 10, 4, 14), rectangle(0, 11, 2, 13), rectangle(6, 10, 8, 11)};
    PolygonList overlap {rectangle(1, 11, 3, 13)};
    QImage image = DlpRasterizer::rasterize({shell, overlap}, build);
    passed &= Testing::expect(!image.isNull(), "DLP raster allocation failed");
    if (image.isNull()) return false;
    passed &= Testing::expect(white(image, 0, 0) && !white(image, 6, 0),
                              "DLP X coordinates must subtract the nonzero printer minimum");
    passed &= Testing::expect(!white(image, 2, 1) && white(image, 3, 1),
                              "DLP holes must remain black, with overlapping mesh geometry unioned");
    passed &= Testing::expect(white(image, 8, 3) && !white(image, 8, 0),
                              "DLP disjoint geometry must retain +Y-up image orientation");
    passed &= Testing::expect(white(image, 3, 0) && white(image, 4, 0),
                              "DLP geometry must remain filled on both sides of a lane seam");

    auto exact  = settings();
    exact.x_max = 10;
    passed &= Testing::expect(
        DlpRasterizer::initialize(build, exact, &error) && build.lane_count == 3 && build.image_width == 12,
        "An exact physical lane multiple must not create an extra lane");
    auto fractional  = settings();
    fractional.x_max = 7.2;
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
        passed &= Testing::expect(saved.width() == 4 && saved.height() == 4,
                                  "Every DLP lane image must have identical full projector dimensions");
        for (int y = 0; y < saved.height(); ++y) {
            for (int x = 0; x < saved.width(); ++x) {
                const int full_x = lane * 4 + x;
                passed &= Testing::expect(white(saved, x, y) == (full_x < image.width() && white(image, full_x, y)),
                                          "Concatenated DLP lanes must reproduce the full image with black padding");
            }
        }
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
    const bool csv_valid     = content.startsWith("layer,z_mm,lane,file,empty\n") &&
                               content.contains("1,0.5,0,Layer 1/Layer_1_Full.bmp,0\n") &&
                               content.contains("1,0.5,2,Layer 1/Layer1_2.bmp,1\n") &&
                               content.contains("2,1.5,3,Layer 2/Layer2_3.bmp,1\n") && content.count('\n') == 9;
    passed &=
        Testing::expect(csv_valid, "DLP CSV must list every full image/lane, Z in millimeters, and pixel emptiness");
    if (!csv_valid) std::cerr << "Actual DLP CSV (" << bit_depth << "-bit):\n" << content.toStdString();
    passed &= Testing::expect(!DlpExporter::exportBuild(build, output, &error),
                              "DLP export must reject nonempty output directories");
    QFile unchanged(output + "/build.csv");
    passed &= Testing::expect(unchanged.open(QIODevice::ReadOnly) && unchanged.readAll() == content,
                              "Rejected DLP exports must leave existing files untouched");
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
    passed &= Testing::expect(!DlpRasterizer::initialize(oversized, project, &error) && !oversized.storage,
                              "The supplied project's oversized images must be rejected before allocating storage.");
    passed &= Testing::expect(
        error.contains("245760") && error.contains("87040") && error.contains("Projection Width in Pixels"),
        "Oversized-image errors must identify the requested dimensions and resolution setting.");

    project.lane_pixel_width = 32;
    DlpBuild reduced;
    passed &= Testing::expect(DlpRasterizer::initialize(reduced, project, &error) && reduced.image_width == 7680 &&
                                  reduced.image_height == 2720 && reduced.lane_count == 240,
                              "The supplied project's lower-resolution layout must remain supported.");

    auto padded_lane             = settings();
    padded_lane.x_min            = 0;
    padded_lane.x_max            = 1;
    padded_lane.y_min            = 0;
    padded_lane.y_max            = 1024;
    padded_lane.projection_width = 100000;
    padded_lane.lane_pixel_width = 100000;
    DlpBuild oversized_lane;
    passed &=
        Testing::expect(!DlpRasterizer::initialize(oversized_lane, padded_lane, &error) && !oversized_lane.storage,
                        "The allocation limit must also reject oversized padded lanes on a small full canvas.");
    QImageReader::setAllocationLimit(previous_limit);
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    bool passed = testRaster();
    passed &= testExport(1);
    passed &= testExport(8);
    passed &= testInvalid();
    passed &= testImageAllocationLimit();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
