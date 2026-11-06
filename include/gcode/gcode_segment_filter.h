#pragma once

#include <QSet>
#include <QSharedPointer>
#include <QVector3D>
#include <QVector>
#include <functional>
#include <vector>

namespace ORNL {
class Point;
class SegmentBase;
}  // namespace ORNL

namespace ORNL::GCodeSegmentFilter {
//! \brief Returns whether two segment endpoints are coincident within visualization tolerance.
bool pointsConnected(const Point& a, const Point& b);

//! \brief Returns whether a segment is a zero-length line segment.
bool isDegenerateLine(const QSharedPointer<SegmentBase>& segment);

//! \brief Samples a segment's centerline into 3D points, scaled by \p scale.
std::vector<QVector3D> segmentCenterlinePoints(const QSharedPointer<SegmentBase>& segment, double scale = 1.0);

//! \brief Returns whether a segment is a non-build path modifier rather than deposited part material.
bool isNonBuildModifierSegment(const QSharedPointer<SegmentBase>& segment);

//! \brief Finds printable segments whose bead footprints contribute to an exposed surface.
//! \param gcode The layers of G-code segments to analyze.
//! \param should_cancel Optional callback returning true if processing should be cancelled early.
QSet<const SegmentBase*> externalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                          const std::function<bool()>& should_cancel = nullptr);

//! \brief Tags non-external printable and non-build modifier segments so the preview can hide them.
//! \param gcode The layers of G-code segments to analyze and tag.
//! \param should_cancel Optional callback returning true if processing should be cancelled early.
void tagInternalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                         const std::function<bool()>& should_cancel = nullptr);
}  // namespace ORNL::GCodeSegmentFilter
