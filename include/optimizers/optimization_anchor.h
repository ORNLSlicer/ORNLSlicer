#pragma once

#include <qsharedpointer.h>

#include "geometry/plane.h"
#include "geometry/point.h"

namespace ORNL {
class SettingsBase;

namespace OptimizationAnchor {
//! \brief Returns the custom island-order anchor in the flattened optimization frame.
Point customIslandOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                             const Point& optimization_shift);

//! \brief Returns the custom path-order anchor in the flattened optimization frame.
Point customPathOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                           const Point& optimization_shift);

//! \brief Returns the custom point-order anchor in the flattened optimization frame.
Point customPointOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                            const Point& optimization_shift);
}  // namespace OptimizationAnchor
}  // namespace ORNL
