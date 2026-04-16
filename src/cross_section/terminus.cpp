#include "cross_section/terminus.h"

#include <qtypes.h>

namespace ORNL {
const Terminus Terminus::INVALID_TERMINUS {~static_cast<Index>(0U)};

Terminus::Terminus() : m_idx(-1) {}

Terminus::Terminus(Index idx) : m_idx(idx) {}

Terminus::Terminus(Index polyline_idx, bool is_end) { m_idx = polyline_idx * 2 + (is_end ? 1 : 0); }

Terminus::Index Terminus::getPolylineIdx() const { return m_idx / 2; }

bool Terminus::isEnd() const { return (m_idx & 1); }

Terminus::Index Terminus::asIndex() const { return m_idx; }

Terminus::Index Terminus::endIndexFromPolylineEndIndex(uint polyline_end_idx) { return polyline_end_idx * 2; }

bool Terminus::operator==(const Terminus& other) { return m_idx == other.m_idx; }

bool Terminus::operator!=(const Terminus& other) { return m_idx != other.m_idx; }
} // namespace ORNL
