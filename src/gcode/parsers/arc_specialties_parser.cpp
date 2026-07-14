#include "gcode/parsers/arc_specialties_parser.h"

#include <QRegularExpression>
#include <QTextStream>
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
} // namespace

ArcSpecialtiesParser::ArcSpecialtiesParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines,
                                           QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {}

GcodeCommand ArcSpecialtiesParser::parseCommand(QString command_string, int line_number) {
    command_string.replace(kG00CommandPattern, "\\1G0");
    command_string.replace(kG01CommandPattern, "\\1G1");
    command_string.replace(kG02CommandPattern, "\\1G2");
    command_string.replace(kG03CommandPattern, "\\1G3");
    return CommonParser::parseCommand(command_string, line_number);
}

void ArcSpecialtiesParser::G0Handler(QVector<QString> params) {
    QVector<QString> filtered_params = normalizeAndStripOrientationAxes(params);

    // Orientation-only moves are valid machine positioning moves but do not produce visible XYZ segments.
    if (!filtered_params.isEmpty()) {
        CommonParser::G0Handler(filtered_params);
    }
}

void ArcSpecialtiesParser::G1Handler(QVector<QString> params) {
    QVector<QString> filtered_params = normalizeAndStripOrientationAxes(params);
    if (!filtered_params.isEmpty()) {
        const bool force_print_state = isCommentedPrintMove();
        if (force_print_state) {
            setExtruderActive(true);
        }

        CommonParser::G1Handler(filtered_params);

        if (force_print_state) {
            setExtruderActive(false);
        }
    }
}

void ArcSpecialtiesParser::G2Handler(QVector<QString> params) { handleArcFeedMove(params, false); }

void ArcSpecialtiesParser::G3Handler(QVector<QString> params) { handleArcFeedMove(params, true); }

QVector<QString> ArcSpecialtiesParser::normalizeAndStripOrientationAxes(QVector<QString> params) {
    QVector<QString> filtered_params;
    QSet<QString> used_keys;

    for (const QString& raw_param : params) {
        const QString param = raw_param.trimmed();
        if (param.isEmpty()) {
            continue;
        }

        QString key;
        QString value;
        splitParameter(param, key, value);

        if (isCommonMotionKey(key)) {
            validateUniqueKey(key, used_keys);
            validateNumericValue(value);
            filtered_params.push_back(key % value);
            continue;
        }

        if (isOrientationKey(key)) {
            validateUniqueKey(key, used_keys);
            validateNumericValue(value);
            continue;
        }

        if (param.contains('=')) {
            throwIllegalArcSpecialtiesParameter(param);
        }

        filtered_params.push_back(param);
    }

    return filtered_params;
}

void ArcSpecialtiesParser::splitParameter(const QString& param, QString& key, QString& value) const {
    const int equal_pos = param.indexOf('=');
    if (equal_pos >= 0) {
        key = param.left(equal_pos).toUpper();
        value = param.mid(equal_pos + 1);
        return;
    }

    const QString upper_param = param.toUpper();
    if (upper_param.startsWith("XR") || upper_param.startsWith("YR") || upper_param.startsWith("ZR") ||
        upper_param.startsWith("AP") || upper_param.startsWith("CP")) {
        key = upper_param.left(2);
        value = param.mid(2);
        return;
    }

    key = upper_param.left(1);
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
        if (key.size() == 1) {
            throwMultipleParameterException(key.at(0).toLatin1());
        }

        QString exceptionString;
        QTextStream(&exceptionString) << "Error: Multiple " << key << " parameters passed on GCode line "
                                      << m_current_gcode_command.getLineNumber() << "\n"
                                      << "With GCode command string: " << getCurrentCommandString();
        throw IllegalParameterException(exceptionString);
    }

    used_keys.insert(key);
}

void ArcSpecialtiesParser::validateNumericValue(const QString& value) {
    bool no_error = false;
    value.toDouble(&no_error);
    if (!no_error) {
        throwFloatConversionErrorException();
    }
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
    return comment.contains(Constants::RegionTypeStrings::kRadial) &&
           !comment.contains(Constants::RegionTypeStrings::kTravel);
}

void ArcSpecialtiesParser::setExtruderActive(bool on) { m_extruder_on = on; }

void ArcSpecialtiesParser::handleArcFeedMove(QVector<QString> params, bool ccw) {
    QVector<QString> filtered_params = normalizeAndStripOrientationAxes(params);
    if (filtered_params.isEmpty()) {
        return;
    }

    const bool force_print_state = isCommentedPrintMove();
    if (force_print_state) {
        setExtruderActive(true);
    }

    if (ccw) {
        CommonParser::G3Handler(filtered_params);
    }
    else {
        CommonParser::G2Handler(filtered_params);
    }

    if (force_print_state) {
        setExtruderActive(false);
    }
}
} // namespace ORNL
