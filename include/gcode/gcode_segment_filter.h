#pragma once

#include <QSet>
#include <QSharedPointer>
#include <QVector>

namespace ORNL {
class SegmentBase;
}

namespace ORNL::GCodeSegmentFilter {
//! \brief Returns whether a segment is a non-build path modifier rather than deposited part material.
bool isNonBuildModifierSegment(const QSharedPointer<SegmentBase>& segment);

//! \brief Finds printable segments whose bead footprints contribute to an exposed surface.
QSet<const SegmentBase*> externalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode);

//! \brief Tags non-external printable and non-build modifier segments so the preview can hide them.
void tagInternalSegments(QVector<QVector<QSharedPointer<SegmentBase>>>& gcode);
} // namespace ORNL::GCodeSegmentFilter
