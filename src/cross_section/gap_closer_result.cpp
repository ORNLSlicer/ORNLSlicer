#include "cross_section/gap_closer_result.h"

namespace ORNL {
GapCloserResult::GapCloserResult() {
    length      = -1;
    polygon_idx = -1;
    point_idx_a = -1;
    point_idx_b = -1;
    a_to_b      = false;
}
}  // namespace ORNL
