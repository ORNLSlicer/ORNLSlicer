#include "cross_section/cross_section_object.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include <psimpl.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "algorithms/knn.h"
#include "cross_section/close_polygon_result.h"
#include "cross_section/cross_section_segment.h"
#include "cross_section/gap_closer_result.h"
#include "cross_section/possible_stitch.h"
#include "cross_section/sparse_point_grid.h"
#include "cross_section/terminus.h"
#include "cross_section/terminus_tracking_map.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/polyline.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
/*!
 * If the distance between segments on adjacent faces is smaller than this,
 * then they are still considered connected
 */
Distance largest_neglected_gap_first_phase;
Distance largest_neglected_gap_second_phase;
Distance max_stitch1;

CrossSectionObject::CrossSectionObject(QSharedPointer<SettingsBase> sb) : m_sb(sb) {
    largest_neglected_gap_first_phase = m_sb->setting<double>(ES::CrossSection::kLargestGap);

    // if unset or 0, default to .1 inches
    if (largest_neglected_gap_first_phase <= 0)
        largest_neglected_gap_first_phase = 2540;

    largest_neglected_gap_second_phase = 2.0 * largest_neglected_gap_first_phase;

    // if unset or 0, default to 1 inch
    max_stitch1 = m_sb->setting<double>(ES::CrossSection::kMaxStitch);
    if (max_stitch1 <= 0)
        max_stitch1 = 25400;
}

bool CrossSectionObject::shorterThan(const Point& p0, int32_t len) {
    if (p0.x() > len || p0.x() < -len)
        return false;
    if (p0.y() > len || p0.y() < -len)
        return false;
    return (p0.x() * p0.x() + p0.y() * p0.y()) <= len * len;
}

