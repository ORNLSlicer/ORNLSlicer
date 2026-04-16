#pragma once

#include <cstdint>
#include <queue>

#include <QSharedPointer>
#include <QVector>
#include <qcontainerfwd.h>
#include <qmap.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "terminus.h"
#include "units/unit.h"

namespace ORNL {
class CrossSectionSegment;
class GapCloserResult;
class ClosePolygonResult;
class PossibleStitch;

class CrossSectionObject {
  public:
    /*!
     * \brief Default Constructor
     *
     * \note Only created, so an object from this class can be put into a
     * QVector
     *
     * \note Should never be used. Instead use \fn
     * CrossSection(QSharedPointer<SettingsBase>* sb)
     */
    CrossSectionObject() = default;

    /*!
     * \brief The constructor that should always be used
     *
     * \param sb the settings base
     */
    CrossSectionObject(QSharedPointer<SettingsBase> sb);

    /*!
     * \brief Connect the segments into polygons for this layer
     */
    PolygonList makePolygons();

    /*!
     * \returns The list of CrossSectionSegment objects that are from this
     * CrossSection
     */
    QVector<CrossSectionSegment> segments();

    /*!
     * \brief Add a segment to this cross section
     *
     * \param segment A Segment that is part of this cross section to add
     */
    void addSegment(CrossSectionSegment segment);

    /*!
     * \brief Add a maping from a face index to a segment index
     *
     * \param face_idx The index of the face that the segment represented by
     * \p segment_idx is from \param segment_idx The index of the segment
     * that is contained by the face represented \p face_idx
     */
    void insertFaceToSegment(int face_idx, int segment_idx);

    /*!
     * \returns The list of polygons that represent the islands from this
     * specific cross section
     */
    PolygonList& getPolygonList();

  private:
    /*!
     * \brief Connect the segment into loops
     */
    void makeBasicPolygonLoops();

    /*!
     * \brief Connect the segments into a loop, starting from the segment
     * with index \p start_idx
     *
     * \param start_idx The index of the first segment to create a loop with
     */
    void makeBasicPolygonLoop(int start_idx);

    /*!
     * \brief Get the next segment connected to the end of \p segment. Used
     * to make closed polygon loops Return ASAP if segment is (also) connect
     * to CrossSection::segments[\p start_idx]
     *
     * \param segment The previous segment in the loop that is being
     * constructed \param start_idx The index of the first segment in the
     * loop that is being constructed \returns The index of the next segment
     * in the loop that is being constructed
     */
    int getNextSegmentIdx(const CrossSectionSegment& segment, int start_idx);

    /*!
     * \brief Try to find a segment from face \p face_idx tp continue \p
     * segment
     *
     * \param segment The previous segment
     * \param face_idx The index of the face to try
     * \param start_idx The index of the first segment in the loop being
     * created \returns The index of the segment if the segment from the
     * proposed face is close enough otherwise -1
     */
    int tryFaceNextSegmentIdx(const CrossSectionSegment& segment, int face_idx, int start_idx);

    /*!
     * \brief Connecting polygons that are not closed yet, as models are not
     * always perfect manifold we need to join
     *        some stuff up to get proper polygons. First links up polygon
     * ends that are with \p threshold
     *
     * \param threshold The maximum distance between two open polylines that
     * can be stitched together
     */
    void connectOpenPolylines(Distance threshold);

    /*!
     * \brief Link up all the missing ends, closing up the smallest gaps
     * first. This is an ineffient implementation which can run in O(n^3)
     * time.
     *
     * Clears all open polylines which are used up in the process
     *
     * TODO: find better implementation
     */
    void stitch();

    /*!
     * \brief
     *
     * \param p0
     * \param p1
     * \returns
     */
    GapCloserResult findPolygonGapCloser(Point p0, Point p1);

    /*!
     * \brief
     *
     * \param p
     * \returns
     */
    ClosePolygonResult findPolygonPointClosestTo(Point p);

    /*!
     * \brief Try to close up polylines into polygons while they have large
     * gaps in them. Clears all open polylines which are used up in the
     * process.
     */
    void stitch_extensive();

    /*!
     * \brief Find possible allowed stitches in order of distance.
     *
     * This finds all stitches that are allowed by the parameters.
     * The stitches are returned in a priority queue in order from best to
     * worse stitch.
     *
     * \param max_dist The maximum distance that a start/end point of one
     * polyline can be from another start/end point and the other be a
     * possible stitch \param cell_size The cell size used for the grid in
     * determining nearby points \param allow_reverse If true then start
     * points can be added to the grid to reverse the current polyline when
     * looking for possible stitches \returns A priority queue containing
     * the possible stitches
     */
    std::priority_queue<PossibleStitch> findPossibleStitches(Distance max_dist, qlonglong cell_size,
                                                             bool allow_reverse) const;

    /*!
     * \brief Plans the best way to perform a stitch
     *
     * Let polyline_0 be open_polylines[terminus_0.getPolylineIdx()] and
     *     polyline_1 be open_polylines[terminux_1.getPolylineIdx()].
     *
     * The plan consists of appending polyline_1 to polyline_0.  If
     * reverse[0] is true, then polyline_0 should be reversed before
     * appending.  If reverse[1] is true, then polyline_1 should be
     * reversed before appending.  Note that terminus_0 and terminus_1
     * may be swapped by this function.
     *
     * \param terminus_0 Contains the polyline index and terminal point for
     * one of the polylines to stitch \param terminus_1 Contains the
     * polyline index and terminal point for the other polyline to stitch
     * \param reverse Whether to reverse the first and/or second polyline
     */
    void planPolylineStitch(Terminus& terminus_0, Terminus& terminus_1, bool reverse[2]) const;

    /*!
     * \brief Joins polyline_1 onto polyline_0
     *
     * Appends polyline_1 to polyline_0.  It reverses the polylines first if
     * either
     * reverse[i] is true.  Clears polyline_1.
     *
     * \param polyline_0 The first of the polylines to join
     * \param polyline_1 The second of the polylines to join
     * \param reverse Whether to reverse the first and/or second polyline
     */
    void joinPolylines(Polyline& polyline_0, Polyline& polyline_1, const bool reverse[2]) const;

    /*!
     * \brief Connecting polylines that are not closed yet
     *
     * Any polylines that are closed by this function are added to
     * this->polygons.  All possible polyline joins that meet the
     * distance and reversal criteria will be performed.  This
     * function will not introduce any copies of the same polyline
     * segment.
     *
     * \param max_dist
     * \param cell_size
     * \param allow_reverse
     */
    void connectOpenPolylinesImpl(Distance max_dist, Distance cell_size, bool allow_reverse);

    QSharedPointer<SettingsBase> m_sb;
    PolygonList m_polygons;
    QVector<Polyline> m_open_polylines;
    QVector<Polygon> m_closeable_polygons;
    QVector<CrossSectionSegment> m_segments;
    QMap<int, int> m_face_idx_to_segment_idx; // topology

    bool shorterThan(const Point& p0, int32_t len);
};
} // namespace ORNL
