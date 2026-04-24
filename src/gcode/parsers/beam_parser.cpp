#include "gcode/parsers/beam_parser.h"

#include <functional>

#include <qcontainerfwd.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
BeamParser::BeamParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();
}

void BeamParser::config() {
    CommonParser::config();

    // BEAM syntax specific
    addCommandMapping("M110", std::bind(&BeamParser::M110Handler, this, std::placeholders::_1));
    addCommandMapping("M111", std::bind(&BeamParser::M111Handler, this, std::placeholders::_1));
}

void BeamParser::M110Handler(QVector<QString> params) {
    if (!params.empty()) {
        return;
    }

    m_extruders_on[0] = true;
}

void BeamParser::M111Handler(QVector<QString> params) {
    if (!params.empty()) {
        return;
    }

    m_extruders_on[0] = false;
}
} // namespace ORNL
