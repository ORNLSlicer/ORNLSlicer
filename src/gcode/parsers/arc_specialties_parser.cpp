#include "gcode/parsers/arc_specialties_parser.h"

#include <QRegularExpression>
#include <QTextStream>
#include <cmath>
#include <functional>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qset.h>
#include <qstring.h>

#include "exceptions/exceptions.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
const QRegularExpression kG00CommandPattern("(^\\s*)G00(?=\\s|;|\\(|$)");
const QRegularExpression kG01CommandPattern("(^\\s*)G01(?=\\s|;|\\(|$)");
const QRegularExpression kG02CommandPattern("(^\\s*)G02(?=\\s|;|\\(|$)");
const QRegularExpression kG03CommandPattern("(^\\s*)G03(?=\\s|;|\\(|$)");
const QRegularExpression kLeadingBlockNumberPattern(
    "^\\s*N(?:\\[[^\\]]+\\]|[+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+))(?=\\s|[A-Z#$;/()\"]|$)\\s*",
    QRegularExpression::CaseInsensitiveOption);
const QRegularExpression kTrafoModePattern("^\\s*#TRAFO\\s+(ON|OFF)\\s*$", QRegularExpression::CaseInsensitiveOption);
const QString kG80ScheduleSpeedVariable = "V.S.SPEED";
constexpr char kCpOptionalParameter     = 'C';

bool isG80ScheduleSpeedVariable(const QString& value) {
    return value.trimmed().compare(kG80ScheduleSpeedVariable, Qt::CaseInsensitive) == 0;
}
}  // namespace

ArcSpecialtiesParser::ArcSpecialtiesParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines,
                                           QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();
}

GcodeCommand ArcSpecialtiesParser::parseCommand(QString command_string, int line_number) {
    command_string.remove(kLeadingBlockNumberPattern);
    const auto trafo_mode = kTrafoModePattern.match(command_string.section(';', 0, 0));
    if (trafo_mode.hasMatch()) {
        m_work_object_motion = trafo_mode.captured(1).compare("ON", Qt::CaseInsensitive) == 0;
    }
    command_string.replace(kG00CommandPattern, "\\1G0");
    command_string.replace(kG01CommandPattern, "\\1G1");
    command_string.replace(kG02CommandPattern, "\\1G2");
    command_string.replace(kG03CommandPattern, "\\1G3");
    return CommonParser::parseCommand(command_string, line_number);
}

void ArcSpecialtiesParser::config() {
    CommonParser::config();
    addCommandMapping("G0", std::bind(&ArcSpecialtiesParser::G0Handler, this, std::placeholders::_1));
    addCommandMapping("G1", std::bind(&ArcSpecialtiesParser::G1Handler, this, std::placeholders::_1));
    addCommandMapping("G2", std::bind(&ArcSpecialtiesParser::G2Handler, this, std::placeholders::_1));
    addCommandMapping("G3", std::bind(&ArcSpecialtiesParser::G3Handler, this, std::placeholders::_1));
    addCommandMapping("G161", std::bind(&ArcSpecialtiesParser::G161Handler, this, std::placeholders::_1));
    addCommandMapping("G162", std::bind(&ArcSpecialtiesParser::G162Handler, this, std::placeholders::_1));
    addCommandMapping("G164", std::bind(&ArcSpecialtiesParser::G164Handler, this, std::placeholders::_1));
    addCommandMapping("G82", std::bind(&ArcSpecialtiesParser::G82Handler, this, std::placeholders::_1));
    addCommandMapping("G83", std::bind(&ArcSpecialtiesParser::G83Handler, this, std::placeholders::_1));
}

void ArcSpecialtiesParser::reset() {
    CommonParser::reset();
    m_tool_angles.fill(0.0);
    m_known_tool_angles = 0;
    m_positioner_angles.fill(0.0);
    m_known_positioner_angles = 0;
    m_work_object_motion      = true;
}

void ArcSpecialtiesParser::G0Handler(QVector<QString> params) {
    QVector<QString> filtered_params = normalizeAndStripOrientationAxes(params);

    // Orientation-only moves are valid machine positioning moves but do not produce visible XYZ segments.
    if (!filtered_params.isEmpty()) { CommonParser::G0Handler(filtered_params); }
}

