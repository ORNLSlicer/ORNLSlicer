#include "step/layer/regions/infill.h"

#include "geometry/path_modifier.h"
#include "geometry/pattern_generator.h"
#include "geometry/segments/line.h"
#include "optimizers/polyline_order_optimizer.h"
#include "utilities/enums.h"

namespace ORNL {
Infill::Infill(const QSharedPointer<SettingsBase>& sb, const int index,
               const QVector<SettingsPolygon>& settings_polygons, const SingleExternalGridInfo& gridInfo)
    : RegionBase(sb, index, settings_polygons, gridInfo) {
    // NOP
}

QString Infill::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kInfill);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kInfill);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kInfill);
    }
    gcode += writer->writeAfterRegion(RegionType::kInfill);
    return gcode;
}

void Infill::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_layer_num = layer_num;

    m_paths.clear();

    // keep around unaltered m_geometry for later connections for travels
    m_geometry_copy = m_geometry.offset(m_sb->setting<Distance>(PS::Infill::kOverlap));

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kInfillNum));

    QVector<SettingsPolygon> settings_holes_to_fill;

    // Every settings polygon will become a hole in the base polygon(s)
    for (auto settings_poly : m_settings_polygons) {
        if (!settingsSame(m_sb, settings_poly.getSettings())) {
            settings_holes_to_fill.push_back(settings_poly);
            m_geometry_copy -= settings_poly;
        }
    }

    // Fill with base geometry and default settings
    fillGeometry(m_geometry_copy, m_sb);

    // Fill any regions with different settings
    for (auto settings_polygon : settings_holes_to_fill) {
        if (settings_polygon.getSettings()->setting<bool>(PS::Infill::kEnable)) {
            PolygonList geometry;
            geometry += (m_geometry & settings_polygon);
            QSharedPointer<SettingsBase> region_settings = QSharedPointer<SettingsBase>::create(*m_sb);
            region_settings->setSetting(PS::Infill::kLineSpacing,
                                        settings_polygon.getSettings()->setting<Distance>(PS::Infill::kLineSpacing));

            fillGeometry(geometry, region_settings);
        }
    }

    m_geometry.clear();
}

