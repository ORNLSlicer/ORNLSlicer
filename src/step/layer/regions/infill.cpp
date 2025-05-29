// Main Module
#include "step/layer/regions/infill.h"

#include "geometry/path_modifier.h"
#include "geometry/pattern_generator.h"
#include "geometry/segments/line.h"
#include "optimizers/polyline_order_optimizer.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

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
    m_geometry_copy = m_geometry.offset(m_sb->setting<Distance>(Constants::ProfileSettings::Infill::kOverlap));

    setMaterialNumber(m_sb->setting<int>(Constants::MaterialSettings::MultiMaterial::kInfillNum));

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
        if (settings_polygon.getSettings()->setting<bool>(Constants::ProfileSettings::Infill::kEnable)) {
            PolygonList geometry;
            geometry += (m_geometry & settings_polygon);
            QSharedPointer<SettingsBase> region_settings = QSharedPointer<SettingsBase>::create(*m_sb);
            region_settings->setSetting(Constants::ProfileSettings::Infill::kLineSpacing,
                                        settings_polygon.getSettings()->setting<Distance>(
                                            Constants::ProfileSettings::Infill::kLineSpacing));

            fillGeometry(geometry, region_settings);
        }
    }

    m_geometry.clear();
}

void Infill::fillGeometry(PolygonList geometry, const QSharedPointer<SettingsBase>& sb) {
    InfillPatterns default_infill_pattern =
        static_cast<InfillPatterns>(sb->setting<int>(Constants::ProfileSettings::Infill::kPattern));
    Distance default_line_spacing = sb->setting<Distance>(Constants::ProfileSettings::Infill::kLineSpacing);
    Distance default_bead_width = sb->setting<Distance>(Constants::ProfileSettings::Infill::kBeadWidth);

    // kAngle in the setting has already been updated for each layer
    Angle default_angle = sb->setting<Angle>(Constants::ProfileSettings::Infill::kAngle);

    PolygonList adjustedGeometry = geometry.offset(-default_bead_width / 2);

    Point min, max;
    bool default_global_printer_area = sb->setting<bool>(Constants::ProfileSettings::Infill::kBasedOnPrinter);
    if (default_global_printer_area) {
        //! Get the bounding box for the printer
        min = Point(sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kXMin),
                    sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kYMin));
        max = Point(sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kXMax),
                    sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kYMax));
    }

    if (!sb->setting<bool>(Constants::ProfileSettings::Infill::kManualLineSpacing)) {
        double density = sb->setting<double>(Constants::ProfileSettings::Infill::kDensity) / 100.0;
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
            //            Point(m_sb->setting<double>(Constants::PrinterSettings::Dimensions::kXOffset),
            //            m_sb->setting<double>(Constants::PrinterSettings::Dimensions::kYOffset)); Point diff = max -
            //            min; Distance radius; if(diff.x() > diff.y())
            //                radius = diff.x() / 2.0 + 10;
            //            else
            //                radius = diff.y() / 2.0 + 10;

            //            QVector<QVector<Polyline>> result = PatternGenerator::GenerateRadialHatch(adjustedGeometry,
            //            default_line_spacing, default_angle,
            //                                                                            sb->setting<int>(Constants::ProfileSettings::Infill::kSectorCount),
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

    PathOrderOptimization pathOrderOptimization = static_cast<PathOrderOptimization>(
        this->getSb()->setting<int>(Constants::ProfileSettings::Optimizations::kPathOrder));
    if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPathXLocation),
                            getSb()->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPathYLocation));

        poo.setStartOverride(startOverride);
    }

    PointOrderOptimization pointOrderOptimization = static_cast<PointOrderOptimization>(
        this->getSb()->setting<int>(Constants::ProfileSettings::Optimizations::kPointOrder));

    if (pointOrderOptimization == PointOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointXLocation),
                            getSb()->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointYLocation));

        poo.setStartPointOverride(startOverride);
    }
    poo.setInfillParameters(
        static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)), m_geometry_copy,
        getSb()->setting<Distance>(Constants::ProfileSettings::Infill::kMinPathLength),
        getSb()->setting<Distance>(Constants::ProfileSettings::Travel::kMinLength));

    poo.setPointParameters(
        pointOrderOptimization, getSb()->setting<bool>(Constants::ProfileSettings::Optimizations::kMinDistanceEnabled),
        getSb()->setting<Distance>(Constants::ProfileSettings::Optimizations::kMinDistanceThreshold),
        getSb()->setting<Distance>(Constants::ProfileSettings::Optimizations::kConsecutiveDistanceThreshold),
        getSb()->setting<bool>(Constants::ProfileSettings::Optimizations::kLocalRandomnessEnable),
        getSb()->setting<Distance>(Constants::ProfileSettings::Optimizations::kLocalRandomnessRadius));

    for (QVector<Polyline> lines : m_computed_geometry) {
        poo.setGeometryToEvaluate(lines, RegionType::kInfill,
                                  static_cast<PathOrderOptimization>(
                                      m_sb->setting<int>(Constants::ProfileSettings::Optimizations::kPathOrder)));

        QVector<Polyline> previouslyLinkedLines;
        while (poo.getCurrentPolylineCount() > 0) {
            Polyline result = poo.linkNextPolyline(previouslyLinkedLines);
            if (result.size() > 0) {
                Path newPath = createPath(result);
                if (newPath.size() > 0) {
                    calculateModifiers(newPath,
                                       m_sb->setting<bool>(Constants::PrinterSettings::MachineSetup::kSupportG3),
                                       innerMostClosedContour);
                    PathModifierGenerator::GenerateTravel(
                        newPath, current_location, m_sb->setting<Velocity>(Constants::ProfileSettings::Travel::kSpeed));
                    current_location = newPath.back()->end();
                    previouslyLinkedLines.push_back(result);
                    m_paths.push_back(newPath);
                }
            }
        }
    }
}

