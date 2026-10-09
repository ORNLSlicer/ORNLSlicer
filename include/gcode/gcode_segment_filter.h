#pragma once

#include <QSharedPointer>
#include <QVector>

namespace ORNL {
class SegmentBase;
}

namespace ORNL::GCodeSegmentFilter {
//! \brief Returns whether a segment is a non-build path modifier rather than deposited part material.
bool isNonBuildModifierSegment(const QSharedPointer<SegmentBase>& segment);

//! \brief Tags non-external printable and non-build modifier segments so the preview can hide them.
void tagInternalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode);
}  // namespace ORNL::GCodeSegmentFilter
