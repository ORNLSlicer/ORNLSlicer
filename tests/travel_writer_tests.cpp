#include <QCoreApplication>
#include <QSharedPointer>
#include <QString>
#include <cstdlib>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/aerobasic_writer.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "gcode/writers/cincinnati_writer.h"
#include "gcode/writers/juggerbot_writer.h"
#include "gcode/writers/kraussmaffei_writer.h"
#include "gcode/writers/marlin_writer.h"
#include "gcode/writers/reprap_writer.h"
#include "gcode/writers/tormach_writer.h"
#include "gcode/writers/wolf_writer.h"
#include "test_utils.h"

namespace {
using namespace ORNL;

const Point kStart(0.0 * mm, 0.0 * mm, 1.0 * mm);
const Point kTarget(3.0 * mm, 0.0 * mm, 1.0 * mm);
const QVector<Point> kWaypoints {Point(0.0 * mm, 2.0 * mm, 1.0 * mm), Point(3.0 * mm, 2.0 * mm, 1.0 * mm)};

QSharedPointer<SettingsBase> writerSettings() {
    auto settings = QSharedPointer<SettingsBase>::create();
    settings->setSetting(PS::Travel::kLiftHeight, 2.0 * mm);
    settings->setSetting(PS::Travel::kMinTravelForLift, 5.0 * mm);
    settings->setSetting(PS::Travel::kMinTravelLength, 5.0 * mm);
    settings->setSetting(PS::Travel::kSpeed, 10.0 * mm / s);
    settings->setSetting(PS::Slicing::kSlicePlaneNormalZ, 1.0f);
    settings->setSetting(PRS::MachineSpeed::kZSpeed, 5.0 * mm / s);
    settings->setSetting(PRS::MachineSpeed::kGearRatio, 1.0f);
    settings->setSetting(PRS::Dimensions::kLayerChangeAxis, LayerChange::kZ_only);
    settings->setSetting(MS::Retraction::kMinTravel, 5.0 * mm);
    settings->setSetting(MS::Retraction::kLength, 1.0 * mm);
    settings->setSetting(MS::Retraction::kSpeed, 2.0 * mm / s);
    return settings;
}

QSharedPointer<SettingsBase> segmentSettings() {
    auto settings = QSharedPointer<SettingsBase>::create();
    settings->setSetting(SS::kSpeed, 10.0 * mm / s);
    settings->setSetting(PS::Travel::kSpeed, 10.0 * mm / s);
    settings->setSetting(SS::kRegionType, RegionType::kPerimeter);
    settings->setSetting(SS::kIsRegionStartSegment, true);
    settings->setSetting(SS::kExtruderSpeed, 100);
    return settings;
}

template <typename Writer>
class WriterHarness : public Writer {
   public:
    explicit WriterHarness(const QSharedPointer<SettingsBase>& settings) : Writer(GcodeMetaList::MarlinMeta, settings) {
        this->writeInitialSetup(0, 0, 0, 0, 1);
        this->writeBeforeLayer(kStart.z(), settings);
        this->writeLayerTravel(kStart, kStart, TravelLiftType::kBoth, segmentSettings());
    }

    void setDepositionActive(bool active) {
        this->m_deposition_active = active;
    }
};

bool hasSingleLiftAndLower(const QString& gcode) {
    return gcode.count("TRAVEL LIFT Z") == 1 && gcode.count("TRAVEL LOWER Z") == 1 && gcode.count(" Z3.0000") == 1 &&
           gcode.count(" Z1.0000") == 1 && gcode.contains(" X0.0000 Y2.0000") && gcode.contains(" X3.0000 Y2.0000") &&
           gcode.contains(" X3.0000 Y0.0000");
}

template <typename Writer>
bool usesCompleteTravelDistance() {
    auto settings = writerSettings();
    WriterHarness<Writer> writer(settings);
    settings->setSetting(MS::Retraction::kEnable, true);
    const auto params    = segmentSettings();
    const QString routed = writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, params, kWaypoints);
    const QString next   = writer.writeLayerTravel(kTarget, kStart, TravelLiftType::kBoth, params);
    return hasSingleLiftAndLower(routed) && routed.count("RETRACT FILAMENT") == 1 &&
           routed.indexOf("RETRACT FILAMENT") < routed.indexOf("TRAVEL LIFT Z") && !next.contains("TRAVEL LIFT Z") &&
           !next.contains("TRAVEL LOWER Z");
}