void ArcSpecialtiesParser::G1Handler(QVector<QString> params) {
    QVector<QString> filtered_params = normalizeAndStripOrientationAxes(params);
    if (!filtered_params.isEmpty()) {
        const bool force_print_state = isCommentedPrintMove();
        if (force_print_state) { setDepositionActive(true); }

        CommonParser::G1Handler(filtered_params);

        if (force_print_state) { setDepositionActive(false); }
    }
}

void ArcSpecialtiesParser::G2Handler(QVector<QString> params) {
    handleArcFeedMove(params, false);
}

void ArcSpecialtiesParser::G3Handler(QVector<QString> params) {
    handleArcFeedMove(params, true);
}

void ArcSpecialtiesParser::G161Handler(QVector<QString>) {
    m_use_absolute_arc_centers = true;
}

void ArcSpecialtiesParser::G162Handler(QVector<QString>) {
    m_use_absolute_arc_centers = false;
}

void ArcSpecialtiesParser::G164Handler(QVector<QString>) {
    m_use_absolute_arc_centers = false;
}

void ArcSpecialtiesParser::G82Handler(QVector<QString>) {
    setDepositionActive(true);
}

void ArcSpecialtiesParser::G83Handler(QVector<QString>) {
    setDepositionActive(false);
}

QVector<QString> ArcSpecialtiesParser::normalizeAndStripOrientationAxes(QVector<QString> params,
                                                                        bool ignore_inline_arc_optional_stop) {
    QVector<QString> filtered_params;
    QSet<QString> used_keys;
    auto tool_angles                     = m_tool_angles;
    unsigned int known_tool_angles       = m_known_tool_angles;
    auto positioner_angles               = m_positioner_angles;
    unsigned int known_positioner_angles = m_known_positioner_angles;

    for (const QString& raw_param : params) {
        const QString param = raw_param.trimmed();
        if (param.isEmpty()) { continue; }

        if (ignore_inline_arc_optional_stop && param.compare("G81", Qt::CaseInsensitive) == 0) { continue; }

        QString key;
        QString value;
        splitParameter(param, key, value);

        if (isCommonMotionKey(key)) {
            validateUniqueKey(key, used_keys);
            if (key == "F" && isG80ScheduleSpeedVariable(value)) {
                clearModalFeedrate();
                continue;
            }

            validateNumericValue(value);
            filtered_params.push_back(key % value);
            continue;
        }

        if (isOrientationKey(key)) {
            validateUniqueKey(key, used_keys);
            validateNumericValue(value);
            if (key == "CP" || key == "AP") {
                const int axis          = key == "AP" ? 0 : 1;
                positioner_angles[axis] = value.toDouble();
                known_positioner_angles |= 1U << axis;
                if (key == "CP") {
                    m_current_gcode_command.addOptionalParameter(kCpOptionalParameter, value.toDouble());
                }
            }
            else {
                const int axis    = key == "XR" ? 0 : (key == "YR" ? 1 : 2);
                tool_angles[axis] = value.toDouble();
                known_tool_angles |= 1U << axis;
            }
            continue;
        }

        if (param.contains('=')) { throwIllegalArcSpecialtiesParameter(param); }

        filtered_params.push_back(param);
    }

    m_tool_angles             = tool_angles;
    m_known_tool_angles       = known_tool_angles;
    m_positioner_angles       = positioner_angles;
    m_known_positioner_angles = known_positioner_angles;
    if (m_known_tool_angles == 7U) {
        const QQuaternion rx = QQuaternion::fromAxisAndAngle(1, 0, 0, std::remainder(m_tool_angles[0], 360.0));
        const QQuaternion ry = QQuaternion::fromAxisAndAngle(0, 1, 0, std::remainder(m_tool_angles[1], 360.0));
        const QQuaternion rz = QQuaternion::fromAxisAndAngle(0, 0, 1, std::remainder(m_tool_angles[2], 360.0));
        m_current_gcode_command.setToolOrientation(rz * ry * rx);
    }
    const bool world_approach =
        m_current_gcode_command.getComment().compare("WORLD APPROACH TRAVEL", Qt::CaseInsensitive) == 0;
    if (m_work_object_motion && !world_approach && m_known_positioner_angles == 3U) {
        const QQuaternion tilt = QQuaternion::fromAxisAndAngle(1, 0, 0, std::remainder(m_positioner_angles[0], 360.0));
        const QQuaternion spin = QQuaternion::fromAxisAndAngle(0, 0, 1, std::remainder(m_positioner_angles[1], 360.0));
        m_current_gcode_command.setPositionerOrientation(tilt * spin);
    }

    return filtered_params;
}

