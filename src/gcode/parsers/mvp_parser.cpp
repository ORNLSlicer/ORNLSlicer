#include "gcode/parsers/mvp_parser.h"

#include <functional>

#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <qcontainerfwd.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
MVPParser::MVPParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();
}

void MVPParser::config() {
    CommonParser::config();

    addCommandMapping("M124", std::bind(&MVPParser::M124Handler, this, std::placeholders::_1));
}

// M124 (Turn Extruder OFF)
void MVPParser::M124Handler(QVector<QString> params) {
    // redirect - essentially an M5 command
    CommonParser::M5Handler(params);
}

} // namespace ORNL
