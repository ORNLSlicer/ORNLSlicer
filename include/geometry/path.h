#pragma once

#include <QList>
#include <qquaternion.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "units/unit.h"

namespace ORNL {
class Polygon;
class PolygonList;
class Polyline;

/*!
 * \class Path
 * \brief A list of path segments.
 */
class Path {
  public:
    //! \brief Add a segment to the end of the path.
    //! \param ps: Segment to add
    void add(const QSharedPointer<SegmentBase>& ps);

    //! \brief Add a segment to the end of the path.
    //! \param ps: Segment to append
    void append(const QSharedPointer<SegmentBase>& ps);

    //! \brief Add a segment to the end of the path.
    //! \param path: Path to append
    void append(Path path);

    //! \brief Add a segment to the start of the path.
    //! \param ps: Segment to prepend
    void prepend(const QSharedPointer<SegmentBase>& ps);

    //! \brief Add a segment at index to path.
    //! \param index: Index to insert at
    //! \param ps: Path segment to insert
    void insert(int index, const QSharedPointer<SegmentBase>& ps);

    //! \brief Remove a segment by pointer.
    //! \param ps: Segment to remove
    void remove(const QSharedPointer<SegmentBase>& ps);

    //! \brief Remove a segment by index
    //! \param index: Index to remove
    void removeAt(int index);

    //! \brief Reverse segment order of path.
    void reverseSegments();

    //! \brief Add a segment to the end of the path.
    Path operator+=(const QSharedPointer<SegmentBase>& ps);

    //! @brief Return an iterator to the start of the segments.
    QList<QSharedPointer<SegmentBase>>::iterator begin();

    //! @brief Return an iterator to the end of the segments.
    QList<QSharedPointer<SegmentBase>>::iterator end();

    //! @brief Return a const iterator to the start of the segments.
    QList<QSharedPointer<SegmentBase>>::const_iterator begin() const;

    //! @brief Return a const iterator to the end of the segments.
    QList<QSharedPointer<SegmentBase>>::const_iterator end() const;

    //! \brief Access the segments at an index.
    QSharedPointer<SegmentBase> operator[](const int index) const;

    //! \brief Access the segments at an index.
    QSharedPointer<SegmentBase> at(const int index) const;

    //! \brief Acess the front of the segments.
    QSharedPointer<SegmentBase> front() const;

    //! \brief Acess the back of the segments.
    QSharedPointer<SegmentBase> back() const;

    //! \brief Size of the segments.
    int size() const;

    //! \brief Moves elements within the path.
    void move(int from, int to);

    //! \brief Clears the path.
    void clear();

    //! \brief Get the segments that compose this path.
    QList<QSharedPointer<SegmentBase>>& getSegments();

    //! \brief Return the total length of the path as a distance
    Distance calculateLength();

    //! \brief Return the total length of the path without travel segments as a distance
    Distance calculateLengthNoTravel();

    //! \brief Return the total length of the printing segments as a distance
    Distance calculatePrintingLength();

    //! \brief rotates and then shifts the path by the given amounts
    void transform(QQuaternion rotation, Point shift);

    //! \brief returns the minimun z-coordinate of a path
    float getMinZ();

    //! \brief remove only travel segments from path
    void removeTravels();

    //! \brief Checks whether path is closed
    //! \return Whether path is closed
    bool isClosed();

    //! \brief Sets whether or not path is counter-clockwise or not (assuming it is closed)
    //! \param ccw: whether or not it is ccw
    void setCCW(bool ccw);

    //! \brief Gets whether or not path is counter-clockwise or not (assuming it is closed)
    //! \return whether or not path is ccw
    bool getCCW();

    //! \brief Adds an extruder/nozzle number to the list of extruders that should
    //!        be on with this path prints
    //! \param extruder/nozzle number, indexed at 0
    void addNozzle(int nozzle);

    //! \brief Adjusts pathing according to multi-nozzle settings
    void adjustMultiNozzle();

  private:
    //! \brief Segments that compose this path.
    QList<QSharedPointer<SegmentBase>> m_segments;

    //! \brief Bools for whether or not path is counter-clockwise or contains origin
    bool m_ccw;
};
} // namespace ORNL
