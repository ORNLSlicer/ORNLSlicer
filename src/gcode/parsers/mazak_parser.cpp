#include "gcode/parsers/mazak_parser.h"

#include <functional>

#include <QStringBuilder>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
MazakParser::MazakParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    m_f_parameter = QChar('F');
    m_feedrate_reference = "#981";
    config();
}

void MazakParser::config() {
    CommonParser::config();

    // Mazak syntax specific
    addCommandMapping("G1", std::bind(&MazakParser::G1Handler, this, std::placeholders::_1));
    addCommandMapping("#981", std::bind(&MazakParser::FeedRateHandler, this, std::placeholders::_1));
    addCommandMapping("G441", std::bind(&MazakParser::G441Handler, this, std::placeholders::_1));
    addCommandMapping("G442", std::bind(&MazakParser::G442Handler, this, std::placeholders::_1));
}

void MazakParser::G1Handler(QVector<QString> params) {
    for (QString& p : params) {
        if (p[0] == m_f_parameter) {
            QString val = p.mid(1).trimmed();
            if (val == m_feedrate_reference) {
                p = m_feedrate.mid(0);
                break;
            }
        }
    }

    CommonParser::G1Handler(params);
}

// G441 (Turn Laser ON)
void MazakParser::G441Handler(QVector<QString> params) {
    if (!params.empty()) {
        return;
    }

    m_extruders_on[0] = true;
}

// G442 (LASER OFF)
void MazakParser::G442Handler(QVector<QString> params) {
    if (!params.empty()) {
        return;
    }

    m_extruders_on[0] = false;
}

void MazakParser::FeedRateHandler(QVector<QString> params) { m_feedrate = "F" % params[1]; }
} // namespace ORNL
