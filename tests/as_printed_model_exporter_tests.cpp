#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSharedPointer>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>

#include "gcode/as_printed_model_exporter.h"
#include "geometry/point.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
constexpr float kLength = 10.0f;
constexpr float kWidth = 2.0f;
constexpr float kHeight = 1.0f;
constexpr float kTolerance = 0.001f;

struct Bounds {
    QVector3D min = QVector3D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max());
    QVector3D max = QVector3D(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest());
};

bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

ORNL::Point pointFromMm(float x, float y, float z = 0.0f) {
    return ORNL::Point(x * ORNL::mm(), y * ORNL::mm(), z * ORNL::mm());
}

Bounds boundsFor(const std::vector<ORNL::AsPrintedModelExporter::Triangle>& triangles) {
    Bounds bounds;
    for (const ORNL::AsPrintedModelExporter::Triangle& triangle : triangles) {
        for (const QVector3D& vertex : {triangle.a, triangle.b, triangle.c}) {
            bounds.min.setX(std::min(bounds.min.x(), vertex.x()));
            bounds.min.setY(std::min(bounds.min.y(), vertex.y()));
            bounds.min.setZ(std::min(bounds.min.z(), vertex.z()));
            bounds.max.setX(std::max(bounds.max.x(), vertex.x()));
            bounds.max.setY(std::max(bounds.max.y(), vertex.y()));
            bounds.max.setZ(std::max(bounds.max.z(), vertex.z()));
        }
    }
    return bounds;
}

bool near(float actual, float expected, float tolerance = kTolerance) {
    return std::abs(actual - expected) <= tolerance;
}

QSharedPointer<ORNL::SegmentBase> makeLineSegment(ORNL::SegmentDisplayType type, uint line_number,
                                                  float y_offset = 0.0f, bool deposition_active = true);

QSharedPointer<ORNL::SegmentBase> makeArcSegment(const ORNL::Point& start, const ORNL::Point& end,
                                                 const ORNL::Point& center, uint line_number,
                                                 bool deposition_active = true);

QSharedPointer<ORNL::SegmentBase> makeLineSegment(const ORNL::Point& start, const ORNL::Point& end, uint line_number,
                                                  ORNL::SegmentDisplayType type = ORNL::SegmentDisplayType::kLine,
                                                  bool deposition_active = true) {
    const float scale = ORNL::Constants::OpenGL::kObjectToView;
    auto segment = QSharedPointer<ORNL::LineSegment>::create(start * scale, end * scale);
    segment->setDisplayInfo(kWidth * ORNL::mm() * scale, start.distance(end)() * scale, kHeight * ORNL::mm() * scale,
                            type, QColor(255, 255, 255), line_number, 0);
    segment->setDepositionActive(deposition_active);
    return segment;
}

QSharedPointer<ORNL::SegmentBase> makeLineSegment(ORNL::SegmentDisplayType type, uint line_number, float y_offset,
                                                  bool deposition_active) {
    return makeLineSegment(pointFromMm(0.0f, y_offset), pointFromMm(kLength, y_offset), line_number, type,
                           deposition_active);
}

QSharedPointer<ORNL::SegmentBase> makeArcSegment(const ORNL::Point& start, const ORNL::Point& end,
                                                 const ORNL::Point& center, uint line_number, bool deposition_active) {
    const float scale = ORNL::Constants::OpenGL::kObjectToView;
    auto segment = QSharedPointer<ORNL::ArcSegment>::create(start * scale, end * scale, center * scale, true);
    segment->setDisplayInfo(kWidth * ORNL::mm() * scale, start.distance(end)() * scale, kHeight * ORNL::mm() * scale,
                            ORNL::SegmentDisplayType::kLine, QColor(255, 255, 255), line_number, 0);
    segment->setDepositionActive(deposition_active);
    return segment;
}

int countFacets(const QString& text) { return text.count(QStringLiteral("facet normal")); }

bool readFile(const QString& path, QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    text = in.readAll();
    return true;
}

bool readBytes(const QString& path, QByteArray& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    bytes = file.readAll();
    return true;
}

