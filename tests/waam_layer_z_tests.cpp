#include <QCoreApplication>
#include <QSharedPointer>
#include <QStringList>
#include <QVector3D>
#include <QVector>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/wolf_writer.h"
#include "geometry/path.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polyline.h"
#include "geometry/segments/line.h"
#include "step/layer/island/island_base.h"
#include "step/layer/layer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
class TestRegion : public ORNL::RegionBase {
   public:
    explicit TestRegion(const QSharedPointer<ORNL::SettingsBase>& settings)
        : RegionBase(settings, QVector<ORNL::SettingsPolygon>(), ORNL::RegionType::kPerimeter) {}

    QString writeGCode(QSharedPointer<ORNL::WriterBase>) override {
        return {};
    }
    void compute(uint) override {}
    void optimize(int, ORNL::Point&, bool&) override {}
    ORNL::Path createPath(ORNL::Polyline) override {
        return {};
    }
    void calculateModifiers(ORNL::Path&, bool) override {}

    void addPath(const ORNL::Path& path) {
        appendPath(path);
    }
};

class TestIsland : public ORNL::IslandBase {
   public:
    explicit TestIsland(const QSharedPointer<ORNL::SettingsBase>& settings)
        : IslandBase(ORNL::PolygonList(), settings, QVector<ORNL::SettingsPolygon>()) {
        m_island_type = ORNL::IslandType::kPolymer;
    }

    void optimize(int, ORNL::Point&, QVector<QSharedPointer<ORNL::RegionBase>>&) override {}
};

bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1.0e-4;
}

QSharedPointer<ORNL::SettingsBase> layerSettings(ORNL::MachineType machine_type, ORNL::Distance layer_height) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::MachineSetup::kMachineType, machine_type);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, layer_height);
    settings->setSetting(ORNL::PS::SpecialModes::kEnableSpiralize, false);
    settings->setSetting(ORNL::PRS::Dimensions::kXOffset, 0.0);
    settings->setSetting(ORNL::PRS::Dimensions::kYOffset, 0.0);
    return settings;
}

QSharedPointer<ORNL::LineSegment> reorientedLine(ORNL::MachineType machine_type, ORNL::Distance layer_height,
                                                 const ORNL::Point& slicing_plane_center,
                                                 const QVector3D& slicing_normal) {
    QSharedPointer<ORNL::SettingsBase> settings = layerSettings(machine_type, layer_height);
    const ORNL::Point flattened_start(slicing_plane_center.x(), slicing_plane_center.y(), 0.0f);
    QSharedPointer<ORNL::LineSegment> segment = QSharedPointer<ORNL::LineSegment>::create(
        flattened_start, flattened_start + ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm));

    ORNL::Path path;
    path.append(segment);

    QSharedPointer<TestRegion> region = QSharedPointer<TestRegion>::create(settings);
    region->addPath(path);

    QSharedPointer<TestIsland> island = QSharedPointer<TestIsland>::create(settings);
    island->addRegion(region);

    ORNL::Layer layer(1, settings);
    layer.addIsland(ORNL::IslandType::kPolymer, island);
    layer.setOrientation(ORNL::Plane(slicing_plane_center, slicing_normal), slicing_plane_center);
    layer.reorient();

    return segment;
}

bool wireArcUsesSurfaceReferencedSequence() {
    const QSharedPointer<ORNL::LineSegment> first_layer =
        reorientedLine(ORNL::MachineType::kWire_Arc, 2.8 * ORNL::mm,
                       ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 1.4 * ORNL::mm), QVector3D(0.0f, 0.0f, 1.0f));
    const QSharedPointer<ORNL::LineSegment> second_layer =
        reorientedLine(ORNL::MachineType::kWire_Arc, 2.8 * ORNL::mm,
                       ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 4.2 * ORNL::mm), QVector3D(0.0f, 0.0f, 1.0f));
    const QSharedPointer<ORNL::LineSegment> third_layer =
        reorientedLine(ORNL::MachineType::kWire_Arc, 2.8 * ORNL::mm,
                       ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 7.0 * ORNL::mm), QVector3D(0.0f, 0.0f, 1.0f));

    return near(ORNL::Distance(first_layer->start().z()).to(ORNL::mm), 0.0) &&
           near(ORNL::Distance(second_layer->start().z()).to(ORNL::mm), 2.8) &&
           near(ORNL::Distance(third_layer->start().z()).to(ORNL::mm), 5.6);
}

bool nonWireArcKeepsTopOfLayerReference() {
    const QSharedPointer<ORNL::LineSegment> segment =
        reorientedLine(ORNL::MachineType::kPellet, 2.8 * ORNL::mm,
                       ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 1.4 * ORNL::mm), QVector3D(0.0f, 0.0f, 1.0f));

    return near(ORNL::Distance(segment->start().z()).to(ORNL::mm), 2.8);
}

bool wireArcUsesEachVariableLayerHeight() {
    const QSharedPointer<ORNL::LineSegment> segment =
        reorientedLine(ORNL::MachineType::kWire_Arc, 3.2 * ORNL::mm,
                       ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 4.4 * ORNL::mm), QVector3D(0.0f, 0.0f, 1.0f));

    return near(ORNL::Distance(segment->start().z()).to(ORNL::mm), 2.8);
}