PolygonList CrossSectionObject::makePolygons() {
    QHash<long long, int> startPointHash, endPointHash;
    endPointHash.reserve(m_segments.size());
    startPointHash.reserve(m_segments.size());

    QVector<Point> possiblePoints;
    possiblePoints.reserve(m_segments.size());

    for (unsigned int startSegment = 0, end = m_segments.size(); startSegment < end; ++startSegment) {
        if (m_segments[startSegment].added_to_polygon)
            continue;

        Polygon poly;
        poly += m_segments[startSegment].start;

        long long key = ((quint64)(m_segments[startSegment].start.x() * 1000) << 32) |
                        ((quint64)(m_segments[startSegment].start.y() * 1000));
        startPointHash.insert(key, startSegment);
        possiblePoints.push_back(m_segments[startSegment].start);

        unsigned int segmentIndex = startSegment;
        bool canClose;
        while (true) {
            canClose = false;
            m_segments[segmentIndex].added_to_polygon = true;
            Point p0 = m_segments[segmentIndex].end;
            poly += p0;

            long long startKey = ((quint64)(m_segments[segmentIndex].start.x() * 1000) << 32) |
                                 ((quint64)(m_segments[segmentIndex].start.y() * 1000));
            long long endKey = ((quint64)(p0.x() * 1000) << 32) | ((quint64)(p0.y() * 1000));

            startPointHash.insert(startKey, segmentIndex);
            endPointHash.insert(endKey, segmentIndex);
            possiblePoints.push_back(p0);

            int nextIndex = -1;
            int other_face_idx = m_segments[segmentIndex].end_other_face_idx;

            if (other_face_idx > -1 && m_face_idx_to_segment_idx.contains(other_face_idx)) {
                Point p1 = m_segments[m_face_idx_to_segment_idx[other_face_idx]].start;
                Point diff = p0 - p1;
                if (shorterThan(diff, 10)) {
                    if (m_face_idx_to_segment_idx[other_face_idx] == static_cast<int>(startSegment))
                        canClose = true;
                    if (m_segments[m_face_idx_to_segment_idx[other_face_idx]].added_to_polygon)
                        break;
                    nextIndex = m_face_idx_to_segment_idx[other_face_idx];
                }
            }

            if (nextIndex == -1)
                break;
            segmentIndex = nextIndex;
        }
        if (canClose)
            m_closeable_polygons.push_back(poly);
        else
            m_open_polylines.push_back(poly);
    }

    // makeBasicPolygonLoops();

    connectOpenPolylines(0.5 * in);

    stitch();

    // std constructs are necessary for psimpl interface
    // tolerance currently based on value from Slicer 1
    if (m_sb->setting<bool>(PS::SpecialModes::kSmoothing)) {
        QVector<Polygon> smoothed_polygons;
        Distance tolerance = m_sb->setting<Distance>(PS::SpecialModes::kSmoothingTolerance);
        std::deque<double> polyline;
        for (Polygon poly : m_closeable_polygons) {
            polyline.clear();
            for (Point p : poly) {
                polyline.push_back(p.x());
                polyline.push_back(p.y());
            }
            std::vector<double> result;
            result.reserve(polyline.size());

            SmoothingType smoothingType =
                static_cast<SmoothingType>(m_sb->setting<int>(PS::SpecialModes::kSmoothingType));
            if(smoothingType == SmoothingType::kDouglasPeucker) {
                psimpl::simplify_douglas_peucker<2>(polyline.begin(), polyline.end(), tolerance(),
                                                std::back_inserter(result));
            }
            else {
                psimpl::simplify_radial_distance<2>(polyline.begin(), polyline.end(), tolerance(),
                                                std::back_inserter(result));
            }

            Polygon newPolygon;
            for (unsigned int n = 0, end = result.size(); n < end; n += 2) {
                newPolygon.append(Point(result[n], result[n + 1]));
            }
            smoothed_polygons.append(newPolygon);
        }

        m_closeable_polygons = smoothed_polygons;
    }
    m_polygons.addAll(m_closeable_polygons);

    m_polygons.removeDegenerateVertices(); // Remove vertices connected to
                                           // overlapping line segments

    m_polygons = m_polygons.simplify();

    for (Polygon& poly : m_polygons) {
        for (Point& p : poly) {
            long long key = (quint64)(p.x() * 1000) << 32 | (quint64)(p.y() * 1000);
            if (!endPointHash.contains(key)) {
                Point currentPoint = p;
                auto it = std::remove_if(possiblePoints.begin(), possiblePoints.end(), [currentPoint](const Point p) {
                    if (p.x() == currentPoint.x() && p.y() == currentPoint.y())
                        return true;

                    return false;
                });
                possiblePoints.erase(it, possiblePoints.end());
                kNN neighbor(possiblePoints, QVector<Point> {currentPoint}, 1);
                neighbor.execute();

                int closest = neighbor.getNearestIndices().first();
                possiblePoints.push_back(currentPoint);

                key =
                    (quint64)(possiblePoints[closest].x() * 1000) << 32 | (quint64)(possiblePoints[closest].y() * 1000);
            }

            QVector<QVector3D> normals;
            normals.push_back(m_segments[endPointHash.value(key)].normal);
            normals.push_back(m_segments[startPointHash.value(key)].normal);
            p.setNormals(normals);
        }
    }

    return m_polygons;
}

QVector<CrossSectionSegment> CrossSectionObject::segments() { return m_segments; }

void CrossSectionObject::addSegment(CrossSectionSegment segment) { m_segments.append(segment); }

void CrossSectionObject::insertFaceToSegment(int face_idx, int segment_idx) {
    m_face_idx_to_segment_idx.insert(face_idx, segment_idx);
}

PolygonList& CrossSectionObject::getPolygonList() { return m_polygons; }

void CrossSectionObject::makeBasicPolygonLoops() {
    // loop through all segments and create loops out of them
    for (int start_idx = 0, end = m_segments.size(); start_idx < end; ++start_idx) {
        if (!m_segments[start_idx].added_to_polygon) {
            makeBasicPolygonLoop(start_idx);
        }
    }

    // Clear the segment list to save memory
    m_segments.clear();
}

