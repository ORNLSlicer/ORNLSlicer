#include "utilities/enums.h"

namespace ORNL {
void to_json(json& j, const InfillPatterns& i) { j = json {{"infill_pattern", static_cast<int>(i)}}; }

void from_json(const json& j, InfillPatterns& i) { i = static_cast<InfillPatterns>(j["infill_pattern"].get<int>()); }

void to_json(json& j, const SkeletonInput& i) { j = json {{"skeleton_input", static_cast<int>(i)}}; }

void from_json(const json& j, SkeletonInput& i) { i = static_cast<SkeletonInput>(j["skeleton_input"].get<int>()); }

void to_json(json& j, const IslandOrderOptimization& o) {
    j = json {{"island_order_optimization", static_cast<int>(o)}};
}

void from_json(const json& j, IslandOrderOptimization& o) {
    o = static_cast<IslandOrderOptimization>(j["island_order_optimization"].get<int>());
}

void to_json(json& j, const PathOrderOptimization& o) { j = json {{"path_order_optimization", static_cast<int>(o)}}; }

void from_json(const json& j, PathOrderOptimization& o) {
    o = static_cast<PathOrderOptimization>(j["path_order_optimization"].get<int>());
}

void to_json(json& j, const PointOrderOptimization& o) { j = json {{"point_order_optimization", static_cast<int>(o)}}; }

void from_json(const json& j, PointOrderOptimization& o) {
    o = static_cast<PointOrderOptimization>(j["point_order_optimization"].get<int>());
}

void to_json(json& j, const SlicingMode& i) { j = json {{"slicing_mode", static_cast<int>(i)}}; }

void from_json(const json& j, SlicingMode& i) {
    const char* key = j.contains("slicing_mode") ? "slicing_mode" : "slicer_type";
    i = static_cast<SlicingMode>(j[key].get<int>());
}

} // namespace ORNL
