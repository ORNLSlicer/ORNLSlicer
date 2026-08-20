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

QStringList upperLines(const QStringList& lines) {
    QStringList upper_lines;
    for (const QString& line : lines) {
        upper_lines.append(line.toUpper());
    }

    return upper_lines;
}

bool accumulatesTravelTimeForNonDepositionMove() {
    QStringList original_lines {"G1 F60 X1 ;TRAVEL"};
    QStringList upper_lines = upperLines(original_lines);
    ORNL::CommonParser parser(ORNL::GcodeMetaList::MarlinMeta, false, original_lines, upper_lines);

    try {
        parser.parseLines();
        return parser.getTravelDistance() > 0.0 * ORNL::mm && parser.getTravelTime() > 0.0 * ORNL::s;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool accumulatesTravelTimeForTravelCommentAfterDepositionMove() {
    QStringList original_lines {"G1 F60 X1 E1 ;PERIMETER", "G1 F60 X2 ;TRAVEL"};
    QStringList upper_lines = upperLines(original_lines);
    ORNL::CommonParser parser(ORNL::GcodeMetaList::MarlinMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 2 &&
               commands.first().first().getDepositionActive() && !commands.first().last().getDepositionActive() &&
               parser.getPrintingDistance() > 0.0 * ORNL::mm && parser.getTravelDistance() > 0.0 * ORNL::mm &&
               parser.getTravelTime() > 0.0 * ORNL::s;
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
    passed &= expect(accumulatesTravelTimeForTravelCommentAfterDepositionMove(),
                     "Common parser did not accumulate travel time for a travel comment after deposition.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