void Infill::fillGeometry(PolygonList geometry, const QSharedPointer<SettingsBase>& sb) {
    InfillPatterns default_infill_pattern = static_cast<InfillPatterns>(sb->setting<int>(PS::Infill::kPattern));
    Distance default_line_spacing = sb->setting<Distance>(PS::Infill::kLineSpacing);
    Distance default_bead_width = sb->setting<Distance>(PS::Infill::kBeadWidth);

    // kAngle in the setting has already been updated for each layer
    Angle default_angle = sb->setting<Angle>(PS::Infill::kAngle);

    PolygonList adjustedGeometry = geometry.offset(-default_bead_width / 2);

    Point min, max;
    bool default_global_printer_area = sb->setting<bool>(PS::Infill::kBasedOnPrinter);
    if (default_global_printer_area) {
        //! Get the bounding box for the printer
        min = Point(sb->setting<Distance>(PRS::Dimensions::kXMin), sb->setting<Distance>(PRS::Dimensions::kYMin));
        max = Point(sb->setting<Distance>(PRS::Dimensions::kXMax), sb->setting<Distance>(PRS::Dimensions::kYMax));
    }

    if (!sb->setting<bool>(PS::Infill::kManualLineSpacing)) {
        double density = sb->setting<double>(PS::Infill::kDensity) / 100.0;
        default_line_spacing = default_bead_width / density;
    }

    switch (default_infill_pattern) {
        case InfillPatterns::kLines:
            m_computed_geometry.append(PatternGenerator::GenerateLines(
                adjustedGeometry, default_line_spacing, default_angle, default_global_printer_area, min, max));
            break;
        case InfillPatterns::kGrid:
            m_computed_geometry.append(PatternGenerator::GenerateGrid(
                adjustedGeometry, default_line_spacing, default_angle, default_global_printer_area, min, max));
            break;
        case InfillPatterns::kConcentric:
        case InfillPatterns::kInsideOutConcentric:
            m_computed_geometry.append(
                PatternGenerator::GenerateConcentric(geometry, default_bead_width, default_line_spacing));
            break;
        case InfillPatterns::kTriangles:
            m_computed_geometry.append(PatternGenerator::GenerateTriangles(
                adjustedGeometry, default_line_spacing, default_angle, default_global_printer_area, min, max));
            break;
        case InfillPatterns::kHexagonsAndTriangles:
            m_computed_geometry.append(PatternGenerator::GenerateHexagonsAndTriangles(
                adjustedGeometry, default_line_spacing, default_angle, default_global_printer_area, min, max));
            break;
        case InfillPatterns::kHoneycomb:
            m_computed_geometry.append(PatternGenerator::GenerateHoneyComb(adjustedGeometry, default_bead_width,
                                                                           default_line_spacing, default_angle,
                                                                           default_global_printer_area, min, max));
            break;
        case InfillPatterns::kRadialHatch:
            //            Point m_center =
            //            Point(m_sb->setting<double>(PRS::Dimensions::kXOffset),
            //            m_sb->setting<double>(PRS::Dimensions::kYOffset)); Point diff = max -
            //            min; Distance radius; if(diff.x() > diff.y())
            //                radius = diff.x() / 2.0 + 10;
            //            else
            //                radius = diff.y() / 2.0 + 10;

            //            QVector<QVector<Polyline>> result = PatternGenerator::GenerateRadialHatch(adjustedGeometry,
            //            default_line_spacing, default_angle,
            //                                                                            sb->setting<int>(PS::Infill::kSectorCount),
            //                                                                            m_center, radius);
            //            QVector<Polyline> final;
            //            for(QVector<Polyline> sector : result)
            //            {
            //                final += sector;
            //            }
            //            m_computed_geometry.append(final);
            break;
    }

    if (default_infill_pattern == InfillPatterns::kInsideOutConcentric)
        this->reversePaths();
}

void Infill::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                      QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) {
    PolylineOrderOptimizer poo(current_location, layerNumber);

    PathOrderOptimization pathOrderOptimization =
        static_cast<PathOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPathOrder));
    if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPathXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomPathYLocation));

        poo.setStartOverride(startOverride);
    }

    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPointOrder));

    if (pointOrderOptimization == PointOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPointXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomPointYLocation));

        poo.setStartPointOverride(startOverride);
    }
    poo.setInfillParameters(static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)), m_geometry_copy,
                            getSb()->setting<Distance>(PS::Infill::kMinPathLength),
                            getSb()->setting<Distance>(PS::Travel::kMinLength),
                            getSb()->setting<bool>(PS::Infill::kLinesPartitionedLinking));

    poo.setPointParameters(pointOrderOptimization, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                           getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                           getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                           getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius));

    for (QVector<Polyline> lines : m_computed_geometry) {
        poo.setGeometryToEvaluate(
            lines, RegionType::kInfill,
            static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

        QVector<Polyline> previouslyLinkedLines;
        while (poo.getCurrentPolylineCount() > 0) {
            Polyline result = poo.linkNextPolyline(previouslyLinkedLines);
            if (result.size() > 0) {
                Path newPath = createPath(result);
                if (newPath.size() > 0) {
                    calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3),
                                       innerMostClosedContour);
                    PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                          m_sb->setting<Velocity>(PS::Travel::kSpeed));
                    current_location = newPath.back()->end();
                    previouslyLinkedLines.push_back(result);
                    m_paths.push_back(newPath);
                }
            }
        }
    }
}

