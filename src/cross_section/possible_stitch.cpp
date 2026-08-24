#include "cross_section/possible_stitch.h"

namespace ORNL {
PossibleStitch::PossibleStitch() : distance_2(-1) {}

bool PossibleStitch::inOrder() const {
    // in order is using back line 0 and front of line 1
    return terminus_0.isEnd() && !terminus_1.isEnd();
}

bool PossibleStitch::operator<(const PossibleStitch& other) const {
    // better if lower distance
    if (distance_2 > other.distance_2) { return true; }
    else if (distance_2 < other.distance_2) { return false; }

    // better if in order instead of reversed
    if (!inOrder() && other.inOrder()) { return true; }

    // better if lower Terminus::Index for terminus_0
    // This just defines a more total order and isn't strictly necessary.
    if (terminus_0.asIndex() > other.terminus_0.asIndex()) { return true; }
    else if (terminus_0.asIndex() < other.terminus_0.asIndex()) { return false; }

    // better if lower Terminus::Index for terminus_1
    // This just defines a more total order and isn't strictly necessary.
    if (terminus_1.asIndex() > other.terminus_1.asIndex()) { return true; }
    else if (terminus_1.asIndex() < other.terminus_1.asIndex()) { return false; }

    // The stitches have equal goodness
    return false;
}
}  // namespace ORNL
