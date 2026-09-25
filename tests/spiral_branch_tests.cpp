#include <QFile>
#include <QList>
#include <QSharedPointer>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/kraussmaffei_writer.h"
#include "gcode/writers/mach4_writer.h"
#include "gcode/writers/marlin_writer.h"
#include "gcode/writers/reprap_writer.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "geometry/settings_polygon.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/regions/inset.h"
#include "step/layer/regions/perimeter.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"

namespace {
class RegionTrackingWriter final : public ORNL::WriterBase {
   public:
    explicit RegionTrackingWriter(const QSharedPointer<ORNL::SettingsBase>& settings)
        : WriterBase(ORNL::GcodeMetaList::ORNLMeta, settings) {}

    QString writeInitialSetup(ORNL::Distance, ORNL::Distance, ORNL::Distance, ORNL::Distance, int) override {
        return {};
    }
    QString writeBeforeLayer(float, QSharedPointer<ORNL::SettingsBase>) override {
        return {};
    }
    QString writeBeforePart(QVector3D) override {
        return {};
    }
    QString writeBeforeIsland() override {
        return {};
    }
    QString writeBeforeRegion(ORNL::RegionType type, int) override {
        before_region_regions.push_back(type);
        return {};
    }
    QString writeBeforePath(ORNL::RegionType type) override {
        before_path_regions.push_back(type);
        return {};
    }
    QString writeBeforePathRegionTransition(ORNL::RegionType type) override {
        path_region_transitions.push_back(type);
        return {};
    }
    QString writeTravel(ORNL::Point, ORNL::Point, ORNL::TravelLiftType, QSharedPointer<ORNL::SettingsBase>) override {
        return {};
    }
    QString writeLine(const ORNL::Point&, const ORNL::Point&, const QSharedPointer<ORNL::SettingsBase>) override {
        return {};
    }
    QString writeAfterPath(ORNL::RegionType type) override {
        after_path_regions.push_back(type);
        return {};
    }
    QString writeAfterRegion(ORNL::RegionType type) override {
        after_region_regions.push_back(type);
        return {};
    }
    QString writeAfterIsland() override {
        return {};
    }
    QString writeAfterPart() override {
        return {};
    }
    QString writeAfterLayer() override {
        return {};
    }
    QString writeShutdown() override {
        return {};
    }
    QString writeDwell(ORNL::Time) override {
        return {};
    }

    QVector<ORNL::RegionType> before_path_regions;
    QVector<ORNL::RegionType> path_region_transitions;
    QVector<ORNL::RegionType> after_path_regions;
    QVector<ORNL::RegionType> before_region_regions;
    QVector<ORNL::RegionType> after_region_regions;
};

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

bool verifyDynamicAccelerationTransitions() {
    QSharedPointer<ORNL::SettingsBase> settings = defaultSettings();
    if (settings.isNull()) { return expect(false, "Expected writer transition settings to load."); }

    settings->setSetting(ORNL::PRS::Acceleration::kEnableDynamic, true);

    ORNL::MarlinWriter marlin(ORNL::GcodeMetaList::MarlinMeta, settings);
    ORNL::RepRapWriter reprap(ORNL::GcodeMetaList::RepRapMeta, settings);
    ORNL::Mach4Writer mach4(ORNL::GcodeMetaList::MarlinMeta, settings);
    ORNL::KraussMaffeiWriter krauss_maffei(ORNL::GcodeMetaList::KraussMaffeiMeta, settings);

    bool passed = true;
    passed &= expect(marlin.writeBeforePathRegionTransition(ORNL::RegionType::kInset).startsWith("M204 S"),
                     "Marlin should update acceleration at a connected inset transition.");
    passed &= expect(reprap.writeBeforePathRegionTransition(ORNL::RegionType::kInset).startsWith("M204 P"),
                     "RepRap should update acceleration at a connected inset transition.");
    passed &= expect(mach4.writeBeforePathRegionTransition(ORNL::RegionType::kInset).startsWith("M204 S"),
                     "Mach4 should update acceleration at a connected inset transition.");
    passed &= expect(krauss_maffei.writeBeforePathRegionTransition(ORNL::RegionType::kInset).startsWith("M204 S"),
                     "KraussMaffei should update acceleration at a connected inset transition.");

    settings->setSetting(ORNL::PRS::Acceleration::kEnableDynamic, false);
    passed &= expect(marlin.writeBeforePathRegionTransition(ORNL::RegionType::kInset).isEmpty(),
                     "Region transitions should not emit acceleration when dynamic acceleration is disabled.");

    return passed;
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

ORNL::PolygonList separatedSquareGeometry() {
    ORNL::PolygonList geometry;
    ORNL::Polygon large_square;
    large_square << ORNL::Point(-100.0, -100.0, 0.0) << ORNL::Point(100.0, -100.0, 0.0)
                 << ORNL::Point(100.0, 100.0, 0.0) << ORNL::Point(-100.0, 100.0, 0.0);
    ORNL::Polygon small_square;
    small_square << ORNL::Point(300.0, 0.0, 0.0) << ORNL::Point(340.0, 0.0, 0.0) << ORNL::Point(340.0, 40.0, 0.0)
                 << ORNL::Point(300.0, 40.0, 0.0);
    geometry += large_square;
    geometry += small_square;
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

bool verifyLoopsCompleteBeforeWipe(const QVector<ORNL::Path>& paths, const std::string& region_name) {
    bool passed             = true;
    bool found_forward_wipe = false;

    for (const ORNL::Path& path : paths) {
        ORNL::Point loop_start;
        bool has_loop_start = false;

        for (const QSharedPointer<ORNL::SegmentBase>& segment : path) {
            if (dynamic_cast<ORNL::TravelSegment*>(segment.data()) != nullptr) { continue; }

            const ORNL::PathModifiers modifiers =
                segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers);
            if (modifiers == ORNL::PathModifiers::kSpiralConnection) {
                has_loop_start = false;
                continue;
            }

            if (!has_loop_start) {
                loop_start     = segment->start();
                has_loop_start = true;
            }

            if (modifiers == ORNL::PathModifiers::kForwardTipWipe) {
                found_forward_wipe = true;
                passed &= expect(segment->start().distance(loop_start) <= ORNL::Distance(1.0e-4),
                                 region_name + " should complete each loop before its forward tip wipe.");
            }
        }
    }

    passed &= expect(found_forward_wipe, region_name + " should exercise a completed loop with a forward tip wipe.");
    return passed;
}

bool verifyLoopRemainsOpenBeforeWipe(const QVector<ORNL::Path>& paths, const std::string& case_name,
                                     ORNL::RegionType region) {
    bool passed             = true;
    bool found_forward_wipe = false;

    for (const ORNL::Path& path : paths) {
        ORNL::Point loop_start;
        bool has_loop_start = false;

        for (const QSharedPointer<ORNL::SegmentBase>& segment : path) {
            if (dynamic_cast<ORNL::TravelSegment*>(segment.data()) != nullptr) { continue; }

            const ORNL::PathModifiers modifiers =
                segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers);
            if (modifiers == ORNL::PathModifiers::kSpiralConnection) {
                has_loop_start = false;
                continue;
            }
            if (segment->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) != region) { continue; }

            if (!has_loop_start) {
                loop_start     = segment->start();
                has_loop_start = true;
            }

            if (modifiers == ORNL::PathModifiers::kForwardTipWipe) {
                found_forward_wipe = true;
                passed &= expect(segment->start().distance(loop_start) > ORNL::Distance(1.0e-4),
                                 case_name + " should leave the loop open before its forward tip wipe.");
            }
        }
    }