bool preservesTravelLiftType() {
    auto settings     = writerSettings();
    const auto params = segmentSettings();
    bool passed       = true;
    for (TravelLiftType type : {TravelLiftType::kBoth, TravelLiftType::kLiftUpOnly, TravelLiftType::kLiftLowerOnly,
                                TravelLiftType::kNoLift}) {
        WriterHarness<WolfWriter> writer(settings);
        const QString gcode = writer.writeLayerTravel(kStart, kTarget, type, params, kWaypoints);
        const bool lift     = type == TravelLiftType::kBoth || type == TravelLiftType::kLiftUpOnly;
        const bool lower    = type == TravelLiftType::kBoth || type == TravelLiftType::kLiftLowerOnly;
        passed &= gcode.count("TRAVEL LIFT Z") == (lift ? 1 : 0) && gcode.count("TRAVEL LOWER Z") == (lower ? 1 : 0);
        if (type == TravelLiftType::kNoLift) { passed &= !gcode.contains(" Z3.0000"); }
        else { passed &= gcode.count(" Z3.0000") == 1; }
    }
    return passed;
}

bool keepsDepositionOffForLongRoutedTravel() {
    auto settings = writerSettings();
    WriterHarness<JuggerBotWriter> writer(settings);
    writer.setDepositionActive(true);
    const QString gcode =
        writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, segmentSettings(), kWaypoints);
    return hasSingleLiftAndLower(gcode) && gcode.count("TURN EXTRUDER OFF") == 1 && !gcode.contains("M3");
}

bool cincinnatiDoesNotStartDepositionOnShortLegs() {
    auto settings = writerSettings();
    WriterHarness<CincinnatiWriter> writer(settings);
    const QString gcode =
        writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, segmentSettings(), kWaypoints);
    return hasSingleLiftAndLower(gcode) && !gcode.contains("M3") && !gcode.contains("TURN EXTRUDER ON");
}

bool togglesNozzleAndExtruderOnlyAtRouteEndpoints() {
    auto settings = writerSettings();
    WriterHarness<KraussMaffeiWriter> nozzle_writer(settings);
    const auto params    = segmentSettings();
    const QString nozzle = nozzle_writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, params, kWaypoints);
    WriterHarness<AeroBasicWriter> extruder_writer(settings);
    extruder_writer.setDepositionActive(true);
    const QString extruder =
        extruder_writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, params, kWaypoints);
    return hasSingleLiftAndLower(nozzle) && nozzle.count("CLOSE NOZZLE") == 1 && nozzle.count("OPEN NOZZLE") == 1 &&
           nozzle.indexOf("OPEN NOZZLE") > nozzle.lastIndexOf("TRAVEL LOWER Z") && hasSingleLiftAndLower(extruder) &&
           extruder.count("Extruder Off for travel") == 1 && extruder.count("Extruder On after travel") == 1 &&
           extruder.indexOf("Extruder On after travel") > extruder.lastIndexOf("TRAVEL LOWER Z");
}

bool pausesOnlyOncePerRoute() {
    auto settings = writerSettings();
    WriterHarness<TormachWriter> writer(settings);
    settings->setSetting(PS::Travel::kEnableTravelPause, true);
    settings->setSetting(PS::Travel::kTravelPauseDuration, 0.5 * s);
    const QString gcode =
        writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, segmentSettings(), kWaypoints);
    return hasSingleLiftAndLower(gcode) && gcode.count("DWELL") == 1;
}