void CrossSectionObject::makeBasicPolygonLoop(int start_idx) {
    Polygon polygon;
    polygon += m_segments[start_idx].start;

    for (int segment_idx = start_idx; segment_idx != -1;) {
        CrossSectionSegment& segment = m_segments[segment_idx];
        polygon += segment.end;
        segment.added_to_polygon = true;
        segment_idx = getNextSegmentIdx(segment, start_idx);
        if (segment_idx == static_cast<int>(start_idx)) {
            // polygon is closed
            m_polygons += polygon;
            return;
        }
    }

    // polygon couldn't be closed
    m_open_polylines += polygon;
}

int CrossSectionObject::getNextSegmentIdx(const CrossSectionSegment& segment, int start_idx) {
    int next_idx = -1;

    bool segment_ended_at_edge = segment.end_vertex == nullptr;
    // If segment ends on an edge get the segment from the face on the other
    // side of the edge
    if (segment_ended_at_edge) {
        int face_to_try = segment.end_other_face_idx;
        if (face_to_try == -1) {
            return -1;
        }
        return tryFaceNextSegmentIdx(segment, face_to_try, start_idx);
    }
    // If segment ends on a vertex loop through potential faces
    else {
        const QVector<int>& faces_to_try = segment.end_vertex->connected_faces;
        for (int face_to_try : faces_to_try) {
            int result_idx = tryFaceNextSegmentIdx(segment, face_to_try, start_idx);
            if (result_idx == static_cast<int>(start_idx)) {
                return start_idx;
            }
            if (result_idx != -1) {
                // Not immediately returned since we might still encounter
                // the start_idx
                next_idx = result_idx;
            }
        }
        return next_idx;
    }
}

int CrossSectionObject::tryFaceNextSegmentIdx(const CrossSectionSegment& segment, int face_idx, int start_idx) {
    auto it_end = m_face_idx_to_segment_idx.end();      //!< The iterator corresponding to the end
    auto it = m_face_idx_to_segment_idx.find(face_idx); //!< The iterator
                                                        //! corresponding to the
    //! face_idx or end if it
    //! doesn't exist
    if (it != it_end) {
        int segment_idx = it.value();
        Point p1 = m_segments[segment_idx].start;
        Point diff = segment.end - p1;
        // If the start of the segment on the trial face is close enough to
        // the end of the previous segment
        if (diff.shorterThan(largest_neglected_gap_first_phase)) {
            if (segment_idx == static_cast<int>(start_idx)) {
                return start_idx;
            }
            if (m_segments[segment_idx].added_to_polygon) {
                return -1;
            }
            return segment_idx;
        }
    }
    // Either the face doesn't exist in this cross section or the gap
    // between the end of the previous segment and the start of this segment
    // is too large
    return -1;
}

void CrossSectionObject::connectOpenPolylines(Distance d) {
    bool allow_reverse = false;
    // Search fewer cells, but at the cost of covering more area
    // Since acceptance area is small to start with, the extra is unlikely
    // to hurt much
    Distance cell_size = largest_neglected_gap_first_phase * 2;
    connectOpenPolylinesImpl(largest_neglected_gap_second_phase, cell_size, allow_reverse);
}

void CrossSectionObject::stitch() {
    bool allow_reverse = true;
    connectOpenPolylinesImpl(max_stitch1, max_stitch1, allow_reverse);
}