Path Infill::createPath(Polyline line) {

    Distance width = m_sb->setting<Distance>(Constants::ProfileSettings::Infill::kBeadWidth);
    Distance height = m_sb->setting<Distance>(Constants::ProfileSettings::Layer::kLayerHeight);
    Velocity speed = m_sb->setting<Velocity>(Constants::ProfileSettings::Infill::kSpeed);
    Acceleration acceleration = m_sb->setting<Acceleration>(Constants::PrinterSettings::Acceleration::kInfill);
    AngularVelocity extruder_speed = m_sb->setting<AngularVelocity>(Constants::ProfileSettings::Infill::kExtruderSpeed);
    int material_number = m_sb->setting<int>(Constants::MaterialSettings::MultiMaterial::kInfillNum);

    Path newPath;
    for (int j = 0, polyEnd = line.size() - 1; j < polyEnd; ++j) {
        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line[j], line[j + 1]);

        segment->getSb()->setSetting(Constants::SegmentSettings::kWidth, width);
        segment->getSb()->setSetting(Constants::SegmentSettings::kHeight, height);
        segment->getSb()->setSetting(Constants::SegmentSettings::kSpeed, speed);
        segment->getSb()->setSetting(Constants::SegmentSettings::kAccel, acceleration);
        segment->getSb()->setSetting(Constants::SegmentSettings::kExtruderSpeed, extruder_speed);
        segment->getSb()->setSetting(Constants::SegmentSettings::kMaterialNumber, material_number);
        segment->getSb()->setSetting(Constants::SegmentSettings::kRegionType, RegionType::kInfill);

        newPath.append(segment);
    }

    //! Creates closing segment if infill pattern is concentric
    if (static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
            InfillPatterns::kConcentric ||
        static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
            InfillPatterns::kInsideOutConcentric) {
        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line.last(), line.first());

        segment->getSb()->setSetting(Constants::SegmentSettings::kWidth, width);
        segment->getSb()->setSetting(Constants::SegmentSettings::kHeight, height);
        segment->getSb()->setSetting(Constants::SegmentSettings::kSpeed, speed);
        segment->getSb()->setSetting(Constants::SegmentSettings::kAccel, acceleration);
        segment->getSb()->setSetting(Constants::SegmentSettings::kExtruderSpeed, extruder_speed);
        segment->getSb()->setSetting(Constants::SegmentSettings::kMaterialNumber, material_number);
        segment->getSb()->setSetting(Constants::SegmentSettings::kRegionType, RegionType::kInfill);

        newPath.append(segment);
    }

    return newPath;
}

