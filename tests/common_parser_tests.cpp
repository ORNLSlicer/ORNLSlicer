#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace {
bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

QStringList upperLines(const QStringList& lines) {
    QStringList upper_lines;
    for (const QString& line : lines) { upper_lines.append(line.toUpper()); }

    return upper_lines;
}

bool lineFeedrate(const QString& line, double& feedrate) {
    static const QRegularExpression feedrate_token("(^|\\s)F([-+]?\\d*\\.?\\d+(?:[Ee][-+]?\\d+)?)",
                                                   QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = feedrate_token.match(line);
    if (!match.hasMatch()) return false;

    bool converted = false;
    feedrate       = match.captured(2).toDouble(&converted);
    return converted;
}

void configureLayerTimeSettings() {
    auto global_settings = ORNL::GSM->getGlobal();
    global_settings->setSetting(ORNL::MS::Cooling::kForceMinLayerTime, true);
    global_settings->setSetting(ORNL::MS::Cooling::kForceMinLayerTimeMethod,
                                static_cast<int>(ORNL::ForceMinimumLayerTime::kSlow_Feedrate));
    global_settings->setSetting(ORNL::MS::Cooling::kMinLayerTime, 5.0);
    global_settings->setSetting(ORNL::MS::Cooling::kMaxLayerTime, 0.0);
    global_settings->setSetting(ORNL::MS::Cooling::kExtruderScaleFactor, 1.0);
    global_settings->setSetting(ORNL::MS::Filament::kFilamentBAxis, false);
    global_settings->setSetting(ORNL::MS::Startup::kDisableFeedrateScaling, false);
    global_settings->setSetting(ORNL::MS::Slowdown::kDisableFeedrateScaling, false);
    global_settings->setSetting(ORNL::MS::TipWipe::kDisableFeedrateScaling, false);
    global_settings->setSetting(ORNL::MS::SpiralLift::kDisableFeedrateScaling, false);
    global_settings->setSetting(ORNL::PS::Travel::kDisableFeedrateScaling, false);
    global_settings->setSetting(ORNL::PRS::MachineSpeed::kMinXYSpeed, 0.0);
    global_settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 1000000.0);
    global_settings->setSetting(ORNL::PS::Layer::kBeadWidth, 0.4);
    global_settings->setSetting(ORNL::PS::Layer::kLayerHeight, 0.2);
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
        return commands.size() == 1 && commands.first().size() == 2 && commands.first().first().getDepositionActive() &&
               !commands.first().last().getDepositionActive() && parser.getPrintingDistance() > 0.0 * ORNL::mm &&
               parser.getTravelDistance() > 0.0 * ORNL::mm && parser.getTravelTime() > 0.0 * ORNL::s;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool keepsTravelFeedrateWhenTravelScalingDisabled() {
    configureLayerTimeSettings();

    QStringList original_lines {
        ";BEGINNING LAYER: 1",
        "G1 F60 X1 ;TRAVEL",
        "G1 F60 X2 E1 ;PERIMETER",
        ";Settings Footer",
        ";disable_travel_feedrate_scaling true",
    };
    QStringList upper_lines = upperLines(original_lines);
    ORNL::CommonParser parser(ORNL::GcodeMetaList::MarlinMeta, true, original_lines, upper_lines);

    try {
        parser.parseFooter();
        parser.parseLines();

        double travel_feedrate            = 0.0;
        double print_feedrate             = 0.0;
        const QList<double> modifiers     = parser.getLayerFeedRateModifiers();
        const double travel_time          = parser.getTravelTime()();
        const double adjusted_travel_time = parser.getAdjustedTravelTime()();

        return parser.getWasModified() && modifiers.size() > 1 && modifiers[1] > 0.0 && modifiers[1] < 1.0 &&
               lineFeedrate(original_lines[1], travel_feedrate) && lineFeedrate(original_lines[2], print_feedrate) &&
               std::abs(travel_feedrate - 60.0) < 1.0e-6 && print_feedrate > 0.0 && print_feedrate < 60.0 &&
               std::abs(adjusted_travel_time - travel_time) < 1.0e-6;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool adjustsTravelTimeWhenTravelFeedrateIsScaled() {
    configureLayerTimeSettings();

    QStringList original_lines {
        ";BEGINNING LAYER: 1",
        "G1 F60 X1 ;TRAVEL",
        "G1 F60 X2 E1 ;PERIMETER",
        ";Settings Footer",
        ";disable_travel_feedrate_scaling false",
    };
    QStringList upper_lines = upperLines(original_lines);
    ORNL::CommonParser parser(ORNL::GcodeMetaList::MarlinMeta, true, original_lines, upper_lines);

    try {
        parser.parseFooter();
        parser.parseLines();

        double travel_feedrate        = 0.0;
        const QList<double> modifiers = parser.getLayerFeedRateModifiers();

        return parser.getWasModified() && modifiers.size() > 1 && modifiers[1] > 0.0 && modifiers[1] < 1.0 &&
               lineFeedrate(original_lines[1], travel_feedrate) && travel_feedrate > 0.0 && travel_feedrate < 60.0 &&
               parser.getAdjustedTravelTime()() > parser.getTravelTime()();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    bool passed = true;
    passed &= expect(accumulatesTravelTimeForNonDepositionMove(),
                     "Common parser did not accumulate travel time for a travel move.");
    passed &= expect(accumulatesTravelTimeForTravelCommentAfterDepositionMove(),
                     "Common parser did not accumulate travel time for a travel comment after deposition.");
    passed &= expect(keepsTravelFeedrateWhenTravelScalingDisabled(),
                     "Common parser scaled a travel move when travel feedrate scaling was disabled.");
    passed &= expect(adjustsTravelTimeWhenTravelFeedrateIsScaled(),
                     "Common parser did not adjust travel time when travel feedrate was scaled.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
