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
    // Tool change mappings
    addCommandMapping("M605", std::bind(&AdamantineParser::M605Handler, this, std::placeholders::_1));
    addCommandMapping("T0", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
    addCommandMapping("T1", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
    addCommandMapping("T2", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
    addCommandMapping("T3", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
    addCommandMapping("T4", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
    addCommandMapping("T5", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
    addCommandMapping("T6", std::bind(&AdamantineParser::THandler, this, std::placeholders::_1));
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

void AdamantineParser::M605Handler(QVector<QString> params) {
    int s_param = -1;
    for (const QString& ref : params) {
        bool no_error = true;
        char current_parameter = ref.at(0).toLatin1();
        int current_value = ref.right(ref.size() - 1).toInt(&no_error);

        if (!no_error) {
            throwFloatConversionErrorException();
        }

        if (current_parameter == 'S')
            s_param = current_value;
    }
    // assumes only two extruders
    if (s_param == 0) // unlink extruders
    {
        m_extruders_active[0] = false;
        m_extruders_active[1] = false;

        // extruder can't be inactive and on
        m_extruders_on[0] = false;
        m_extruders_on[1] = false;
    }
    else if (s_param == 2) // link extruders
    {
        m_extruders_active[0] = true;
        m_extruders_active[1] = true;
    }
}

void AdamantineParser::THandler(QVector<QString> params) {
    int extruder_number = m_current_gcode_command.getCommandID();
    for (int i = 0; i < m_num_extruders; ++i) {
        if (i == extruder_number)
            m_extruders_active[i] = true;
        else {
            // inactive extruders can not be on
            m_extruders_active[i] = false;
            m_extruders_on[i] = false;
        }
    }
}

} // namespace ORNL
