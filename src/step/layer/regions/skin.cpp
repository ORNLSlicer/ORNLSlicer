#include "step/layer/regions/skin.h"

#include "geometry/path_modifier.h"
#include "geometry/pattern_generator.h"
#include "geometry/segments/line.h"

namespace ORNL {
Skin::Skin(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons,
           const SingleExternalGridInfo& gridInfo)
    : RegionBase(sb, index, settings_polygons, gridInfo) {
    // NOP
}

QString Skin::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kSkin);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kSkin);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kSkin);
    }
    gcode += writer->writeAfterRegion(RegionType::kSkin);
    return gcode;
}

void Skin::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kSkinNum));

    Distance overlap = m_sb->setting<Distance>(PS::Skin::kOverlap);
    m_geometry = m_geometry.offset(overlap);

    Distance beadWidth = m_sb->setting<Distance>(PS::Skin::kBeadWidth);
    Angle patternAngle = m_sb->setting<Angle>(PS::Skin::kAngle);

    int top_count = m_sb->setting<int>(PS::Skin::kTopCount);
    int bottom_count = m_sb->setting<int>(PS::Skin::kBottomCount);
    int gradual_count = m_sb->setting<int>(PS::Skin::kInfillSteps);

    //! If skin region belongs to top or bottom layer there is no need to compute top or bottom skin
    if (!(top_count > 0 && m_upper_geometry.isEmpty()) || !(bottom_count > 0 && m_lower_geometry.isEmpty())) {
#pragma omp parallel sections
        {
#pragma omp section
            { computeTopSkin(top_count); }

#pragma omp section
            { computeBottomSkin(bottom_count); }
        }
    }
    if (m_sb->setting<bool>(PS::Skin::kInfillEnable))
        computeGradualSkinSteps(gradual_count);

    bool anyGeometry = false;
    if (!m_skin_geometry.isEmpty()) {
        m_geometry -= m_skin_geometry;
        PolygonList skin_offset = m_skin_geometry.offset(-beadWidth / 2);
        m_computed_geometry = createPatternForArea(static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)),
                                                   skin_offset, beadWidth, beadWidth, patternAngle);
        anyGeometry = true;
    }

    if (!m_gradual_skin_geometry.isEmpty()) {
        Angle infillPatternAngle = m_sb->setting<Angle>(PS::Skin::kInfillAngle);
        InfillPatterns gradualPattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kInfillPattern));
        double percentage = 0;
        if (m_sb->setting<bool>(PS::Infill::kEnable))
            percentage =
                m_sb->setting<double>(PS::Infill::kBeadWidth) / m_sb->setting<double>(PS::Infill::kLineSpacing);

        double densityStep = (1.0 - percentage) / (gradual_count + 1);
        for (int i = 0, end = m_gradual_skin_geometry.size(); i < end; ++i) {
            if (!m_gradual_skin_geometry[i].isEmpty()) {
                m_geometry -= m_gradual_skin_geometry[i];
                PolygonList skin_offset = m_gradual_skin_geometry[i].offset(-beadWidth / 2);
                m_gradual_computed_geometry.push_back(
                    createPatternForArea(gradualPattern, skin_offset, beadWidth,
                                         beadWidth / (percentage + densityStep * (end - i)), infillPatternAngle));
            }
        }
        anyGeometry = true;
    }
}

QVector<Polyline> Skin::createPatternForArea(InfillPatterns pattern, PolygonList& geometry, Distance beadWidth,
                                             Distance lineSpacing, Angle patternAngle) {
    QVector<Polyline> result;

    switch (pattern) {
        case InfillPatterns::kLines:
            result = PatternGenerator::GenerateLines(geometry, lineSpacing, patternAngle);
            break;
        case InfillPatterns::kGrid:
            result = PatternGenerator::GenerateGrid(geometry, lineSpacing, patternAngle);
            break;
        case InfillPatterns::kConcentric:
        case InfillPatterns::kInsideOutConcentric:
            result = PatternGenerator::GenerateConcentric(geometry, beadWidth, lineSpacing);
            break;
        case InfillPatterns::kTriangles:
            result = PatternGenerator::GenerateTriangles(geometry, lineSpacing, patternAngle);
            break;
        case InfillPatterns::kHexagonsAndTriangles:
            result = PatternGenerator::GenerateHexagonsAndTriangles(geometry, lineSpacing, patternAngle);
            break;
        case InfillPatterns::kHoneycomb:
            result = PatternGenerator::GenerateHoneyComb(geometry, beadWidth, lineSpacing, patternAngle);
            break;
        case InfillPatterns::kRadialHatch:
            result = PatternGenerator::GenerateRadialHatch(geometry, geometry.boundingRectCenter(), lineSpacing, 0,
                                                           patternAngle);
            break;
    }

    return result;
}

void Skin::computeTopSkin(const int& top_count) {
    PolygonList temp_geometry = m_geometry;

    //! If skin is within top_count of top layer, compute common geometry
    if (m_upper_geometry_includes_top && m_upper_geometry.size() < top_count)
        for (PolygonList poly : m_upper_geometry)
            temp_geometry &= poly;
    else
        temp_geometry.clear();

    //! Compute difference geometry
    for (PolygonList poly : m_upper_geometry)
        temp_geometry += m_geometry - poly;

    m_skin_geometry += temp_geometry;
}

