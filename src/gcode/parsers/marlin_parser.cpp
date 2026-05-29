#include "gcode/parsers/marlin_parser.h"

#include <functional>

#include <QString>
#include <qcontainerfwd.h>
#include <qlatin1stringview.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
MarlinParser::MarlinParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();

    m_home_string = QLatin1String("X0 Y0 Z0");
    m_home_parameters = m_home_string.split(' ');
}

void MarlinParser::config() {
    CommonParser::config();

    // addtional Marlin
    addCommandMapping("G28", std::bind(&MarlinParser::G28Handler, this, std::placeholders::_1));
    addCommandMapping("G92", std::bind(&MarlinParser::G92Handler, this, std::placeholders::_1));
    addCommandMapping("M83", std::bind(&MarlinParser::M83Handler, this, std::placeholders::_1));
}

// G28 X0 Y0 F1500 ; Home X and Y
void MarlinParser::G28Handler(QVector<QString> params) {
    // redirect - essentially G1 with predetermined location
    CommonParser::G1Handler(m_home_parameters);
}

// G92 E0 ; reset filament axis to 0
void MarlinParser::G92Handler(QVector<QString> params) {
    // redirect - essentially G1 with E parameter
    CommonParser::G1Handler(params);
}

// M83 ; use relative distances for extrusion
void MarlinParser::M83Handler(QVector<QString> params) { m_e_absolute = false; }
} // namespace ORNL