    passed &= expect(found_forward_wipe, case_name + " should exercise an open loop with a forward tip wipe.");
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

bool containsModifierWithRegion(const QVector<ORNL::Path>& paths, ORNL::PathModifiers modifier,
                                ORNL::RegionType region) {
    for (const ORNL::Path& path : paths) {
        for (int i = 0; i < path.size(); ++i) {
            const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
            if (segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers) == modifier &&
                segment->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) == region) {
                return true;
            }
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

bool insetPrintingSegmentsUseWidth(const QVector<ORNL::Path>& paths, ORNL::Distance expected_width,
                                   const std::string& case_name) {
    bool passed              = true;
    bool found_inset_segment = false;

    for (const ORNL::Path& path : paths) {
        for (const QSharedPointer<ORNL::SegmentBase>& segment : path) {
            if (!segment->isPrintingSegment() ||
                segment->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) != ORNL::RegionType::kInset) {
                continue;
            }

            found_inset_segment        = true;
            const ORNL::Distance width = segment->getSb()->setting<ORNL::Distance>(ORNL::SS::kWidth);
            passed &= expect(std::abs(width() - expected_width()) <= 1.0e-4,
                             case_name + " should preserve the localized inset bead width.");
        }
    }

    return expect(found_inset_segment, case_name + " should contain an inset printing segment.") && passed;
}

bool containsPoint(const QVector<ORNL::Path>& paths, const ORNL::Point& point) {
    for (const ORNL::Path& path : paths) {
        for (int i = 0; i < path.size(); ++i) {
            const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
            if (segment->start().distance(point) <= ORNL::Distance(1.0e-4) ||
                segment->end().distance(point) <= ORNL::Distance(1.0e-4)) {
                return true;
            }
        }
    }

    return false;
}

ORNL::RegionType printingRegion(const ORNL::Path& path, bool first) {
    int index      = first ? 0 : path.size() - 1;
    const int step = first ? 1 : -1;
    while (index >= 0 && index < path.size()) {
        if (path[index]->isPrintingSegment()) {
            return path[index]->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType);
        }
        index += step;
    }

    return ORNL::RegionType::kPerimeter;
}