GapCloserResult CrossSectionObject::findPolygonGapCloser(Point ip0, Point ip1) {
    GapCloserResult ret;
    ClosePolygonResult c1 = findPolygonPointClosestTo(ip0);
    ClosePolygonResult c2 = findPolygonPointClosestTo(ip1);
    if (c1.polygon_idx < 0 || c1.polygon_idx != c2.polygon_idx) {
        ret.length = -1;
        return ret;
    }
    ret.polygon_idx = c1.polygon_idx;
    ret.point_idx_a = c1.point_idx;
    ret.point_idx_b = c2.point_idx;
    ret.a_to_b = true;

    if (ret.point_idx_a == ret.point_idx_b) {
        // Connection points are on the same line segment
        ret.length = ip0.distance(ip1);
    }
    else {
        // Find out if we should go from A to B or the other way around
        Point p0 = m_polygons[ret.polygon_idx][ret.point_idx_a];
        Distance length_a = p0.distance(ip0);
        for (uint i = ret.point_idx_a; i != ret.point_idx_b; i = (i + 1) % m_polygons[ret.polygon_idx].size()) {
            Point p1 = m_polygons[ret.polygon_idx][i];
            length_a += p0.distance(p1);
            p0 = p1;
        }
        length_a += p0.distance(ip1);

        p0 = m_polygons[ret.polygon_idx][ret.point_idx_b];
        Distance length_b = p0.distance(ip1);
        for (uint i = ret.point_idx_b; i != ret.point_idx_a; i = (i + 1) % m_polygons[ret.polygon_idx].size()) {
            Point p1 = m_polygons[ret.polygon_idx][i];
            length_b += p0.distance(p1);
            p0 = p1;
        }
        length_b += p0.distance(ip0);

        if (length_a < length_b) {
            ret.a_to_b = true;
            ret.length = length_a;
        }
        else {
            ret.a_to_b = false;
            ret.length = length_b;
        }
    }
    return ret;
}

ClosePolygonResult CrossSectionObject::findPolygonPointClosestTo(Point p) {
    ClosePolygonResult ret;
    for (int n = 0, end = m_polygons.size(); n < end; ++n) {
        Point p0 = m_polygons[n][m_polygons[n].size() - 1];
        for (int i = 0; i < m_polygons[n].size(); ++i) {
            Point p1 = m_polygons[n][i];

            // Q = A + Normal(B - A) * (((B - A) dot (P - A)) / VSize(A -
            // B));
            Point p_diff = p1 - p0;
            Distance line_length = p1.distance(p0);

            if (line_length() > 1) {
                double dist_on_line = p_diff.dot(p - p0) / line_length();

                if (dist_on_line >= 0 && dist_on_line <= line_length()) {
                    Point q = p0 + p_diff * dist_on_line / line_length();
                    if ((q - p).shorterThan(.01f * in)) {
                        ret.intersection_point = q;
                        ret.polygon_idx = n;
                        ret.point_idx = i;
                        return ret;
                    }
                }
            }
            p0 = p1;
        }
    }
    ret.polygon_idx = -1;
    return ret;
}

