#include <QSharedPointer>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/point.h"
#include "geometry/segments/line.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

bool closeTo(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= 1.0e-4;
}

bool expectPoint(const ORNL::Point& point, double x, double y, const std::string& message) {
    return expect(closeTo(point.x(), x) && closeTo(point.y(), y), message);
}

void appendLine(ORNL::Path& path, const ORNL::Point& start, const ORNL::Point& end) {
    path.append(QSharedPointer<ORNL::LineSegment>::create(start, end));
}

QSharedPointer<ORNL::SettingsBase> sharpCornerSettings(ORNL::Distance close_points_threshold, bool enabled = true) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::SpecialModes::kEnableSharpCornerExtension, enabled);
    settings->setSetting(ORNL::PS::SpecialModes::kSharpCornerExtensionAngle, ORNL::Angle(M_PI / 3.0));
    settings->setSetting(ORNL::PS::SpecialModes::kSharpCornerExtensionDistance, ORNL::Distance(2.0));
    settings->setSetting(ORNL::PS::SpecialModes::kSharpCornerClosePointsThreshold, close_points_threshold);
    settings->setSetting(ORNL::PS::SpecialModes::kSharpCornerSharpeningLegLength, ORNL::Distance(4.0));
    return settings;
}
}  // namespace

int main() {
    bool passed = true;

    ORNL::Path direct_corner;
    appendLine(direct_corner, ORNL::Point(-10.0f, 5.0f, 0.0f), ORNL::Point(0.0f, 0.0f, 0.0f));
    appendLine(direct_corner, ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(-10.0f, -5.0f, 0.0f));

    ORNL::PathModifierGenerator::GenerateSharpCornerExtension(direct_corner, sharpCornerSettings(ORNL::Distance()));
    passed &= expect(direct_corner.size() == 4, "Expected direct sharp corner to insert two sharpened segments.");
    passed &= expectPoint(direct_corner[0]->end(), -3.5777087, 1.7888544,
                          "Expected direct corner previous leg to be cut back.");
    passed &= expectPoint(direct_corner[1]->end(), 2.0, 0.0,
                          "Expected direct corner merge point to extend along the bisector.");
    passed &=
        expectPoint(direct_corner[2]->end(), -3.5777087, -1.7888544, "Expected direct corner next leg to be cut back.");
    passed &= expect(direct_corner[1]->start() == direct_corner[0]->end() &&
                         direct_corner[2]->start() == direct_corner[1]->end() &&
                         direct_corner[3]->start() == direct_corner[2]->end(),
                     "Expected direct sharpened corner to remain continuous.");

    ORNL::Path connected_corner;
    appendLine(connected_corner, ORNL::Point(-10.0f, 5.0f, 0.0f), ORNL::Point(-1.0f, 0.5f, 0.0f));
    appendLine(connected_corner, ORNL::Point(-1.0f, 0.5f, 0.0f), ORNL::Point(-1.0f, -0.5f, 0.0f));
    appendLine(connected_corner, ORNL::Point(-1.0f, -0.5f, 0.0f), ORNL::Point(-10.0f, -5.0f, 0.0f));

    ORNL::PathModifierGenerator::GenerateSharpCornerExtension(connected_corner,
                                                              sharpCornerSettings(ORNL::Distance(2.0)));
    passed &= expect(connected_corner.size() == 4,
                     "Expected close connector to be removed and replaced by sharpened geometry.");
    passed &= expectPoint(connected_corner[0]->end(), -4.5777087, 2.2888544,
                          "Expected connected corner previous leg to be cut back.");
    passed &= expectPoint(connected_corner[1]->end(), 2.0, 0.0,
                          "Expected connected corner merge point to extend from the leg-axis intersection.");
    passed &= expectPoint(connected_corner[2]->end(), -4.5777087, -2.2888544,
                          "Expected connected corner next leg to be cut back.");
    passed &= expect(connected_corner[1]->start() == connected_corner[0]->end() &&
                         connected_corner[2]->start() == connected_corner[1]->end() &&
                         connected_corner[3]->start() == connected_corner[2]->end(),
                     "Expected connected sharpened corner to remain continuous.");

    ORNL::Path threshold_rejected_corner;
    appendLine(threshold_rejected_corner, ORNL::Point(-10.0f, 5.0f, 0.0f), ORNL::Point(-1.0f, 0.5f, 0.0f));
    appendLine(threshold_rejected_corner, ORNL::Point(-1.0f, 0.5f, 0.0f), ORNL::Point(-1.0f, -0.5f, 0.0f));
    appendLine(threshold_rejected_corner, ORNL::Point(-1.0f, -0.5f, 0.0f), ORNL::Point(-10.0f, -5.0f, 0.0f));

    ORNL::PathModifierGenerator::GenerateSharpCornerExtension(threshold_rejected_corner,
                                                              sharpCornerSettings(ORNL::Distance(0.5)));
    passed &= expect(threshold_rejected_corner.size() == 3,
                     "Expected connector longer than close-points threshold to remain unchanged.");

    ORNL::Path local_corner;
    appendLine(local_corner, ORNL::Point(-10.0f, 5.0f, 0.0f), ORNL::Point(0.0f, 0.0f, 0.0f));
    appendLine(local_corner, ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(-10.0f, -5.0f, 0.0f));

    QSharedPointer<ORNL::SettingsBase> local_settings = sharpCornerSettings(ORNL::Distance());
    local_corner[0]->getSb()->populate(local_settings);
    local_corner[1]->getSb()->populate(local_settings);

    ORNL::PathModifierGenerator::GenerateSharpCornerExtension(local_corner,
                                                              sharpCornerSettings(ORNL::Distance(), false));
    passed &= expect(local_corner.size() == 4,
                     "Expected segment-local sharp-corner settings to override disabled fallback settings.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