bool verifyPathHookRegions(ORNL::Perimeter& perimeter, const QSharedPointer<ORNL::SettingsBase>& settings,
                           const std::string& case_name, bool require_mixed_path = false) {
    QSharedPointer<RegionTrackingWriter> writer = QSharedPointer<RegionTrackingWriter>::create(settings);
    perimeter.writeGCode(writer.staticCast<ORNL::WriterBase>());

    const QVector<ORNL::Path>& paths = perimeter.getPaths();
    QVector<ORNL::RegionType> expected_before_path_regions;
    QVector<ORNL::RegionType> expected_path_region_transitions;
    QVector<ORNL::RegionType> expected_after_path_regions;
    QVector<ORNL::RegionType> expected_region_regions;
    ORNL::RegionType current_region = ORNL::RegionType::kPerimeter;
    bool region_open                = false;
    bool found_mixed_path           = false;

    for (const ORNL::Path& path : paths) {
        ORNL::RegionType path_region = ORNL::RegionType::kPerimeter;
        bool path_open               = false;
        int path_region_count        = 0;

        for (const QSharedPointer<ORNL::SegmentBase>& segment : path) {
            ORNL::RegionType segment_region = segment->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType);
            if (segment_region == ORNL::RegionType::kUnknown) {
                segment_region =
                    path_open ? path_region : (region_open ? current_region : ORNL::RegionType::kPerimeter);
            }

            if (!path_open) {
                expected_before_path_regions.push_back(segment_region);
                path_region = segment_region;
                path_open   = true;
                ++path_region_count;
            }
            else if (segment_region != path_region) {
                expected_path_region_transitions.push_back(segment_region);
                path_region = segment_region;
                ++path_region_count;
            }

            if (!region_open || segment_region != current_region) {
                expected_region_regions.push_back(segment_region);
                current_region = segment_region;
                region_open    = true;
            }
        }

        found_mixed_path |= path_region_count > 1;
        if (path_open) { expected_after_path_regions.push_back(path_region); }
    }

    bool passed = expect(writer->before_path_regions == expected_before_path_regions,
                         case_name + " should open each geometric path exactly once.");
    passed &= expect(writer->path_region_transitions == expected_path_region_transitions,
                     case_name + " should update path-level writer state at each in-path region transition.");
    passed &= expect(writer->after_path_regions == expected_after_path_regions,
                     case_name + " should close each geometric path exactly once using its final region.");
    passed &= expect(writer->before_region_regions == expected_region_regions,
                     case_name + " before-region hooks should follow each process-region transition.");
    passed &= expect(writer->after_region_regions == expected_region_regions,
                     case_name + " after-region hooks should follow each process-region transition.");
    if (require_mixed_path) {
        passed &= expect(found_mixed_path,
                         case_name + " should exercise perimeter and inset settings in one continuous path.");
    }
    return passed;
}

bool verifyTraveledInsetPreservesPerimeterLift(const QVector<ORNL::Path>& paths, const std::string& case_name) {
    bool passed               = true;
    bool found_traveled_inset = false;

    for (int i = 1; i < paths.size(); ++i) {
        const ORNL::Path& path = paths[i];
        if (path.size() == 0 || dynamic_cast<ORNL::TravelSegment*>(path.front().data()) == nullptr ||
            printingRegion(path, true) != ORNL::RegionType::kInset) {
            continue;
        }

        found_traveled_inset = true;
        passed &= expect(
            containsModifierWithRegion({paths[i - 1]}, ORNL::PathModifiers::kSpiralLift, ORNL::RegionType::kPerimeter),
            case_name + " should retain the terminal perimeter lift before traveling to an inset.");
    }

    passed &= expect(found_traveled_inset, case_name + " should exercise a traveled inset fallback.");
    return passed;
}

bool verifySeparatedBranchGroups(const QVector<ORNL::Path>& paths, const std::string& region_name,
                                 ORNL::Distance max_branch_length) {
    bool passed = expect(paths.size() > 1, region_name + " should split unrelated loops into traveled path groups.");

    for (const ORNL::Path& path : paths) {
        int travel_count = 0;
        for (int i = 0; i < path.size(); ++i) {
            const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
            if (dynamic_cast<ORNL::TravelSegment*>(segment.data()) != nullptr) { ++travel_count; }
            if (segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers) ==
                ORNL::PathModifiers::kSpiralConnection) {
                passed &= expect(segment->length() <= max_branch_length,
                                 region_name + " should not extrude a long branch between unrelated loops.");
            }
        }
        passed &= expect(travel_count == 1, region_name + " path groups should each begin with one travel.");
    }

    return passed;
}