void CrossSectionObject::stitch_extensive() {
    // For extensive stitching find 2 open polygons that are touching 2
    // closed polygons. Then find the shortest path over this polygon that
    // can be used to connect the open polygons, and generate a path over
    // this shortest bit to link up the 2 open polygons. (If these 2 open
    // polygons are the same polygon, then the final result is a closed
    // polygon)

    while (1) {
        uint best_polyline_1_idx = -1;
        uint best_polyline_2_idx = -1;
        GapCloserResult best_result;
        best_result.length = std::numeric_limits<double>::max();
        best_result.polygon_idx = -1;
        best_result.point_idx_a = -1;
        best_result.point_idx_b = -1;

        //! \note m_open_polylines size must be evaluated
        for (int polyline_1_idx = 0; polyline_1_idx < m_open_polylines.size(); ++polyline_1_idx) {
            Polygon polyline_1 = m_open_polylines[polyline_1_idx];
            if (polyline_1.size() < 1) {
                continue;
            }

            // Extra brackets cause res to be scoped
            {
                GapCloserResult res = findPolygonGapCloser(polyline_1[0], polyline_1.back());
                if (res.length() > 0 && res.length < best_result.length) {
                    best_polyline_1_idx = polyline_1_idx;
                    best_polyline_2_idx = polyline_1_idx;
                    best_result = res;
                }
            }

            for (int polyline_2_idx = 0; polyline_2_idx < m_open_polylines.size(); polyline_2_idx++) {
                Polygon polyline_2 = m_open_polylines[polyline_2_idx];
                if (polyline_2.size() < 1 || polyline_1_idx == polyline_2_idx) {
                    continue;
                }

                GapCloserResult res = findPolygonGapCloser(polyline_1[0], polyline_2.back());
                if (res.length() > 0 && res.length < best_result.length) {
                    best_polyline_1_idx = polyline_1_idx;
                    best_polyline_2_idx = polyline_2_idx;
                    best_result = res;
                }
            }
        }

        if (best_result.length() < std::numeric_limits<double>::max()) {
            if (best_polyline_1_idx == best_polyline_2_idx) {
                if (best_result.point_idx_a == best_result.point_idx_b) {
                    m_polygons += m_open_polylines[best_polyline_1_idx];
                    m_open_polylines[best_polyline_1_idx].clear();
                }
                else if (best_result.a_to_b) {
                    Polygon polygon;
                    for (uint j = best_result.point_idx_a; j != best_result.point_idx_b;
                         j = (j + 1) % m_polygons[best_result.polygon_idx].size()) {
                        polygon += m_polygons[best_result.polygon_idx][j];
                    }
                    for (int j = m_open_polylines[best_polyline_1_idx].size() - 1; j >= 0; j--) {
                        polygon += m_open_polylines[best_polyline_1_idx][j];
                    }
                    m_polygons += polygon;
                    m_open_polylines[best_polyline_1_idx].clear();
                }
                else {
                    uint n = m_polygons.size();
                    m_polygons += m_open_polylines[best_polyline_1_idx];
                    for (uint j = best_result.point_idx_b; j != best_result.point_idx_a;
                         j = (j + 1) % m_polygons[best_result.polygon_idx].size()) {
                        m_polygons[n] += m_polygons[best_result.polygon_idx][j];
                    }
                    m_open_polylines[best_polyline_1_idx].clear();
                }
            }
            else {
                if (best_result.point_idx_a == best_result.point_idx_b) {
                    for (int n = 0; n < m_open_polylines[best_polyline_1_idx].size(); n++) {
                        m_open_polylines[best_polyline_2_idx] += m_open_polylines[best_polyline_1_idx][n];
                    }
                    m_open_polylines[best_polyline_1_idx].clear();
                }
                else if (best_result.a_to_b) {
                    Polygon polygon;
                    for (int n = best_result.point_idx_a; n != best_result.point_idx_b;
                         n = (n + 1) % m_polygons[best_result.polygon_idx].size()) {
                        polygon += m_polygons[best_result.polygon_idx][n];
                    }
                    for (int n = polygon.size() - 1; n >= 0; n--) {
                        m_open_polylines[best_polyline_2_idx] += polygon[n];
                    }
                    for (int n = 0; n < m_open_polylines[best_polyline_1_idx].size(); n++) {
                        m_open_polylines[best_polyline_2_idx] += m_open_polylines[best_polyline_1_idx][n];
                    }
                    m_open_polylines[best_polyline_1_idx].clear();
                }
                else {
                    for (uint n = best_result.point_idx_b; n != best_result.point_idx_a;
                         n = (n + 1) % m_polygons[best_result.polygon_idx].size()) {
                        m_open_polylines[best_polyline_2_idx] += m_polygons[best_result.polygon_idx][n];
                    }
                    for (int n = m_open_polylines[best_polyline_1_idx].size() - 1; n >= 0; n--) {
                        m_open_polylines[best_polyline_2_idx] += m_open_polylines[best_polyline_1_idx][n];
                    }
                    m_open_polylines[best_polyline_1_idx].clear();
                }
            }
        }
        else {
            break;
        }
    }
}

