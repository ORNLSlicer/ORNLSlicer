#include "gcode/parsers/common_parser.h"

#include "exceptions/exceptions.h"
#include "gcode/gcode_motion_estimate.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"

#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <QtMath>
#include <QRegExp>

namespace ORNL {
CommonParser::CommonParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : m_e_absolute(true), m_space(' '), m_distance_unit(meta.m_distance_unit), m_time_unit(meta.m_time_unit),
      m_angle_unit(meta.m_angle_unit), m_mass_unit(meta.m_mass_unit), m_velocity_unit(meta.m_velocity_unit),
      m_acceleration_unit(meta.m_acceleration_unit), m_angular_velocity_unit(meta.m_angular_velocity_unit),
      m_layer_delimiter(meta.m_layer_delimiter), m_layer_count_delimiter(meta.m_layer_count_delimiter),
      m_allow_layer_alter(allowLayerAlter), m_lines(lines), m_upper_lines(upperLines), m_current_line(0),
      m_current_end_line(m_lines.size() - 1), m_was_modified(false), m_last_layer_line_start(0), m_g4_prefix("G4 P"),
      m_g4_comment("DWELL"), m_f_param_and_value("F[^\\D]\\d*\\.*\\d*"), m_q_param_and_value("Q[^\\D]\\d*\\.*\\d*"),
      m_s_param_and_value("S[^\\D]\\d*\\.*\\d*"), m_f_parameter('F'), m_q_parameter('Q'), m_s_parameter('S'),
      m_should_cancel(false), m_negate_z_value(false) {
    setBlockCommentDelimiters(meta.m_comment_starting_delimiter, meta.m_comment_ending_delimiter);

    MotionEstimation::Init();

    if (meta == GcodeMetaList::SkyBaamMeta || meta == GcodeMetaList::KraussMaffeiMeta)
        m_g4_prefix = "G4 S";

    if (meta == GcodeMetaList::MVPMeta) {
        m_negate_z_value = true;
        m_g4_prefix = "G4 F";
    }

    m_insertions = 0;

    m_current_nozzle = 0; // 0th nozzle is default

    config();

    MotionEstimation::m_total_distance = 0;
    MotionEstimation::m_printing_distance = 0;
    MotionEstimation::m_travel_distance = 0;
}

Distance CommonParser::getCurrentGXDistance() {
    QSharedPointer<SettingsBase> sb = GSM->getGlobal();
    bool uses_b = sb->setting<bool>(Constants::MaterialSettings::Filament::kFilamentBAxis);

    return MotionEstimation::calculateTimeAndVolume(
        m_current_layer, m_with_F_value, m_current_gcode_command.getCommandID() == 0, m_extruders_on,
        m_layer_G1F_times[m_current_layer], m_layer_times[m_current_layer][m_current_nozzle],
        m_layer_volumes[m_current_layer], uses_b);
}

// currently nothing of interest in header, so skip as long as line starts with
// comment or is whitespace
void CommonParser::parseHeader() {
    int totalLines = m_lines.size();
    QStringMatcher delimiterSearch(m_layer_count_delimiter);
    while ((startsWithDelimiter(m_upper_lines[m_current_line]) &&
            delimiterSearch.indexIn(m_upper_lines[m_current_line]) == -1) ||
           m_upper_lines[m_current_line].length() == 0) {
        ++m_current_line;
        if (m_current_line > totalLines)
            break;
    }
}

QHash<QString, double> CommonParser::parseFooter() {
    QLatin1String printerHeader("PRINTER SETTINGS");
    QLatin1String materialHeader("MATERIAL SETTINGS");
    Velocity v;
    m_necessary_variables_copy = Constants::GcodeFileVariables::kNecessaryVariables;

    bool foundForcedMinLayerTime = false;
    bool foundForcedMinLayerTimeMethod = false;
    while (startsWithDelimiter(m_upper_lines[m_current_end_line]) || m_upper_lines[m_current_end_line].length() == 0) {
        if (m_upper_lines[m_current_end_line].length() > 0) {
            QString setting = parseComment(m_upper_lines[m_current_end_line]);
            if (setting.compare(printerHeader) != 0 && setting.compare(materialHeader) != 0) {
                QVector<QString> setting_split = setting.split(' ', Qt::SkipEmptyParts);
                // make sure we have a valid pair
                if (setting_split.size() == 2) {
                    QString key = setting_split[0]; // may or may not be suffixed
                    QString key_root = key;

                    // if key is suffixed, remove the suffix
                    int suffix_loc = -1;
                    suffix_loc = key_root.lastIndexOf(QRegularExpression("_\\d+"));
                    if (suffix_loc >= 0)
                        key_root.truncate(suffix_loc);

                    // search for root when deciding whether or not to parse the setting
                    QHash<QString, QString>::const_iterator it = m_necessary_variables_copy.find(key_root);
                    if (it != m_necessary_variables_copy.end()) {
                        double value = setting_split[1].toDouble();
                        if (Constants::GcodeFileVariables::kRequiredConversion.find(key_root) !=
                            Constants::GcodeFileVariables::kRequiredConversion.end()) {
                            m_file_settings.insert(it.value(), v.from(value, mm / s));
                        }
                        else {
                            // material type is 0 based in slicer 2
                            if (key == Constants::GcodeFileVariables::kPlasticType)
                                m_file_settings.insert(key.toLower(), value - 1);
                            else
                                m_file_settings.insert(key.toLower(), value);

                            if (key == Constants::GcodeFileVariables::kForceMinLayerTime)
                                foundForcedMinLayerTime = true;
                            else if (key == Constants::GcodeFileVariables::kForceMinLayerTimeMethod)
                                foundForcedMinLayerTimeMethod = true;

                            if (key_root.toLower() == Constants::ExperimentalSettings::MultiNozzle::kNozzleCount) {
                                m_num_extruders = (int)value >= 1 ? (int)value : 1;
                                for (int i = 0; i < m_num_extruders; ++i) {
                                    m_extruders_on.push_back(false);
                                    m_extruders_active.push_back(false);
                                }
                                m_extruders_active[0] = true;
                            }
                        }

                        QString possible_other_key = it.value().toUpper();
                        m_necessary_variables_copy.remove(it.key());
                        m_necessary_variables_copy.remove(possible_other_key);
                    }
                }
            }
        }
        --m_current_end_line;
        if (m_current_end_line < 0)
            break;
    }

    // convert force minimum layer time setting from Slicer-1 to Slicer-2 if needed
    // Slicer-1
    //   force_minimum_layer_time: enum, 0=DISABLED, 1=Dwell time, 2=Modify feedrate
    // Slicer-2
    //   force_minimum_layer_time: bool
    //   minimum_layer_time_method: enum, 0=Dwell time, 1=Modify feedrate
    if (foundForcedMinLayerTime && !foundForcedMinLayerTimeMethod) {
        int oldValue = int(m_file_settings[Constants::MaterialSettings::Cooling::kForceMinLayerTime]);
        if (oldValue > 0) {
            m_file_settings[Constants::MaterialSettings::Cooling::kForceMinLayerTime] = 1;
            m_file_settings.insert(Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod, oldValue - 1);
            m_necessary_variables_copy.remove(Constants::GcodeFileVariables::kForceMinLayerTimeMethod);
        }
    }

    checkAndSetNecessarySettings();

    if (m_num_extruders == 0) {
        m_num_extruders = 1;
        for (int i = 0; i < m_num_extruders; ++i) {
            m_extruders_on.push_back(false);
            m_extruders_active.push_back(false);
        }
        m_extruders_active[0] = true;
    }

    // after parsing all nozzle offsets, group by extruder and put into vector
    for (int i = 0; i < m_num_extruders; ++i) {
        QString x_key = Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetX + "_" + QString::number(i);
        QString y_key = Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetY + "_" + QString::number(i);
        QString z_key = Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetZ + "_" + QString::number(i);

        double x = m_file_settings[x_key];
        double y = m_file_settings[y_key];
        double z = m_file_settings[z_key];
        m_extruder_offsets.push_back(Point(x, y, z));
    }

    // return copy to gcode loader as several settings are required to calculate visualization
    return m_file_settings;
}

void CommonParser::checkAndSetNecessarySettings() {
    // some settings weren't found, so load from current settings
    if (m_necessary_variables_copy.size() > 0) {
        QSharedPointer<SettingsBase> sb = GSM->getGlobal();
        QHashIterator<QString, QString> i(m_necessary_variables_copy);
        while (i.hasNext()) {
            i.next();
            QString currentVal = i.value();
            if (currentVal == Constants::MaterialSettings::Cooling::kForceMinLayerTime)
                m_file_settings.insert(i.value(), (double)sb->setting<bool>(currentVal));
            else
                m_file_settings.insert(i.value(), sb->setting<double>(currentVal));
        }
    }

    MotionEstimation::z_speed = m_file_settings[Constants::PrinterSettings::MachineSpeed::kZSpeed];
    MotionEstimation::max_xy_speed = m_file_settings[Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed];
    MotionEstimation::w_table_speed = m_file_settings[Constants::PrinterSettings::MachineSpeed::kWTableSpeed];
    MotionEstimation::layerThickness = m_file_settings[Constants::ProfileSettings::Layer::kLayerHeight];
    MotionEstimation::extrusionWidth = m_file_settings[Constants::ProfileSettings::Layer::kBeadWidth];

    if (MotionEstimation::max_xy_speed == 0) {
        MotionEstimation::max_xy_speed = 25400;
        emit forwardInfoToMainWindow("Machine max speed is not set, using max speed as 1.00 in/sec");
    }

    if (m_allow_layer_alter) {
        if (m_file_settings[Constants::MaterialSettings::Cooling::kForceMinLayerTime]) {
            m_min_layer_time_choice = static_cast<ForceMinimumLayerTime>(
                (int)m_file_settings[Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod]);
            m_min_layer_time_allowed = m_file_settings[Constants::MaterialSettings::Cooling::kMinLayerTime];
            m_max_layer_time_allowed = m_file_settings[Constants::MaterialSettings::Cooling::kMaxLayerTime];
        }
    }
}

void CommonParser::preallocateVisualCommands(int layerSkip) {
    // iterate through file looking only for the number of layers and peeking at first char
    // to determine number of motion commands
    // this is to preallocate memory for later return for visualization
    QStringMatcher layerCountIdentifier(m_layer_count_delimiter);
    bool yetToFindLayerCount = true;
    QStringMatcher layerDelimiter(m_layer_delimiter);
    int commandsInLayer = 0;
    QRegExp digitExpression("\\d+");
    QRegularExpression gMotionCommand("^G0|^G1|^G2|^G3|^G5");
    m_current_layer = 0;
    bool skip = false;
    for (int i = m_current_line; i < m_current_end_line; ++i) {
        // find layer total, only executed once
        if (yetToFindLayerCount && layerCountIdentifier.indexIn(m_upper_lines[i]) != -1) {
            if (digitExpression.indexIn(m_upper_lines[i]) != -1) {
                m_motion_commands.reserve(digitExpression.cap(0).toInt() + 1);
            }
            yetToFindLayerCount = false;
        }

        if (m_upper_lines[i].length() > 0) {
            if (skip) {
                if (layerDelimiter.indexIn(m_upper_lines[i]) != -1) {
                    m_motion_commands.push_back(QList<GcodeCommand>());
                    ++m_current_layer;
                    skip = m_current_layer % layerSkip != 0;
                }
            }
            else {
                // most common case, valid motion command
                if (m_upper_lines[i].indexOf(gMotionCommand) == 0) {
                    commandsInLayer++;
                }
                // lastly, when reaching a new layer or the final line, allocate memory
                else if (layerDelimiter.indexIn(m_upper_lines[i]) != -1) {
                    QList<GcodeCommand> commands;
                    commands.reserve(commandsInLayer);
                    m_motion_commands.push_back(commands);
                    commandsInLayer = 0;
                    ++m_current_layer;
                    skip = m_current_layer % layerSkip != 0;
                }
            }
        }
    }
    // allocate final layer not captured by loop
    QList<GcodeCommand> commands;
    commands.reserve(commandsInLayer);
    m_motion_commands.push_back(commands);

    // reset layer
    m_current_layer = 0;
}

QList<QList<GcodeCommand>> CommonParser::parseLines(int layerSkip) {
    QSharedPointer<SettingsBase> sb = GSM->getGlobal();
    preallocateVisualCommands(layerSkip);

    m_layer_start_lines.reserve(m_motion_commands.size());
    m_layer_start_lines.push_back(1);

    QList<Time> extruder_times;
    for (int i = 0; i < m_num_extruders; ++i)
        extruder_times.push_back(Time());

    m_layer_times.push_back(extruder_times);
    m_layer_FR_modifiers.push_back(1.0);
    m_layer_G1F_times.push_back(Time());
    m_layer_volumes.push_back(Volume());

    bool skip = false;
    int actualLayer = 0;
    QStringMatcher layerDelimiter(m_layer_delimiter);

    QString newCurrentLine, zOffsetString;
    double currentZOffset = sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kZOffset).to(m_distance_unit);
    bool no_error;

