#include <QColor>
#include <QString>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "managers/preferences_manager.h"
#define private public
#include "threading/gcode_loader.h"
#undef private
#include "utilities/enums.h"

namespace {
struct ExpectedVisualizationColor {
    ORNL::VisualizationColors color;
    const char* name;
    QColor default_color;
};

const std::vector<ExpectedVisualizationColor>& expectedVisualizationColors() {
    static const std::vector<ExpectedVisualizationColor> expected {
        {ORNL::VisualizationColors::kBrim, "Brim", QColor(200, 113, 55, 255)},
        {ORNL::VisualizationColors::kCoasting, "Coasting", QColor(211, 95, 141, 255)},
        {ORNL::VisualizationColors::kInfill, "Infill", QColor(0, 255, 0, 255)},
        {ORNL::VisualizationColors::kInitialStartup, "InitialStartup", QColor(135, 222, 205, 255)},
        {ORNL::VisualizationColors::kInset, "Inset", QColor(0, 204, 255, 255)},
        {ORNL::VisualizationColors::kInsetArc, "InsetArc", QColor(0, 184, 255, 255)},
        {ORNL::VisualizationColors::kLaserScan, "LaserScan", QColor(90, 255, 90, 255)},
        {ORNL::VisualizationColors::kLeadIn, "LeadIn", QColor(255, 153, 51, 255)},
        {ORNL::VisualizationColors::kFlyingStart, "FlyingStart", QColor(120, 150, 250)},
        {ORNL::VisualizationColors::kPerimeter, "Perimeter", QColor(0, 0, 255, 255)},
        {ORNL::VisualizationColors::kPerimeterArc, "PerimeterArc", QColor(32, 64, 255, 255)},
        {ORNL::VisualizationColors::kPrestart, "Prestart", QColor(204, 0, 255, 255)},
        {ORNL::VisualizationColors::kRaft, "Raft", QColor(102, 102, 102, 255)},
        {ORNL::VisualizationColors::kRadial, "Radial", QColor(47, 82, 102, 255)},
        {ORNL::VisualizationColors::kHelical, "Helical", QColor(127, 0, 255, 255)},
        {ORNL::VisualizationColors::kRampingDown, "RampingDown", QColor(22, 99, 137, 255)},
        {ORNL::VisualizationColors::kRampingUp, "RampingUp", QColor(99, 22, 137, 255)},
        {ORNL::VisualizationColors::kSkeleton, "Skeleton", QColor(160, 44, 44, 255)},
        {ORNL::VisualizationColors::kSkin, "Skin", QColor(0, 128, 0, 255)},
        {ORNL::VisualizationColors::kSkirt, "Skirt", QColor(211, 188, 95, 255)},
        {ORNL::VisualizationColors::kSlowDown, "SlowDown", QColor(44, 160, 137, 255)},
        {ORNL::VisualizationColors::kSpiralLift, "SpiralLift", QColor(113, 55, 200, 255)},
        {ORNL::VisualizationColors::kSupport, "Support", QColor(255, 102, 0, 255)},
        {ORNL::VisualizationColors::kSupportRoof, "SupportRoof", QColor(255, 179, 128, 255)},
        {ORNL::VisualizationColors::kThermalScan, "ThermalScan", QColor(240, 130, 130, 255)},
        {ORNL::VisualizationColors::kTipWipeAngled, "TipWipeAngled", QColor(179, 128, 255, 255)},
        {ORNL::VisualizationColors::kTipWipeForward, "TipWipeForward", QColor(179, 128, 255, 255)},
        {ORNL::VisualizationColors::kTipWipeReverse, "TipWipeReverse", QColor(179, 128, 255, 255)},
        {ORNL::VisualizationColors::kTravel, "Travel", QColor(233, 175, 198, 255)},
        {ORNL::VisualizationColors::kUnknown, "Unknown", QColor(0, 0, 0, 255)},
        {ORNL::VisualizationColors::kHelicalPerimeter, "HelicalPerimeter", QColor(0, 0, 255, 255)},
        {ORNL::VisualizationColors::kHelicalInset, "HelicalInset", QColor(0, 204, 255, 255)},
        {ORNL::VisualizationColors::kHelicalInfill, "HelicalInfill", QColor(0, 255, 0, 255)},
    };

    return expected;
}

bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

bool throwsInvalidArgumentForName(ORNL::VisualizationColors color) {
    try {
        ORNL::VisualizationColorsName(color);
    } catch (const std::invalid_argument&) { return true; } catch (const std::exception& ex) {
        std::cerr << "Expected std::invalid_argument, got: " << ex.what() << '\n';
        return false;
    }

    return false;
}

bool throwsInvalidArgumentForDefault(ORNL::VisualizationColors color) {
    try {
        ORNL::VisualizationColorsDefaults(color);
    } catch (const std::invalid_argument&) { return true; } catch (const std::exception& ex) {
        std::cerr << "Expected std::invalid_argument, got: " << ex.what() << '\n';
        return false;
    }

    return false;
}

QColor configuredColor(ORNL::VisualizationColors color) {
    return ORNL::PreferencesManager::getInstance()->getVisualizationColor(color);
}
}  // namespace