void Infill::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    if (m_sb->setting<bool>(Constants::ExperimentalSettings::Ramping::kTrajectoryAngleEnabled)) {
        PathModifierGenerator::GenerateTrajectorySlowdown(path, m_sb);
    }

    if (m_sb->setting<bool>(Constants::MaterialSettings::Slowdown::kInfillEnable)) {
        PathModifierGenerator::GenerateSlowdown(
            path, m_sb->setting<Distance>(Constants::MaterialSettings::Slowdown::kInfillDistance),
            m_sb->setting<Distance>(Constants::MaterialSettings::Slowdown::kInfillLiftDistance),
            m_sb->setting<Distance>(Constants::MaterialSettings::Slowdown::kInfillCutoffDistance),
            m_sb->setting<Velocity>(Constants::MaterialSettings::Slowdown::kInfillSpeed),
            m_sb->setting<AngularVelocity>(Constants::MaterialSettings::Slowdown::kInfillExtruderSpeed),
            m_sb->setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableWidthHeight),
            m_sb->setting<double>(Constants::MaterialSettings::Slowdown::kSlowDownAreaModifier));
    }
    if (m_sb->setting<bool>(Constants::MaterialSettings::TipWipe::kInfillEnable)) {
        // If angled slicing, force tip wipe to be reverse
        if (m_sb->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorX) != 0 ||
            m_sb->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorY) != 0 ||
            m_sb->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorZ) != 1) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe,
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillDistance),
                m_sb->setting<Velocity>(Constants::MaterialSettings::TipWipe::kInfillSpeed),
                m_sb->setting<Angle>(Constants::MaterialSettings::TipWipe::kInfillAngle),
                m_sb->setting<AngularVelocity>(Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed),
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillLiftHeight),
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillCutoffDistance));
        }
        // if Forward OR (if Optimal AND (Perimeter OR Inset)) OR (if Optimal AND (Concentric or Inside Out Concentric))
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(
                     Constants::MaterialSettings::TipWipe::kInfillDirection)) == TipWipeDirection::kForward ||
                 (static_cast<TipWipeDirection>(m_sb->setting<int>(
                      Constants::MaterialSettings::TipWipe::kInfillDirection)) == TipWipeDirection::kOptimal &&
                  (m_sb->setting<int>(Constants::ProfileSettings::Perimeter::kEnable) ||
                   m_sb->setting<int>(Constants::ProfileSettings::Inset::kEnable))) ||
                 (static_cast<TipWipeDirection>(m_sb->setting<int>(
                      Constants::MaterialSettings::TipWipe::kInfillDirection)) == TipWipeDirection::kOptimal &&
                  (static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
                       InfillPatterns::kConcentric ||
                   static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
                       InfillPatterns::kInsideOutConcentric))) {
            if (static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
                    InfillPatterns::kConcentric ||
                static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
                    InfillPatterns::kInsideOutConcentric)
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe,
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillDistance),
                    m_sb->setting<Velocity>(Constants::MaterialSettings::TipWipe::kInfillSpeed),
                    m_sb->setting<Angle>(Constants::MaterialSettings::TipWipe::kInfillAngle),
                    m_sb->setting<AngularVelocity>(Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed),
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillLiftHeight),
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillCutoffDistance));
            else if (m_sb->setting<int>(Constants::ProfileSettings::Perimeter::kEnable) ||
                     m_sb->setting<int>(Constants::ProfileSettings::Inset::kEnable))
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe,
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillDistance),
                    m_sb->setting<Velocity>(Constants::MaterialSettings::TipWipe::kInfillSpeed), innerMostClosedContour,
                    m_sb->setting<Angle>(Constants::MaterialSettings::TipWipe::kInfillAngle),
                    m_sb->setting<AngularVelocity>(Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed),
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillLiftHeight),
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillCutoffDistance));
            else
                PathModifierGenerator::GenerateForwardTipWipeOpenLoop(
                    path, PathModifiers::kForwardTipWipe,
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillDistance),
                    m_sb->setting<Velocity>(Constants::MaterialSettings::TipWipe::kInfillSpeed),
                    m_sb->setting<AngularVelocity>(Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed),
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillLiftHeight),
                    m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillCutoffDistance));
        }
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(
                     Constants::MaterialSettings::TipWipe::kInfillDirection)) == TipWipeDirection::kAngled) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kAngledTipWipe,
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillDistance),
                m_sb->setting<Velocity>(Constants::MaterialSettings::TipWipe::kInfillSpeed),
                m_sb->setting<Angle>(Constants::MaterialSettings::TipWipe::kInfillAngle),
                m_sb->setting<AngularVelocity>(Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed),
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillLiftHeight),
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillCutoffDistance));
        }
        else
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe,
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillDistance),
                m_sb->setting<Velocity>(Constants::MaterialSettings::TipWipe::kInfillSpeed),
                m_sb->setting<Angle>(Constants::MaterialSettings::TipWipe::kInfillAngle),
                m_sb->setting<AngularVelocity>(Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed),
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillLiftHeight),
                m_sb->setting<Distance>(Constants::MaterialSettings::TipWipe::kInfillCutoffDistance));
    }
    if (m_sb->setting<bool>(Constants::MaterialSettings::SpiralLift::kInfillEnable)) {
        // Prevent spiral lifts during angled slicing to avoid collisions
        if (m_sb->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorX) == 0 &&
            m_sb->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorY) == 0 &&
            m_sb->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorZ) == 1) {
            PathModifierGenerator::GenerateSpiralLift(
                path, m_sb->setting<Distance>(Constants::MaterialSettings::SpiralLift::kLiftRadius),
                m_sb->setting<Distance>(Constants::MaterialSettings::SpiralLift::kLiftHeight),
                m_sb->setting<int>(Constants::MaterialSettings::SpiralLift::kLiftPoints),
                m_sb->setting<Velocity>(Constants::MaterialSettings::SpiralLift::kLiftSpeed), supportsG3);
        }
    }
    if (m_sb->setting<bool>(Constants::MaterialSettings::Startup::kInfillEnable)) {
        if (m_sb->setting<bool>(Constants::MaterialSettings::Startup::kInfillRampUpEnable)) {
            PathModifierGenerator::GenerateInitialStartupWithRampUp(
                path, m_sb->setting<Distance>(Constants::MaterialSettings::Startup::kInfillDistance),
                m_sb->setting<Velocity>(Constants::MaterialSettings::Startup::kInfillSpeed),
                m_sb->setting<Velocity>(Constants::ProfileSettings::Infill::kSpeed),
                m_sb->setting<AngularVelocity>(Constants::MaterialSettings::Startup::kInfillExtruderSpeed),
                m_sb->setting<AngularVelocity>(Constants::ProfileSettings::Infill::kExtruderSpeed),
                m_sb->setting<int>(Constants::MaterialSettings::Startup::kInfillSteps),
                m_sb->setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(Constants::MaterialSettings::Startup::kStartUpAreaModifier));
        }
        else {
            PathModifierGenerator::GenerateInitialStartup(
                path, m_sb->setting<Distance>(Constants::MaterialSettings::Startup::kInfillDistance),
                m_sb->setting<Velocity>(Constants::MaterialSettings::Startup::kInfillSpeed),
                m_sb->setting<AngularVelocity>(Constants::MaterialSettings::Startup::kInfillExtruderSpeed),
                m_sb->setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(Constants::MaterialSettings::Startup::kStartUpAreaModifier));
        }
    }
    if (m_sb->setting<bool>(Constants::ProfileSettings::Infill::kPrestart) &&
        (m_sb->setting<int>(Constants::ProfileSettings::Perimeter::kEnable) ||
         m_sb->setting<int>(Constants::ProfileSettings::Inset::kEnable))) {
        if (static_cast<InfillPatterns>(m_sb->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
            InfillPatterns::kLines) {
            PathModifierGenerator::GeneratePreStart(
                path, m_sb->setting<Distance>(Constants::ProfileSettings::Infill::kPrestartDistance),
                m_sb->setting<Velocity>(Constants::ProfileSettings::Infill::kPrestartSpeed),
                m_sb->setting<AngularVelocity>(Constants::ProfileSettings::Infill::kPrestartExtruderSpeed),
                innerMostClosedContour);
        }
    }
}