    // parse each line
    for (; m_current_line <= m_current_end_line; ++m_current_line) {
        // extract components of command
        // handlers will also update appropriate state
        // if the line is not just an empty line or whitespace
        if (m_upper_lines[m_current_line].contains("[#") &&
            sb->setting<int>(Constants::PrinterSettings::Dimensions::kUseVariableForZ)) {
            // If using a variable Z, find the value added to the variable, calculate the total value of Z by replacing
            // the variable with the value from the printer settings then create a new line of g-code to be sent to the
            // parser that is formatted the way a line is normally formatted (without the variable)
            double zVal;
            int second = m_upper_lines[m_current_line].indexOf("]");
            int zLoc = m_upper_lines[m_current_line].indexOf("Z");
            if (m_upper_lines[m_current_line].contains("+")) {
                int first = m_upper_lines[m_current_line].indexOf("+") + 2;
                QString zAdditionString = m_upper_lines[m_current_line].mid(first, second - first);
                zVal = zAdditionString.toDouble(&no_error);
                if (!no_error) {
                    throwFloatConversionErrorException();
                }
            }
            else {
                zVal = 0;
            }

            zVal += currentZOffset;
            newCurrentLine = m_upper_lines[m_current_line].mid(0, zLoc + 1) % QString::number(zVal, 'f', 4) %
                             m_upper_lines[m_current_line].mid(second + 1);
        }
        else if (m_upper_lines[m_current_line].contains("[#")) {
            // Ignore lines with variable definition if variable Z is not enabled
            continue;
        }
        else if (m_upper_lines[m_current_line].contains("Z=VPSLZ")) {
            // Ignore this line from the Okuma header
            continue;
        }
        else if (m_upper_lines[m_current_line].contains("EXTRUDER(0)")) {
            for (int i = 0, end = m_extruders_on.size(); i < end; ++i) {
                if (m_extruders_active[i])
                    m_extruders_on[i] = false;
            }
            continue;
        }
        else if (m_upper_lines[m_current_line].contains("EXTRUDER(")) {
            for (int i = 0, end = m_extruders_on.size(); i < end; ++i) {
                if (m_extruders_active[i])
                    m_extruders_on[i] = true;
            }
            continue;
        }
        else if (m_upper_lines[m_current_line].contains("RZ") && m_upper_lines[m_current_line].contains("PC")) {
            newCurrentLine = removeRotations(m_upper_lines[m_current_line]);
        }
        else {
            // Save the current line to be sent for the parseCommand function
            newCurrentLine = m_upper_lines[m_current_line];
        }
        if (!m_upper_lines[m_current_line].mid(0).trimmed().isEmpty()) {
            if (skip) {
                m_layer_skip_lines.insert(m_current_line);
                if (layerDelimiter.indexIn(m_upper_lines[m_current_line]) != -1) {
                    ++m_current_layer;
                    skip = m_current_layer % layerSkip != 0;

                    QList<Time> extruder_times;
                    for (int i = 0; i < m_num_extruders; ++i)
                        extruder_times.push_back(Time());

                    m_layer_times.push_back(extruder_times);
                    m_layer_FR_modifiers.push_back(1.0);
                    m_layer_G1F_times.push_back(Time());
                    m_layer_volumes.push_back(Volume());

                    m_layer_start_lines.push_back(m_current_line + 1);
                }
            }
            else {
                parseCommand(newCurrentLine, m_current_line + m_insertions);

                // If a new layer has just started, check if previous layer needs to be adjusted
                // to meet the minimum layer time
                if (m_current_gcode_command.getCommandIsEndOfLayer() || m_current_line == m_current_end_line) {
                    if (m_file_settings[Constants::MaterialSettings::Cooling::kForceMinLayerTime] &&
                        m_allow_layer_alter && m_current_layer > 0) {
                        Time increaseTime = m_min_layer_time_allowed - m_layer_times[m_current_layer][m_current_nozzle];
                        Time decreaseTime = m_layer_times[m_current_layer][m_current_nozzle] - m_max_layer_time_allowed;

                        double minModifier = std::numeric_limits<double>::max();
                        double maxModifier = 1;
                        if (increaseTime > 0 || decreaseTime > 0)
                            getMinMaxModifier(minModifier, maxModifier);

                        if (m_layer_G1F_times[m_current_layer] > 0) {
                            if (increaseTime > 0) { // If layer time less than minimum, slow feedrate or add dwell
                                if (m_min_layer_time_choice == ForceMinimumLayerTime::kSlow_Feedrate) {
                                    // Ratio uses the layer time as well as the total time for all G1 F moves, which are
                                    // what get adjusted
                                    double ratio = (increaseTime / m_layer_G1F_times[m_current_layer])();
                                    double modifier = 1 / (1.0 + ratio);

                                    if (modifier < minModifier && minModifier > 0 && minModifier < 1) {
                                        modifier = minModifier;
                                        emit forwardInfoToMainWindow("Computed speed is lower than min machine speed, "
                                                                     "machine min speed will be used");
                                    }

                                    if (modifier > 0 && modifier < 1) {
                                        AdjustFeedrate(modifier);
                                        m_was_modified = true;
                                    }
                                }
                                else if (m_min_layer_time_choice == ForceMinimumLayerTime::kUse_Purge_Dwells) {
                                    AddDwell(increaseTime());
                                    m_was_modified = true;
                                }
                            }
                            else if (decreaseTime > 0) { // If layer time more than maximum, increase feedrate
                                if (m_min_layer_time_choice == ForceMinimumLayerTime::kSlow_Feedrate) {
                                    // Ratio uses the layer time as well as the total time for all G1 F moves, which are
                                    // what get adjusted
                                    double ratio = (decreaseTime / m_layer_G1F_times[m_current_layer])();
                                    double modifier = 1 / (1.0 - ratio);

                                    if (modifier > maxModifier && maxModifier > 1) {
                                        modifier = maxModifier;
                                        emit forwardInfoToMainWindow(
                                            "Computed speed exceeds max machine speed, machine max speed will be used");
                                    }

                                    if (modifier > 1) {
                                        AdjustFeedrate(modifier);
                                        m_was_modified = true;
                                    }
                                }
                                else if (m_min_layer_time_choice == ForceMinimumLayerTime::kUse_Purge_Dwells) {
                                    emit forwardInfoToMainWindow(
                                        "Add dwell time method was selected, can not modify layer times");
                                }
                            }
                        }
                    }

                    if (m_current_line == m_current_end_line)
                        break;

                    m_last_layer_line_start = m_current_line;
                    //++actualLayer;
                    ++m_current_layer;
                    if (m_current_layer <= 1 || layerSkip == 1)
                        skip = false;
                    else
                        skip = m_current_layer % layerSkip != 0;

                    // add empty slots to arrays for this layer
                    QList<Time> extruder_times;
                    for (int i = 0; i < m_num_extruders; ++i)
                        extruder_times.push_back(Time());

                    m_layer_times.push_back(extruder_times);
                    m_layer_FR_modifiers.push_back(1.0);
                    m_layer_G1F_times.push_back(Time());
                    m_layer_volumes.push_back(Volume());

                    m_layer_start_lines.push_back(m_current_line + 1);
                }
            }

            emit statusUpdate(StatusUpdateStepType::kGcodeParsing,
                              qRound((double)(m_current_line + 1) / (double)(m_current_end_line + 1) * 100));
        }

        if (m_should_cancel)
            return QList<QList<GcodeCommand>>();
    }

