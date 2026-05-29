#include "gcode/parsers/radial_parser.h"

#include <algorithm>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qstring.h>

#include "utilities/constants.h"

namespace ORNL {
RadialParser::RadialParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {}

void RadialParser::G0Handler(QVector<QString> params) {
    QVector<QString> filtered_params = stripRotaryAxes(params);

    // A/C-only moves are valid machine positioning moves but do not produce a
    // visible linear segment, so there is nothing to pass to CommonParser.
    if (!filtered_params.isEmpty()) {
        CommonParser::G0Handler(filtered_params);
    }
}

void RadialParser::G1Handler(QVector<QString> params) {
    QVector<QString> filtered_params = stripRotaryAxes(params);
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

QVector<QString> RadialParser::stripRotaryAxes(QVector<QString> params) {
    QVector<QString> filtered_params;
    bool a_not_used = true;
    bool c_not_used = true;

    for (const QString& param : params) {
        if (param.isEmpty()) {
            filtered_params.push_back(param);
            continue;
        }

        const QChar axis = param.at(0);
        if (!isRotaryAxis(axis)) {
            filtered_params.push_back(param);
            continue;
        }

        // Validate rotary axes here so malformed Radial3Plus2 gcode still
        // reports parser errors even though rotary values are not visualized.
        if ((axis == 'A' || axis == 'a') && !a_not_used) {
            throwMultipleParameterException(axis.toLatin1());
        }
        if ((axis == 'C' || axis == 'c') && !c_not_used) {
            throwMultipleParameterException(axis.toLatin1());
        }

        bool no_error = false;
        param.right(param.size() - 1).toDouble(&no_error);
        if (!no_error) {
            throwFloatConversionErrorException();
        }

        if (axis == 'A' || axis == 'a') {
            a_not_used = false;
        }
        else {
            c_not_used = false;
        }
    }

    return filtered_params;
}

bool RadialParser::isRotaryAxis(QChar axis) const { return axis == 'A' || axis == 'a' || axis == 'C' || axis == 'c'; }

bool RadialParser::isCommentedPrintMove() const {
    const QString comment = m_current_gcode_command.getComment().toUpper();
    return (comment.contains(Constants::RegionTypeStrings::kRadial) ||
            comment.contains(Constants::RegionTypeStrings::kHelical)) &&
           !comment.contains(Constants::RegionTypeStrings::kTravel);
}

void RadialParser::setExtruderActive(bool on) { m_extruder_on = on; }
} // namespace ORNL