std::priority_queue<PossibleStitch> CrossSectionObject::findPossibleStitches(Distance max_dist, qlonglong cell_size,
                                                                             bool allow_reverse) const {
    std::priority_queue<PossibleStitch> stitch_queue;

    // Maximum distance squared
    Area max_distance2 = max_dist * max_dist;

    // Represents a terminal point of a polyline in open_polylines.
    struct StitchGridVal {
        uint polyline_idx;
        Point polyline_term_point; //<! Depending on the
                                   // SparsePointGridInclusive,
                                   // either the start point or the end
                                   // point of the
                                   // polyline
    };

    struct StitchGridValLocator {
        Point operator()(const StitchGridVal& val) const { return val.polyline_term_point; }
    };

    // Used to find nearby end points within a fixed maximum radius
    SparsePointGrid<StitchGridVal, StitchGridValLocator> grid_ends(cell_size);
    // Used to find nearby start points within a fixed maximum radius
    SparsePointGrid<StitchGridVal, StitchGridValLocator> grid_starts(cell_size);

    // populate grids
    // Inserts the ends of all polylines into the grid (does not insert the
    // starts of the polylines).
    for (int polyline_0_idx = 0, end = m_open_polylines.size(); polyline_0_idx < end; ++polyline_0_idx) {
        Polyline polyline_0 = m_open_polylines[polyline_0_idx];

        if (polyline_0.empty()) {
            continue;
        }

        StitchGridVal grid_val;
        grid_val.polyline_idx = static_cast<uint>(polyline_0_idx);
        grid_val.polyline_term_point = polyline_0.back();
        grid_ends.insert(grid_val);
    }

    // Inserts the start of all polylines into the grid.
    if (allow_reverse) {
        for (int polyline_0_idx = 0, end = m_open_polylines.size(); polyline_0_idx < end; ++polyline_0_idx) {
            Polygon polyline_0 = m_open_polylines[polyline_0_idx];

            if (polyline_0.empty()) {
                continue;
            }

            StitchGridVal grid_val;
            grid_val.polyline_idx = static_cast<uint>(polyline_0_idx);
            grid_val.polyline_term_point = polyline_0.front();
            grid_starts.insert(grid_val);
        }
    }

    // search for nearby end points
    //! /note m_open_polylines size must be evalauted
    for (int polyline_1_idx = 0; polyline_1_idx < m_open_polylines.size(); ++polyline_1_idx) {
        Polyline polyline_1 = m_open_polylines[polyline_1_idx];

        if (polyline_1.empty()) {
            continue;
        }

        /*
         * Check for stitches that append polyline_1 onto polyline_0
         * in natural order.  These are stitches that use the end of
         * polyline_0 and the start of polyline_1.
         */
        QVector<StitchGridVal> nearby_ends = grid_ends.getNearby(polyline_1.front(), max_dist);
        for (const auto& nearby_end : nearby_ends) {
            Distance diff = nearby_end.polyline_term_point.distance(polyline_1.front());
            Area dist2 = diff * diff;
            if (dist2 < max_distance2) {
                PossibleStitch poss_stitch;
                poss_stitch.distance_2 = dist2;
                poss_stitch.terminus_0 = Terminus {nearby_end.polyline_idx, true};
                poss_stitch.terminus_1 = Terminus {static_cast<Terminus::Index>(polyline_1_idx), false};
                stitch_queue.push(poss_stitch);
            }
        }

        if (allow_reverse) {
            /*
             * Check for stitches that append polyline_1 onto polyline_0
             * by reversing order of polyline_1.  These are stitches that
             * use the end of polyline_0 and the end of polyline_1.
             */
            nearby_ends = grid_ends.getNearby(polyline_1.back(), max_dist);
            for (const auto& nearby_end : nearby_ends) {
                // Disallow stitching with self with same end point
                if (nearby_end.polyline_idx == static_cast<uint>(polyline_1_idx)) {
                    continue;
                }

                Distance diff = nearby_end.polyline_term_point.distance(polyline_1.back());
                Area dist2 = diff * diff;
                if (dist2 < max_distance2) {
                    PossibleStitch poss_stitch;
                    poss_stitch.distance_2 = dist2;
                    poss_stitch.terminus_0 = Terminus {nearby_end.polyline_idx, true};
                    poss_stitch.terminus_1 = Terminus {static_cast<Terminus::Index>(polyline_1_idx), true};
                    stitch_queue.push(poss_stitch);
                }
            }

            /*
             * Check for stitches that append polyline_1 onto polyline_0
             * by reversing order of polyline_0.  These are stitches that
             * use the start of polyline_0 and the start of polyline_1.
             */
            QVector<StitchGridVal> nearby_starts = grid_starts.getNearby(polyline_1.front(), max_dist);
            for (const auto& nearby_start : nearby_starts) {
                // Disallow stitching with self with same end point
                if (nearby_start.polyline_idx == static_cast<uint>(polyline_1_idx)) {
                    continue;
                }

                Distance diff = nearby_start.polyline_term_point.distance(polyline_1.front());
                Area dist2 = diff * diff;
                if (dist2 < max_distance2) {
                    PossibleStitch poss_stitch;
                    poss_stitch.distance_2 = dist2;
                    poss_stitch.terminus_0 = Terminus {nearby_start.polyline_idx, false};
                    poss_stitch.terminus_1 = Terminus {static_cast<Terminus::Index>(polyline_1_idx), false};
                    stitch_queue.push(poss_stitch);
                }
            }
        }
    }

    return stitch_queue;
}

