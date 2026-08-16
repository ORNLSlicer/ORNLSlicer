#include <cstdlib>
#include <exception>
#include <iostream>

#include <QCoreApplication>
#include <QStringList>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"
#include "units/unit.h"

namespace {
bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

bool accumulatesTravelTimeForNonDepositionMove() {
    QStringList original_lines {"G1 F60 X1 ;TRAVEL"};
    QStringList upper_lines {original_lines.first().toUpper()};
    ORNL::CommonParser parser(ORNL::GcodeMetaList::MarlinMeta, false, original_lines, upper_lines);

    try {
        parser.parseLines();
        return parser.getTravelDistance() > 0.0 * ORNL::mm && parser.getTravelTime() > 0.0 * ORNL::s;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    bool passed = true;
    passed &= expect(accumulatesTravelTimeForNonDepositionMove(),
                     "Common parser did not accumulate travel time for a travel move.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
