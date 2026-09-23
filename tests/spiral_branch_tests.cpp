#include <QFile>
#include <QSharedPointer>
#include <cstdlib>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "step/layer/regions/inset.h"
#include "step/layer/regions/perimeter.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

QSharedPointer<ORNL::SettingsBase> defaultSettings() {
    QFile master_file(":/configs/master.conf");
    if (!master_file.open(QIODevice::ReadOnly)) return {};

    const fifojson master = fifojson::parse(master_file.readAll().toStdString());
    fifojson defaults     = fifojson::array({fifojson::object()});
    for (const auto& item : master.items()) {
        defaults[0][item.key()] = item.value()[ORNL::Constants::Settings::Master::kDefault];
    }

    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->json(defaults);
    return settings;
}

ORNL::PolygonList squareGeometry() {
    ORNL::Polygon square;
    square << ORNL::Point(0.0f, 0.0f, 0.0f) << ORNL::Point(50.0f, 0.0f, 0.0f) << ORNL::Point(50.0f, 50.0f, 0.0f)
           << ORNL::Point(0.0f, 50.0f, 0.0f);

    ORNL::PolygonList geometry;
    geometry += square;
    return geometry;
}

bool verifyContinuousBranch(const QVector<ORNL::Path>& paths, const std::string& region_name) {
    bool passed = expect(paths.size() == 1, region_name + " branch should be emitted as one continuous path.");
    if (paths.size() != 1) return false;

    const ORNL::Path& path = paths.front();
    int travel_count       = 0;
    bool found_branch      = false;

    for (int i = 0; i < path.size(); ++i) {
        const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
        if (dynamic_cast<ORNL::TravelSegment*>(segment.data()) != nullptr) {
            ++travel_count;
            passed &= expect(i == 0, region_name + " branch should not contain an inter-loop travel.");
        }

        if (i == 0) continue;

        const QSharedPointer<ORNL::SegmentBase>& previous = path[i - 1];
        passed &= expect(previous->end().distance(segment->start()) <= ORNL::Distance(1.0e-4),
                         region_name + " branch path should remain geometrically continuous.");

        const ORNL::PathModifiers previous_modifiers =
            previous->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers);
        const ORNL::PathModifiers modifiers = segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers);
        if (previous_modifiers == ORNL::PathModifiers::kForwardTipWipe && modifiers == ORNL::PathModifiers::kNone &&
            dynamic_cast<ORNL::LineSegment*>(segment.data()) != nullptr && segment->isPrintingSegment()) {
            found_branch             = true;
            const double outgoing_x  = previous->end().x() - previous->start().x();
            const double outgoing_y  = previous->end().y() - previous->start().y();
            const double branch_x    = segment->end().x() - segment->start().x();
            const double branch_y    = segment->end().y() - segment->start().y();
            const double forward_dot = (outgoing_x * branch_x) + (outgoing_y * branch_y);
            passed &= expect(forward_dot > 0.0,
                             region_name + " branch should continue in the tip wipe's forward direction (dot " +
                                 std::to_string(forward_dot) + ", outgoing " + std::to_string(outgoing_x) + "," +
                                 std::to_string(outgoing_y) + ", branch " + std::to_string(branch_x) + "," +
                                 std::to_string(branch_y) + ").");
        }
    }

    passed &= expect(travel_count == 1, region_name + " branch should contain only its initial travel.");
    passed &= expect(found_branch, region_name + " branch should extrude from the tip wipe to the next loop.");
    return passed;
}
}  // namespace

int main() {
    Q_INIT_RESOURCE(configs);

    bool passed                      = true;
    const ORNL::PolygonList geometry = squareGeometry();

    QSharedPointer<ORNL::SettingsBase> perimeter_settings = defaultSettings();
    passed &= expect(!perimeter_settings.isNull(), "Expected the master settings resource to load.");
    if (perimeter_settings.isNull()) return EXIT_FAILURE;

    perimeter_settings->setSetting(ORNL::PS::Perimeter::kCount, 2);
    perimeter_settings->setSetting(ORNL::PS::Perimeter::kBeadWidth, ORNL::Distance(5.0));
    perimeter_settings->setSetting(ORNL::PS::Perimeter::kEnableSpiralPerimeter, true);
    perimeter_settings->setSetting(ORNL::PS::Perimeter::kBranchAfterTipWipe, true);
    perimeter_settings->setSetting(ORNL::PS::Perimeter::kCompletePathBeforeConnecting, false);
    perimeter_settings->setSetting(ORNL::PS::Perimeter::kConnectToInsets, false);
    perimeter_settings->setSetting(ORNL::PS::Ordering::kPerimeterReverseDirection,
                                   static_cast<int>(ORNL::PrintDirection::kReverse_All_Layers));
    perimeter_settings->setSetting(ORNL::MS::TipWipe::kPerimeterEnable, true);
    perimeter_settings->setSetting(ORNL::MS::TipWipe::kPerimeterDistance, ORNL::Distance(2.0));
    perimeter_settings->setSetting(ORNL::MS::TipWipe::kPerimeterDirection,
                                   static_cast<int>(ORNL::TipWipeDirection::kForward));

    ORNL::Perimeter perimeter(perimeter_settings, 0, {}, geometry);
    perimeter.setGeometry(geometry);
    perimeter.compute(0);

    ORNL::Point perimeter_location(-10.0f, -10.0f, 0.0f);
    bool should_next_path_be_ccw = false;
    perimeter.optimize(0, perimeter_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(perimeter.getPaths(), "Perimeter");

    perimeter_settings->setSetting(ORNL::PS::Perimeter::kCompletePathBeforeConnecting, true);
    perimeter_settings->setSetting(ORNL::PS::Ordering::kPerimeterReverseDirection,
                                   static_cast<int>(ORNL::PrintDirection::kReverse_off));
    perimeter_location = ORNL::Point(-10.0f, -10.0f, 0.0f);
    perimeter.optimize(0, perimeter_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(perimeter.getPaths(), "Completed perimeter");

    QSharedPointer<ORNL::SettingsBase> inset_settings = defaultSettings();
    inset_settings->setSetting(ORNL::PS::Inset::kCount, 2);
    inset_settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(5.0));
    inset_settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    inset_settings->setSetting(ORNL::PS::Inset::kBranchAfterTipWipe, true);
    inset_settings->setSetting(ORNL::PS::Inset::kCompletePathBeforeConnecting, false);
    inset_settings->setSetting(ORNL::PS::Ordering::kInsetReverseDirection,
                               static_cast<int>(ORNL::PrintDirection::kReverse_All_Layers));
    inset_settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    inset_settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(2.0));
    inset_settings->setSetting(ORNL::MS::TipWipe::kInsetDirection, static_cast<int>(ORNL::TipWipeDirection::kForward));

    ORNL::Inset inset(inset_settings, 0, {});
    inset.setGeometry(geometry);
    inset.compute(0);

    ORNL::Point inset_location(-10.0f, -10.0f, 0.0f);
    inset.optimize(0, inset_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(inset.getPaths(), "Inset");

    inset_settings->setSetting(ORNL::PS::Inset::kCompletePathBeforeConnecting, true);
    inset_settings->setSetting(ORNL::PS::Ordering::kInsetReverseDirection,
                               static_cast<int>(ORNL::PrintDirection::kReverse_off));
    inset_location = ORNL::Point(-10.0f, -10.0f, 0.0f);
    inset.optimize(0, inset_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(inset.getPaths(), "Completed inset");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
