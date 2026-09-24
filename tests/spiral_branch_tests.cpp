#include <QFile>
#include <QSharedPointer>
#include <cmath>
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
#include "step/layer/island/polymer_island.h"
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

ORNL::PolygonList circularGeometry() {
    constexpr int point_count = 64;
    constexpr double radius   = 100.0;
    constexpr double center   = 100.0;
    constexpr double two_pi   = 6.283185307179586;

    ORNL::PolygonList geometry;
    ORNL::Polygon circle;
    for (int i = 0; i < point_count; ++i) {
        const double angle = two_pi * i / point_count;
        circle << ORNL::Point(center + (radius * std::cos(angle)), center + (radius * std::sin(angle)), 0.0);
    }
    geometry += circle;
    return geometry;
}

ORNL::PolygonList squareGeometry() {
    ORNL::PolygonList geometry;
    ORNL::Polygon square;
    square << ORNL::Point(-100.0, -100.0, 0.0) << ORNL::Point(100.0, -100.0, 0.0) << ORNL::Point(100.0, 100.0, 0.0)
           << ORNL::Point(-100.0, 100.0, 0.0);
    geometry += square;
    return geometry;
}

bool verifyContinuousBranch(const QVector<ORNL::Path>& paths, const std::string& region_name,
                            ORNL::Distance max_branch_length, bool require_all_branches_angled = false) {
    constexpr double angled_dot_tolerance     = 0.15;
    constexpr double orthogonal_dot_tolerance = 0.075;
    bool passed = expect(paths.size() == 1, region_name + " branch should be emitted as one continuous path.");
    if (paths.size() != 1) return false;

    const ORNL::Path& path  = paths.front();
    int travel_count        = 0;
    int branch_count        = 0;
    int angled_branch_count = 0;
    bool found_branch       = false;

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
        if (previous_modifiers == ORNL::PathModifiers::kForwardTipWipe &&
            modifiers == ORNL::PathModifiers::kSpiralConnection &&
            dynamic_cast<ORNL::LineSegment*>(segment.data()) != nullptr && segment->isPrintingSegment()) {
            found_branch = true;
            ++branch_count;
            const double outgoing_x      = previous->end().x() - previous->start().x();
            const double outgoing_y      = previous->end().y() - previous->start().y();
            const double branch_x        = segment->end().x() - segment->start().x();
            const double branch_y        = segment->end().y() - segment->start().y();
            const double outgoing_length = std::hypot(outgoing_x, outgoing_y);
            const double branch_length   = std::hypot(branch_x, branch_y);
            const double forward_dot     = (outgoing_x * branch_x) + (outgoing_y * branch_y);
            const double normalized_dot  = forward_dot / (outgoing_length * branch_length);
            passed &= expect(i + 1 < path.size(), region_name + " branch should connect to a following loop segment.");
            if (i + 1 >= path.size()) { continue; }

            const QSharedPointer<ORNL::SegmentBase>& next = path[i + 1];
            const double next_x                           = next->end().x() - next->start().x();
            const double next_y                           = next->end().y() - next->start().y();
            const double next_length                      = std::hypot(next_x, next_y);
            const double approach_dot = ((branch_x * next_x) + (branch_y * next_y)) / (branch_length * next_length);
            passed &= expect(forward_dot > 0.0,
                             region_name + " branch should continue in the tip wipe's forward direction (dot " +
                                 std::to_string(forward_dot) + ", outgoing " + std::to_string(outgoing_x) + "," +
                                 std::to_string(outgoing_y) + ", branch " + std::to_string(branch_x) + "," +
                                 std::to_string(branch_y) + ").");
            passed &= expect(branch_length <= max_branch_length(),
                             region_name + " branch should use a nearby point on the next loop (length " +
                                 std::to_string(branch_length) + ").");
            const bool is_angled = std::abs(normalized_dot - std::sqrt(0.5)) <= angled_dot_tolerance &&
                                   std::abs(approach_dot - std::sqrt(0.5)) <= angled_dot_tolerance;
            const bool is_orthogonal =
                normalized_dot <= orthogonal_dot_tolerance && std::abs(approach_dot) <= orthogonal_dot_tolerance;
            if (is_angled) { ++angled_branch_count; }
            if (require_all_branches_angled && !is_angled) {
                passed &=
                    expect(false, region_name + " branch from (" + std::to_string(segment->start().x()) + "," +
                                      std::to_string(segment->start().y()) + ") to (" +
                                      std::to_string(segment->end().x()) + "," + std::to_string(segment->end().y()) +
                                      ") should be angled (departure dot " + std::to_string(normalized_dot) +
                                      ", approach dot " + std::to_string(approach_dot) + ").");
            }
            passed &=
                expect(is_angled || is_orthogonal,
                       region_name +
                           " branch should approach at approximately 45 degrees or fall back to orthogonal "
                           "(departure dot " +
                           std::to_string(normalized_dot) + ", approach dot " + std::to_string(approach_dot) + ").");
        }
    }

    passed &= expect(travel_count == 1, region_name + " branch should contain only its initial travel.");
    passed &= expect(found_branch, region_name + " branch should extrude from the tip wipe to the next loop.");
    passed &= expect(angled_branch_count > 0, region_name + " should use an angled branch when space permits.");
    if (require_all_branches_angled) {
        passed &= expect(angled_branch_count == branch_count,
                         region_name + " should angle every branch when the adjacent square edges have space (" +
                             std::to_string(angled_branch_count) + " of " + std::to_string(branch_count) +
                             " branches were angled).");
    }
    return passed;
}

