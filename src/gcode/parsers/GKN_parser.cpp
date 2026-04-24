#include "gcode/parsers/GKN_parser.h"

#include <functional>

#include <QStringBuilder>
#include <qcontainerfwd.h>
#include <qlatin1stringview.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
GKNParser::GKNParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    m_x_param = QLatin1String("X");
    m_y_param = QLatin1String(" Y");
    m_z_param = QLatin1String(" Z");
    m_f_param = QLatin1String(" F");
    m_p_param = QLatin1String("P");

    config();
}

void GKNParser::config() {
    CommonParser::config();

    // override G0, G1, and G4 of common parser
    addCommandMapping("G0", std::bind(&GKNParser::G0Handler, this, std::placeholders::_1));
    addCommandMapping("G1", std::bind(&GKNParser::G1Handler, this, std::placeholders::_1));
    addCommandMapping("G4", std::bind(&GKNParser::G4Handler, this, std::placeholders::_1));

    // GKN syntax specific
    addCommandMapping("M3", std::bind(&GKNParser::M3Handler, this, std::placeholders::_1));
    addCommandMapping("M5", std::bind(&GKNParser::M5Handler, this, std::placeholders::_1));
    addCommandMapping("M6", std::bind(&GKNParser::M6Handler, this, std::placeholders::_1));
    addCommandMapping("M7", std::bind(&GKNParser::M7Handler, this, std::placeholders::_1));

    // multiple print speeds can be defined
    addCommandMapping("PRINT_SPEED", std::bind(&GKNParser::PrintSpeedHandler, this, std::placeholders::_1));
    addCommandMapping("PRINT_SPEED1", std::bind(&GKNParser::PrintSpeedHandler, this, std::placeholders::_1));
    addCommandMapping("PRINT_SPEED2", std::bind(&GKNParser::PrintSpeedHandler, this, std::placeholders::_1));
    addCommandMapping("PRINT_SPEED3", std::bind(&GKNParser::PrintSpeedHandler, this, std::placeholders::_1));
    addCommandMapping("PRINT_SPEED4", std::bind(&GKNParser::PrintSpeedHandler, this, std::placeholders::_1));

    addCommandMapping("RAPID_SPEED", std::bind(&GKNParser::RapidSpeedHandler, this, std::placeholders::_1));
    addCommandMapping("SCAN_SPEED", std::bind(&GKNParser::ScanSpeedHandler, this, std::placeholders::_1));
}

void GKNParser::G0Handler(QVector<QString> params) {
    QString newParameterList = m_x_param % params[0] % m_y_param % params[1] % m_z_param % params[2];

    CommonParser::G0Handler(newParameterList.split(' '));
}

void GKNParser::G1Handler(QVector<QString> params) {
    // sometimes speed is a hard-coded value, other times it is a variable to be filled in
    QString speed = params[8];
    if (m_default_values.contains(speed))
        speed = m_default_values[speed];

    QString newParameterList =
        m_x_param % params[0] % m_y_param % params[1] % m_z_param % params[2] % m_f_param % speed;

    CommonParser::G1Handler(newParameterList.split(' '));
}

void GKNParser::G4Handler(QVector<QString> params) {
    if (params.size() != 1) {
        return;
    }

    QString newParam = m_p_param % params[0];

    CommonParser::G4Handler(QVector<QString> {newParam.mid(0)});
}

void GKNParser::M3Handler(QVector<QString> params) {
    // NOP
}

void GKNParser::M5Handler(QVector<QString> params) {
    // NOP
}

void GKNParser::M6Handler(QVector<QString> params) {
    if (params.size() != 1) {
        // throwing errors deemed too restrictive, for now, if improperly formatted, skip command
        return;
    }
    m_extruders_on[0] = true;
}

void GKNParser::M7Handler(QVector<QString> params) {
    if (!params.empty()) {
        return;
    }

    m_extruders_on[0] = false;
}

// assume we find them in order
void GKNParser::PrintSpeedHandler(QVector<QString> params) {
    if (m_default_values.size() == 0)
        m_default_values.insert("PRINT_SPEED", params[1]);
    else
        m_default_values.insert("PRINT_SPEED" % QString::number(m_default_values.size()), params[1]);
}

void GKNParser::RapidSpeedHandler(QVector<QString> params) { m_default_values.insert("RAPID_SPEED", params[1]); }

void GKNParser::ScanSpeedHandler(QVector<QString> params) { m_default_values.insert("SCAN_SPEED", params[1]); }
} // namespace ORNL