bool Infill::settingsSame(QSharedPointer<SettingsBase> a, QSharedPointer<SettingsBase> b) {
    return static_cast<InfillPatterns>(a->setting<int>(Constants::ProfileSettings::Infill::kPattern)) ==
               static_cast<InfillPatterns>(b->setting<int>(Constants::ProfileSettings::Infill::kPattern)) &&
           qFuzzyCompare(a->setting<Distance>(Constants::ProfileSettings::Infill::kLineSpacing)(),
                         b->setting<Distance>(Constants::ProfileSettings::Infill::kLineSpacing)()) &&
           qFuzzyCompare(a->setting<Distance>(Constants::ProfileSettings::Infill::kBeadWidth)(),
                         b->setting<Distance>(Constants::ProfileSettings::Infill::kBeadWidth)()) &&
           a->setting<int>(Constants::ProfileSettings::Infill::kSectorCount) ==
               b->setting<int>(Constants::ProfileSettings::Infill::kSectorCount) &&
           a->setting<bool>(Constants::ProfileSettings::Infill::kBasedOnPrinter) ==
               b->setting<bool>(Constants::ProfileSettings::Infill::kBasedOnPrinter) &&
           qFuzzyCompare(a->setting<Angle>(Constants::ProfileSettings::Infill::kAngle)(),
                         b->setting<Angle>(Constants::ProfileSettings::Infill::kAngle)()) &&
           a->setting<bool>(Constants::ProfileSettings::Infill::kEnable) ==
               b->setting<bool>(Constants::ProfileSettings::Infill::kEnable);
}