bool verifyBranchConnectionsStayAtLayerZ(const QVector<ORNL::Path>& paths, const std::string& region_name) {
    bool passed      = true;
    int branch_count = 0;

    for (const ORNL::Path& path : paths) {
        for (int i = 0; i < path.size(); ++i) {
            const QSharedPointer<ORNL::SegmentBase>& segment = path[i];
            if (segment->getSb()->setting<ORNL::PathModifiers>(ORNL::SS::kPathModifiers) !=
                ORNL::PathModifiers::kSpiralConnection) {
                continue;
            }

            ++branch_count;
            passed &= expect(std::abs(segment->start().z()) <= 1.0e-6 && std::abs(segment->end().z()) <= 1.0e-6,
                             region_name + " branch connections should not extrude down from a lifted wipe.");
        }
    }

    passed &= expect(branch_count > 0, region_name + " should contain at least one branch connection.");
    return passed;
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

    passed &= verifyDynamicAccelerationTransitions();

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
    perimeter_settings->setSetting(ORNL::PS::Perimeter::kMinSegmentLength, ORNL::Distance(1.0));
    perimeter_settings->setSetting(ORNL::PS::Ordering::kPerimeterReverseDirection,
                                   static_cast<int>(ORNL::PrintDirection::kReverse_off));
    perimeter_location = ORNL::Point(-10.0f, -10.0f, 0.0f);
    perimeter.optimize(0, perimeter_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(perimeter.getPaths(), "Completed perimeter", ORNL::Distance(8.5));
    passed &= verifyLoopsCompleteBeforeWipe(perimeter.getPaths(), "Completed perimeter");

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
    inset_settings->setSetting(ORNL::PS::Inset::kMinSegmentLength, ORNL::Distance(1.0));
    inset_settings->setSetting(ORNL::PS::Ordering::kInsetReverseDirection,
                               static_cast<int>(ORNL::PrintDirection::kReverse_off));
    inset_location = ORNL::Point(-10.0f, -10.0f, 0.0f);
    inset.optimize(0, inset_location, should_next_path_be_ccw);
    passed &= verifyContinuousBranch(inset.getPaths(), "Completed inset", ORNL::Distance(8.5));
    passed &= verifyLoopsCompleteBeforeWipe(inset.getPaths(), "Completed inset");

    QSharedPointer<ORNL::SettingsBase> lifted_branch_settings = defaultSettings();
    lifted_branch_settings->setSetting(ORNL::PS::Inset::kCount, 5);
    lifted_branch_settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(5.0));
    lifted_branch_settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    lifted_branch_settings->setSetting(ORNL::PS::Inset::kBranchAfterTipWipe, true);
    lifted_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    lifted_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(2.0));
    lifted_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetLiftHeight, ORNL::Distance(3.0));
    lifted_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetDirection,
                                       static_cast<int>(ORNL::TipWipeDirection::kForward));
    lifted_branch_settings->setSetting(ORNL::MS::Slowdown::kInsetEnable, true);
    lifted_branch_settings->setSetting(ORNL::MS::Slowdown::kInsetDistance, ORNL::Distance(2.0));
    lifted_branch_settings->setSetting(ORNL::MS::Slowdown::kInsetLiftDistance, ORNL::Distance(4.0));
    lifted_branch_settings->setSetting(ORNL::MS::Slowdown::kInsetCutoffDistance, ORNL::Distance(0.0));
    lifted_branch_settings->setSetting(ORNL::MS::Slowdown::kInsetSpeed, ORNL::Velocity(1.0));
    lifted_branch_settings->setSetting(ORNL::MS::Slowdown::kInsetExtruderSpeed, ORNL::AngularVelocity(1.0));
    lifted_branch_settings->setSetting(ORNL::MS::SpiralLift::kInsetEnable, true);
    lifted_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftRadius, ORNL::Distance(1.0));
    lifted_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftHeight, ORNL::Distance(1.0));
    lifted_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftPoints, 8);

    ORNL::Inset lifted_branch_inset(lifted_branch_settings, 0, {});
    lifted_branch_inset.setGeometry(geometry);
    lifted_branch_inset.compute(0);
    ORNL::Point lifted_branch_location(-10.0, -10.0, 0.0);
    lifted_branch_inset.optimize(0, lifted_branch_location, should_next_path_be_ccw);
    passed &= verifyBranchConnectionsStayAtLayerZ(lifted_branch_inset.getPaths(), "Lifted inset");

    QSharedPointer<ORNL::SettingsBase> filtered_branch_settings = defaultSettings();
    filtered_branch_settings->setSetting(ORNL::PS::Inset::kCount, 5);
    filtered_branch_settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(5.0));
    filtered_branch_settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    filtered_branch_settings->setSetting(ORNL::PS::Inset::kBranchAfterTipWipe, true);
    filtered_branch_settings->setSetting(ORNL::PS::Inset::kCompletePathBeforeConnecting, false);
    filtered_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    filtered_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(2.0));
    filtered_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetDirection,
                                         static_cast<int>(ORNL::TipWipeDirection::kForward));
    filtered_branch_settings->setSetting(ORNL::MS::SpiralLift::kInsetEnable, true);
    filtered_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftRadius, ORNL::Distance(1.0));
    filtered_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftHeight, ORNL::Distance(1.0));
    filtered_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftPoints, 8);
    filtered_branch_settings->setSetting(ORNL::MS::SpiralLift::kLiftSpeed, ORNL::Velocity(1.0));

    ORNL::Inset branch_filter_probe(filtered_branch_settings, 0, {});
    branch_filter_probe.setGeometry(geometry);
    branch_filter_probe.compute(0);
    passed &= expect(branch_filter_probe.getComputedGeometry().size() >= 2,
                     "Filtered branch regression should compute adjacent inset loops.");
    if (branch_filter_probe.getComputedGeometry().size() >= 2) {
        const ORNL::Polyline& last_loop    = branch_filter_probe.getComputedGeometry().last();
        const ORNL::Distance closed_length = last_loop.length() + last_loop.back().distance(last_loop.front());
        filtered_branch_settings->setSetting(ORNL::PS::Inset::kMinPathLength, closed_length - ORNL::Distance(2.5));

        ORNL::Inset filtered_branch_inset(filtered_branch_settings, 0, {});
        filtered_branch_inset.setGeometry(geometry);
        filtered_branch_inset.compute(0);
        ORNL::Point filtered_branch_location(-10.0, -10.0, 0.0);
        filtered_branch_inset.optimize(0, filtered_branch_location, should_next_path_be_ccw);
        passed &= expect(containsModifier(filtered_branch_inset.getPaths(), ORNL::PathModifiers::kSpiralLift),
                         "A branch whose successor is filtered should retain terminal spiral-lift modifiers.");
    }

    QSharedPointer<ORNL::SettingsBase> separated_branch_settings = defaultSettings();
    separated_branch_settings->setSetting(ORNL::PS::Inset::kCount, 2);
    separated_branch_settings->setSetting(ORNL::PS::Inset::kBeadWidth, ORNL::Distance(5.0));
    separated_branch_settings->setSetting(ORNL::PS::Inset::kEnableSpiralInset, true);
    separated_branch_settings->setSetting(ORNL::PS::Inset::kBranchAfterTipWipe, true);
    separated_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetEnable, true);
    separated_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetDistance, ORNL::Distance(2.0));
    separated_branch_settings->setSetting(ORNL::MS::TipWipe::kInsetDirection,
                                          static_cast<int>(ORNL::TipWipeDirection::kForward));

    const ORNL::PolygonList separated_geometry = separatedSquareGeometry();
    ORNL::Inset separated_branch_inset(separated_branch_settings, 0, {});
    separated_branch_inset.setGeometry(separated_geometry);
    separated_branch_inset.compute(0);
    ORNL::Point separated_branch_location(-110.0, -110.0, 0.0);
    separated_branch_inset.optimize(0, separated_branch_location, should_next_path_be_ccw);
    passed &= verifySeparatedBranchGroups(separated_branch_inset.getPaths(), "Separated inset", ORNL::Distance(10.0));

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
    connected_settings->setSetting(ORNL::MS::Slowdown::kInsetEnable, true);
    connected_settings->setSetting(ORNL::MS::Slowdown::kInsetDistance, ORNL::Distance(10.0));
    connected_settings->setSetting(ORNL::MS::Slowdown::kInsetLiftDistance, ORNL::Distance(0.0));
    connected_settings->setSetting(ORNL::MS::Slowdown::kInsetCutoffDistance, ORNL::Distance(10000.0));
    connected_settings->setSetting(ORNL::MS::Slowdown::kInsetSpeed, ORNL::Velocity(1.0));
    connected_settings->setSetting(ORNL::MS::Slowdown::kInsetExtruderSpeed, ORNL::AngularVelocity(1.0));
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
        passed &= expect(containsModifierWithRegion(connected_perimeter->getPaths(), ORNL::PathModifiers::kCoasting,
                                                    ORNL::RegionType::kInset),
                         "Expected the connected inset cutoff to coast within the inset tail.");
        passed &= expect(!containsModifierWithRegion(connected_perimeter->getPaths(), ORNL::PathModifiers::kCoasting,
                                                     ORNL::RegionType::kPerimeter),
                         "Expected the connected inset cutoff to leave preceding perimeter segments unchanged.");
        passed &= verifyPathHookRegions(*connected_perimeter, connected_settings, "Connected perimeter", true);
    }

    QSharedPointer<ORNL::SettingsBase> localized_width_settings = defaultSettings();
    configureConnectedInsets(localized_width_settings);

    QSharedPointer<ORNL::SettingsBase> localized_width_override = QSharedPointer<ORNL::SettingsBase>::create();
    const ORNL::Distance localized_inset_width(13.0);
    localized_width_override->setSetting(ORNL::PS::Inset::kBeadWidth, localized_inset_width);

    ORNL::Polygon localized_bounds;
    localized_bounds << ORNL::Point(-200.0, -200.0, 0.0) << ORNL::Point(400.0, -200.0, 0.0)
                     << ORNL::Point(400.0, 400.0, 0.0) << ORNL::Point(-200.0, 400.0, 0.0);
    QVector<ORNL::Polygon> localized_geometry {localized_bounds};
    ORNL::SettingsPolygon localized_width_polygon(localized_geometry, localized_width_override);

    ORNL::PolymerIsland localized_width_island(geometry, localized_width_settings, {localized_width_polygon});
    localized_width_island.compute(0);
    localized_width_island.reorderRegions();
    ORNL::Point localized_width_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> localized_width_previous_regions;
    localized_width_island.optimize(0, localized_width_location, localized_width_previous_regions);

    QSharedPointer<ORNL::Perimeter> localized_width_perimeter =
        localized_width_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> localized_width_inset =
        localized_width_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    passed &= expect(!localized_width_perimeter.isNull() && !localized_width_inset.isNull(),
                     "Localized connected inset should create both regions.");
    if (!localized_width_perimeter.isNull() && !localized_width_inset.isNull()) {
        passed &= expect(localized_width_perimeter->connectedInsetGeometryConsumed(),
                         "Localized connected inset geometry should be emitted by the perimeter.");
        passed &= expect(localized_width_inset->getPaths().isEmpty(),
                         "Localized connected insets should not be emitted twice.");
        passed &= insetPrintingSegmentsUseWidth(localized_width_perimeter->getPaths(), localized_inset_width,
                                                "Localized connected inset");
    }
    for (const int perimeter_count : {1, 2}) {
        QSharedPointer<ORNL::SettingsBase> lifted_connected_settings = defaultSettings();
        configureConnectedInsets(lifted_connected_settings);
        lifted_connected_settings->setSetting(ORNL::PS::Perimeter::kCount, perimeter_count);
        lifted_connected_settings->setSetting(ORNL::PS::Perimeter::kBranchAfterTipWipe, true);
        lifted_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterEnable, true);
        lifted_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterDistance, ORNL::Distance(2.0));
        lifted_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterLiftHeight, ORNL::Distance(3.0));
        lifted_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterDirection,
                                              static_cast<int>(ORNL::TipWipeDirection::kForward));
        lifted_connected_settings->setSetting(ORNL::MS::Slowdown::kPerimeterEnable, true);
        lifted_connected_settings->setSetting(ORNL::MS::Slowdown::kPerimeterDistance, ORNL::Distance(2.0));
        lifted_connected_settings->setSetting(ORNL::MS::Slowdown::kPerimeterLiftDistance, ORNL::Distance(4.0));
        lifted_connected_settings->setSetting(ORNL::MS::Slowdown::kPerimeterCutoffDistance, ORNL::Distance(0.0));
        lifted_connected_settings->setSetting(ORNL::MS::Slowdown::kPerimeterSpeed, ORNL::Velocity(1.0));
        lifted_connected_settings->setSetting(ORNL::MS::Slowdown::kPerimeterExtruderSpeed, ORNL::AngularVelocity(1.0));
        lifted_connected_settings->setSetting(ORNL::MS::SpiralLift::kPerimeterEnable, true);
        lifted_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftRadius, ORNL::Distance(1.0));
        lifted_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftHeight, ORNL::Distance(1.0));
        lifted_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftPoints, 8);

        ORNL::PolymerIsland lifted_connected_island(geometry, lifted_connected_settings, {});
        lifted_connected_island.compute(0);
        lifted_connected_island.reorderRegions();
        ORNL::Point lifted_connected_location(-10.0, -10.0, 0.0);
        QVector<QSharedPointer<ORNL::RegionBase>> lifted_connected_previous_regions;
        lifted_connected_island.optimize(0, lifted_connected_location, lifted_connected_previous_regions);

        QSharedPointer<ORNL::Perimeter> lifted_connected_perimeter =
            lifted_connected_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
        QSharedPointer<ORNL::Inset> lifted_connected_inset =
            lifted_connected_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
        const std::string case_name = "Lifted connected inset with " + std::to_string(perimeter_count) +
                                      (perimeter_count == 1 ? " perimeter" : " perimeters");
        passed &= expect(!lifted_connected_perimeter.isNull() && !lifted_connected_inset.isNull(),
                         case_name + " should create both regions.");
        if (!lifted_connected_perimeter.isNull() && !lifted_connected_inset.isNull()) {
            passed &= expect(lifted_connected_perimeter->connectedInsetGeometryConsumed(),
                             case_name + " should emit every connected inset contour.");
            passed &= expect(lifted_connected_inset->getPaths().isEmpty(),
                             case_name + " should avoid duplicate inset output.");
            passed &= verifyBranchConnectionsStayAtLayerZ(lifted_connected_perimeter->getPaths(), case_name);
            if (perimeter_count == 1) {
                passed &= verifyLoopRemainsOpenBeforeWipe(lifted_connected_perimeter->getPaths(), case_name,
                                                          ORNL::RegionType::kPerimeter);
            }
            passed &=
                expect(containsModifierWithRegion(lifted_connected_perimeter->getPaths(),
                                                  ORNL::PathModifiers::kSpiralConnection, ORNL::RegionType::kPerimeter),
                       case_name + " should retain perimeter settings on the perimeter-to-inset bridge.");
        }
    }

    QSharedPointer<ORNL::SettingsBase> unsafe_connected_settings = defaultSettings();
    configureConnectedInsets(unsafe_connected_settings);
    unsafe_connected_settings->setSetting(ORNL::PS::Perimeter::kCount, 1);
    unsafe_connected_settings->setSetting(ORNL::PS::Perimeter::kBranchAfterTipWipe, true);
    unsafe_connected_settings->setSetting(ORNL::PS::Inset::kCount, 1);
    unsafe_connected_settings->setSetting(ORNL::PS::Inset::kMinPathLength, ORNL::Distance(150.0));
    unsafe_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterEnable, true);
    unsafe_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterDistance, ORNL::Distance(2.0));
    unsafe_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterLiftHeight, ORNL::Distance(3.0));
    unsafe_connected_settings->setSetting(ORNL::MS::TipWipe::kPerimeterDirection,
                                          static_cast<int>(ORNL::TipWipeDirection::kForward));
    unsafe_connected_settings->setSetting(ORNL::MS::SpiralLift::kPerimeterEnable, true);
    unsafe_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftRadius, ORNL::Distance(1.0));
    unsafe_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftHeight, ORNL::Distance(1.0));
    unsafe_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftPoints, 8);
    unsafe_connected_settings->setSetting(ORNL::MS::SpiralLift::kLiftSpeed, ORNL::Velocity(1.0));
    unsafe_connected_settings->setSetting(ORNL::MS::Startup::kInsetEnable, true);
    unsafe_connected_settings->setSetting(ORNL::MS::Startup::kInsetDistance, ORNL::Distance(2.0));
    unsafe_connected_settings->setSetting(ORNL::MS::Startup::kInsetSpeed, ORNL::Velocity(1.0));
    unsafe_connected_settings->setSetting(ORNL::MS::Startup::kInsetExtruderSpeed, ORNL::AngularVelocity(1.0));

    ORNL::PolymerIsland unsafe_connected_island(separated_geometry, unsafe_connected_settings, {});
    unsafe_connected_island.compute(0);
    unsafe_connected_island.reorderRegions();
    ORNL::Point unsafe_connected_location(-110.0, -110.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> unsafe_connected_previous_regions;
    unsafe_connected_island.optimize(0, unsafe_connected_location, unsafe_connected_previous_regions);

    QSharedPointer<ORNL::Perimeter> unsafe_connected_perimeter =
        unsafe_connected_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> unsafe_connected_inset =
        unsafe_connected_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    passed &= expect(!unsafe_connected_perimeter.isNull() && !unsafe_connected_inset.isNull(),
                     "Expected separated connected perimeter and inset regions.");
    if (!unsafe_connected_perimeter.isNull() && !unsafe_connected_inset.isNull()) {
        passed &= expect(unsafe_connected_inset->getComputedGeometry().size() == 1,
                         "The safety regression should leave the final perimeter component without an inset.");
        passed &= verifySeparatedBranchGroups(unsafe_connected_perimeter->getPaths(), "Separated connected inset",
                                              ORNL::Distance(15.0));
        passed &= expect(unsafe_connected_perimeter->connectedInsetGeometryConsumed(),
                         "Separated connected insets should still be emitted after a safe travel.");
        passed &= expect(unsafe_connected_inset->getPaths().isEmpty(),
                         "Traveled connected insets should not be emitted a second time.");
        passed &= expect(containsModifierWithRegion(unsafe_connected_perimeter->getPaths(),
                                                    ORNL::PathModifiers::kInitialStartup, ORNL::RegionType::kInset),
                         "Traveled connected insets should receive inset startup modifiers.");
        passed &=
            verifyPathHookRegions(*unsafe_connected_perimeter, unsafe_connected_settings, "Separated connected inset");
        passed &= verifyTraveledInsetPreservesPerimeterLift(unsafe_connected_perimeter->getPaths(),
                                                            "Separated connected inset");
    }

    QSharedPointer<ORNL::SettingsBase> separated_connected_settings = defaultSettings();
    configureConnectedInsets(separated_connected_settings);
    separated_connected_settings->setSetting(ORNL::PS::Perimeter::kMinPathLength, ORNL::Distance(200.0));
    ORNL::PolymerIsland separated_connected_island(separated_geometry, separated_connected_settings, {});
    separated_connected_island.compute(0);
    separated_connected_island.reorderRegions();
    ORNL::Point separated_connected_location(-110.0, -110.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> separated_connected_previous_regions;
    separated_connected_island.optimize(0, separated_connected_location, separated_connected_previous_regions);

    QSharedPointer<ORNL::Perimeter> separated_connected_perimeter =
        separated_connected_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> separated_connected_inset =
        separated_connected_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    if (!separated_connected_perimeter.isNull() && !separated_connected_inset.isNull()) {
        passed &= expect(separated_connected_perimeter->connectedInsetGeometryConsumed(),
                         "Expected every separated connected-inset contour to be emitted.");
        for (const ORNL::Polyline& line : separated_connected_inset->getComputedGeometry()) {
            if (!line.isEmpty()) {
                passed &= expect(containsPoint(separated_connected_perimeter->getPaths(), line.front()),
                                 "Expected connected output to retain every inset contour.");
            }
        }
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

    QSharedPointer<ORNL::SettingsBase> localized_material_settings = defaultSettings();
    configureConnectedInsets(localized_material_settings);
    localized_material_settings->setSetting(ORNL::MS::MultiMaterial::kEnable, true);
    localized_material_settings->setSetting(ORNL::MS::MultiMaterial::kPerimeterNum, 0);
    localized_material_settings->setSetting(ORNL::MS::MultiMaterial::kInsetNum, 0);

    QSharedPointer<ORNL::SettingsBase> localized_material_override = QSharedPointer<ORNL::SettingsBase>::create();
    localized_material_override->setSetting(ORNL::MS::MultiMaterial::kInsetNum, 1);
    ORNL::SettingsPolygon localized_material_polygon(localized_geometry, localized_material_override);

    ORNL::PolymerIsland localized_material_island(geometry, localized_material_settings, {localized_material_polygon});
    localized_material_island.compute(0);
    localized_material_island.reorderRegions();
    ORNL::Point localized_material_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> localized_material_previous_regions;
    localized_material_island.optimize(0, localized_material_location, localized_material_previous_regions);

    QSharedPointer<ORNL::Perimeter> localized_material_perimeter =
        localized_material_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> localized_material_inset =
        localized_material_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    passed &= expect(!localized_material_perimeter.isNull() && !localized_material_inset.isNull(),
                     "Localized multi-material regression should create perimeter and inset regions.");
    if (!localized_material_perimeter.isNull() && !localized_material_inset.isNull()) {
        passed &= expect(!localized_material_perimeter->connectedInsetGeometryConsumed(),
                         "A localized inset material override should prevent a continuous connected path.");
        passed &= expect(!containsRegion(localized_material_perimeter->getPaths(), ORNL::RegionType::kInset),
                         "Perimeter paths should not consume locally different-material inset geometry.");
        passed &= expect(!localized_material_inset->getPaths().isEmpty(),
                         "Locally different-material insets should remain in their own region.");
    }

    QSharedPointer<ORNL::SettingsBase> intervening_region_settings = defaultSettings();
    configureConnectedInsets(intervening_region_settings);
    intervening_region_settings->setSetting(ORNL::PS::Skin::kEnable, true);
    intervening_region_settings->setSetting(
        ORNL::PS::Ordering::kRegionOrder,
        QList<QString> {QStringLiteral("Perimeter"), QStringLiteral("Skin"), QStringLiteral("Inset"),
                        QStringLiteral("Infill"), QStringLiteral("Skeleton")});
    ORNL::PolymerIsland intervening_region_island(geometry, intervening_region_settings, {});
    intervening_region_island.compute(0);
    intervening_region_island.reorderRegions();
    ORNL::Point intervening_region_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> intervening_region_previous_regions;
    intervening_region_island.optimize(0, intervening_region_location, intervening_region_previous_regions);

    QSharedPointer<ORNL::Perimeter> intervening_region_perimeter =
        intervening_region_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> intervening_region_inset =
        intervening_region_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    passed &= expect(!intervening_region_perimeter.isNull() && !intervening_region_inset.isNull(),
                     "Intervening-region regression should create perimeter and inset regions.");
    if (!intervening_region_perimeter.isNull() && !intervening_region_inset.isNull()) {
        passed &= expect(!intervening_region_perimeter->connectedInsetGeometryConsumed(),
                         "An intervening region should prevent perimeter-to-inset reordering.");
        passed &= expect(!containsRegion(intervening_region_perimeter->getPaths(), ORNL::RegionType::kInset),
                         "Perimeter paths should not pull insets ahead of an intervening region.");
        passed &= expect(!intervening_region_inset->getPaths().isEmpty(),
                         "Insets separated by another region should retain their configured order.");
    }

    QSharedPointer<ORNL::SettingsBase> reoptimized_settings = defaultSettings();
    configureConnectedInsets(reoptimized_settings);
    ORNL::PolymerIsland reoptimized_island(geometry, reoptimized_settings, {});
    reoptimized_island.compute(0);
    reoptimized_island.reorderRegions();
    ORNL::Point reoptimized_location(-10.0, -10.0, 0.0);
    QVector<QSharedPointer<ORNL::RegionBase>> reoptimized_previous_regions;
    reoptimized_island.optimize(0, reoptimized_location, reoptimized_previous_regions);

    QSharedPointer<ORNL::Perimeter> reoptimized_perimeter =
        reoptimized_island.getRegion(ORNL::RegionType::kPerimeter).dynamicCast<ORNL::Perimeter>();
    QSharedPointer<ORNL::Inset> reoptimized_inset =
        reoptimized_island.getRegion(ORNL::RegionType::kInset).dynamicCast<ORNL::Inset>();
    passed &= expect(!reoptimized_perimeter.isNull() && !reoptimized_inset.isNull(),
                     "Re-optimization regression should create perimeter and inset regions.");
    if (!reoptimized_perimeter.isNull() && !reoptimized_inset.isNull()) {
        passed &= expect(reoptimized_perimeter->connectedInsetGeometryConsumed(),
                         "The initial optimization should consume connected inset geometry.");

        reoptimized_settings->setSetting(ORNL::MS::MultiMaterial::kEnable, true);
        reoptimized_settings->setSetting(ORNL::MS::MultiMaterial::kPerimeterNum, 0);
        reoptimized_settings->setSetting(ORNL::MS::MultiMaterial::kInsetNum, 1);
        reoptimized_location = ORNL::Point(-10.0, -10.0, 0.0);
        reoptimized_previous_regions.clear();
        reoptimized_island.optimize(0, reoptimized_location, reoptimized_previous_regions);

        passed &= expect(!reoptimized_perimeter->connectedInsetGeometryConsumed(),
                         "Re-optimization should clear stale connected geometry after materials diverge.");
        passed &= expect(!containsRegion(reoptimized_perimeter->getPaths(), ORNL::RegionType::kInset),
                         "Re-optimized perimeter paths should not retain stale inset segments.");
        passed &= expect(!reoptimized_inset->getPaths().isEmpty(),
                         "Re-optimized different-material insets should be emitted exactly by their own region.");
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