    // emit layer times and key info
    return m_motion_commands;
}

void CommonParser::config() {
    // Clears the command mappings to prevent any previous from interferring
    reset();

    addCommandMapping("G0", std::bind(&CommonParser::G0Handler, this, std::placeholders::_1));
    addCommandMapping("G1", std::bind(&CommonParser::G1Handler, this, std::placeholders::_1));
    addCommandMapping("G2", std::bind(&CommonParser::G2Handler, this, std::placeholders::_1));
    addCommandMapping("G3", std::bind(&CommonParser::G3Handler, this, std::placeholders::_1));
    addCommandMapping("G4", std::bind(&CommonParser::G4Handler, this, std::placeholders::_1));
    addCommandMapping("G5", std::bind(&CommonParser::G5Handler, this, std::placeholders::_1));
    addCommandMapping("M3", std::bind(&CommonParser::M3Handler, this, std::placeholders::_1));
    addCommandMapping("M5", std::bind(&CommonParser::M5Handler, this, std::placeholders::_1));
}

NT CommonParser::getXPos() const { return getXPos(m_distance_unit); }

NT CommonParser::getXPos(Distance distance_unit) const { return MotionEstimation::m_current_x.to(distance_unit); }

NT CommonParser::getYPos() const { return getYPos(m_distance_unit); }

NT CommonParser::getYPos(Distance distance_unit) const { return MotionEstimation::m_current_y.to(distance_unit); }

NT CommonParser::getZPos() const { return getZPos(m_distance_unit); }

NT CommonParser::getZPos(Distance distance_unit) const { return MotionEstimation::m_current_z.to(distance_unit); }

NT CommonParser::getWPos() const { return getWPos(m_distance_unit); }

NT CommonParser::getWPos(Distance distance_unit) const { return MotionEstimation::m_current_w.to(distance_unit); }

NT CommonParser::getArcXPos() const { return getArcXPos(m_distance_unit); }

NT CommonParser::getArcXPos(Distance distance_unit) const { return m_current_arc_center_x.to(distance_unit); }

NT CommonParser::getArcYPos() const { return getArcYPos(m_distance_unit); }

