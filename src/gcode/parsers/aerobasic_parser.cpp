#include "gcode/parsers/aerobasic_parser.h"

#include <functional>

#include <QString>
#include <qcontainerfwd.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
AeroBasicParser::AeroBasicParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines)
    : CommonParser(meta, allowLayerAlter, lines, upperLines) {
    config();
}

void AeroBasicParser::G0Handler(QVector<QString> params) { CommonParser::G0Handler(params); }

void AeroBasicParser::G1Handler(QVector<QString> params) { CommonParser::G1Handler(params); }

void AeroBasicParser::G2Handler(QVector<QString> params) {
    // AeroBasic supports strining G2/3 with G1 to linearly interpolate in the Z axis,
    // this behaves like if Z was also specified
    QVector<QString> cleaned_params;
    for (auto& param : params)
        if (param != "G1")
            cleaned_params.append(param);

    CommonParser::G2Handler(cleaned_params);
}

void AeroBasicParser::G3Handler(QVector<QString> params) {
    // AeroBasic supports strining G2/3 with G1 to linearly interpolate in the Z axis,
    // this behaves like if Z was also specified
    QVector<QString> cleaned_params;
    for (auto& param : params)
        if (param != "G1")
            cleaned_params.append(param);

    CommonParser::G3Handler(cleaned_params);
}

void AeroBasicParser::G4Handler(QVector<QString> params) { CommonParser::G4Handler(params); }

void AeroBasicParser::config() {
    CommonParser::config();

    // Lines
    addCommandMapping("RAPID", std::bind(&AeroBasicParser::G0Handler, this, std::placeholders::_1));
    addCommandMapping("LINEAR", std::bind(&AeroBasicParser::G1Handler, this, std::placeholders::_1));

    // Arcs
    addCommandMapping("CW", std::bind(&AeroBasicParser::G2Handler, this, std::placeholders::_1));
    addCommandMapping("CCW", std::bind(&AeroBasicParser::G3Handler, this, std::placeholders::_1));

    // Dwell
    addCommandMapping("DWELL", std::bind(&AeroBasicParser::G4Handler, this, std::placeholders::_1));
}
} // namespace ORNL