QVector<QSharedPointer<SegmentBase>> Infill::applyGrid(QSharedPointer<SegmentBase> seg) {
    QVector<QSharedPointer<SegmentBase>> segments;

    Point originPoint = m_grid_info.m_object_origin;
    Point newStart = seg->start() - originPoint, newEnd = seg->end() - originPoint;

    int maxX = m_grid_info.m_grid.size() - 1;
    int maxY = m_grid_info.m_grid[0].size() - 1;

    int xStart = qFloor((newStart.x() - m_grid_info.m_x_min) / m_grid_info.m_x_step);
    xStart = qMax(0, qMin(xStart, maxX));
    int yStart = qFloor((newStart.y() - m_grid_info.m_y_min) / m_grid_info.m_y_step);
    yStart = qMax(0, qMin(yStart, maxY));
    int xEnd = qFloor((newEnd.x() - m_grid_info.m_x_min) / m_grid_info.m_x_step);
    xEnd = qMax(0, qMin(xEnd, maxX));
    int yEnd = qFloor((newEnd.y() - m_grid_info.m_y_min) / m_grid_info.m_y_step);
    yEnd = qMax(0, qMin(yEnd, maxY));

    int dx = qAbs(xEnd - xStart);
    int dy = qAbs(yEnd - yStart);
    int x = xStart;
    int y = yStart;
    int n = dx + dy;
    int x_inc = (xEnd > xStart) ? 1 : -1;
    int y_inc = (yEnd > yStart) ? 1 : -1;
    int error = dx - dy;
    dx *= 2;
    dy *= 2;

    Point currentStart = newStart, currentEnd = newEnd;
    for (; n > 0; --n) {
        Point intersect;
        if (error > 0) {
            Point nextGrid1, nextGrid2;
            if (x_inc > 0) {
                nextGrid1 = Point((x + x_inc) * m_grid_info.m_x_step + m_grid_info.m_x_min, 0);
                nextGrid2 = Point((x + x_inc) * m_grid_info.m_x_step + m_grid_info.m_x_min,
                                  maxY * m_grid_info.m_y_step + m_grid_info.m_y_min);
            }
            else {
                nextGrid1 = Point(x * m_grid_info.m_x_step + m_grid_info.m_x_min, 0);
                nextGrid2 = Point(x * m_grid_info.m_x_step + m_grid_info.m_x_min,
                                  maxY * m_grid_info.m_y_step + m_grid_info.m_y_min);
            }
            intersect = MathUtils::lineIntersection(currentStart, currentEnd, nextGrid1, nextGrid2);

            QSharedPointer<SegmentBase> newSegment = QSharedPointer<LineSegment>::create(currentStart, intersect);
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kWidth,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kWidth));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kHeight,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kHeight));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kSpeed,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kSpeed));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kAccel,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kAccel));
            newSegment->getSb()->setSetting(
                Constants::SegmentSettings::kExtruderSpeed,
                seg->getSb()->setting<Distance>(Constants::SegmentSettings::kExtruderSpeed));
            newSegment->getSb()->setSetting(
                Constants::SegmentSettings::kMaterialNumber,
                seg->getSb()->setting<Distance>(Constants::SegmentSettings::kMaterialNumber));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kRegionType, RegionType::kInfill);
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kRecipe,
                                            getBlendVal(currentStart, intersect, maxX, maxY));
            segments.push_back(newSegment);

            currentStart = intersect;
            x += x_inc;
            error -= dy;
        }
        else if (error < 0) {
            Point nextGrid1, nextGrid2;
            if (y_inc > 0) {
                nextGrid1 = Point(0, (y + y_inc) * m_grid_info.m_y_step + m_grid_info.m_y_min);
                nextGrid2 = Point(maxX * m_grid_info.m_x_step + m_grid_info.m_x_min,
                                  (y + y_inc) * m_grid_info.m_y_step + m_grid_info.m_y_min);
            }
            else {
                nextGrid1 = Point(0, y * m_grid_info.m_y_step + m_grid_info.m_y_min);
                nextGrid2 = Point(maxX * m_grid_info.m_x_step + m_grid_info.m_x_min,
                                  y * m_grid_info.m_y_step + m_grid_info.m_y_min);
            }
            intersect = MathUtils::lineIntersection(currentStart, currentEnd, nextGrid1, nextGrid2);

            QSharedPointer<SegmentBase> newSegment = QSharedPointer<LineSegment>::create(currentStart, intersect);
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kWidth,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kWidth));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kHeight,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kHeight));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kSpeed,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kSpeed));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kAccel,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kAccel));
            newSegment->getSb()->setSetting(
                Constants::SegmentSettings::kExtruderSpeed,
                seg->getSb()->setting<Distance>(Constants::SegmentSettings::kExtruderSpeed));
            newSegment->getSb()->setSetting(
                Constants::SegmentSettings::kMaterialNumber,
                seg->getSb()->setting<Distance>(Constants::SegmentSettings::kMaterialNumber));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kRegionType, RegionType::kInfill);
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kRecipe,
                                            getBlendVal(currentStart, intersect, maxX, maxY));
            segments.push_back(newSegment);

            currentStart = intersect;

            y += y_inc;
            error += dx;
        }
        else if (error == 0) {
            // perfectly diagonal so choose either vertical or horizontal intersection
            Point nextGrid1;
            Point nextGrid2;
            if (x_inc > 0) {
                nextGrid1 = Point((x + x_inc) * m_grid_info.m_x_step + m_grid_info.m_x_min, 0);
                nextGrid2 = Point((x + x_inc) * m_grid_info.m_x_step + m_grid_info.m_x_min,
                                  maxY * m_grid_info.m_y_step + m_grid_info.m_y_min);
            }
            else {
                nextGrid1 = Point(x * m_grid_info.m_x_step + m_grid_info.m_x_min, 0);
                nextGrid2 = Point(x * m_grid_info.m_x_step + m_grid_info.m_x_min,
                                  maxY * m_grid_info.m_y_step + m_grid_info.m_y_min);
            }

            intersect = MathUtils::lineIntersection(currentStart, currentEnd, nextGrid1, nextGrid2);

            QSharedPointer<SegmentBase> newSegment = QSharedPointer<LineSegment>::create(currentStart, intersect);
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kWidth,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kWidth));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kHeight,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kHeight));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kSpeed,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kSpeed));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kAccel,
                                            seg->getSb()->setting<Distance>(Constants::SegmentSettings::kAccel));
            newSegment->getSb()->setSetting(
                Constants::SegmentSettings::kExtruderSpeed,
                seg->getSb()->setting<Distance>(Constants::SegmentSettings::kExtruderSpeed));
            newSegment->getSb()->setSetting(
                Constants::SegmentSettings::kMaterialNumber,
                seg->getSb()->setting<Distance>(Constants::SegmentSettings::kMaterialNumber));
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kRegionType, RegionType::kInfill);
            newSegment->getSb()->setSetting(Constants::SegmentSettings::kRecipe,
                                            getBlendVal(currentStart, intersect, maxX, maxY));
            segments.push_back(newSegment);

            currentStart = intersect;

            x += x_inc;
            y += y_inc;
            error -= dy;
            error += dx;
            --n;
        }
    }

    QSharedPointer<SegmentBase> newSegment = QSharedPointer<LineSegment>::create(currentStart, currentEnd);
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kWidth,
                                    seg->getSb()->setting<Distance>(Constants::SegmentSettings::kWidth));
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kHeight,
                                    seg->getSb()->setting<Distance>(Constants::SegmentSettings::kHeight));
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kSpeed,
                                    seg->getSb()->setting<Distance>(Constants::SegmentSettings::kSpeed));
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kAccel,
                                    seg->getSb()->setting<Distance>(Constants::SegmentSettings::kAccel));
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kExtruderSpeed,
                                    seg->getSb()->setting<Distance>(Constants::SegmentSettings::kExtruderSpeed));
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kMaterialNumber,
                                    seg->getSb()->setting<Distance>(Constants::SegmentSettings::kMaterialNumber));
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kRegionType, RegionType::kInfill);
    newSegment->getSb()->setSetting(Constants::SegmentSettings::kRecipe,
                                    getBlendVal(currentStart, currentEnd, maxX, maxY));
    segments.push_back(newSegment);

    for (QSharedPointer<SegmentBase> seg : segments) {
        seg->setStart(seg->start() + originPoint);
        seg->setEnd(seg->end() + originPoint);
    }
    return segments;
}

