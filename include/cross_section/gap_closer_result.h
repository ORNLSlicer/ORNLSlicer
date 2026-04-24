#pragma once

#include <qtypes.h>

#include "units/unit.h"

namespace ORNL {
class GapCloserResult {
  public:
    GapCloserResult();
    Distance length;
    int polygon_idx;
    uint point_idx_a;
    uint point_idx_b;
    bool a_to_b;
};
} // namespace ORNL