int main() {
    bool passed = true;

    const auto& definitions = ORNL::VisualizationColorDefinitions();
    const auto& expected    = expectedVisualizationColors();

    passed &= expect(definitions.size() == expected.size(), "Expected one visualization color definition per color.");
    passed &= expect(definitions.size() == static_cast<std::size_t>(ORNL::VisualizationColors::Length),
                     "Expected definitions to stop before the Length sentinel.");

    std::set<ORNL::VisualizationColors> seen_colors;
    std::set<std::string> seen_names;
    std::vector<bool> seen_by_ordinal(static_cast<std::size_t>(ORNL::VisualizationColors::Length), false);

    for (std::size_t i = 0; i < definitions.size() && i < expected.size(); ++i) {
        const ORNL::VisualizationColorDefinition& definition = definitions[i];
        const ExpectedVisualizationColor& current_expected   = expected[i];

        passed &= expect(definition.color == current_expected.color, "Unexpected visualization color enum order.");
        passed &= expect(QString(definition.name) == current_expected.name, "Unexpected visualization color name.");
        passed &= expect(!QString(definition.name).isEmpty(), "Visualization color names must not be empty.");
        passed &= expect(definition.default_color == current_expected.default_color,
                         "Unexpected visualization default color.");

        const auto ordinal = static_cast<std::size_t>(definition.color);
        passed &= expect(ordinal < seen_by_ordinal.size(), "Visualization color enum value is outside the real range.");
        if (ordinal < seen_by_ordinal.size()) {
            passed &= expect(!seen_by_ordinal[ordinal], "Duplicate visualization color enum ordinal.");
            seen_by_ordinal[ordinal] = true;
        }

        passed &= expect(seen_colors.insert(definition.color).second, "Duplicate visualization color enum entry.");
        passed &= expect(seen_names.insert(definition.name).second, "Duplicate visualization color name.");

        ORNL::VisualizationColors color_from_name;
        passed &= expect(ORNL::VisualizationColorFromName(QString(definition.name), color_from_name),
                         "Expected visualization color name lookup to succeed.");
        passed &=
            expect(color_from_name == definition.color, "Expected visualization color name lookup to return enum.");

        passed &= expect(ORNL::VisualizationColorsName(definition.color) == definition.name,
                         "Expected public name helper to use the definition table.");
        passed &= expect(ORNL::VisualizationColorsDefaults(definition.color) == definition.default_color,
                         "Expected public default helper to use the definition table.");
    }

    for (bool seen : seen_by_ordinal) { passed &= expect(seen, "Missing visualization color enum ordinal."); }

    passed &= expect(ORNL::VisualizationColorsName(ORNL::VisualizationColors::kUnknown) == "Unknown",
                     "Expected kUnknown to keep the Unknown persisted name.");
    passed &= expect(ORNL::VisualizationColorsDefaults(ORNL::VisualizationColors::kUnknown) == QColor(0, 0, 0, 255),
                     "Expected kUnknown to keep the black default color.");
    passed &= expect(throwsInvalidArgumentForName(ORNL::VisualizationColors::Length),
                     "Expected Length name lookup to throw.");
    passed &= expect(throwsInvalidArgumentForDefault(ORNL::VisualizationColors::Length),
                     "Expected Length default lookup to throw.");

    ORNL::VisualizationColors invalid_color;
    passed &= expect(!ORNL::VisualizationColorFromName(QStringLiteral("NotAVisualizationColor"), invalid_color),
                     "Expected unknown visualization color name lookup to fail.");

    const std::map<std::string, QColor> preference_colors =
        ORNL::PreferencesManager::getInstance()->getVisualizationColors();
    passed &= expect(preference_colors.size() == definitions.size(),
                     "Expected preferences to register every visualization color.");

    for (const ORNL::VisualizationColorDefinition& definition : definitions) {
        const auto color_it = preference_colors.find(definition.name);
        passed &= expect(color_it != preference_colors.end(), "Expected visualization color in preferences.");
        if (color_it != preference_colors.end()) {
            passed &= expect(color_it->second == definition.default_color,
                             "Expected preference visualization color default to match definition.");
        }
    }

    ORNL::GCodeLoader loader(QString(), false);
    passed &= expect(loader.determineFontColor(QStringLiteral("HELICAL PERIMETER")) ==
                         configuredColor(ORNL::VisualizationColors::kHelicalPerimeter),
                     "Expected helical perimeter comments to use the helical perimeter color.");
    passed &= expect(loader.determineFontColor(QStringLiteral("HELICAL INSET")) ==
                         configuredColor(ORNL::VisualizationColors::kHelicalInset),
                     "Expected helical inset comments to use the helical inset color.");
    passed &= expect(loader.determineFontColor(QStringLiteral("HELICAL INFILL")) ==
                         configuredColor(ORNL::VisualizationColors::kHelicalInfill),
                     "Expected helical infill comments to use the helical infill color.");
    passed &= expect(
        loader.determineFontColor(QStringLiteral("HELICAL")) == configuredColor(ORNL::VisualizationColors::kHelical),
        "Expected generic helical comments to keep the generic helical color.");
    passed &= expect(loader.determineFontColor(QStringLiteral("HELICAL SUPPORT PERIMETER")) ==
                         configuredColor(ORNL::VisualizationColors::kSupport),
                     "Expected support comments to keep precedence over helical region colors.");
    passed &= expect(loader.determineSegmentColor(2, QStringLiteral("HELICAL PERIMETER")) ==
                         configuredColor(ORNL::VisualizationColors::kHelicalPerimeter),
                     "Expected helical perimeter arcs to keep the helical perimeter color.");
    passed &= expect(loader.determineSegmentColor(3, QStringLiteral("HELICAL INSET")) ==
                         configuredColor(ORNL::VisualizationColors::kHelicalInset),
                     "Expected helical inset arcs to keep the helical inset color.");
    passed &= expect(loader.determineSegmentColor(2, QStringLiteral("PERIMETER")) ==
                         configuredColor(ORNL::VisualizationColors::kPerimeterArc),
                     "Expected planar perimeter arcs to keep the perimeter arc color.");
    passed &= expect(loader.determineSegmentColor(3, QStringLiteral("INSET")) ==
                         configuredColor(ORNL::VisualizationColors::kInsetArc),
                     "Expected planar inset arcs to keep the inset arc color.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
