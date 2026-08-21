#include "gcode/gcode_segment_filter.h"

#include <Qt>

#include "utilities/constants.h"

namespace ORNL::GCodeSegmentFilter {
namespace {
bool containsRegion(const QString& comment, const QString& region) {
    return comment.contains(region, Qt::CaseInsensitive);
}
} // namespace

bool isExternalBeadComment(const QString& comment) {
    return containsRegion(comment, Constants::RegionTypeStrings::kPerimeter) ||
           containsRegion(comment, Constants::RegionTypeStrings::kRadial) ||
           containsRegion(comment, Constants::RegionTypeStrings::kHelical) ||
           containsRegion(comment, Constants::RegionTypeStrings::kTopSkin) ||
           containsRegion(comment, Constants::RegionTypeStrings::kBottomSkin) ||
           containsRegion(comment, Constants::RegionTypeStrings::kSkin);
}

bool isInternalBeadComment(const QString& comment) {
    return containsRegion(comment, Constants::RegionTypeStrings::kInset) ||
           containsRegion(comment, Constants::RegionTypeStrings::kInfill) ||
           containsRegion(comment, Constants::RegionTypeStrings::kSkeleton);
}
} // namespace ORNL::GCodeSegmentFilter
