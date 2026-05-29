#include "gcode/parsers/adamantine_parser.h"

#include <functional>

#include <QString>
#include <qcontainerfwd.h>
#include <qlatin1stringview.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
AdamantineParser::AdamantineParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();

    m_home_string = QLatin1String("X0 Y0 Z0");
    m_home_parameters = m_home_string.split(' ');
}

void AdamantineParser::config() {
    CommonParser::config();

    // addtional Adamantine
    addCommandMapping("G28", std::bind(&AdamantineParser::G28Handler, this, std::placeholders::_1));
    addCommandMapping("G92", std::bind(&AdamantineParser::G92Handler, this, std::placeholders::_1));
    addCommandMapping("M83", std::bind(&AdamantineParser::M83Handler, this, std::placeholders::_1));
}

// G28 X0 Y0 F1500 ; Home X and Y
void AdamantineParser::G28Handler(QVector<QString> params) {
    // redirect - essentially G1 with predetermined location
    CommonParser::G1Handler(m_home_parameters);
}

// G92 E0 ; reset filament axis to 0
void AdamantineParser::G92Handler(QVector<QString> params) {
    // redirect - essentially G1 with E parameter
    CommonParser::G1Handler(params);
}

// M83 ; use relative distances for extrusion
void AdamantineParser::M83Handler(QVector<QString> params) { m_e_absolute = false; }

} // namespace ORNL