Path Infill::createPath(Polyline line) {

    Distance width = m_sb->setting<Distance>(PS::Infill::kBeadWidth);
    Distance height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity speed = m_sb->setting<Velocity>(PS::Infill::kSpeed);
    Acceleration acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kInfill);
    AngularVelocity extruder_speed = m_sb->setting<AngularVelocity>(PS::Infill::kExtruderSpeed);
    int material_number = m_sb->setting<int>(MS::MultiMaterial::kInfillNum);

    Path newPath;
    for (int j = 0, polyEnd = line.size() - 1; j < polyEnd; ++j) {
        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line[j], line[j + 1]);

        segment->getSb()->setSetting(SS::kWidth, width);
        segment->getSb()->setSetting(SS::kHeight, height);
        segment->getSb()->setSetting(SS::kSpeed, speed);
        segment->getSb()->setSetting(SS::kAccel, acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
        segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kInfill);

        newPath.append(segment);
    }

    //! Creates closing segment if infill pattern is concentric
    if (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)) == InfillPatterns::kConcentric ||
        static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)) == InfillPatterns::kInsideOutConcentric) {
        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line.last(), line.first());

        segment->getSb()->setSetting(SS::kWidth, width);
        segment->getSb()->setSetting(SS::kHeight, height);
        segment->getSb()->setSetting(SS::kSpeed, speed);
        segment->getSb()->setSetting(SS::kAccel, acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
        segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kInfill);

        newPath.append(segment);
    }

    return newPath;
}