void Skin::computeBottomSkin(const int& bottom_count) {
    PolygonList temp_geometry = m_geometry;

    //! If skin is within bottom_count of botton layer, compute common geometry
    if (m_lower_geometry_includes_bottom && m_lower_geometry.size() < bottom_count)
        for (PolygonList poly : m_lower_geometry)
            temp_geometry &= poly;
    else
        temp_geometry.clear();

    //! Compute difference geometry
    for (PolygonList poly : m_lower_geometry)
        temp_geometry += m_geometry - poly;

    m_skin_geometry += temp_geometry;
}

void Skin::computeGradualSkinSteps(const int& gradual_count) {
    PolygonList currentGradual;
    for (PolygonList poly : m_gradual_geometry) {
        m_gradual_skin_geometry.push_back(m_geometry - m_skin_geometry - currentGradual - poly);
        currentGradual += m_gradual_skin_geometry[m_gradual_skin_geometry.size() - 1];
    }
    if (m_gradual_geometry_includes_top && m_gradual_geometry.size() < gradual_count) {
        m_gradual_skin_geometry.push_back(m_geometry - m_skin_geometry - currentGradual);

        while (m_gradual_skin_geometry.size() < gradual_count)
            m_gradual_skin_geometry.push_back(PolygonList());
    }
}

void Skin::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
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

    poo.setPointParameters(pointOrderOptimization, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                           getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                           getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                           getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius));

    m_paths.clear();
    bool supportsG3 = m_sb->setting<bool>(PRS::MachineSetup::kSupportG3);
    InfillPatterns skinPattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern));
    optimizeHelper(poo, supportsG3, innerMostClosedContour, current_location, skinPattern, m_computed_geometry,
                   m_skin_geometry);

    InfillPatterns gradualPattern = InfillPatterns::kLines;
    for (int i = 0, end = m_gradual_computed_geometry.size(); i < end; ++i) {
        optimizeHelper(poo, supportsG3, innerMostClosedContour, current_location, gradualPattern,
                       m_gradual_computed_geometry[i], m_gradual_skin_geometry[i]);
    }
}

void Skin::optimizeHelper(PolylineOrderOptimizer poo, bool supportsG3, QVector<Path>& innerMostClosedContour,
                          Point& current_location, InfillPatterns pattern, QVector<Polyline> lines,
                          PolygonList geometry) {
    poo.setInfillParameters(pattern, geometry, getSb()->setting<Distance>(PS::Skin::kMinPathLength),
                            getSb()->setting<Distance>(PS::Travel::kMinLength));

    poo.setGeometryToEvaluate(lines, RegionType::kSkin,
                              static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

    QVector<Polyline> previouslyLinkedLines;
    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline(previouslyLinkedLines);
        if (result.size() > 0) {
            Path newPath = createPath(result);
            if (newPath.size() > 0) {
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3), innerMostClosedContour);
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));
                current_location = newPath.back()->end();
                previouslyLinkedLines.push_back(result);
                m_paths.push_back(newPath);
            }
        }
    }
}

Path Skin::createPath(Polyline line) {

    Distance width = m_sb->setting<Distance>(PS::Skin::kBeadWidth);
    Distance height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity speed = m_sb->setting<Velocity>(PS::Skin::kSpeed);
    Acceleration acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kInfill);
    AngularVelocity extruder_speed = m_sb->setting<AngularVelocity>(PS::Skin::kExtruderSpeed);
    int material_number = m_sb->setting<int>(MS::MultiMaterial::kSkinNum);

    Path newPath;
    for (int i = 0; i < line.size() - 1; i++) {
        QSharedPointer<LineSegment> line_segment = QSharedPointer<LineSegment>::create(line[i], line[i + 1]);

        line_segment->getSb()->setSetting(SS::kWidth, width);
        line_segment->getSb()->setSetting(SS::kHeight, height);
        line_segment->getSb()->setSetting(SS::kSpeed, speed);
        line_segment->getSb()->setSetting(SS::kAccel, acceleration);
        line_segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
        line_segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        line_segment->getSb()->setSetting(SS::kRegionType, RegionType::kSkin);

        newPath.append(line_segment);
    }

    //! Creates closing segment if infill pattern is concentric
    if (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) == InfillPatterns::kConcentric ||
        static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) == InfillPatterns::kInsideOutConcentric) {
        QSharedPointer<LineSegment> line_segment = QSharedPointer<LineSegment>::create(line.last(), line.first());

        line_segment->getSb()->setSetting(SS::kWidth, width);
        line_segment->getSb()->setSetting(SS::kHeight, height);
        line_segment->getSb()->setSetting(SS::kSpeed, speed);
        line_segment->getSb()->setSetting(SS::kAccel, acceleration);
        line_segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
        line_segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        line_segment->getSb()->setSetting(SS::kRegionType, RegionType::kSkin);

        newPath.append(line_segment);
    }

    return newPath;
}