void CrossSectionObject::planPolylineStitch(Terminus& terminus_0, Terminus& terminus_1, bool reverse[2]) const {
    size_t polyline_0_idx = terminus_0.getPolylineIdx();
    size_t polyline_1_idx = terminus_1.getPolylineIdx();
    bool back_0 = terminus_0.isEnd();
    bool back_1 = terminus_1.isEnd();
    reverse[0] = false;
    reverse[1] = false;
    if (back_0) {
        if (back_1) {
            /*
             * back of both polylines
             * we can reverse either one and then append onto the other
             * reverse the smaller polyline
             */
            if (m_open_polylines[static_cast<int>(polyline_0_idx)].size() <
                m_open_polylines[static_cast<int>(polyline_1_idx)].size()) {
                std::swap(terminus_0, terminus_1);
            }
            reverse[1] = true;
        }
        else {
            /*
             * back of 0, front of 1
             * already in order, nothing to do
             */
        }
    }
    else {
        if (back_1) {
            /*
             * front of 0, back of 1
             * in order if we swap 0 and 1
             */
            std::swap(terminus_0, terminus_1);
        }
        else {
            /*
             * front of both polylines
             * we can reverse either one and then prepend to the other
             * reverse the smaller polyline
             */
            if (m_open_polylines[polyline_0_idx].size() > m_open_polylines[polyline_1_idx].size()) {
                std::swap(terminus_0, terminus_1);
            }
            reverse[0] = true;
        }
    }
}

void CrossSectionObject::joinPolylines(Polyline& polyline_0, Polyline& polyline_1, const bool reverse[2]) const {
    if (reverse[0]) {
        // reverse polyline_0
        std::reverse(polyline_0.begin(), polyline_1.end());
    }
    if (reverse[1]) {
        // reverse polyline_1 by adding in reverse order
        for (int poly_idx = polyline_1.size() - 1; poly_idx >= 0; --poly_idx) {
            polyline_0 += polyline_1[poly_idx];
        }
    }
    else {
        // append polyline_1 onto polyline_0
        for (const Point& p : polyline_1) {
            polyline_0 += p;
        }
    }
    polyline_1.clear();
}

