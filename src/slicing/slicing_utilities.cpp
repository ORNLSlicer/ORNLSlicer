#include "slicing/slicing_utilities.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmap.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "cross_section/cross_section.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! \brief Small angular tolerance used when a threshold coincides with the end of a path.
constexpr double kArcSweepTolerance = 1.0e-9;

//! \brief Maximum center drift allowed before a clipped cylindrical arc falls back to a line.
const Distance kArcGeometryTolerance = 50.0 * micron;

//! \brief Relative tolerance for large cylindrical paths.
constexpr double kArcGeometryRelativeTolerance = 1.0e-6;

//! \brief Smallest angular span worth emitting as a G2/G3 move.
constexpr double kMinArcAngularSpan = 1.0e-6;

//! \brief Upper bound for user-configured G2/G3 arc subdivision density.
constexpr int kMaxCylindricalArcsPerRevolution = 360;

int clampArcsPerRevolution(int arcs_per_revolution) {
    return std::clamp(arcs_per_revolution, 1, kMaxCylindricalArcsPerRevolution);
}

double radialDistance(const Point& point, const Point& center) {
    return std::hypot(point.x() - center.x(), point.y() - center.y());
}

//! \brief Returns the counter-clockwise sweep from start to end in the range [0, 2*pi).
double counterClockwiseSweep(const Point& start, const Point& end, const Point& center) {
    const double start_x = start.x() - center.x();
    const double start_y = start.y() - center.y();
    const double end_x = end.x() - center.x();
    const double end_y = end.y() - center.y();

    double sweep = std::atan2((start_x * end_y) - (start_y * end_x), (start_x * end_x) + (start_y * end_y));
    if (sweep < 0.0) {
        sweep += 2.0 * M_PI;
    }
    return sweep;
}
} // namespace

QVector<Point> SlicingUtilities::GetCylindricalArcPoints(const Polyline& polyline, const Point& center, Distance radius,
                                                         int arcs_per_revolution) {
    QVector<Point> arc_points;
    if (polyline.size() < 2 || radius <= 0 || arcs_per_revolution <= 0) {
        return arc_points;
    }
    arcs_per_revolution = clampArcsPerRevolution(arcs_per_revolution);

    QVector<double> cumulative_sweeps;
    cumulative_sweeps.reserve(polyline.size());
    cumulative_sweeps.push_back(0.0);

    double total_sweep = 0.0;
    for (int i = 1, end = polyline.size(); i < end; ++i) {
        total_sweep += counterClockwiseSweep(polyline[i - 1], polyline[i], center);
        cumulative_sweeps.push_back(total_sweep);
    }

    if (total_sweep <= kArcSweepTolerance) {
        return arc_points;
    }

    // Retain clipping intersections exactly. Intermediate arc boundaries are
    // projected to the analytic cylinder below.
    arc_points.push_back(polyline.first());

    const double arc_sweep = (2.0 * M_PI) / static_cast<double>(arcs_per_revolution);
    int source_segment = 1;
    for (double target_sweep = arc_sweep; target_sweep < total_sweep - kArcSweepTolerance; target_sweep += arc_sweep) {
        while (source_segment < cumulative_sweeps.size() - 1 && cumulative_sweeps[source_segment] < target_sweep) {
            ++source_segment;
        }

        const double segment_start_sweep = cumulative_sweeps[source_segment - 1];
        const double segment_sweep = cumulative_sweeps[source_segment] - segment_start_sweep;
        if (segment_sweep <= std::numeric_limits<double>::epsilon()) {
            continue;
        }

        const double fraction = std::clamp((target_sweep - segment_start_sweep) / segment_sweep, 0.0, 1.0);
        const Point& source_start = polyline[source_segment - 1];
        const Point& source_end = polyline[source_segment];
        const double start_angle = std::atan2(source_start.y() - center.y(), source_start.x() - center.x());
        const double angle = start_angle + segment_sweep * fraction;
        const double z = source_start.z() + ((source_end.z() - source_start.z()) * fraction);

        arc_points.push_back(
            Point(center.x() + (radius() * std::cos(angle)), center.y() + (radius() * std::sin(angle)), z));
    }

    arc_points.push_back(polyline.last());
    return arc_points;
}

bool SlicingUtilities::IsCylindricalArcSegment(const Point& start, const Point& end, const Point& center,
                                               Distance radius, int arcs_per_revolution) {
    const double expected_radius = radius();
    if (expected_radius <= std::numeric_limits<double>::epsilon() || arcs_per_revolution <= 0) {
        return false;
    }
    arcs_per_revolution = clampArcsPerRevolution(arcs_per_revolution);

    const double tolerance = std::max(kArcGeometryTolerance(), expected_radius * kArcGeometryRelativeTolerance);
    const double angular_tolerance = std::max(kMinArcAngularSpan, tolerance / expected_radius);
    const double planar_chord = std::hypot(end.x() - start.x(), end.y() - start.y());
    if (planar_chord <= tolerance) {
        return arcs_per_revolution == 1 && std::abs(radialDistance(start, center) - expected_radius) <= tolerance &&
               std::abs(radialDistance(end, center) - expected_radius) <= tolerance;
    }

    const double sweep = counterClockwiseSweep(start, end, center);
    const double max_sweep = (2.0 * M_PI) / static_cast<double>(arcs_per_revolution);
    if (sweep <= angular_tolerance || sweep > max_sweep + angular_tolerance) {
        return false;
    }

    const Point arc_center = GetCylindricalArcCenter(start, end, center);
    return radialDistance(arc_center, center) <= tolerance;
}