quint32 binaryFacetCount(const QByteArray& bytes) {
    if (bytes.size() < 84)
        return 0;

    const auto* data = reinterpret_cast<const uchar*>(bytes.constData() + 80);
    return static_cast<quint32>(data[0]) | (static_cast<quint32>(data[1]) << 8) |
           (static_cast<quint32>(data[2]) << 16) | (static_cast<quint32>(data[3]) << 24);
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    bool passed = true;

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> printable_only;
    printable_only.push_back({makeLineSegment(ORNL::SegmentDisplayType::kLine, 1)});

    const auto printable_triangles = ORNL::AsPrintedModelExporter::generateTriangles(printable_only);
    passed &= expect(!printable_triangles.empty(), "Expected printable bead mesh triangles.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> mixed_segments;
    mixed_segments.push_back({makeLineSegment(ORNL::SegmentDisplayType::kLine, 1),
                              makeLineSegment(ORNL::SegmentDisplayType::kSupport, 2, 20.0f),
                              makeLineSegment(ORNL::SegmentDisplayType::kTravel, 3, 40.0f)});

    const auto default_triangles = ORNL::AsPrintedModelExporter::generateTriangles(mixed_segments);
    passed &= expect(default_triangles.size() == printable_triangles.size(),
                     "Expected support and travel segments to be excluded by default.");

    ORNL::AsPrintedModelExporter::Options include_all;
    include_all.include_support = true;
    include_all.include_travel = true;
    const auto all_triangles = ORNL::AsPrintedModelExporter::generateTriangles(mixed_segments, include_all);
    passed &= expect(all_triangles.size() == printable_triangles.size() * 3,
                     "Expected optional support and travel output to include all three segments.");

    const Bounds printable_bounds = boundsFor(printable_triangles);
    passed &= expect(near(printable_bounds.min.x(), 0.0f), "Expected STL vertices to start at local X zero.");
    passed &= expect(near(printable_bounds.min.y(), 0.0f), "Expected STL vertices to start at local Y zero.");
    passed &= expect(near(printable_bounds.min.z(), 0.0f), "Expected STL vertices to start at local Z zero.");
    passed &= expect(near(printable_bounds.max.x() - printable_bounds.min.x(), kLength),
                     "Expected bead length to be written in millimeters.");

    QSharedPointer<ORNL::SegmentBase> radial_segment =
        makeLineSegment(pointFromMm(10.0f, -5.0f), pointFromMm(10.0f, 5.0f), 4);
    radial_segment->setCylindricalBeadCenter(pointFromMm(0.0f, 0.0f) * ORNL::Constants::OpenGL::kObjectToView);
    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> radial_segments;
    radial_segments.push_back({radial_segment});
    const auto radial_triangles = ORNL::AsPrintedModelExporter::generateTriangles(radial_segments);
    const Bounds radial_bounds = boundsFor(radial_triangles);
    passed &= expect(near(radial_bounds.max.x() - radial_bounds.min.x(), kHeight),
                     "Expected cylindrical bead radial thickness to match bead height.");
    passed &= expect(near(radial_bounds.max.z() - radial_bounds.min.z(), kWidth),
                     "Expected cylindrical bead vertical span to match bead width.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> lfam_style_segments;
    const ORNL::Point lfam_start(120.0f * ORNL::in(), 42.5f * ORNL::in(), -15.25f * ORNL::in());
    const ORNL::Point lfam_end(136.0f * ORNL::in(), 42.5f * ORNL::in(), -15.25f * ORNL::in());
    lfam_style_segments.push_back({makeLineSegment(lfam_start, lfam_end, 1)});
    const auto lfam_triangles = ORNL::AsPrintedModelExporter::generateTriangles(lfam_style_segments);
    const Bounds lfam_bounds = boundsFor(lfam_triangles);
    passed &= expect(near(lfam_bounds.min.x(), 0.0f), "Expected LFAM-style export to use local X zero.");
    passed &= expect(near(lfam_bounds.max.x() - lfam_bounds.min.x(), 16.0f * 25.4f, 0.01f),
                     "Expected LFAM-style inch coordinates to export as millimeters.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> filtered_segments;
    filtered_segments.push_back(
        {makeLineSegment(pointFromMm(1000.0f, 1000.0f, 1000.0f), pointFromMm(1000.0f, 1000.0f, 1000.0f), 1,
                         ORNL::SegmentDisplayType::kLine, false),
         makeLineSegment(pointFromMm(-500.0f, 0.0f), pointFromMm(-400.0f, 0.0f), 2, ORNL::SegmentDisplayType::kLine,
                         false),
         makeLineSegment(ORNL::SegmentDisplayType::kLine, 3)});
    const auto filtered_triangles = ORNL::AsPrintedModelExporter::generateTriangles(filtered_segments);
    passed &= expect(filtered_triangles.size() == printable_triangles.size(),
                     "Expected non-deposition and degenerate segments to be skipped by default.");
    const Bounds filtered_bounds = boundsFor(filtered_triangles);
    passed &= expect(near(filtered_bounds.max.x() - filtered_bounds.min.x(), kLength),
                     "Expected skipped segments not to expand the exported bounds.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> full_circle_arc_segments;
    const ORNL::Point arc_start = pointFromMm(10.0f, 0.0f);
    full_circle_arc_segments.push_back({makeArcSegment(arc_start, arc_start, pointFromMm(0.0f, 0.0f), 1)});
    const auto full_circle_arc_triangles = ORNL::AsPrintedModelExporter::generateTriangles(full_circle_arc_segments);
    passed &= expect(!full_circle_arc_triangles.empty(),
                     "Expected same-endpoint full-circle arcs to be exported as bead mesh triangles.");
    const Bounds full_circle_arc_bounds = boundsFor(full_circle_arc_triangles);
    passed &= expect(near(full_circle_arc_bounds.max.z() - full_circle_arc_bounds.min.z(), kHeight),
                     "Expected arc bead mesh height to match the display bead height.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> corner_segments;
    corner_segments.push_back({makeLineSegment(pointFromMm(0.0f, 0.0f, 0.0f), pointFromMm(10.0f, 0.0f, 0.0f), 1),
                               makeLineSegment(pointFromMm(10.0f, 0.0f, 0.0f), pointFromMm(10.0f, 10.0f, 0.0f), 2)});

    ORNL::AsPrintedModelExporter::Options without_blends;
    without_blends.blend_corners = false;
    const auto corner_triangles_without_blends =
        ORNL::AsPrintedModelExporter::generateTriangles(corner_segments, without_blends);
    const auto corner_triangles_with_blends = ORNL::AsPrintedModelExporter::generateTriangles(corner_segments);
    passed &= expect(corner_triangles_with_blends.size() > corner_triangles_without_blends.size(),
                     "Expected connected corner segments to add blend triangles.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> closed_corner_segments;
    closed_corner_segments.push_back(
        {makeLineSegment(pointFromMm(0.0f, 0.0f, 0.0f), pointFromMm(10.0f, 0.0f, 0.0f), 1),
         makeLineSegment(pointFromMm(10.0f, 0.0f, 0.0f), pointFromMm(5.0f, 8.0f, 0.0f), 2),
         makeLineSegment(pointFromMm(5.0f, 8.0f, 0.0f), pointFromMm(0.0f, 0.0f, 0.0f), 3)});
    const auto open_two_segment_blend_count =
        corner_triangles_with_blends.size() - corner_triangles_without_blends.size();
    const auto closed_corner_triangles_without_blends =
        ORNL::AsPrintedModelExporter::generateTriangles(closed_corner_segments, without_blends);
    const auto closed_corner_triangles_with_blends =
        ORNL::AsPrintedModelExporter::generateTriangles(closed_corner_segments);
    passed &= expect(closed_corner_triangles_with_blends.size() ==
                         closed_corner_triangles_without_blends.size() + (open_two_segment_blend_count * 3),
                     "Expected closed loops to blend each corner, including the start/end corner.");

    QTemporaryDir temp_dir;
    passed &= expect(temp_dir.isValid(), "Could not create temporary directory.");
    if (temp_dir.isValid()) {
        const QString binary_stl_path = temp_dir.path() + "/as_printed_binary.stl";
        QString error;
        passed &= expect(ORNL::AsPrintedModelExporter::writeStl(binary_stl_path, printable_only, &error),
                         ("Could not write binary STL: " + error.toStdString()));

        QByteArray binary_stl_bytes;
        passed &= expect(readBytes(binary_stl_path, binary_stl_bytes), "Could not read generated binary STL.");
        passed &= expect(binaryFacetCount(binary_stl_bytes) == static_cast<quint32>(printable_triangles.size()),
                         "Generated binary STL facet count did not match generated triangles.");
        passed &= expect(binary_stl_bytes.size() == 84 + (static_cast<int>(printable_triangles.size()) * 50),
                         "Generated binary STL size did not match the binary STL triangle layout.");

        const QString ascii_stl_path = temp_dir.path() + "/as_printed_ascii.stl";
        ORNL::AsPrintedModelExporter::Options ascii_options;
        ascii_options.format = ORNL::AsPrintedModelExporter::StlFormat::kAscii;
        passed &= expect(ORNL::AsPrintedModelExporter::writeStl(ascii_stl_path, printable_only, &error, ascii_options),
                         ("Could not write ASCII STL: " + error.toStdString()));
        QString stl_text;
        passed &= expect(readFile(ascii_stl_path, stl_text), "Could not read generated ASCII STL.");
        passed &= expect(stl_text.startsWith(QStringLiteral("solid ornlslicer_as_printed")),
                         "Generated ASCII STL did not contain the expected header.");
        passed &= expect(countFacets(stl_text) == static_cast<int>(printable_triangles.size()),
                         "Generated ASCII STL facet count did not match generated triangles.");
        passed &= expect(QFileInfo(binary_stl_path).size() < QFileInfo(ascii_stl_path).size(),
                         "Expected binary STL to be smaller than ASCII STL.");
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