void ArcSpecialtiesParser::splitParameter(const QString& param, QString& key, QString& value) const {
    const int equal_pos = param.indexOf('=');
    if (equal_pos >= 0) {
        key   = param.left(equal_pos).toUpper();
        value = param.mid(equal_pos + 1);
        return;
    }

    const QString upper_param = param.toUpper();
    if (upper_param.startsWith("XR") || upper_param.startsWith("YR") || upper_param.startsWith("ZR") ||
        upper_param.startsWith("AP") || upper_param.startsWith("CP")) {
        key   = upper_param.left(2);
        value = param.mid(2);
        return;
    }

    key   = upper_param.left(1);
    value = param.mid(1);
}

bool ArcSpecialtiesParser::isCommonMotionKey(const QString& key) const {
    return key == "X" || key == "Y" || key == "Z" || key == "I" || key == "J" || key == "K" || key == "R" || key == "F";
}

bool ArcSpecialtiesParser::isOrientationKey(const QString& key) const {
    return key == "XR" || key == "YR" || key == "ZR" || key == "AP" || key == "CP";
}

void ArcSpecialtiesParser::validateUniqueKey(const QString& key, QSet<QString>& used_keys) {
    if (used_keys.contains(key)) {
        if (key.size() == 1) { throwMultipleParameterException(key.at(0).toLatin1()); }

        QString exceptionString;
        QTextStream(&exceptionString) << "Error: Multiple " << key << " parameters passed on GCode line "
                                      << m_current_gcode_command.getLineNumber() << "\n"
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    used_keys.insert(key);
}

void ArcSpecialtiesParser::validateNumericValue(const QString& value) {
    bool no_error       = false;
    const double number = value.toDouble(&no_error);
    if (!no_error || !std::isfinite(number)) { throwFloatConversionErrorException(); }
}

void ArcSpecialtiesParser::throwIllegalArcSpecialtiesParameter(const QString& param) {
    QString exceptionString;
    QTextStream(&exceptionString) << "Error: Unknown Arc Specialties parameter " << param << " on GCode line "
                                  << m_current_gcode_command.getLineNumber() << "\n"
                                  << "With GCode command string: " << getCurrentCommandString();
    throw IllegalParameterException(exceptionString);
}

bool ArcSpecialtiesParser::isCommentedPrintMove() const {
    const QString comment = m_current_gcode_command.getComment().toUpper();
    return (comment.contains(Constants::RegionTypeStrings::kRadial) ||
            comment.contains(Constants::RegionTypeStrings::kHelical)) &&
           !comment.contains(Constants::RegionTypeStrings::kTravel);
}

QVector<QString> ArcSpecialtiesParser::convertAbsoluteArcCenterParams(const QVector<QString>& params) {
    if (!m_use_absolute_arc_centers) { return params; }

    QVector<QString> converted_params;
    converted_params.reserve(params.size());

    for (const QString& param : params) {
        const QString key = param.left(1).toUpper();
        if (key == "I" || key == "J") {
            bool no_error               = false;
            const double absolute_value = param.mid(1).toDouble(&no_error);
            if (!no_error) { throwFloatConversionErrorException(); }

            const double current_position = key == "I" ? getXPos() : getYPos();
            converted_params.push_back(key % QString::number(absolute_value - current_position, 'g', 17));
        }
        else { converted_params.push_back(param); }
    }

    return converted_params;
}

void ArcSpecialtiesParser::setDepositionActive(bool on) {
    m_deposition_active = on;
}

void ArcSpecialtiesParser::handleArcFeedMove(QVector<QString> params, bool ccw) {
    QVector<QString> filtered_params = normalizeAndStripOrientationAxes(params, true);
    if (filtered_params.isEmpty()) { return; }

    filtered_params = convertAbsoluteArcCenterParams(filtered_params);

    const bool force_print_state = isCommentedPrintMove();
    if (force_print_state) { setDepositionActive(true); }

    if (ccw) { CommonParser::G3Handler(filtered_params); }
    else { CommonParser::G2Handler(filtered_params); }

    if (force_print_state) { setDepositionActive(false); }
}
}  // namespace ORNL