Point SlicingUtilities::GetCylindricalArcCenter(const Point& start, const Point& end, const Point& center) {
    const double chord_x = end.x() - start.x();
    const double chord_y = end.y() - start.y();
    const double chord_length_squared = (chord_x * chord_x) + (chord_y * chord_y);
    if (chord_length_squared <= std::numeric_limits<double>::epsilon()) {
        return Point(center.x(), center.y(), start.z());
    }

    const double midpoint_x = (start.x() + end.x()) / 2.0;
    const double midpoint_y = (start.y() + end.y()) / 2.0;
    const double projection =
        (((center.x() - midpoint_x) * chord_x) + ((center.y() - midpoint_y) * chord_y)) / chord_length_squared;

    return Point(center.x() - (projection * chord_x), center.y() - (projection * chord_y), start.z());
}

QVector<QSharedPointer<MeshBase>> SlicingUtilities::GetMeshesByType(QMap<QString, QSharedPointer<Part>> parts,
                                                                    MeshType mt) {
    QVector<QSharedPointer<MeshBase>> meshes;
    for (QSharedPointer<Part> part : parts) {
        if (part->rootMesh()->type() == mt)
            meshes.push_back(part->rootMesh());
    }
    return meshes;
}

QVector<QSharedPointer<Part>> SlicingUtilities::GetPartsByType(QMap<QString, QSharedPointer<Part>> parts, MeshType mt) {
    QVector<QSharedPointer<Part>> found_parts;
    for (QSharedPointer<Part> part : parts) {
        if (part->rootMesh()->type() == mt)
            found_parts.push_back(part);
    }
    return found_parts;
}

void SlicingUtilities::ClipMesh(QSharedPointer<MeshBase> mesh, QVector<QSharedPointer<MeshBase>> clippers) {
    for (QSharedPointer<MeshBase> clipper : clippers) {
        auto closed_clipper = dynamic_cast<ClosedMesh*>(clipper.get());
        auto closed_mesh = dynamic_cast<ClosedMesh*>(mesh.get());
        if (closed_clipper != nullptr && closed_mesh != nullptr)
            closed_mesh->difference(*closed_clipper);
    }
}

void SlicingUtilities::IntersectMesh(QSharedPointer<ClosedMesh> mesh, QSharedPointer<ClosedMesh> intersect) {
    mesh->intersection(*intersect);
}

void SlicingUtilities::UnionMesh(QSharedPointer<ClosedMesh> mesh, QSharedPointer<ClosedMesh> to_union) {
    mesh->mesh_union(to_union);
}

int SlicingUtilities::GetPartStart(QSharedPointer<Part> part, int current_steps) {
    int part_start = 0;
    if (part->countStepPairs() > 0) {
        while (part_start < current_steps && !part->stepGroupContains(part_start, StepType::kLayer))
            ++part_start;
    }
    return part_start;
}

std::tuple<Plane, Point, Point> SlicingUtilities::GetDefaultSlicingAxis(QSharedPointer<SettingsBase> sb,
                                                                        QSharedPointer<MeshBase> mesh) {
    // Retrieve the slicing plane normal
    QVector3D slicing_vector = {sb->setting<float>(PS::Slicing::kSlicePlaneNormalX),
                                sb->setting<float>(PS::Slicing::kSlicePlaneNormalY),
                                sb->setting<float>(PS::Slicing::kSlicePlaneNormalZ)};
    slicing_vector.normalize();

    // Retrieve the mesh extrema along the slicing plane normal
    auto [min, max] = mesh->getAxisExtrema(slicing_vector);

    // Create the slicing plane
    Plane slicing_plane(min, slicing_vector);

    return {slicing_plane, min, max};
}

void SlicingUtilities::ShiftSlicingPlane(QSharedPointer<SettingsBase> sb, Plane& slicing_plane, Distance last_height) {
    // Retrieve the layer height
    const Distance& layer_height = sb->setting<Distance>(PS::Layer::kLayerHeight);

    // Shift the slicing plane along the normal by half the layer height
    slicing_plane.shiftAlongNormal((layer_height() / 2.) + (last_height() / 2.));
}

bool SlicingUtilities::doPartsOverlap(QVector<QSharedPointer<Part>> parts, Plane slicing_plane) {
    // Cross-section parts
    QVector<Polygon> polygons;
    for (auto part : parts) {
        Point tmp_point;
        QVector3D tmp_vec;

        PolygonList geometry =
            CrossSection::doCrossSection(part->rootMesh(), slicing_plane, tmp_point, tmp_vec, part->getSb());

        // Since settings meshes are always rectangular prisms there is only a single island
        if (!geometry.isEmpty()) {
            polygons.push_back(geometry.first());
        }
    }

    // Polygon in Polygon test
    bool overlap = false;
    for (int i = 0, end = polygons.size(); i < end; ++i) {
        for (int j = i + 1; j < end; ++j) {
            Polygon first = polygons[i];
            Polygon second = polygons[j];

            if (first.overlaps(second)) {
                overlap = true;
                break;
            }
        }
    }

    return overlap;
}
} // namespace ORNL
