#include "cross_section/cross_section_segment.h"

namespace ORNL {
CrossSectionSegment::CrossSectionSegment() {
    face_index         = -1;
    end_other_face_idx = -1;
    end_vertex         = nullptr;
    added_to_polygon   = false;
}
}  // namespace ORNL