int Infill::getBlendVal(Point start, Point end, int xMax, int yMax) {
    Point mid = (start + end) / 2;
    double xConv = (mid.x() - m_grid_info.m_x_min) / m_grid_info.m_x_step;
    double yConv = (mid.y() - m_grid_info.m_y_min) / m_grid_info.m_y_step;
    int xLow = qMax(0, qMin(qFloor(xConv), xMax));
    int xHigh = qMax(0, qMin(qCeil(xConv), xMax));
    int yLow = qMax(0, qMin(qFloor(yConv), yMax));
    int yHigh = qMax(0, qMin(qCeil(yConv), yMax));

    QVector<double> vals;
    // corner
    if ((xLow == 0 && xHigh == 0 || xLow == xMax && xHigh == xMax) &&
        (yLow == 0 && yHigh == 0 || yLow == yMax && yHigh == yMax)) {
        vals.push_back(m_grid_info.m_grid[xLow][yLow]);
    }
    // edge
    else if ((xLow == 0 && xHigh == 0 || xLow == xMax && xHigh == xMax) ||
             (yLow == 0 && yHigh == 0 || yLow == yMax && yHigh == yMax)) {
        if (xLow == 0 || xLow == xMax) {
            vals.push_back(m_grid_info.m_grid[xLow][yLow]);
            vals.push_back(m_grid_info.m_grid[xLow][yHigh]);
        }
        else {
            vals.push_back(m_grid_info.m_grid[xLow][yLow]);
            vals.push_back(m_grid_info.m_grid[xHigh][yLow]);
        }
    }
    // center
    else {
        vals.push_back(m_grid_info.m_grid[xLow][yLow]);
        vals.push_back(m_grid_info.m_grid[xHigh][yLow]);
        vals.push_back(m_grid_info.m_grid[xLow][yHigh]);
        vals.push_back(m_grid_info.m_grid[xHigh][yHigh]);
    }

    double blendVal = 0.0;
    for (double val : vals)
        blendVal += val;

    blendVal /= vals.size();

    int recipe = 1;
    for (RecipeMap rMap : m_grid_info.m_recipe_maps) {
        if (blendVal >= rMap.m_min && blendVal <= rMap.m_max) {
            recipe = rMap.m_id;
            break;
        }
    }

    return recipe;
}

void Infill::setLayerCount(uint layer_count) { m_layer_count = layer_count - 1; }
} // namespace ORNL
