#include "gcode/parsers/tormach_parser.h"

#include <functional>

#include <qcontainerfwd.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
TormachParser::TormachParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();
}

void TormachParser::config() {
    CommonParser::config();

    // BEAM syntax specific
    addCommandMapping("M64", std::bind(&TormachParser::M64Handler, this, std::placeholders::_1));
    addCommandMapping("M65", std::bind(&TormachParser::M65Handler, this, std::placeholders::_1));
}

void TormachParser::M64Handler(QVector<QString> params) { m_extruders_on[0] = true; }

void TormachParser::M65Handler(QVector<QString> params) { m_extruders_on[0] = false; }

} // namespace ORNL