bool wireArcAdjustsAlongSlicingNormal() {
    QVector3D slicing_normal(0.0f, 1.0f, 1.0f);
    slicing_normal.normalize();
    const QVector3D lower_surface(5.0 * ORNL::mm(), 7.0 * ORNL::mm(), 11.0 * ORNL::mm());
    const QVector3D slicing_plane_center = lower_surface + (slicing_normal * (2.8 * ORNL::mm)() / 2.0f);

    const QSharedPointer<ORNL::LineSegment> segment = reorientedLine(
        ORNL::MachineType::kWire_Arc, 2.8 * ORNL::mm, ORNL::Point::fromQVector3D(slicing_plane_center), slicing_normal);

    const ORNL::Point start = segment->start();
    return near(start.x(), lower_surface.x()) && near(start.y(), lower_surface.y()) &&
           near(start.z(), lower_surface.z());
}

QSharedPointer<ORNL::SettingsBase> wolfSettings() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::Dimensions::kZOffset, 1.5 * ORNL::mm);
    settings->setSetting(ORNL::PRS::GCode::kStartCode, QString());
    settings->setSetting(ORNL::PRS::MachineSetup::kMachineType, ORNL::MachineType::kWire_Arc);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 2.8 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 12.7 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 1200.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 0.0f);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalY, 0.0f);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalZ, 1.0f);
    return settings;
}

bool wolfFirstApproachUsesTargetLiftAndOneOffset() {
    QSharedPointer<ORNL::SettingsBase> settings = wolfSettings();
    ORNL::WolfWriter writer(ORNL::GcodeMetaList::WolfMeta, settings);
    writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);

    const QString travel    = writer.writeTravel(ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::Point(10.0 * ORNL::mm, 20.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::TravelLiftType::kBoth, segment_settings);
    const QStringList lines = travel.split('\n', Qt::SkipEmptyParts);

    return lines.size() == 2 && lines[0].contains(" Z14.2000") && lines[0].contains(";TRAVEL") &&
           lines[1].contains(" Z1.5000") && lines[1].contains(";TRAVEL LOWER Z");
}

bool wolfShortFirstTravelDoesNotLift() {
    QSharedPointer<ORNL::SettingsBase> settings = wolfSettings();
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 25.0 * ORNL::mm);
    ORNL::WolfWriter writer(ORNL::GcodeMetaList::WolfMeta, settings);
    writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);

    const QString travel    = writer.writeTravel(ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::Point(10.0 * ORNL::mm, 20.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::TravelLiftType::kBoth, segment_settings);
    const QStringList lines = travel.split('\n', Qt::SkipEmptyParts);

    return lines.size() == 1 && lines[0].contains(" Z1.5000") && lines[0].contains(";TRAVEL");
}

bool wolfFirstTravelHonorsNoLift() {
    QSharedPointer<ORNL::SettingsBase> settings = wolfSettings();
    ORNL::WolfWriter writer(ORNL::GcodeMetaList::WolfMeta, settings);
    writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);

    const QString travel    = writer.writeTravel(ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::Point(10.0 * ORNL::mm, 20.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::TravelLiftType::kNoLift, segment_settings);
    const QStringList lines = travel.split('\n', Qt::SkipEmptyParts);

    return lines.size() == 1 && lines[0].contains(" Z1.5000") && lines[0].contains(";TRAVEL");
}

bool wolfNormalizesNegativeZero() {
    QSharedPointer<ORNL::SettingsBase> settings = wolfSettings();
    settings->setSetting(ORNL::PRS::Dimensions::kZOffset, 0.0 * ORNL::mm);
    ORNL::WolfWriter writer(ORNL::GcodeMetaList::WolfMeta, settings);
    writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);

    const QString travel    = writer.writeTravel(ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::Point(10.0 * ORNL::mm, 20.0 * ORNL::mm, -0.00001 * ORNL::mm),
                                                 ORNL::TravelLiftType::kBoth, segment_settings);
    const QStringList lines = travel.split('\n', Qt::SkipEmptyParts);

    return lines.size() == 2 && lines[1].contains(" Z0.0000") && !lines[1].contains(" Z-0.0000");
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    bool passed = true;
    passed &= expect(wireArcUsesSurfaceReferencedSequence(),
                     "Wire Arc layers did not follow the expected 0.0, 2.8, 5.6 mm surface sequence.");
    passed &= expect(nonWireArcKeepsTopOfLayerReference(),
                     "Non-Wire Arc layer no longer used the existing top-of-layer reference.");
    passed &= expect(wireArcUsesEachVariableLayerHeight(),
                     "Wire Arc variable-height layer did not start on the preceding layer surface.");
    passed &= expect(wireArcAdjustsAlongSlicingNormal(),
                     "Wire Arc layer datum adjustment did not follow the slicing-plane normal.");
    passed &= expect(wolfFirstApproachUsesTargetLiftAndOneOffset(),
                     "Wolf first approach did not apply target lift and Z offset exactly once.");
    passed &= expect(wolfShortFirstTravelDoesNotLift(),
                     "Wolf lifted a first travel shorter than the configured minimum lift distance.");
    passed &= expect(wolfFirstTravelHonorsNoLift(), "Wolf lifted a first travel marked as no-lift.");
    passed &= expect(wolfNormalizesNegativeZero(), "Wolf emitted a negative-zero Z coordinate.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