bool containsModifier(const QVector<ORNL::Path>& paths, ORNL::PathModifiers modifier) {
    for (const ORNL::Path& path : paths) {
        for (int i = 0; i < path.size(); ++i) {
            const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
            if (segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers) == modifier) { return true; }
        }
    }

    return false;
}

bool containsRegion(const QVector<ORNL::Path>& paths, ORNL::RegionType region) {
    for (const ORNL::Path& path : paths) {
        for (int i = 0; i < path.size(); ++i) {
            const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
            if (segment->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) == region) { return true; }
        }
    }

    return false;
}

void configureConnectedInsets(const QSharedPointer<ORNL::SettingsBase>& settings) {
    settings->setSetting(ORNL::PS::Perimeter::kEnable, true);
    settings->setSetting(ORNL::PS::Perimeter::kCount, 2);
    settings->setSetting(ORNL::PS::Perimeter::kBeadWidth, ORNL::Distance(5.0));
    settings->setSetting(ORNL::PS::Perimeter::kEnableSpiralPerimeter, true);
    settings->setSetting(ORNL::PS::Perimeter::kConnectToInsets, true);
    settings->setSetting(ORNL::PS::Perimeter::kBranchAfterTipWipe, false);
    settings->setSetting(ORNL::PS::Inset::kEnable, true);
    settings->setSetting(ORNL::PS::Inset::kCount, 2);
    settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(5.0));
    settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    settings->setSetting(ORNL::MS::TipWipe::kPerimeterEnable, false);
    settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(2.0));
    settings->setSetting(ORNL::MS::TipWipe::kInsetDirection, static_cast<int>(ORNL::TipWipeDirection::kForward));
}
}  // namespace

