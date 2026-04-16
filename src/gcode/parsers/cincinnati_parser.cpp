#include "gcode/parsers/cincinnati_parser.h"

#include "QString"
#include "QStringList"
#include "QTextStream"
#include "QVector"
#include "exceptions/exceptions.h"
#include "units/unit.h"

namespace ORNL {
CincinnatiParser::CincinnatiParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();
}

void CincinnatiParser::config() {
    CommonParser::config();

    addCommandMapping("M0", std::bind(&CincinnatiParser::M0Handler, this, std::placeholders::_1));
    addCommandMapping("M1", std::bind(&CincinnatiParser::M1Handler, this, std::placeholders::_1));
    addCommandMapping("M10", std::bind(&CincinnatiParser::M10Handler, this, std::placeholders::_1));
    addCommandMapping("M11", std::bind(&CincinnatiParser::M11Handler, this, std::placeholders::_1));
    addCommandMapping("M12", std::bind(&CincinnatiParser::M12Handler, this, std::placeholders::_1));
    addCommandMapping("M13", std::bind(&CincinnatiParser::M13Handler, this, std::placeholders::_1));
    addCommandMapping("M14", std::bind(&CincinnatiParser::M14Handler, this, std::placeholders::_1));
    addCommandMapping("M15", std::bind(&CincinnatiParser::M15Handler, this, std::placeholders::_1));
    addCommandMapping("M16", std::bind(&CincinnatiParser::M16Handler, this, std::placeholders::_1));
    addCommandMapping("M30", std::bind(&CincinnatiParser::M30Handler, this, std::placeholders::_1));
    addCommandMapping("M60", std::bind(&CincinnatiParser::M60Handler, this, std::placeholders::_1));
    addCommandMapping("M61", std::bind(&CincinnatiParser::M61Handler, this, std::placeholders::_1));
    addCommandMapping("M64", std::bind(&CincinnatiParser::M64Handler, this, std::placeholders::_1));
    addCommandMapping("M65", std::bind(&CincinnatiParser::M65Handler, this, std::placeholders::_1));
    addCommandMapping("M66", std::bind(&CincinnatiParser::M66Handler, this, std::placeholders::_1));
    addCommandMapping("M68", std::bind(&CincinnatiParser::M68Handler, this, std::placeholders::_1));
    addCommandMapping("M69", std::bind(&CincinnatiParser::M69Handler, this, std::placeholders::_1));
    addCommandMapping("T", std::bind(&CincinnatiParser::ToolChangeHandler, this, std::placeholders::_1));
}

// TODO: This
void CincinnatiParser::M0Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M0 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

// TODO: This
void CincinnatiParser::M1Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M1 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M10Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M10 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    m_dynamic_spindle_control = true;
}

void CincinnatiParser::M11Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M11 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    m_dynamic_spindle_control = false;
}