NT CommonParser::getArcYPos(Distance distance_unit) const { return m_current_arc_center_y.to(distance_unit); }

NT CommonParser::getSpeed() const { return getSpeed(m_velocity_unit); }

NT CommonParser::getSpeed(Velocity velocity_unit) const { return MotionEstimation::m_current_speed.to(velocity_unit); }

NT CommonParser::getSpindleSpeed() const { return getSpindleSpeed(rev / minute); }

NT CommonParser::getSpindleSpeed(AngularVelocity angular_velocity_unit) const {
    return m_current_spindle_speed.to(angular_velocity_unit);
}

NT CommonParser::getAcceleration() const { return getAcceleration(m_acceleration_unit); }

NT CommonParser::getAcceleration(Acceleration acceleration_unit) const {
    return MotionEstimation::m_current_acceleration.to(acceleration_unit);
}

NT CommonParser::getSleepTime() const { return getSleepTime(m_time_unit); }

NT CommonParser::getSleepTime(Time time_unit) const { return m_sleep_time.to(time_unit); }

void CommonParser::reset() {
    resetInternalState();
    MotionEstimation::m_current_x = 0 * m_distance_unit;
    MotionEstimation::m_current_y = 0 * m_distance_unit;
    MotionEstimation::m_current_z = 0 * m_distance_unit;
    MotionEstimation::m_current_w = 0 * m_distance_unit;
    MotionEstimation::m_current_e = 0 * m_distance_unit;

    m_current_arc_center_x = 0 * m_distance_unit;
    m_current_arc_center_y = 0 * m_distance_unit;

    MotionEstimation::m_current_speed = 0 * m_velocity_unit;

    m_current_spindle_speed = 0.0 * m_angle_unit / m_time_unit;

    m_current_extruders_speed = 0.0;

    // all machines, except for BAAM, use a constant acceleration that is not set through the Slicer in any way
    // So don't reset to 0 here and leave it to the default acceleration set in the constructor
    // m_current_acceleration = 0 * m_acceleration_unit;

    m_sleep_time = 0 * m_time_unit;
    m_purge_time = 0 * m_time_unit;
    m_wait_to_wipe_time = 0 * m_time_unit;
    m_wait_time_to_start_purge = 0 * m_time_unit;

    for (int i = 0; i < m_num_extruders; ++i)
        m_extruders_on[i] = false;
    //        m_extruder_ON                 = false;
    m_dynamic_spindle_control = false;
    m_park = false;
}

QList<QList<Time>> CommonParser::getLayerTimes() { return m_layer_times; }

QList<double> CommonParser::getLayerFeedRateModifiers() { return m_layer_FR_modifiers; }

QList<Volume> CommonParser::getLayerVolumes() { return m_layer_volumes; }

Distance CommonParser::getTotalDistance() { return MotionEstimation::m_total_distance; }

Distance CommonParser::getPrintingDistance() { return MotionEstimation::m_printing_distance; }

Distance CommonParser::getTravelDistance() { return MotionEstimation::m_travel_distance; }

bool CommonParser::getWasModified() { return m_was_modified; }

void CommonParser::cancelSlice() { m_should_cancel = true; }

QList<int> CommonParser::getLayerStartLines() { return m_layer_start_lines; }

QSet<int> CommonParser::getLayerSkipLines() { return m_layer_skip_lines; }

int CommonParser::getCurrentLine() { return m_current_line; }

void CommonParser::alterCurrentEndLine(int count) { m_current_end_line += count; }

void CommonParser::setModified() { m_was_modified = true; }

void CommonParser::setXPos(NT value) { MotionEstimation::m_current_x = value; }

void CommonParser::setYPos(NT value) { MotionEstimation::m_current_y = value; }

void CommonParser::setZPos(NT value) { MotionEstimation::m_current_z = value; }

void CommonParser::setWPos(NT value) { MotionEstimation::m_current_w = value; }

void CommonParser::setArcXPos(NT value) { m_current_arc_center_x = value; }

void CommonParser::setArcYPos(NT value) { m_current_arc_center_y = value; }

void CommonParser::setArcZPos(NT value) { m_current_arc_center_z = value; }

void CommonParser::setArcRPos(NT value) { m_current_arc_radius = value; }

void CommonParser::setSpeed(NT value) { MotionEstimation::m_current_speed = value; }

void CommonParser::setSpindleSpeed(NT value) { m_current_spindle_speed = value * m_angular_velocity_unit; }

void CommonParser::setAcceleration(NT value) { MotionEstimation::m_current_acceleration = value; }

void CommonParser::setSleepTime(NT value) { m_sleep_time = value; }