int main() {
    Q_INIT_RESOURCE(configs);

    bool passed                      = true;
    const ORNL::PolygonList geometry = circularGeometry();

    QSharedPointer<ORNL::SettingsBase> perimeter_settings = defaultSettings();
    passed &= expect(!perimeter_settings.isNull(), "Expected the master settings resource to load.");
    if (perimeter_settings.isNull()) return EXIT_FAILURE;

    perimeter_settings->setSetting(ORNL::PS::Perimeter::kCount, 5);
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
    passed &= verifyContinuousBranch(perimeter.getPaths(), "Perimeter", ORNL::Distance(8.5));

    perimeter_settings->setSetting(ORNL::PS::Perimeter::kCompletePathBeforeConnecting, true);
    perimeter_settings->setSetting(ORNL::PS::Ordering::kPerimeterReverseDirection,
                                   static_cast<int>(ORNL::PrintDirection::kReverse_off));
    perimeter_location = ORNL::Point(-10.0f, -10.0f, 0.0f);
    perimeter.optimize(0, perimeter_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(perimeter.getPaths(), "Completed perimeter", ORNL::Distance(8.5));

    QSharedPointer<ORNL::SettingsBase> inset_settings = defaultSettings();
    inset_settings->setSetting(ORNL::PS::Inset::kCount, 5);
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
    passed &= verifyContinuousBranch(inset.getPaths(), "Inset", ORNL::Distance(8.5));

    inset_settings->setSetting(ORNL::PS::Inset::kCompletePathBeforeConnecting, true);
    inset_settings->setSetting(ORNL::PS::Ordering::kInsetReverseDirection,
                               static_cast<int>(ORNL::PrintDirection::kReverse_off));
    inset_location = ORNL::Point(-10.0f, -10.0f, 0.0f);
    inset.optimize(0, inset_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(inset.getPaths(), "Completed inset", ORNL::Distance(8.5));

    QSharedPointer<ORNL::SettingsBase> square_inset_settings = defaultSettings();
    square_inset_settings->setSetting(ORNL::PS::Inset::kCount, 100);
    square_inset_settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(4.5));
    square_inset_settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    square_inset_settings->setSetting(ORNL::PS::Inset::kBranchAfterTipWipe, true);
    square_inset_settings->setSetting(ORNL::PS::Inset::kCompletePathBeforeConnecting, true);
    square_inset_settings->setSetting(ORNL::PS::Optimizations::kPointOrder,
                                      static_cast<int>(ORNL::PointOrderOptimization::kConsecutive));
    square_inset_settings->setSetting(ORNL::PS::Optimizations::kConsecutiveDistanceThreshold, ORNL::Distance(100.0));
    square_inset_settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    square_inset_settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(1.2));
    square_inset_settings->setSetting(ORNL::MS::TipWipe::kInsetDirection,
                                      static_cast<int>(ORNL::TipWipeDirection::kForward));

    const ORNL::PolygonList square_geometry = squareGeometry();
    ORNL::Inset square_inset(square_inset_settings, 0, {});
    square_inset.setGeometry(square_geometry);
    square_inset.compute(0);

    ORNL::Point square_inset_location(-110.0, -110.0, 0.0);
    square_inset.optimize(0, square_inset_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(square_inset.getPaths(), "Square inset", ORNL::Distance(8.0), true);

    QSharedPointer<ORNL::SettingsBase> reverse_wipe_settings = defaultSettings();
    reverse_wipe_settings->setSetting(ORNL::PS::Inset::kCount, 5);
    reverse_wipe_settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(5.0));
    reverse_wipe_settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    reverse_wipe_settings->setSetting(ORNL::PS::Inset::kBranchAfterTipWipe, true);
    reverse_wipe_settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    reverse_wipe_settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(2.0));
    reverse_wipe_settings->setSetting(ORNL::MS::TipWipe::kInsetDirection,
                                      static_cast<int>(ORNL::TipWipeDirection::kReverse));

    ORNL::Inset reverse_wipe_inset(reverse_wipe_settings, 0, {});
    reverse_wipe_inset.setGeometry(geometry);
    reverse_wipe_inset.compute(0);
    ORNL::Point reverse_wipe_location(-10.0, -10.0, 0.0);
    reverse_wipe_inset.optimize(0, reverse_wipe_location, should_next_path_be_ccw);
    passed &= expect(!containsModifier(reverse_wipe_inset.getPaths(), ORNL::PathModifiers::kSpiralConnection),
                     "Reverse tip wipes should not use the forward-only branch connection algorithm.");
    passed &= expect(containsModifier(reverse_wipe_inset.getPaths(), ORNL::PathModifiers::kReverseTipWipe),
                     "Reverse tip wipes should retain normal spiral path modifier behavior.");

    QSharedPointer<ORNL::SettingsBase> connected_settings = defaultSettings();
    configureConnectedInsets(connected_settings);
    ORNL::PolymerIsland connected_island(geometry, connected_settings, {});
    connected_island.compute(0);
    connected_island.reorderRegions();
    ORNL::Point connected_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> connected_previous_regions;
    connected_island.optimize(0, connected_location, connected_previous_regions);

    QSharedPointer<ORNL::Perimeter> connected_perimeter =
        connected_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> connected_inset =
        connected_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    passed &= expect(!connected_perimeter.isNull() && !connected_inset.isNull(),
                     "Expected connected perimeter and inset regions.");
    if (!connected_perimeter.isNull() && !connected_inset.isNull()) {
        passed &= expect(connected_perimeter->connectedInsetGeometryConsumed(),
                         "Expected the perimeter to report emitted connected inset geometry.");
        passed &= expect(connected_inset->getPaths().isEmpty(),
                         "Expected a consumed inset region to avoid duplicate output.");
        passed &= expect(containsRegion(connected_perimeter->getPaths(), ORNL::RegionType::kInset),
                         "Expected connected paths to contain inset process settings.");
        passed &= expect(containsModifier(connected_perimeter->getPaths(), ORNL::PathModifiers::kForwardTipWipe),
                         "Expected connected paths to use the inset terminal tip wipe.");
    }

    QSharedPointer<ORNL::SettingsBase> spiralize_settings = defaultSettings();
    configureConnectedInsets(spiralize_settings);
    spiralize_settings->setSetting(ORNL::PS::SpecialModes::kEnableSpiralize, true);
    ORNL::PolymerIsland spiralize_island(geometry, spiralize_settings, {});
    spiralize_island.compute(0);
    spiralize_island.reorderRegions();
    ORNL::Point spiralize_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> spiralize_previous_regions;
    spiralize_island.optimize(0, spiralize_location, spiralize_previous_regions);

    QSharedPointer<ORNL::Perimeter> spiralize_perimeter =
        spiralize_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> spiralize_inset =
        spiralize_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    if (!spiralize_perimeter.isNull() && !spiralize_inset.isNull()) {
        passed &= expect(!spiralize_perimeter->connectedInsetGeometryConsumed(),
                         "Global spiralize should not claim unused connected inset geometry.");
        passed &= expect(!spiralize_inset->getPaths().isEmpty(),
                         "Global spiralize should not silently suppress unconsumed inset geometry.");
    }

    QSharedPointer<ORNL::SettingsBase> multi_material_settings = defaultSettings();
    configureConnectedInsets(multi_material_settings);
    multi_material_settings->setSetting(ORNL::MS::MultiMaterial::kEnable, true);
    multi_material_settings->setSetting(ORNL::MS::MultiMaterial::kPerimeterNum, 0);
    multi_material_settings->setSetting(ORNL::MS::MultiMaterial::kInsetNum, 1);
    ORNL::PolymerIsland multi_material_island(geometry, multi_material_settings, {});
    multi_material_island.compute(0);
    multi_material_island.reorderRegions();
    ORNL::Point multi_material_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> multi_material_previous_regions;
    multi_material_island.optimize(0, multi_material_location, multi_material_previous_regions);

    QSharedPointer<ORNL::Perimeter> multi_material_perimeter =
        multi_material_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> multi_material_inset =
        multi_material_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    if (!multi_material_perimeter.isNull() && !multi_material_inset.isNull()) {
        passed &= expect(!multi_material_perimeter->connectedInsetGeometryConsumed(),
                         "Different perimeter and inset materials should remain separate regions.");
        passed &= expect(!multi_material_inset->getPaths().isEmpty(),
                         "Different-material insets should retain region-level transition handling.");
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