void Infill::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    if (m_sb->setting<bool>(ES::Ramping::kTrajectoryAngleEnabled)) {
        PathModifierGenerator::GenerateTrajectorySlowdown(path, m_sb);
    }

    if (m_sb->setting<bool>(MS::Slowdown::kInfillEnable)) {
        PathModifierGenerator::GenerateSlowdown(path, m_sb->setting<Distance>(MS::Slowdown::kInfillDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kInfillLiftDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kInfillCutoffDistance),
                                                m_sb->setting<Velocity>(MS::Slowdown::kInfillSpeed),
                                                m_sb->setting<AngularVelocity>(MS::Slowdown::kInfillExtruderSpeed),
                                                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                                                m_sb->setting<double>(MS::Slowdown::kSlowDownAreaModifier));
    }
    if (m_sb->setting<bool>(MS::TipWipe::kInfillEnable)) {
        // If angled slicing, force tip wipe to be reverse
        if (m_sb->setting<float>(PS::SlicingVector::kSlicingVectorX) != 0 ||
            m_sb->setting<float>(PS::SlicingVector::kSlicingVectorY) != 0 ||
            m_sb->setting<float>(PS::SlicingVector::kSlicingVectorZ) != 1) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInfillDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kInfillSpeed), m_sb->setting<Angle>(MS::TipWipe::kInfillAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kInfillExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kInfillLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kInfillCutoffDistance));
        }
        // if Forward OR (if Optimal AND (Perimeter OR Inset)) OR (if Optimal AND (Concentric or Inside Out Concentric))
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInfillDirection)) ==
                     TipWipeDirection::kForward ||
                 (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInfillDirection)) ==
                      TipWipeDirection::kOptimal &&
                  (m_sb->setting<int>(PS::Perimeter::kEnable) || m_sb->setting<int>(PS::Inset::kEnable))) ||
                 (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInfillDirection)) ==
                      TipWipeDirection::kOptimal &&
                  (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)) ==
                       InfillPatterns::kConcentric ||
                   static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)) ==
                       InfillPatterns::kInsideOutConcentric))) {
            if (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)) == InfillPatterns::kConcentric ||
                static_cast<InfillPatterns>(m_sb->setting<int>(PS::Infill::kPattern)) ==
                    InfillPatterns::kInsideOutConcentric)
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInfillDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kInfillSpeed), m_sb->setting<Angle>(MS::TipWipe::kInfillAngle),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kInfillExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kInfillLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kInfillCutoffDistance));
            else if (m_sb->setting<int>(PS::Perimeter::kEnable) || m_sb->setting<int>(PS::Inset::kEnable))
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInfillDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kInfillSpeed), innerMostClosedContour,
                    m_sb->setting<Angle>(MS::TipWipe::kInfillAngle),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kInfillExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kInfillLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kInfillCutoffDistance));
            else
                PathModifierGenerator::GenerateForwardTipWipeOpenLoop(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInfillDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kInfillSpeed),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kInfillExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kInfillLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kInfillCutoffDistance));
        }
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInfillDirection)) ==
                 TipWipeDirection::kAngled) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kAngledTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInfillDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kInfillSpeed), m_sb->setting<Angle>(MS::TipWipe::kInfillAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kInfillExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kInfillLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kInfillCutoffDistance));
        }
        else
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInfillDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kInfillSpeed), m_sb->setting<Angle>(MS::TipWipe::kInfillAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kInfillExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kInfillLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kInfillCutoffDistance));
    }
    if (m_sb->setting<bool>(MS::SpiralLift::kInfillEnable)) {
        // Prevent spiral lifts during angled slicing to avoid collisions
        if (m_sb->setting<float>(PS::SlicingVector::kSlicingVectorX) == 0 &&
            m_sb->setting<float>(PS::SlicingVector::kSlicingVectorY) == 0 &&
            m_sb->setting<float>(PS::SlicingVector::kSlicingVectorZ) == 1) {
            PathModifierGenerator::GenerateSpiralLift(path, m_sb->setting<Distance>(MS::SpiralLift::kLiftRadius),
                                                      m_sb->setting<Distance>(MS::SpiralLift::kLiftHeight),
                                                      m_sb->setting<int>(MS::SpiralLift::kLiftPoints),
                                                      m_sb->setting<Velocity>(MS::SpiralLift::kLiftSpeed), supportsG3);
        }
    }
    if (m_sb->setting<bool>(MS::Startup::kInfillEnable)) {
        if (m_sb->setting<bool>(MS::Startup::kInfillRampUpEnable)) {
            PathModifierGenerator::GenerateInitialStartupWithRampUp(
                path, m_sb->setting<Distance>(MS::Startup::kInfillDistance),
                m_sb->setting<Velocity>(MS::Startup::kInfillSpeed), m_sb->setting<Velocity>(PS::Infill::kSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kInfillExtruderSpeed),
                m_sb->setting<AngularVelocity>(PS::Infill::kExtruderSpeed),
                m_sb->setting<int>(MS::Startup::kInfillSteps),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
        else {
            PathModifierGenerator::GenerateInitialStartup(
                path, m_sb->setting<Distance>(MS::Startup::kInfillDistance),
                m_sb->setting<Velocity>(MS::Startup::kInfillSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kInfillExtruderSpeed),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
    }
}

bool Infill::settingsSame(QSharedPointer<SettingsBase> a, QSharedPointer<SettingsBase> b) {
    return static_cast<InfillPatterns>(a->setting<int>(PS::Infill::kPattern)) ==
               static_cast<InfillPatterns>(b->setting<int>(PS::Infill::kPattern)) &&
           qFuzzyCompare(a->setting<Distance>(PS::Infill::kLineSpacing)(),
                         b->setting<Distance>(PS::Infill::kLineSpacing)()) &&
           qFuzzyCompare(a->setting<Distance>(PS::Infill::kBeadWidth)(),
                         b->setting<Distance>(PS::Infill::kBeadWidth)()) &&
           a->setting<int>(PS::Infill::kSectorCount) == b->setting<int>(PS::Infill::kSectorCount) &&
           a->setting<bool>(PS::Infill::kBasedOnPrinter) == b->setting<bool>(PS::Infill::kBasedOnPrinter) &&
           qFuzzyCompare(a->setting<Angle>(PS::Infill::kAngle)(), b->setting<Angle>(PS::Infill::kAngle)()) &&
           a->setting<bool>(PS::Infill::kEnable) == b->setting<bool>(PS::Infill::kEnable);
}

void Infill::setLayerCount(uint layer_count) { m_layer_count = layer_count - 1; }
} // namespace ORNL