void CincinnatiParser::M12Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M12 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M13Handler(QVector<QString> params) {
    // TODO: Insert spindle override, have no idea what this means.
    //       Basically add or subtract the value to the correct variable
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M13 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M14Handler(QVector<QString> params) {
    // TODO: Infill Spindle override. Have no idea what this does.
    //       Basically add or subtract the value to the correct variable
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M14 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M15Handler(QVector<QString> params) {
    // TODO: Skin Spindle override. Have no idea what this does.
    //       Basically add or subtract the value to the correct variable
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M15 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M16Handler(QVector<QString> params) {
    // TODO: Spindle Override Reset. Resets the 3 Commands above (M13, M14,
    // M15) back to 100%.
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M16 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M30Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M30 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M60Handler(QVector<QString> params) {
    // TODO: Add feed shaker variables and control logic.
}

void CincinnatiParser::M61Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M61 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M64Handler(QVector<QString> params) {
    // TODO: Voltage control (range of 0 - 1), Need to do this after I fix
    //       control commands having parameters
}

void CincinnatiParser::M65Handler(QVector<QString> params) {
    // TODO: Voltage control, (resets to 0? or a boolean on/off?)
    if (params.size() > 0)
        if (!params.empty()) {
            QString exceptionString;
            QTextStream(&exceptionString) << "M65 command should have no parameters . Error occured "
                                             "on GCode line "
                                          << m_current_gcode_command.getLineNumber() << Qt::endl
                                          << "."
                                          << "With GCode command string: " << getCurrentCommandString();
            throw IllegalParameterException(exceptionString);
        }

    m_voltage_control = false;
    m_voltage_control_value = 1.0;
}

void CincinnatiParser::M66Handler(QVector<QString> params) {
    // TODO: Sets the acceleration value, has a parameter.
    // NOTE: Apparently this command sets the inverse acceleration thing???
    char current_parameter;
    NT current_value;
    bool no_error, l_not_used = true;

    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command M66, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << Qt::endl
                                      << "GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);

        if (!no_error) {
            throwFloatConversionErrorException();
        }

        m_current_gcode_command.addParameter(current_parameter, current_value);

        switch (current_parameter) {
            case ('L'):
            case ('l'):
                if (l_not_used) {
                    // original equation to calculate acceleration
                    // this creates an acceleration as a percentage of g force
                    // 1 = encoder ticks per micrometer
                    // 1000000 = micron conversion
                    // 25.4 = mm per inch conversion
                    // 162560 = encoder ticks per inch
                    // 1000 = m to mm conversion
                    // 9.81 = accel due to gravity (g force) in m/s^2
                    // current_value = (1 / travelAccel * 1000000 * 25.4 / 162560) / (1000 * 9.81));

                    // to reverse this mess, we do the following
                    double encoderTicksPerInch = 162560;
                    Distance mToMM, mToMicron, mmToInch;
                    mToMM.from(1, mm);
                    mToMicron.from(1, m);
                    // ORNLSlicer is micron internally. So, instead of 1 to go from inch->mm,
                    //.001 to go from inch->micron
                    mmToInch.from(.001, inch);
                    // the raw form of the equation below is:
                    //(1 / current_value) * 1000000 * 25.4 / 162560 * 1000;

                    setAcceleration(((1 / current_value) * mToMicron * mmToInch / encoderTicksPerInch * mToMM)());
                    l_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
        }
    }

    if (l_not_used) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error not all required parameters passed for GCode command "
                                         "on line  "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}

void CincinnatiParser::M68Handler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "M68 command should have no parameters . Error occured on "
                                         "GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "."
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    m_park = true;
}

void CincinnatiParser::M69Handler(QVector<QString> params) {
    // TODO: Purges the extruder.
    // Sets defaults
    m_return_to_prev_location = false;
    setSpindleSpeed(250.0);
    // TODO: make setters and getters for this handler
    m_purge_time = 60 * s;
    m_wait_to_wipe_time = 0 * s;
    m_wait_time_to_start_purge = 0 * s;

    char current_parameter;
    NT current_value;
    bool no_error, f_not_used = true, l_not_used = true, p_not_used = true, s_not_used = true, t_not_used = true;

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        if (!no_error) {
            throwFloatConversionErrorException();
        }

        m_current_gcode_command.addParameter(current_parameter, current_value);

        switch (current_parameter) {
            case ('F'):
            case ('f'):
                if (f_not_used) {
                    setSpindleSpeed(current_value);
                    f_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }

                break;

            case ('L'):
            case ('l'):
                if (l_not_used) {
                    m_return_to_prev_location = static_cast<bool>(current_value);
                    l_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }

                break;

            case ('P'):
            case ('p'):
                if (p_not_used) {
                    m_purge_time = current_value * s;
                    p_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }

                break;

            case ('S'):
            case ('s'):
                if (s_not_used) {
                    m_wait_to_wipe_time = current_value * s;
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }

                break;

            case ('T'):
            case ('t'):
                if (t_not_used) {
                    m_wait_time_to_start_purge = current_value * s;
                    t_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }

                break;

            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command M64." << Qt::endl
                    << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
                break;
        }
    }
}

void CincinnatiParser::ToolChangeHandler(QVector<QString> params) {
    if (!params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Tool change command should have no parameters . Error "
                                         "occured on GCode line "
                                      << m_current_gcode_command.getLineNumber() << Qt::endl
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
}
} // namespace ORNL