bool avoidancePreservesExplicitCentroidPause() {
    auto settings = writerSettings();
    settings->setSetting(PS::Travel::kEnableTravelPause, true);
    settings->setSetting(PS::Travel::kEnableTravelCentroidMove, true);
    settings->setSetting(PS::Travel::kTravelPauseDuration, 0.5 * s);
    for (bool avoid : {false, true}) {
        settings->setSetting(PS::Travel::kAvoidCrossingPerimeters, avoid);
        WriterHarness<TormachWriter> writer(settings);
        auto params         = segmentSettings();
        const QString gcode = writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, params, kWaypoints);
        if (gcode.count("DWELL") != 1 || gcode.count("TRAVEL TO PAUSE LOCATION") != 1) return false;
    }
    return true;
}

bool preservesStandaloneApproachHeightUntilRouteEnds() {
    auto settings = writerSettings();
    settings->setSetting(PRS::Dimensions::kZOffset, 10.0 * mm);
    settings->setSetting(PS::Travel::kLiftHeight, 0.0 * mm);
    settings->setSetting(PS::Travel::kMinTravelForLift, 100.0 * mm);
    CincinnatiWriter writer(GcodeMetaList::MarlinMeta, settings);
    writer.writeInitialSetup(0, 0, 0, 0, 1);
    QString routed;
    writer.writeStandaloneLayer(0, kStart.z(), settings, [&]() {
        routed = writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kLiftUpOnly, segmentSettings(), kWaypoints);
        return routed;
    });
    return routed.count(" Z20.0000") == 1 && routed.count(" Z11.0000") == 1 && routed.count("TRAVEL LOWER Z") == 1 &&
           routed.indexOf(" Z11.0000") > routed.lastIndexOf(" Y2.0000");
}

bool arcBeginsOneBeadAfterRoutedTravel() {
    auto settings = writerSettings();
    WriterHarness<ArcSpecialtiesWriter> writer(settings);
    const QString gcode =
        writer.writeLayerTravel(kStart, kTarget, TravelLiftType::kBoth, segmentSettings(), kWaypoints);
    return gcode.count("TRAVEL LIFT") == 1 && gcode.count("TRAVEL LOWER") == 1 && gcode.count("BEGINNING BEAD:") == 1 &&
           gcode.count("OPTIONAL STOP ROUTINE") == 1;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    bool passed = true;
    passed &= Testing::expect(usesCompleteTravelDistance<MarlinWriter>(),
                              "Marlin routed travel must retract once and retain lift across short legs.");
    passed &= Testing::expect(usesCompleteTravelDistance<RepRapWriter>(),
                              "RepRap routed travel must retract once and retain lift across short legs.");
    passed &= Testing::expect(preservesTravelLiftType(), "Routed travel must preserve the requested lift type.");
    passed &= Testing::expect(keepsDepositionOffForLongRoutedTravel(),
                              "JuggerBot must keep deposition off throughout a long routed travel.");
    passed &= Testing::expect(cincinnatiDoesNotStartDepositionOnShortLegs(),
                              "Cincinnati must evaluate deposition and lift thresholds for the complete route.");
    passed &= Testing::expect(togglesNozzleAndExtruderOnlyAtRouteEndpoints(),
                              "Routed travel must toggle nozzle and extruder only once at route endpoints.");
    passed &= Testing::expect(pausesOnlyOncePerRoute(), "Tormach must pause once for a routed travel.");
    passed &= Testing::expect(avoidancePreservesExplicitCentroidPause(),
                              "Perimeter avoidance must preserve the explicitly configured Tormach centroid pause.");
    passed &= Testing::expect(preservesStandaloneApproachHeightUntilRouteEnds(),
                              "Standalone routed travel must retain its approach height until the final lower.");
    passed &= Testing::expect(arcBeginsOneBeadAfterRoutedTravel(),
                              "ARC routed travel must begin one bead and issue one stop and lower.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
