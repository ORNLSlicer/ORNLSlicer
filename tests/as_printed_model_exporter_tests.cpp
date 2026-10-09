#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QQuaternion>
#include <QSharedPointer>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "configs/settings_base.h"
#include "gcode/as_printed_model_exporter.h"
#include "gcode/gcode_segment_filter.h"
#include "geometry/point.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
constexpr float kLength             = 10.0f;
constexpr float kWidth              = 2.0f;
constexpr float kHeight             = 1.0f;
constexpr float kCenterlineDiameter = 0.1f;
constexpr float kTolerance          = 1.0e-3f;

struct Bounds {
    QVector3D min = QVector3D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max());
    QVector3D max = QVector3D(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest());
};

ORNL::Point pointFromMm(float x, float y, float z = 0.0f) {
    return ORNL::Point(x * ORNL::mm(), y * ORNL::mm(), z * ORNL::mm());
}

ORNL::Point pointFromIn(float x, float y, float z = 0.0f) {
    return ORNL::Point(x * ORNL::in(), y * ORNL::in(), z * ORNL::in());
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

QVector3D normalFor(const ORNL::AsPrintedModelExporter::Triangle& triangle) {
    QVector3D normal = QVector3D::crossProduct(triangle.b - triangle.a, triangle.c - triangle.a);
    if (normal.lengthSquared() > std::numeric_limits<float>::epsilon()) { normal.normalize(); }
    return normal;
}

QSharedPointer<ORNL::SegmentBase> makeLineSegment(ORNL::SegmentDisplayType type, uint line_number,
                                                  float y_offset = 0.0f, bool deposition_active = true);

QSharedPointer<ORNL::SegmentBase> makeArcSegment(const ORNL::Point& start, const ORNL::Point& end,
                                                 const ORNL::Point& center, uint line_number,
                                                 bool deposition_active = true);
QSharedPointer<ORNL::SegmentBase> makeTaggedLineSegment(const QString& comment, uint line_number,
                                                        float y_offset = 0.0f);
QSharedPointer<ORNL::SegmentBase> makeTaggedLineSegment(const ORNL::Point& start, const ORNL::Point& end,
                                                        const QString& comment, uint line_number);
QVector<QSharedPointer<ORNL::SegmentBase>> makeTaggedSquare(const QString& comment, uint first_line_number,
                                                            float min_xy = 0.0f, float max_xy = kLength);
QVector<QSharedPointer<ORNL::SegmentBase>> makeTaggedLoop(const QVector<ORNL::Point>& points, const QString& comment,
                                                          uint first_line_number);

QSharedPointer<ORNL::SegmentBase> makeLineSegment(const ORNL::Point& start, const ORNL::Point& end, uint line_number,
                                                  ORNL::SegmentDisplayType type = ORNL::SegmentDisplayType::kLine,
                                                  bool deposition_active        = true) {
    const float scale = ORNL::Constants::OpenGL::kObjectToView;
    auto segment      = QSharedPointer<ORNL::LineSegment>::create(start * scale, end * scale);
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
    auto segment      = QSharedPointer<ORNL::ArcSegment>::create(start * scale, end * scale, center * scale, true);
    segment->setDisplayInfo(kWidth * ORNL::mm() * scale, start.distance(end)() * scale, kHeight * ORNL::mm() * scale,
                            ORNL::SegmentDisplayType::kLine, QColor(255, 255, 255), line_number, 0);
    segment->setDepositionActive(deposition_active);
    return segment;
}

QSharedPointer<ORNL::SegmentBase> makeTaggedLineSegment(const QString& comment, uint line_number, float y_offset) {
    QSharedPointer<ORNL::SegmentBase> segment = makeLineSegment(ORNL::SegmentDisplayType::kLine, line_number, y_offset);
    segment->m_segment_info_meta.type         = comment;
    return segment;
}

QSharedPointer<ORNL::SegmentBase> makeTaggedLineSegment(const ORNL::Point& start, const ORNL::Point& end,
                                                        const QString& comment, uint line_number) {
    QSharedPointer<ORNL::SegmentBase> segment =
        makeLineSegment(start, end, line_number, ORNL::SegmentDisplayType::kLine);
    segment->m_segment_info_meta.type = comment;
    return segment;
}

QVector<QSharedPointer<ORNL::SegmentBase>> makeTaggedSquare(const QString& comment, uint first_line_number,
                                                            float min_xy, float max_xy) {
    const ORNL::Point bottom_left  = pointFromMm(min_xy, min_xy);
    const ORNL::Point bottom_right = pointFromMm(max_xy, min_xy);
    const ORNL::Point top_right    = pointFromMm(max_xy, max_xy);
    const ORNL::Point top_left     = pointFromMm(min_xy, max_xy);

    return {makeTaggedLineSegment(bottom_left, bottom_right, comment, first_line_number),
            makeTaggedLineSegment(bottom_right, top_right, comment, first_line_number + 1),
            makeTaggedLineSegment(top_right, top_left, comment, first_line_number + 2),
            makeTaggedLineSegment(top_left, bottom_left, comment, first_line_number + 3)};
}

QVector<QSharedPointer<ORNL::SegmentBase>> makeTaggedLoop(const QVector<ORNL::Point>& points, const QString& comment,
                                                          uint first_line_number) {
    QVector<QSharedPointer<ORNL::SegmentBase>> loop;
    loop.reserve(points.size());
    for (int i = 0; i < points.size(); ++i) {
        loop.push_back(
            makeTaggedLineSegment(points[i], points[(i + 1) % points.size()], comment, first_line_number + i));
    }

    return loop;
}

int countFacets(const QString& text) {
    return text.count(QStringLiteral("facet normal"));
}

bool readFile(const QString& path, QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    text = in.readAll();
    return true;
}

bool readBytes(const QString& path, QByteArray& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    bytes = file.readAll();
    return true;
}

quint32 binaryFacetCount(const QByteArray& bytes) {
    if (bytes.size() < 84) return 0;

    const auto* data = reinterpret_cast<const uchar*>(bytes.constData() + 80);
    return static_cast<quint32>(data[0]) | (static_cast<quint32>(data[1]) << 8) |
           (static_cast<quint32>(data[2]) << 16) | (static_cast<quint32>(data[3]) << 24);
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    bool passed = true;

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> printable_only;
    printable_only.push_back({makeLineSegment(ORNL::SegmentDisplayType::kLine, 1)});

    const auto printable_triangles = ORNL::AsPrintedModelExporter::generateTriangles(printable_only);
    passed &= ORNL::Testing::expect(!printable_triangles.empty(), "Expected printable bead mesh triangles.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> mixed_segments;
    mixed_segments.push_back({makeLineSegment(ORNL::SegmentDisplayType::kLine, 1),
                              makeLineSegment(ORNL::SegmentDisplayType::kSupport, 2, 20.0f),
                              makeLineSegment(ORNL::SegmentDisplayType::kTravel, 3, 40.0f)});

    const auto default_triangles = ORNL::AsPrintedModelExporter::generateTriangles(mixed_segments);
    passed &= ORNL::Testing::expect(default_triangles.size() == printable_triangles.size(),
                                    "Expected support and travel segments to be excluded by default.");

    ORNL::AsPrintedModelExporter::Options include_all;
    include_all.include_support = true;
    include_all.include_travel  = true;
    const auto all_triangles    = ORNL::AsPrintedModelExporter::generateTriangles(mixed_segments, include_all);
    passed &= ORNL::Testing::expect(all_triangles.size() == printable_triangles.size() * 3,
                                    "Expected optional support and travel output to include all three segments.");

    ORNL::AsPrintedModelExporter::Options without_blends;
    without_blends.blend_corners = false;
    ORNL::AsPrintedModelExporter::Options external_only_options;
    external_only_options.blend_corners = false;
    external_only_options.external_only = true;
    const auto printable_triangles_without_blends =
        ORNL::AsPrintedModelExporter::generateTriangles(printable_only, without_blends);

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> stacked_open_segments;
    stacked_open_segments.push_back({makeTaggedLineSegment(ORNL::Constants::RegionTypeStrings::kInfill, 4)});
    stacked_open_segments.push_back({makeTaggedLineSegment(ORNL::Constants::RegionTypeStrings::kInfill, 5)});
    stacked_open_segments.push_back({makeTaggedLineSegment(ORNL::Constants::RegionTypeStrings::kInfill, 6)});
    ORNL::GCodeSegmentFilter::tagInternalSegments(stacked_open_segments);
    const auto stacked_open_external_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(stacked_open_segments, external_only_options);
    passed &=
        ORNL::Testing::expect(stacked_open_external_triangles.size() == printable_triangles_without_blends.size() * 2,
                              "Expected external-only STL output to skip covered middle open beads.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> stacked_inset_boundary_segments;
    stacked_inset_boundary_segments.push_back(makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, 7));
    stacked_inset_boundary_segments.push_back(makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, 11));
    stacked_inset_boundary_segments.push_back(makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, 15));
    const auto inset_boundary_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(stacked_inset_boundary_segments, without_blends);
    ORNL::GCodeSegmentFilter::tagInternalSegments(stacked_inset_boundary_segments);
    const auto external_inset_boundary_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(stacked_inset_boundary_segments, external_only_options);
    passed &= ORNL::Testing::expect(external_inset_boundary_triangles.size() == inset_boundary_triangles.size(),
                                    "Expected external-only STL output to keep outermost closed inset boundaries.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> nested_inset_segments;
    QVector<QSharedPointer<ORNL::SegmentBase>> nested_middle_inset;
    for (int layer_index = 0; layer_index < 3; ++layer_index) {
        QVector<QSharedPointer<ORNL::SegmentBase>> layer =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, 19 + (layer_index * 9));
        QVector<QSharedPointer<ORNL::SegmentBase>> inset =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, 23 + (layer_index * 9), 2.0f, 8.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> infill = {
            makeTaggedLineSegment(pointFromMm(3.0f, 5.0f), pointFromMm(7.0f, 5.0f),
                                  ORNL::Constants::RegionTypeStrings::kInfill, 27 + (layer_index * 9))};
        if (layer_index == 1) { nested_middle_inset = inset; }

        layer += inset;
        layer += infill;
        nested_inset_segments.push_back(layer);
    }
    ORNL::GCodeSegmentFilter::tagInternalSegments(nested_inset_segments);
    for (const QSharedPointer<ORNL::SegmentBase>& segment : nested_middle_inset) {
        passed &=
            ORNL::Testing::expect(static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
                                  "Expected covered nested middle-layer insets to be hidden with internal beads.");
    }
    const auto nested_inset_external_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(nested_inset_segments, external_only_options);
    passed &=
        ORNL::Testing::expect(nested_inset_external_triangles.size() == printable_triangles_without_blends.size() * 22,
                              "Expected external-only STL output to hide covered nested middle-layer insets.");

    const QVector<ORNL::Point> hex_perimeter   = {pointFromIn(112.1960f, 42.5246f), pointFromIn(116.0770f, 35.7541f),
                                                  pointFromIn(123.8800f, 35.7294f), pointFromIn(127.8040f, 42.4753f),
                                                  pointFromIn(123.9230f, 49.2458f), pointFromIn(116.1200f, 49.2706f)};
    const QVector<ORNL::Point> hex_outer_inset = {pointFromIn(112.5890f, 42.5233f), pointFromIn(116.2740f, 36.0935f),
                                                  pointFromIn(123.6850f, 36.0700f), pointFromIn(127.4110f, 42.4767f),
                                                  pointFromIn(123.7260f, 48.9065f), pointFromIn(116.3150f, 48.9299f)};
    const QVector<ORNL::Point> hex_inner_inset = {pointFromIn(112.9820f, 42.5221f), pointFromIn(116.4710f, 36.4329f),
                                                  pointFromIn(123.4900f, 36.4107f), pointFromIn(127.0190f, 42.4779f),
                                                  pointFromIn(123.5290f, 48.5671f), pointFromIn(116.5100f, 48.5893f)};

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> hex_segments;
    QVector<QSharedPointer<ORNL::SegmentBase>> hex_middle_insets;
    for (int layer_index = 0; layer_index < 3; ++layer_index) {
        QVector<QSharedPointer<ORNL::SegmentBase>> layer =
            makeTaggedLoop(hex_perimeter, ORNL::Constants::RegionTypeStrings::kPerimeter, 43 + (layer_index * 18));
        QVector<QSharedPointer<ORNL::SegmentBase>> outer_inset =
            makeTaggedLoop(hex_outer_inset, ORNL::Constants::RegionTypeStrings::kInset, 49 + (layer_index * 18));
        QVector<QSharedPointer<ORNL::SegmentBase>> inner_inset =
            makeTaggedLoop(hex_inner_inset, ORNL::Constants::RegionTypeStrings::kInset, 55 + (layer_index * 18));
        if (layer_index == 1) { hex_middle_insets = outer_inset; }

        layer += outer_inset;
        layer += inner_inset;
        hex_segments.push_back(layer);
    }

    ORNL::GCodeSegmentFilter::tagInternalSegments(hex_segments);
    for (int i = 0; i < 6; ++i) {
        passed &= ORNL::Testing::expect(
            !static_cast<bool>(hex_segments[1][i]->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected middle-layer hexagon perimeter to remain external.");
    }
    for (const QSharedPointer<ORNL::SegmentBase>& segment : hex_middle_insets) {
        passed &=
            ORNL::Testing::expect(static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
                                  "Expected covered middle-layer hexagon insets to be hidden with internal beads.");
    }
    for (int i = 12; i < 18; ++i) {
        passed &= ORNL::Testing::expect(
            !static_cast<bool>(hex_segments[1][i]->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected middle-layer hexagon inner exposed inset to remain external.");
    }
    const auto hex_external_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(hex_segments, external_only_options);
    passed &= ORNL::Testing::expect(hex_external_triangles.size() == printable_triangles_without_blends.size() * 48,
                                    "Expected external-only STL output for hexagon to keep perimeters and inner "
                                    "exposed insets, and hide covered insets.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> through_hole_segments;
    QVector<QSharedPointer<ORNL::SegmentBase>> through_hole_middle_insets;
    for (int layer_index = 0; layer_index < 3; ++layer_index) {
        QVector<QSharedPointer<ORNL::SegmentBase>> layer =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, 60 + (layer_index * 12), 0.0f, 20.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> outer_inset =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, 64 + (layer_index * 12), 2.0f, 18.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> hole_perimeter =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, 68 + (layer_index * 12), 6.0f, 14.0f);
        if (layer_index == 1) { through_hole_middle_insets = outer_inset; }

        layer += outer_inset;
        layer += hole_perimeter;
        through_hole_segments.push_back(layer);
    }

    ORNL::GCodeSegmentFilter::tagInternalSegments(through_hole_segments);
    for (int i = 0; i < 4; ++i) {
        passed &= ORNL::Testing::expect(
            !static_cast<bool>(through_hole_segments[1][i]->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected middle-layer through-hole outer perimeter to remain external.");
    }
    for (const QSharedPointer<ORNL::SegmentBase>& segment : through_hole_middle_insets) {
        passed &= ORNL::Testing::expect(
            static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected covered middle-layer inset around through-hole to be hidden with internal beads.");
    }
    for (int i = 8; i < 12; ++i) {
        passed &= ORNL::Testing::expect(
            !static_cast<bool>(through_hole_segments[1][i]->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected middle-layer inner hole perimeter to remain external.");
    }
    const auto through_hole_external_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(through_hole_segments, external_only_options);
    passed &= ORNL::Testing::expect(
        through_hole_external_triangles.size() == printable_triangles_without_blends.size() * 32,
        "Expected external-only STL output for through-hole part to keep outer and hole perimeters and hide covered "
        "insets.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> concentric_segments;
    QVector<QSharedPointer<ORNL::SegmentBase>> concentric_middle_insets;
    QVector<QSharedPointer<ORNL::SegmentBase>> concentric_middle_perimeters;
    for (int layer_index = 0; layer_index < 3; ++layer_index) {
        const uint base_line = 100 + (layer_index * 24);
        QVector<QSharedPointer<ORNL::SegmentBase>> outer_perimeter =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, base_line, 0.0f, 40.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> outer_inset =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, base_line + 4, 2.0f, 38.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> outer_hole =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, base_line + 8, 8.0f, 32.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> inner_perimeter =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, base_line + 12, 12.0f, 28.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> inner_inset =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kInset, base_line + 16, 14.0f, 26.0f);
        QVector<QSharedPointer<ORNL::SegmentBase>> inner_hole =
            makeTaggedSquare(ORNL::Constants::RegionTypeStrings::kPerimeter, base_line + 20, 18.0f, 22.0f);
        if (layer_index == 1) {
            concentric_middle_insets     = outer_inset + inner_inset;
            concentric_middle_perimeters = outer_perimeter + outer_hole + inner_perimeter + inner_hole;
        }

        QVector<QSharedPointer<ORNL::SegmentBase>> layer;
        layer += outer_perimeter;
        layer += outer_inset;
        layer += outer_hole;
        layer += inner_perimeter;
        layer += inner_inset;
        layer += inner_hole;
        concentric_segments.push_back(layer);
    }

    ORNL::GCodeSegmentFilter::tagInternalSegments(concentric_segments);
    for (const QSharedPointer<ORNL::SegmentBase>& segment : concentric_middle_perimeters) {
        passed &= ORNL::Testing::expect(
            !static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected middle-layer external and internal object perimeter walls to remain external.");
    }
    for (const QSharedPointer<ORNL::SegmentBase>& segment : concentric_middle_insets) {
        passed &= ORNL::Testing::expect(
            static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected covered middle-layer insets in concentric objects to be hidden with internal beads.");
    }
    const auto concentric_external_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(concentric_segments, external_only_options);
    passed &= ORNL::Testing::expect(
        concentric_external_triangles.size() == printable_triangles_without_blends.size() * 64,
        "Expected external-only STL output for concentric objects to keep outer-facing and inner-facing perimeters "
        "and hide covered insets.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> hex_with_rect_segments;
    QVector<QSharedPointer<ORNL::SegmentBase>> hex_with_rect_middle_insets;
    QVector<QSharedPointer<ORNL::SegmentBase>> hex_inner_insets_middle;
    for (int layer_index = 0; layer_index < 3; ++layer_index) {
        const uint base_line = 200 + (layer_index * 30);
        QVector<QSharedPointer<ORNL::SegmentBase>> hex_perm =
            makeTaggedLoop(hex_perimeter, ORNL::Constants::RegionTypeStrings::kPerimeter, base_line);
        QVector<QSharedPointer<ORNL::SegmentBase>> hex_out_inset =
            makeTaggedLoop(hex_outer_inset, ORNL::Constants::RegionTypeStrings::kInset, base_line + 6);
        QVector<QSharedPointer<ORNL::SegmentBase>> hex_in_inset =
            makeTaggedLoop(hex_inner_inset, ORNL::Constants::RegionTypeStrings::kInset, base_line + 12);

        const QVector<ORNL::Point> rect_perm_pts  = {pointFromIn(118.0f, 41.0f), pointFromIn(122.0f, 41.0f),
                                                     pointFromIn(122.0f, 44.0f), pointFromIn(118.0f, 44.0f)};
        const QVector<ORNL::Point> rect_inset_pts = {pointFromIn(118.5f, 41.5f), pointFromIn(121.5f, 41.5f),
                                                     pointFromIn(121.5f, 43.5f), pointFromIn(118.5f, 43.5f)};
        const QVector<ORNL::Point> rect_hole_pts  = {pointFromIn(119.0f, 42.0f), pointFromIn(121.0f, 42.0f),
                                                     pointFromIn(121.0f, 43.0f), pointFromIn(119.0f, 43.0f)};

        QVector<QSharedPointer<ORNL::SegmentBase>> rect_perm =
            makeTaggedLoop(rect_perm_pts, ORNL::Constants::RegionTypeStrings::kPerimeter, base_line + 18);
        QVector<QSharedPointer<ORNL::SegmentBase>> rect_inset =
            makeTaggedLoop(rect_inset_pts, ORNL::Constants::RegionTypeStrings::kInset, base_line + 22);
        QVector<QSharedPointer<ORNL::SegmentBase>> rect_hole =
            makeTaggedLoop(rect_hole_pts, ORNL::Constants::RegionTypeStrings::kPerimeter, base_line + 26);

        if (layer_index == 1) {
            hex_with_rect_middle_insets = hex_out_inset + rect_inset;
            hex_inner_insets_middle     = hex_in_inset;
        }

        QVector<QSharedPointer<ORNL::SegmentBase>> layer;
        layer += hex_perm;
        layer += hex_out_inset;
        layer += hex_in_inset;
        layer += rect_perm;
        layer += rect_inset;
        layer += rect_hole;
        hex_with_rect_segments.push_back(layer);
    }

    ORNL::GCodeSegmentFilter::tagInternalSegments(hex_with_rect_segments);
    for (const QSharedPointer<ORNL::SegmentBase>& segment : hex_inner_insets_middle) {
        passed &= ORNL::Testing::expect(
            !static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected hexagonal inner pointing wall on the same layer as nested rectangle to remain external.");
    }
    for (const QSharedPointer<ORNL::SegmentBase>& segment : hex_with_rect_middle_insets) {
        passed &= ORNL::Testing::expect(
            static_cast<bool>(segment->displayType() & ORNL::SegmentDisplayType::kInternal),
            "Expected covered middle-layer insets in hexagon with nested rectangle to be hidden with internal beads.");
    }
    const auto hex_with_rect_external_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(hex_with_rect_segments, external_only_options);
    passed &= ORNL::Testing::expect(
        hex_with_rect_external_triangles.size() == printable_triangles_without_blends.size() * 80,
        "Expected external-only STL output for hexagon with nested rectangle to keep hexagon inner exposed wall "
        "and rectangle walls, and hide covered insets.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> modifier_segments;
    QSharedPointer<ORNL::SegmentBase> parsed_tip_wipe = makeTaggedLineSegment(
        ORNL::Constants::RegionTypeStrings::kPerimeter + " " + ORNL::Constants::PathModifierStrings::kForwardTipWipe,
        19);
    QSharedPointer<ORNL::SegmentBase> settings_tip_wipe =
        makeTaggedLineSegment(ORNL::Constants::RegionTypeStrings::kInset, 20, 20.0f);
    settings_tip_wipe->getSb()->setSetting(ORNL::SS::kPathModifiers, ORNL::PathModifiers::kAngledTipWipe);
    modifier_segments.push_back({makeTaggedLineSegment(ORNL::Constants::RegionTypeStrings::kPerimeter, 21, 40.0f),
                                 parsed_tip_wipe, settings_tip_wipe});
    const auto modifier_filtered_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(modifier_segments, without_blends);
    passed &= ORNL::Testing::expect(modifier_filtered_triangles.size() == printable_triangles_without_blends.size(),
                                    "Expected non-build path modifiers to be skipped by STL export.");

    ORNL::GCodeSegmentFilter::tagInternalSegments(modifier_segments);
    passed &=
        ORNL::Testing::expect(static_cast<bool>(parsed_tip_wipe->displayType() & ORNL::SegmentDisplayType::kInternal),
                              "Expected parsed tip-wipe segments to be hidden with internal beads.");
    passed &=
        ORNL::Testing::expect(static_cast<bool>(settings_tip_wipe->displayType() & ORNL::SegmentDisplayType::kInternal),
                              "Expected settings-tagged tip-wipe segments to be hidden with internal beads.");

    const Bounds printable_bounds = boundsFor(printable_triangles);
    passed &= ORNL::Testing::expect(ORNL::Testing::near3DPoint(printable_bounds.min, 0.0f, 0.0f, 0.0f, kTolerance),
                                    "Expected STL vertices to start at local origin.");
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near(printable_bounds.max.x() - printable_bounds.min.x(), kLength, kTolerance),
        "Expected bead length to be written in millimeters.");

    ORNL::AsPrintedModelExporter::Options centerline_options;
    centerline_options.geometry_mode = ORNL::AsPrintedModelExporter::GeometryMode::kCenterlines;
    const auto centerline_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(printable_only, centerline_options);
    passed &= ORNL::Testing::expect(!centerline_triangles.empty(), "Expected centerline STL triangles.");
    passed &= ORNL::Testing::expect(
        centerline_triangles.size() < printable_triangles.size(),
        "Expected centerline STL to use less geometry than true bead-width STL for a straight segment.");
    const Bounds centerline_bounds = boundsFor(centerline_triangles);
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near(centerline_bounds.max.x() - centerline_bounds.min.x(), kLength, kTolerance),
        "Expected centerline STL length to follow the toolpath centerline.");
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near(centerline_bounds.max.y() - centerline_bounds.min.y(), kCenterlineDiameter, kTolerance),
        "Expected centerline STL width to ignore the true bead width.");
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near(centerline_bounds.max.z() - centerline_bounds.min.z(), kCenterlineDiameter, kTolerance),
        "Expected centerline STL height to use the centerline tube diameter.");
    bool checked_centerline_side_normal = false;
    for (const ORNL::AsPrintedModelExporter::Triangle& triangle : centerline_triangles) {
        const QVector3D normal = normalFor(triangle);
        if (normal.lengthSquared() <= std::numeric_limits<float>::epsilon() || std::abs(normal.x()) > 0.5f) {
            continue;
        }

        const QVector3D centroid = (triangle.a + triangle.b + triangle.c) / 3.0f;
        QVector3D outward(0.0f, centroid.y() - (kCenterlineDiameter / 2.0f),
                          centroid.z() - (kCenterlineDiameter / 2.0f));
        if (outward.lengthSquared() <= std::numeric_limits<float>::epsilon()) { continue; }
        outward.normalize();

        passed &= ORNL::Testing::expect(QVector3D::dotProduct(normal, outward) > 0.0f,
                                        "Expected centerline STL side-wall normals to point outward.");
        checked_centerline_side_normal = true;
        break;
    }
    passed &=
        ORNL::Testing::expect(checked_centerline_side_normal, "Expected a centerline STL side-wall normal to test.");

    QSharedPointer<ORNL::SegmentBase> radial_segment =
        makeLineSegment(pointFromMm(10.0f, -5.0f), pointFromMm(10.0f, 5.0f), 4);
    radial_segment->setCylindricalBeadCenter(pointFromMm(0.0f, 0.0f) * ORNL::Constants::OpenGL::kObjectToView);
    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> radial_segments;
    radial_segments.push_back({radial_segment});
    const auto radial_triangles = ORNL::AsPrintedModelExporter::generateTriangles(radial_segments);
    const Bounds radial_bounds  = boundsFor(radial_triangles);
    passed &=
        ORNL::Testing::expect(ORNL::Testing::near(radial_bounds.max.x() - radial_bounds.min.x(), kHeight, kTolerance),
                              "Expected cylindrical bead radial thickness to match bead height.");
    passed &=
        ORNL::Testing::expect(ORNL::Testing::near(radial_bounds.max.z() - radial_bounds.min.z(), kWidth, kTolerance),
                              "Expected cylindrical bead vertical span to match bead width.");

    QSharedPointer<ORNL::SegmentBase> transformed_axis_segment =
        makeLineSegment(pointFromMm(0.0f, 0.0f), pointFromMm(0.0f, 10.0f), 5);
    transformed_axis_segment->setCylindricalBeadCenter(pointFromMm(1.0f, 2.0f) *
                                                       ORNL::Constants::OpenGL::kObjectToView);
    transformed_axis_segment->rotate(QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 0.0f, 1.0f), 90.0f));
    transformed_axis_segment->shift(pointFromMm(3.0f, 4.0f) * ORNL::Constants::OpenGL::kObjectToView);
    const ORNL::Point transformed_axis = transformed_axis_segment->cylindricalBeadCenter();
    const ORNL::Point expected_axis    = pointFromMm(1.0f, 5.0f) * ORNL::Constants::OpenGL::kObjectToView;
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near2DPoint(transformed_axis, expected_axis.x(), expected_axis.y(), kTolerance),
        "Expected transformed segments to carry the cylindrical bead center.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> lfam_style_segments;
    const ORNL::Point lfam_start(120.0f * ORNL::in(), 42.5f * ORNL::in(), -15.25f * ORNL::in());
    const ORNL::Point lfam_end(136.0f * ORNL::in(), 42.5f * ORNL::in(), -15.25f * ORNL::in());
    lfam_style_segments.push_back({makeLineSegment(lfam_start, lfam_end, 1)});
    const auto lfam_triangles = ORNL::AsPrintedModelExporter::generateTriangles(lfam_style_segments);
    const Bounds lfam_bounds  = boundsFor(lfam_triangles);
    passed &= ORNL::Testing::expect(ORNL::Testing::near(lfam_bounds.min.x(), 0.0f, kTolerance),
                                    "Expected LFAM-style export to use local X zero.");
    passed &=
        ORNL::Testing::expect(ORNL::Testing::near(lfam_bounds.max.x() - lfam_bounds.min.x(), 16.0f * 25.4f, 0.01f),
                              "Expected LFAM-style inch coordinates to export as millimeters.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> filtered_segments;
    filtered_segments.push_back(
        {makeLineSegment(pointFromMm(1000.0f, 1000.0f, 1000.0f), pointFromMm(1000.0f, 1000.0f, 1000.0f), 1,
                         ORNL::SegmentDisplayType::kLine, false),
         makeLineSegment(pointFromMm(-500.0f, 0.0f), pointFromMm(-400.0f, 0.0f), 2, ORNL::SegmentDisplayType::kLine,
                         false),
         makeLineSegment(ORNL::SegmentDisplayType::kLine, 3)});
    const auto filtered_triangles = ORNL::AsPrintedModelExporter::generateTriangles(filtered_segments);
    passed &= ORNL::Testing::expect(filtered_triangles.size() == printable_triangles.size(),
                                    "Expected non-deposition and degenerate segments to be skipped by default.");
    const Bounds filtered_bounds = boundsFor(filtered_triangles);
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near(filtered_bounds.max.x() - filtered_bounds.min.x(), kLength, kTolerance),
        "Expected skipped segments not to expand the exported bounds.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> full_circle_arc_segments;
    const ORNL::Point arc_start = pointFromMm(10.0f, 0.0f);
    full_circle_arc_segments.push_back({makeArcSegment(arc_start, arc_start, pointFromMm(0.0f, 0.0f), 1)});
    const auto full_circle_arc_triangles = ORNL::AsPrintedModelExporter::generateTriangles(full_circle_arc_segments);
    passed &= ORNL::Testing::expect(!full_circle_arc_triangles.empty(),
                                    "Expected same-endpoint full-circle arcs to be exported as bead mesh triangles.");
    const Bounds full_circle_arc_bounds = boundsFor(full_circle_arc_triangles);
    passed &= ORNL::Testing::expect(
        ORNL::Testing::near(full_circle_arc_bounds.max.z() - full_circle_arc_bounds.min.z(), kHeight, kTolerance),
        "Expected arc bead mesh height to match the display bead height.");
    const auto centerline_full_circle_arc_triangles =
        ORNL::AsPrintedModelExporter::generateTriangles(full_circle_arc_segments, centerline_options);
    passed &=
        ORNL::Testing::expect(!centerline_full_circle_arc_triangles.empty(),
                              "Expected same-endpoint full-circle arcs to be exported as centerline STL triangles.");

    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> corner_segments;
    corner_segments.push_back({makeLineSegment(pointFromMm(0.0f, 0.0f, 0.0f), pointFromMm(10.0f, 0.0f, 0.0f), 1),
                               makeLineSegment(pointFromMm(10.0f, 0.0f, 0.0f), pointFromMm(10.0f, 10.0f, 0.0f), 2)});

    const auto corner_triangles_without_blends =
        ORNL::AsPrintedModelExporter::generateTriangles(corner_segments, without_blends);
    const auto corner_triangles_with_blends = ORNL::AsPrintedModelExporter::generateTriangles(corner_segments);
    passed &= ORNL::Testing::expect(corner_triangles_with_blends.size() > corner_triangles_without_blends.size(),
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
    passed &=
        ORNL::Testing::expect(closed_corner_triangles_with_blends.size() ==
                                  closed_corner_triangles_without_blends.size() + (open_two_segment_blend_count * 3),
                              "Expected closed loops to blend each corner, including the start/end corner.");

    QTemporaryDir temp_dir;
    passed &= ORNL::Testing::expect(temp_dir.isValid(), "Could not create temporary directory.");
    if (temp_dir.isValid()) {
        const QString binary_stl_path = temp_dir.path() + "/as_printed_binary.stl";
        QString error;
        passed &= ORNL::Testing::expect(ORNL::AsPrintedModelExporter::writeStl(binary_stl_path, printable_only, &error),
                                        ("Could not write binary STL: " + error.toStdString()));

        QByteArray binary_stl_bytes;
        passed &=
            ORNL::Testing::expect(readBytes(binary_stl_path, binary_stl_bytes), "Could not read generated binary STL.");
        passed &= ORNL::Testing::expect(
            binaryFacetCount(binary_stl_bytes) == static_cast<quint32>(printable_triangles.size()),
            "Generated binary STL facet count did not match generated triangles.");
        passed &=
            ORNL::Testing::expect(binary_stl_bytes.size() == 84 + (static_cast<int>(printable_triangles.size()) * 50),
                                  "Generated binary STL size did not match the binary STL triangle layout.");

        const QString ascii_stl_path = temp_dir.path() + "/as_printed_ascii.stl";
        ORNL::AsPrintedModelExporter::Options ascii_options;
        ascii_options.format = ORNL::AsPrintedModelExporter::StlFormat::kAscii;
        passed &= ORNL::Testing::expect(
            ORNL::AsPrintedModelExporter::writeStl(ascii_stl_path, printable_only, &error, ascii_options),
            ("Could not write ASCII STL: " + error.toStdString()));
        QString stl_text;
        passed &= ORNL::Testing::expect(readFile(ascii_stl_path, stl_text), "Could not read generated ASCII STL.");
        passed &= ORNL::Testing::expect(stl_text.startsWith(QStringLiteral("solid ornlslicer_as_printed")),
                                        "Generated ASCII STL did not contain the expected header.");
        passed &= ORNL::Testing::expect(countFacets(stl_text) == static_cast<int>(printable_triangles.size()),
                                        "Generated ASCII STL facet count did not match generated triangles.");
        passed &= ORNL::Testing::expect(QFileInfo(binary_stl_path).size() < QFileInfo(ascii_stl_path).size(),
                                        "Expected binary STL to be smaller than ASCII STL.");

        const QString centerline_stl_path = temp_dir.path() + "/as_printed_centerline.stl";
        passed &= ORNL::Testing::expect(
            ORNL::AsPrintedModelExporter::writeStl(centerline_stl_path, printable_only, &error, centerline_options),
            ("Could not write centerline STL: " + error.toStdString()));
        QByteArray centerline_stl_bytes;
        passed &= ORNL::Testing::expect(readBytes(centerline_stl_path, centerline_stl_bytes),
                                        "Could not read centerline STL.");
        passed &= ORNL::Testing::expect(
            binaryFacetCount(centerline_stl_bytes) == static_cast<quint32>(centerline_triangles.size()),
            "Generated centerline STL facet count did not match generated centerline triangles.");
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