void CommonParser::G0Handler(QVector<QString> params) {
    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command G0, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << "\n"
                                      << "GCode command string: " << getCurrentCommandString();

        throw IllegalParameterException(exceptionString);
    }

    char current_parameter;
    NT current_value;
    bool no_error, x_not_used = true, y_not_used = true, z_not_used = true, w_not_used = true,
                   is_motion_command = false;

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        if (!no_error) {
            throwFloatConversionErrorException();
        }

        current_value *= m_distance_unit();
        m_current_gcode_command.addParameter(current_parameter, current_value);
        switch (current_parameter) {
            case ('X'):
            case ('x'):
                if (x_not_used) {
                    setXPos(current_value);
                    x_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Y'):
            case ('y'):
                if (y_not_used) {
                    setYPos(current_value);
                    y_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Z'):
            case ('z'):
                if (z_not_used) {
                    setZPos(current_value);
                    z_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('W'):
            case ('w'):
                if (w_not_used) {
                    setWPos(current_value);
                    w_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command G0" << "\n"
                    << "With GCode command string: " << getCurrentCommandString();
                //                    throw IllegalParameterException(exceptionString);
                break;
        }
    }

    m_current_gcode_command.setExtrudersOn(m_extruders_on);
    m_current_gcode_command.setExtruderOffsets(m_extruder_offsets);
    m_current_gcode_command.setExtrudersSpeed(m_current_extruders_speed);

    if (is_motion_command) {
        m_motion_commands[m_current_layer].push_back(m_current_gcode_command);
    }

    Distance temp = getCurrentGXDistance();
    MotionEstimation::m_total_distance += temp;

    bool isPrinting = false;
    for (int i = 0; i < m_extruders_on.size(); i++) {
        if (m_extruders_on[i]) {
            MotionEstimation::m_printing_distance += temp;
            isPrinting = true;
            break;
        }
    }
    if (!isPrinting)
        MotionEstimation::m_travel_distance += temp;
}

void CommonParser::G1Handler(QVector<QString> params) {
    // validate parameters
    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command G1, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << "\n"
                                      << "GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    char current_parameter;
    NT current_value;
    bool no_error, x_not_used = true, y_not_used = true, z_not_used = true, w_not_used = true, f_not_used = true,
                   s_not_used = true, e_not_used = true, is_motion_command = false;

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        if (!no_error) {
            throwFloatConversionErrorException();
        }

        switch (current_parameter) {
            case ('X'):
            case ('x'):
                if (x_not_used) {
                    current_value *= m_distance_unit();
                    setXPos(current_value);
                    x_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Y'):
            case ('y'):
                if (y_not_used) {
                    current_value *= m_distance_unit();
                    setYPos(current_value);
                    y_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Z'):
            case ('z'):
                if (z_not_used) {
                    if (m_negate_z_value) {
                        current_value = current_value * -1;
                    }
                    current_value *= m_distance_unit();
                    setZPos(current_value);
                    z_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('W'):
            case ('w'):
                if (w_not_used) {
                    current_value *= m_distance_unit();
                    setWPos(current_value);
                    w_not_used = false;
                    is_motion_command = true;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('F'):
            case ('f'):
                if (f_not_used) {
                    current_value *= m_velocity_unit();
                    setSpeed(current_value);
                    f_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('S'):
            case ('s'):
                if (s_not_used) {
                    current_value *= m_angular_velocity_unit();
                    setSpindleSpeed(current_value);
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Q'):
            case ('q'):
                if (s_not_used) {
                    current_value *= m_angular_velocity_unit();
                    setSpindleSpeed(current_value);
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('M'):
            case ('m'):
                break;

            case ('L'):
            case ('l'):
                break;

            case ('A'):
            case ('a'):
            case ('B'):
            case ('b'):
            case ('E'):
            case ('e'):
                if (e_not_used) {
                    current_value *= m_distance_unit();
                    if (m_e_absolute) {
                        if (current_value > MotionEstimation::m_previous_e)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    else {
                        if (current_value > 0)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    MotionEstimation::m_current_e = current_value;
                    e_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command G1" << "\n"
                    << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
        }
        m_current_gcode_command.addParameter(current_parameter, current_value);
    }

    m_current_gcode_command.setExtrudersOn(m_extruders_on);
    m_current_gcode_command.setExtruderOffsets(m_extruder_offsets);
    m_current_gcode_command.setExtrudersSpeed(m_current_extruders_speed);

    if (is_motion_command) {
        m_motion_commands[m_current_layer].push_back(m_current_gcode_command);
    }

    m_with_F_value = m_current_spindle_speed != 0;

    Distance temp = getCurrentGXDistance();
    MotionEstimation::m_total_distance += temp;

    bool isPrinting = false;
    for (int i = 0; i < m_extruders_on.size(); i++) {
        if (m_extruders_on[i]) {
            MotionEstimation::m_printing_distance += temp;
            isPrinting = true;
            break;
        }
    }
    if (!isPrinting)
        MotionEstimation::m_travel_distance += temp;

    m_with_F_value = false;
}

void CommonParser::G1HandlerHelper(QVector<QString> params, QVector<QString> optionalParams) {
    char current_parameter;
    NT current_value;
    bool no_error;

    for (QString ref : optionalParams) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        m_current_gcode_command.addOptionalParameter(current_parameter, current_value * m_distance_unit());
    }

    G1Handler(params);
}

void CommonParser::G2Handler(QVector<QString> params) {
    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command G2, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << "\n"
                                      << "GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    char current_parameter;
    NT current_value;
    NT temp_x = getXPos(), temp_y = getYPos(), temp_z = getZPos();
    bool no_error, x_not_used = true, y_not_used = true, z_not_used = true, i_not_used = true, j_not_used = true,
                   k_not_used = true, f_not_used = true, s_not_used = true, w_not_used = true, e_not_used = true,
                   r_not_used = true;
    ;

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        if (!no_error) {
            throwFloatConversionErrorException();
        }

        current_value *= m_distance_unit();
        m_current_gcode_command.addParameter(current_parameter, current_value);

        switch (current_parameter) {
            case ('X'):
            case ('x'):
                if (x_not_used) {
                    temp_x = current_value;
                    x_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Y'):
            case ('y'):
                if (y_not_used) {
                    temp_y = current_value;
                    y_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Z'):
            case ('z'):
                if (z_not_used) {
                    temp_z = current_value;
                    z_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('F'):
            case ('f'):
                if (f_not_used) {
                    setSpeed(current_value);
                    f_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('I'):
            case ('i'):
                if (i_not_used) {
                    setArcXPos(getXPos() + current_value);
                    i_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('J'):
            case ('j'):
                if (j_not_used) {
                    setArcYPos(getYPos() + current_value);
                    j_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('K'):
            case ('k'):
                if (k_not_used) {
                    setArcZPos(getZPos() + current_value);
                    k_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('R'):
            case ('r'):
                if (r_not_used) {
                    setArcRPos(current_value);
                    r_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('S'):
            case ('s'):
                if (s_not_used) {
                    current_value *= m_angular_velocity_unit();
                    setSpindleSpeed(current_value);
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('W'):
            case ('w'):
                if (w_not_used) {
                    setWPos(current_value);
                    w_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('E'):
            case ('e'):
                if (e_not_used) {
                    current_value *= m_distance_unit();
                    if (m_e_absolute) {
                        if (current_value > MotionEstimation::m_previous_e)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    else {
                        if (current_value > 0)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    MotionEstimation::m_current_e = current_value;
                    e_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command G2" << "\n"
                    << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
        }
    }

    m_current_gcode_command.setExtrudersOn(m_extruders_on);
    m_current_gcode_command.setExtruderOffsets(m_extruder_offsets);
    m_current_gcode_command.setExtrudersSpeed(m_current_extruders_speed);

    m_motion_commands[m_current_layer].push_back(m_current_gcode_command);

    // Checks if all required paramters have been used
    // TODO: Need this to be 2/3 and the associated thing.
    if (x_not_used || y_not_used) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error not all required parameters passed for GCode command "
                                         "on line  "
                                      << m_current_gcode_command.getLineNumber() << "\n"
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
    setXPos(temp_x);
    setYPos(temp_y);
}

void CommonParser::G3Handler(QVector<QString> params) {
    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command G3, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << "\n"
                                      << "GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    char current_parameter;
    NT current_value;
    NT temp_x = getXPos(), temp_y = getYPos(), temp_z = getZPos();
    bool no_error, x_not_used = true, y_not_used = true, z_not_used = true, i_not_used = true, j_not_used = true,
                   k_not_used = true, f_not_used = true, s_not_used = true, w_not_used = true, e_not_used = true,
                   r_not_used = true;

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        if (!no_error) {
            QString exceptionString;
            QTextStream(&exceptionString) << "Error with float conversion on GCode line "
                                          << m_current_gcode_command.getLineNumber() << "." << "\n"
                                          << "With GCode command string: " << getCurrentCommandString();
            throw IllegalParameterException(exceptionString);
        }

        current_value *= m_distance_unit();
        m_current_gcode_command.addParameter(current_parameter, current_value);

        switch (current_parameter) {
            case ('X'):
            case ('x'):
                if (x_not_used) {
                    temp_x = current_value;
                    x_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Y'):
            case ('y'):
                if (y_not_used) {
                    temp_y = current_value;
                    y_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('Z'):
            case ('z'):
                if (z_not_used) {
                    temp_z = current_value;
                    z_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('F'):
            case ('f'):
                if (f_not_used) {
                    setSpeed(current_value);
                    f_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('I'):
            case ('i'):
                if (i_not_used) {
                    setArcXPos(getXPos() + current_value);
                    i_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('J'):
            case ('j'):
                if (j_not_used) {
                    setArcYPos(getYPos() + current_value);
                    j_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;

            case ('K'):
            case ('k'):
                if (k_not_used) {
                    setArcZPos(getZPos() + current_value);
                    k_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('R'):
            case ('r'):
                if (r_not_used) {
                    setArcRPos(current_value);
                    r_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('S'):
            case ('s'):
                if (s_not_used) {
                    current_value *= m_angular_velocity_unit();
                    setSpindleSpeed(current_value);
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('W'):
            case ('w'):
                if (w_not_used) {
                    setWPos(current_value);
                    w_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('E'):
            case ('e'):
                if (e_not_used) {
                    current_value *= m_distance_unit();
                    if (m_e_absolute) {
                        if (current_value > MotionEstimation::m_previous_e)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    else {
                        if (current_value > 0)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    MotionEstimation::m_current_e = current_value;
                    e_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command G3" << "\n"
                    << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
        }
    }

    m_current_gcode_command.setExtrudersOn(m_extruders_on);
    m_current_gcode_command.setExtruderOffsets(m_extruder_offsets);
    m_current_gcode_command.setExtrudersSpeed(m_current_extruders_speed);

    m_motion_commands[m_current_layer].push_back(m_current_gcode_command);

    // Checks if all required paramters have been used
    // TODO: Need this to be 2/3 and the associated thing.
    // TODO: Need to add logic for Z.
    int num_not_used = 0;
    num_not_used += x_not_used + y_not_used + z_not_used;
    if (num_not_used > 1) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error: Too little parameters used for";
    }
    if (x_not_used || y_not_used) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error not all required parameters passed for GCode command "
                                         "on line  "
                                      << m_current_gcode_command.getLineNumber() << "\n"
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
    setXPos(temp_x);
    setYPos(temp_y);
    setZPos(temp_z);
}

void CommonParser::G4Handler(QVector<QString> params) {
    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command G4, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << "\n"
                                      << "GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    char current_parameter;
    NT current_value;
    bool no_error, p_not_used = true, s_not_used = true, f_not_used = true, x_not_used = true;

    for (QString ref : params) {
        // Retriving the first character in the QString and making it a char
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&no_error);
        if (!no_error) {
            throwFloatConversionErrorException();
        }

        current_value *= m_time_unit();
        m_current_gcode_command.addParameter(current_parameter, current_value);

        switch (current_parameter) {
            case ('P'):
            case ('p'):
                if (p_not_used) {
                    setSleepTime(current_value);
                    p_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('S'):
            case ('s'):
                if (s_not_used) {
                    setSleepTime(current_value);
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('F'):
            case ('f'):
                if (f_not_used) {
                    setSleepTime(current_value);
                    f_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('X'):
            case ('x'):
                if (x_not_used) {
                    setSleepTime(current_value);
                    x_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command G4" << "\n"
                    << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
        }
    }

    if (p_not_used && s_not_used && f_not_used && x_not_used) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error not all required parameters passed for GCode command "
                                         "on line  "
                                      << m_current_gcode_command.getLineNumber() << "\n"
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
    m_layer_times[m_current_layer][m_current_nozzle] += m_current_gcode_command.getParameters()['P'];
}

void CommonParser::G5Handler(QVector<QString> params) {
    if (params.empty()) {
        QString exceptionString;
        QTextStream(&exceptionString) << "No parameters for command G5, on line number "
                                      << m_current_gcode_command.getLineNumber()
                                      << ". Need at least one for this command." << "\n"
                                      << "GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    char current_parameter;
    NT current_value;
    NT temp_x = getXPos(), temp_y = getYPos(), temp_z = getZPos();
    bool number_error, x_not_used = true, y_not_used = true, z_not_used = true, i_not_used = true, j_not_used = true,
                       p_not_used = true, q_not_used = true, f_not_used = true, s_not_used = true, w_not_used = true,
                       e_not_used = true;

    for (const QString& ref : params) {
        current_parameter = ref.at(0).toLatin1();
        current_value = ref.right(ref.size() - 1).toDouble(&number_error);
        if (!number_error) {
            QString exceptionString;
            QTextStream(&exceptionString) << "Error with float conversion on GCode line "
                                          << m_current_gcode_command.getLineNumber() << "." << "\n"
                                          << "With GCode command string: " << getCurrentCommandString();
            throw IllegalParameterException(exceptionString);
        }

        current_value *= m_distance_unit();
        m_current_gcode_command.addParameter(current_parameter, current_value);

        switch (current_parameter) {
            case ('X'):
            case ('x'):
                if (x_not_used) {
                    temp_x = current_value;
                    x_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('Y'):
            case ('y'):
                if (y_not_used) {
                    temp_y = current_value;
                    y_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('Z'):
            case ('z'):
                if (z_not_used) {
                    temp_z = current_value;
                    z_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('F'):
            case ('f'):
                if (f_not_used) {
                    setSpeed(current_value);
                    f_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('I'):
            case ('i'):
                if (i_not_used) {
                    m_current_spline_control_a_x = getXPos() + current_value;
                    i_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('J'):
            case ('j'):
                if (j_not_used) {
                    m_current_spline_control_a_y = getYPos() + current_value;
                    j_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('P'):
            case ('p'):
                if (p_not_used) {
                    m_current_spline_control_b_x = temp_x + current_value;
                    p_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('Q'):
            case ('q'):
                if (q_not_used) {
                    m_current_spline_control_b_y = temp_y + current_value;
                    q_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('S'):
            case ('s'):
                if (s_not_used) {
                    current_value *= m_angular_velocity_unit();
                    setSpindleSpeed(current_value);
                    s_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('W'):
            case ('w'):
                if (w_not_used) {
                    setWPos(current_value);
                    w_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            case ('E'):
            case ('e'):
                if (e_not_used) {
                    current_value *= m_distance_unit();
                    if (m_e_absolute) {
                        if (current_value > MotionEstimation::m_previous_e)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    else {
                        if (current_value > 0)
                            turnOnActiveExtruders();
                        else
                            turnOffActiveExtruders();
                    }
                    MotionEstimation::m_current_e = current_value;
                    e_not_used = false;
                }
                else {
                    throwMultipleParameterException(current_parameter);
                }
                break;
            default:
                QString exceptionString;
                QTextStream(&exceptionString)
                    << "Error: Unknown parameter " << ref << " on GCode line "
                    << m_current_gcode_command.getLineNumber() << ", for GCode command G3" << "\n"
                    << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
        }
    }

    m_current_gcode_command.setExtrudersOn(m_extruders_on);
    m_current_gcode_command.setExtruderOffsets(m_extruder_offsets);
    m_current_gcode_command.setExtrudersSpeed(m_current_extruders_speed);

    m_motion_commands[m_current_layer].push_back(m_current_gcode_command);

    // Enforce XYIJPQ required parameters
    if (x_not_used || y_not_used || i_not_used || j_not_used || p_not_used || q_not_used) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error not all required parameters(XYIJPQ) passed for GCode command "
                                         "on line  "
                                      << m_current_gcode_command.getLineNumber() << "\n"
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }
    setXPos(temp_x);
    setYPos(temp_y);
    setZPos(temp_z);
}

void CommonParser::M3Handler(QVector<QString> params) {
    char current_parameter;
    bool no_error;
    for (QString ref : params) {
        current_parameter = ref.at(0).toLatin1();
        if (current_parameter == 'S' || current_parameter == 's') {
            m_current_extruders_speed = ref.right(ref.size() - 1).toDouble(&no_error);

            if (!no_error)
                m_current_extruders_speed = 0;

            setSpindleSpeed(m_current_extruders_speed * m_angular_velocity_unit());
        }
    }

    for (int i = 0, end = m_extruders_on.size(); i < end; ++i) {
        if (m_extruders_active[i])
            m_extruders_on[i] = true;
    }
}

void CommonParser::M5Handler(QVector<QString> params) {
    m_current_spindle_speed = m_current_extruders_speed = 0;
    for (int i = 0, end = m_extruders_on.size(); i < end; ++i) {
        if (m_extruders_active[i])
            m_extruders_on[i] = false;
    }
}

void CommonParser::AddDwell(double dwellTime) {
    int insertIndex = m_current_line - 1 + m_insertions;
    QSharedPointer<SettingsBase> sb = GSM->getGlobal();

    if (sb->setting<bool>(Constants::MaterialSettings::Purge::kEnablePurgeDwell)) {
        QString rv;

        QString custom_code = sb->setting<QString>(Constants::MaterialSettings::Cooling::kPostPauseCode);
        if (!(custom_code.isNull() || custom_code.isEmpty())) {
            auto custom_code_lines = custom_code.split('\n');
            auto custom_code_lines_count = custom_code_lines.length();
            for (auto i = custom_code_lines_count; i > 0;) {
                m_lines.insert(insertIndex, custom_code_lines[--i]);
                ++m_insertions;
            }
        }

        double new_dwellTime;
        double purgeTime = sb->setting<double>(Constants::MaterialSettings::Purge::kPurgeDwellDuration);
        double purgeLength;
        double purgeRate;
        if (sb->setting<int>(Constants::PrinterSettings::MachineSetup::kMachineType) == 1) {
            double purgeLength = sb->setting<double>(Constants::MaterialSettings::Purge::kPurgeLength);
            double purgeRate = sb->setting<double>(Constants::MaterialSettings::Purge::kPurgeFeedrate);
            new_dwellTime = dwellTime - purgeLength / purgeRate;
        }
        else {
            new_dwellTime = dwellTime - purgeTime;
        }

        // Purge
        if (sb->setting<int>(Constants::PrinterSettings::MachineSetup::kMachineType) == 1) {
            MotionEstimation::m_previous_e += sb->setting<Distance>(Constants::MaterialSettings::Purge::kPurgeLength);
            if (sb->setting<bool>(Constants::MaterialSettings::Filament::kFilamentBAxis)) {
                rv =
                    "G1 F" %
                    QString::number(
                        sb->setting<Velocity>(Constants::MaterialSettings::Purge::kPurgeFeedrate).to(m_velocity_unit)) %
                    " B" % QString::number(Distance(MotionEstimation::m_previous_e).to(m_distance_unit)) % m_space %
                    getCommentStartDelimiter() % "PURGE" % getCommentEndDelimiter();
            }
            else {
                rv =
                    "G1 F" %
                    QString::number(
                        sb->setting<Velocity>(Constants::MaterialSettings::Purge::kPurgeFeedrate).to(m_velocity_unit)) %
                    " E" % QString::number(Distance(MotionEstimation::m_previous_e).to(m_distance_unit)) % m_space %
                    getCommentStartDelimiter() % "PURGE" % getCommentEndDelimiter();
            }

            m_lines.insert(insertIndex, rv);
            ++m_insertions;
        }
        else {
            rv = "M69 F" % QString::number(sb->setting<int>(Constants::MaterialSettings::Purge::kPurgeDwellRPM)) %
                 " P" %
                 QString::number(
                     sb->setting<Time>(Constants::MaterialSettings::Purge::kPurgeDwellDuration).to(m_time_unit)) %
                 " S" %
                 QString::number(
                     sb->setting<Time>(Constants::MaterialSettings::Purge::kPurgeDwellTipWipeDelay).to(m_time_unit)) %
                 m_space % getCommentStartDelimiter() % "PURGE" % getCommentEndDelimiter();
            m_lines.insert(insertIndex, rv);
            ++m_insertions;
        }

        if (new_dwellTime > 0) {
            rv = m_g4_prefix % QString::number(new_dwellTime, 'f', 1) % m_space % getCommentStartDelimiter() %
                 m_g4_comment % getCommentEndDelimiter();
            m_lines.insert(insertIndex, rv);
            ++m_insertions;
            // insertIndex++;
        }

        // Move to purge location
        if (sb->setting<int>(Constants::PrinterSettings::MachineSetup::kSyntax) == 1) {
            rv = "M68" % m_space % getCommentStartDelimiter() % "PARK" % getCommentEndDelimiter();
            m_lines.insert(insertIndex, rv);
            ++m_insertions;
        }
        else {
            rv =
                "G1 F" %
                QString::number(sb->setting<Velocity>(Constants::ProfileSettings::Travel::kSpeed).to(m_velocity_unit)) %
                " X" %
                QString::number(
                    sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kPurgeX).to(m_distance_unit)) %
                " Y" %
                QString::number(
                    sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kPurgeY).to(m_distance_unit)) %
                " Z" %
                QString::number(
                    sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kPurgeZ).to(m_distance_unit)) %
                m_space % getCommentStartDelimiter() % "MOVE TO PURGE LOCATION" % getCommentEndDelimiter();
            m_lines.insert(insertIndex, rv);
            ++m_insertions;
        }

        custom_code = sb->setting<QString>(Constants::MaterialSettings::Cooling::kPrePauseCode);
        if (!(custom_code.isNull() || custom_code.isEmpty())) {
            auto custom_code_lines = custom_code.split('\n');
            auto custom_code_lines_count = custom_code_lines.length();
            for (auto i = custom_code_lines_count; i > 0;) {
                m_lines.insert(insertIndex, custom_code_lines[--i]);
                ++m_insertions;
            }
        }
    }
    else {
        QString custom_code = sb->setting<QString>(Constants::MaterialSettings::Cooling::kPostPauseCode);
        if (!(custom_code.isNull() || custom_code.isEmpty())) {
            auto custom_code_lines = custom_code.split('\n');
            auto custom_code_lines_count = custom_code_lines.length();
            for (auto i = custom_code_lines_count; i > 0;) {
                m_lines.insert(insertIndex, custom_code_lines[--i]);
                ++m_insertions;
            }
        }

        QString dwell = m_g4_prefix % QString::number(dwellTime, 'f', 1) % m_space % getCommentStartDelimiter() %
                        m_g4_comment % getCommentEndDelimiter();
        m_lines.insert(insertIndex, dwell);
        ++m_insertions;

        custom_code = sb->setting<QString>(Constants::MaterialSettings::Cooling::kPrePauseCode);
        if (!(custom_code.isNull() || custom_code.isEmpty())) {
            auto custom_code_lines = custom_code.split('\n');
            auto custom_code_lines_count = custom_code_lines.length();
            for (auto i = custom_code_lines_count; i > 0;) {
                m_lines.insert(insertIndex, custom_code_lines[--i]);
                ++m_insertions;
            }
        }
    }
}

void CommonParser::getMinMaxModifier(double& minModifier, double& maxModifier) {
    QSharedPointer<SettingsBase> sb = GSM->getGlobal();
    if (m_motion_commands[m_current_layer].size() > 0) {
        double maxFeedRate = 1;
        double minFeedRate = minModifier;

        QList<GcodeCommand>::iterator current_layer_motion_end = m_motion_commands[m_current_layer].end();
        --current_layer_motion_end;

        QList<GcodeCommand>::const_iterator current_layer_motion_begin = m_motion_commands[m_current_layer].begin();
        --current_layer_motion_begin;

        while (current_layer_motion_end != current_layer_motion_begin &&
               current_layer_motion_end->getLineNumber() > m_last_layer_line_start) {
            auto parameters = current_layer_motion_end->getParameters();
            if (parameters.contains(m_f_parameter.toLatin1())) {
                QString& line = m_lines[current_layer_motion_end->getLineNumber()];
                QRegularExpressionMatch myMatch = m_f_param_and_value.match(line);
                double value = myMatch.captured().mid(1).toDouble();

                minFeedRate = std::min(minFeedRate, value);
                maxFeedRate = std::max(maxFeedRate, value);
            }
            --current_layer_motion_end;
        }

        Velocity velocity;
        maxModifier = (sb->setting<Velocity>(Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed) /
                       velocity.from(maxFeedRate, m_velocity_unit))();
        minModifier = (sb->setting<Velocity>(Constants::PrinterSettings::MachineSpeed::kMinXYSpeed) /
                       velocity.from(minFeedRate, m_velocity_unit))();
    }
}

void CommonParser::AdjustFeedrate(double modifier) {
    QSharedPointer<SettingsBase> sb = GSM->getGlobal();
    if (m_motion_commands[m_current_layer].size() > 0) {
        m_layer_FR_modifiers[m_current_layer] = modifier;

        QList<GcodeCommand>::iterator current_layer_motion_end = m_motion_commands[m_current_layer].end();
        --current_layer_motion_end;

        QList<GcodeCommand>::const_iterator current_layer_motion_begin = m_motion_commands[m_current_layer].begin();
        --current_layer_motion_begin;

        while (current_layer_motion_end != current_layer_motion_begin &&
               current_layer_motion_end->getLineNumber() > m_last_layer_line_start) {
            auto parameters = current_layer_motion_end->getParameters();
            if (parameters.contains(m_f_parameter.toLatin1())) {
                if (sb->setting<int>(Constants::MaterialSettings::Extruder::kEnableM3S)) {
                    int cmd_index = current_layer_motion_end->getLineNumber() - 1;
                    QString& line = m_lines[cmd_index];

                    if (line.startsWith("M3 ")) {
                        QRegularExpressionMatch myMatch = m_s_param_and_value.match(line);
                        double value = myMatch.captured().mid(1).toDouble();

                        if (value != 0) {
                            double extruderModifier =
                                sb->setting<double>(Constants::MaterialSettings::Cooling::kExtruderScaleFactor);

                            // If slowing down, the multiplier for the extruder should be the inverse of the scale
                            // factor
                            if (modifier < 1) {
                                extruderModifier = 1 / extruderModifier;
                            }

                            line = line.left(myMatch.capturedStart()) % m_s_parameter %
                                   QString::number(value * modifier * extruderModifier, 'f', 4) %
                                   line.mid(myMatch.capturedEnd());

                            m_lines.insert(cmd_index + 1, line);
                            m_lines.removeAt(cmd_index);
                        }
                    }
                }

                QString& line = m_lines[current_layer_motion_end->getLineNumber()];
                QRegularExpressionMatch myMatch = m_f_param_and_value.match(line);
                double value = myMatch.captured().mid(1).toDouble();
                line = line.left(myMatch.capturedStart()) % m_f_parameter %
                       QString::number(value * modifier, 'f', 4) % line.mid(myMatch.capturedEnd());
                current_layer_motion_end->addParameter(m_f_parameter.toLatin1(),
                                                       parameters[m_f_parameter.toLatin1()] * modifier);
            }
            if (parameters.contains(m_q_parameter.toLatin1()) &&
                current_layer_motion_end->getCommandID() != 5) // G5 also used the Q param, so spline are not supported
                                                               // by syntaxes that use it for spindle control
            {
                QString& line = m_lines[current_layer_motion_end->getLineNumber()];
                QRegularExpressionMatch myMatch = m_q_param_and_value.match(line);
                double value = myMatch.captured().mid(1).toDouble();
                double extruderModifier =
                    sb->setting<double>(Constants::MaterialSettings::Cooling::kExtruderScaleFactor);
                // If slowing down, the multiplier for the extruder should be the inverse of the scale factor
                if (modifier < 1)
                    extruderModifier = 1 / extruderModifier;
                line = line.left(myMatch.capturedStart()) % m_q_parameter %
                       QString::number(value * modifier * extruderModifier, 'f', 4) %
                       line.mid(myMatch.capturedEnd());
                current_layer_motion_end->addParameter(m_q_parameter.toLatin1(),
                                                       parameters[m_q_parameter.toLatin1()] * modifier);
            }
            if (parameters.contains(m_s_parameter.toLatin1()) &&
                !sb->setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableWidthHeight)) {
                QString& line = m_lines[current_layer_motion_end->getLineNumber()];
                QRegularExpressionMatch myMatch = m_s_param_and_value.match(line);
                double value = myMatch.captured().mid(1).toDouble();
                double extruderModifier =
                    sb->setting<double>(Constants::MaterialSettings::Cooling::kExtruderScaleFactor);
                // If slowing down, the multiplier for the extruder should be the inverse of the scale factor
                if (modifier < 1)
                    extruderModifier = 1 / extruderModifier;
                line = line.left(myMatch.capturedStart()) % m_s_parameter %
                       QString::number(value * modifier * extruderModifier, 'f', 4) %
                       line.mid(myMatch.capturedEnd());
                current_layer_motion_end->addParameter(m_s_parameter.toLatin1(),
                                                       parameters[m_s_parameter.toLatin1()] * modifier);
            }
            --current_layer_motion_end;
        }
    }
}

QString CommonParser::removeRotations(QString currentLine){
    QChar space(' '), x('X'), y('Y'), z('Z'), f('F'), s('S'), zero('0'), newline('\n');
    QString RX("RX"), RY("RY"), RZ("RZ"), PA("PA"), PC("PC");
    QString G0("G0"), G1("G1");
    QString xval, yval, zval, velocity, feedrate, newLine, temp, comment;

    if(currentLine.startsWith(G0) || currentLine.startsWith(G1))
    {
		if(currentLine.contains('='))
			temp = currentLine.remove('=');
        temp = temp.mid(0, temp.indexOf(getCommentStartDelimiter()));
        comment = temp.mid(temp.indexOf(getCommentStartDelimiter()), temp.indexOf(newline));
    }
    else
    {
        return currentLine;
    }

    QVector<QString> params = temp.split(space);

    if(params[0] == G1)
    {
        newLine = "G1 ";
    }
    else if (params[0] == G0)
    {
        newLine = "G0 ";
    }
    else
    {
        return currentLine;
    }

    for(int i = 1, end = params.size(); i < end; ++i)
    {
        if(params[i].startsWith(RX) || params[i].startsWith(RY) || params[i].startsWith(RZ)
            || params[i].startsWith(PA) || params[i].startsWith(PC))
            continue;
        else
            newLine += params[i] % space;
    }

    newLine += comment;

    return newLine;
}

void CommonParser::throwMultipleParameterException(char parameter) {
    QString exceptionString;
    QTextStream(&exceptionString) << "Error: Multiple " << parameter << " parameters passed on GCode line "
                                  << m_current_gcode_command.getLineNumber() << "\n"
                                  << "With GCode command srting: " << getCurrentCommandString();
    throw IllegalParameterException(exceptionString);
}

void CommonParser::throwFloatConversionErrorException() {
    QString exceptionString;
    QTextStream(&exceptionString) << "Error with float conversion on GCode line "
                                  << m_current_gcode_command.getLineNumber() << "." << "\n"
                                  << "With GCode command string: " << getCurrentCommandString();
    throw IllegalParameterException(exceptionString);
}

void CommonParser::throwIntegerConversionErrorException() {
    QString exceptionString;
    QTextStream(&exceptionString) << "Error with interger conversion on GCode line "
                                  << m_current_gcode_command.getLineNumber() << "." << "\n"
                                  << "With GCode command string: " << getCurrentCommandString();
    throw IllegalParameterException(exceptionString);
}

void CommonParser::turnOnActiveExtruders() {
    for (int i = 0; i < m_num_extruders; ++i) {
        if (m_extruders_active[i])
            m_extruders_on[i] = true;
    }
}

void CommonParser::turnOffActiveExtruders() {
    for (int i = 0; i < m_num_extruders; ++i) {
        if (m_extruders_active[i])
            m_extruders_on[i] = false;
    }
}
} // namespace ORNL