void Skin::addUpperGeometry(const PolygonList& poly_list) { m_upper_geometry.push_back(poly_list); }

void Skin::addLowerGeometry(const PolygonList& poly_list) { m_lower_geometry.push_back(poly_list); }

void Skin::addGradualGeometry(const PolygonList& poly_list) { m_gradual_geometry.push_back(poly_list); }

void Skin::setGeometryIncludes(bool top, bool bottom, bool gradual) {
    m_upper_geometry_includes_top = top;
    m_lower_geometry_includes_bottom = bottom;
    m_gradual_geometry_includes_top = gradual;
}

void Skin::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    if (m_sb->setting<bool>(ES::Ramping::kTrajectoryAngleEnabled)) {
        PathModifierGenerator::GenerateTrajectorySlowdown(path, m_sb);
    }

    // add modifiers
    if (m_sb->setting<bool>(MS::Slowdown::kSkinEnable)) {
        PathModifierGenerator::GenerateSlowdown(path, m_sb->setting<Distance>(MS::Slowdown::kSkinDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kSkinLiftDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kSkinCutoffDistance),
                                                m_sb->setting<Velocity>(MS::Slowdown::kSkinSpeed),
                                                m_sb->setting<AngularVelocity>(MS::Slowdown::kSkinExtruderSpeed),
                                                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                                                m_sb->setting<double>(MS::Slowdown::kSlowDownAreaModifier));
    }
    if (m_sb->setting<bool>(MS::TipWipe::kSkinEnable)) {
        // If angled slicing, force tip wipe to be reverse
        if (m_sb->setting<float>(PS::SlicingVector::kSlicingVectorX) != 0 ||
            m_sb->setting<float>(PS::SlicingVector::kSlicingVectorY) != 0 ||
            m_sb->setting<float>(PS::SlicingVector::kSlicingVectorZ) != 1) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed), m_sb->setting<Angle>(MS::TipWipe::kSkinAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kSkinExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kSkinLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kSkinCutoffDistance));
        }
        // if Forward OR (if Optimal AND (Perimeter OR Inset)) OR (if Optimal AND (Concentric or Inside Out Concentric))
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                     TipWipeDirection::kForward ||
                 (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                      TipWipeDirection::kOptimal &&
                  (m_sb->setting<int>(PS::Perimeter::kEnable) || m_sb->setting<int>(PS::Inset::kEnable))) ||
                 (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                      TipWipeDirection::kOptimal &&
                  (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) == InfillPatterns::kConcentric ||
                   static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) ==
                       InfillPatterns::kInsideOutConcentric))) {
            if (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) == InfillPatterns::kConcentric ||
                static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) ==
                    InfillPatterns::kInsideOutConcentric)
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed), m_sb->setting<Angle>(MS::TipWipe::kSkinAngle),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kSkinExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kSkinLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kSkinCutoffDistance));
            else if (m_sb->setting<int>(PS::Perimeter::kEnable) || m_sb->setting<int>(PS::Inset::kEnable))
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed), innerMostClosedContour,
                    m_sb->setting<Angle>(MS::TipWipe::kSkinAngle),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kSkinExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kSkinLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kSkinCutoffDistance));
            else
                PathModifierGenerator::GenerateForwardTipWipeOpenLoop(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kSkinExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kSkinLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kSkinCutoffDistance));
        }
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                 TipWipeDirection::kAngled) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kAngledTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed), m_sb->setting<Angle>(MS::TipWipe::kSkinAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kSkinExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kSkinLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kSkinCutoffDistance));
        }
        else
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed), m_sb->setting<Angle>(MS::TipWipe::kSkinAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kSkinExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kSkinLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kSkinCutoffDistance));
    }
    if (m_sb->setting<bool>(MS::SpiralLift::kSkinEnable)) {
        PathModifierGenerator::GenerateSpiralLift(path, m_sb->setting<Distance>(MS::SpiralLift::kLiftRadius),
                                                  m_sb->setting<Distance>(MS::SpiralLift::kLiftHeight),
                                                  m_sb->setting<int>(MS::SpiralLift::kLiftPoints),
                                                  m_sb->setting<Velocity>(MS::SpiralLift::kLiftSpeed), supportsG3);
    }
    if (m_sb->setting<bool>(MS::Startup::kSkinEnable)) {
        if (m_sb->setting<bool>(MS::Startup::kSkinRampUpEnable)) {
            PathModifierGenerator::GenerateInitialStartupWithRampUp(
                path, m_sb->setting<Distance>(MS::Startup::kSkinDistance),
                m_sb->setting<Velocity>(MS::Startup::kSkinSpeed), m_sb->setting<Velocity>(PS::Skin::kSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kSkinExtruderSpeed),
                m_sb->setting<AngularVelocity>(PS::Skin::kExtruderSpeed), m_sb->setting<int>(MS::Startup::kSkinSteps),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
        else {
            PathModifierGenerator::GenerateInitialStartup(
                path, m_sb->setting<Distance>(MS::Startup::kSkinDistance),
                m_sb->setting<Velocity>(MS::Startup::kSkinSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kSkinExtruderSpeed),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
    }
}
} // namespace ORNL
