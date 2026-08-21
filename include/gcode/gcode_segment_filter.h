#pragma once

#include <QString>

namespace ORNL::GCodeSegmentFilter {
//! \brief Returns whether a parsed G-code comment describes a bead on the exterior shell.
bool isExternalBeadComment(const QString& comment);

//! \brief Returns whether a parsed G-code comment describes printable material inside the exterior shell.
bool isInternalBeadComment(const QString& comment);
} // namespace ORNL::GCodeSegmentFilter