void CrossSectionObject::connectOpenPolylinesImpl(Distance max_dist, Distance cell_size, bool allow_reverse) {
    // below code closes smallest gaps first
    std::priority_queue<PossibleStitch> stitch_queue = findPossibleStitches(max_dist, cell_size(), allow_reverse);

    Terminus::Index terminus_end_idx = Terminus::endIndexFromPolylineEndIndex(m_open_polylines.size());
    // Keeps track of how polyline end point locations move around
    TerminusTrackingMap terminus_tracking_map(terminus_end_idx);

    while (!stitch_queue.empty()) {
        // Get the next best stitch
        PossibleStitch next_stitch = stitch_queue.top();
        stitch_queue.pop();
        Terminus old_terminus_0 = next_stitch.terminus_0;
        Terminus terminus_0 = terminus_tracking_map.getCurrentFromOld(old_terminus_0);
        unsigned long long idx = terminus_0.asIndex();
        if (terminus_0 == Terminus::INVALID_TERMINUS) {
            // if we already used this terminus, then this stitch is no
            // longer usable
            continue;
        }
        Terminus old_terminus_1 = next_stitch.terminus_1;
        Terminus terminus_1 = terminus_tracking_map.getCurrentFromOld(old_terminus_1);
        if (terminus_1 == Terminus::INVALID_TERMINUS) {
            // if we already used this terminus, then this stitch is no
            // longer usable
            continue;
        }

        size_t best_polyline_0_idx = terminus_0.getPolylineIdx();
        size_t best_polyline_1_idx = terminus_1.getPolylineIdx();

        // check to see if this completes a polygon
        if (best_polyline_0_idx == best_polyline_1_idx) {
            // finished polygon
            Polyline& polyline_0 = m_open_polylines[best_polyline_0_idx];
            m_closeable_polygons.append(polyline_0.close());
            polyline_0.clear(); // Clear instead of removing, so that the
                                // indices are still correct
            Terminus cur_terms[2] = {{best_polyline_0_idx, false}, {best_polyline_0_idx, true}};
            for (size_t idx = 0U; idx != 2U; ++idx) {
                terminus_tracking_map.markRemoved(cur_terms[idx]);
            }
            continue;
        }

        // we need to join these polylines

        // plan how to join polylines
        bool reverse[2];
        planPolylineStitch(terminus_0, terminus_1, reverse);

        // need to reread since planPolylineStitch can swap terminus_0/1
        best_polyline_0_idx = terminus_0.getPolylineIdx();
        best_polyline_1_idx = terminus_1.getPolylineIdx();
        // Polyline polyline_0 = m_open_polylines[best_polyline_0_idx];
        // Polyline polyline_1 = m_open_polylines[best_polyline_1_idx];

        // join polylines according to plan
        // joinPolylines(polyline_0, polyline_1, reverse);
        joinPolylines(m_open_polylines[best_polyline_0_idx], m_open_polylines[best_polyline_1_idx], reverse);

        // update terminus_tracking_map
        Terminus cur_terms[4] = {{best_polyline_0_idx, false},
                                 {best_polyline_0_idx, true},
                                 {best_polyline_1_idx, false},
                                 {best_polyline_1_idx, true}};
        Terminus next_terms[4] = {{best_polyline_0_idx, false},
                                  Terminus::INVALID_TERMINUS,
                                  Terminus::INVALID_TERMINUS,
                                  {best_polyline_0_idx, true}};
        if (reverse[0]) {
            std::swap(next_terms[0], next_terms[1]);
        }
        if (reverse[1]) {
            std::swap(next_terms[2], next_terms[3]);
        }
        // cur_terms -> next_terms has movement map
        // best_polyline_1 is always removed
        terminus_tracking_map.updateMap(4U, cur_terms, next_terms, 2U, &cur_terms[2]);
    }
}
} // namespace ORNL
