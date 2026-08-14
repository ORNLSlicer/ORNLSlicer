#include "step/layer/regions/skin.h"

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/pattern_generator.h"
#include "geometry/point.h"
#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/line.h"
#include "geometry/settings_polygon.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
PolygonList polylineFootprint(const Polyline& line, Distance bead_width, const PolygonList& clipping_geometry) {
    PolygonList footprint;
    for (int i = 0, end = line.size() - 1; i < end; ++i) {
        if (line[i] == line[i + 1]) continue;

        footprint += Polyline({line[i], line[i + 1]}).makeReal(bead_width);
    }

    return footprint & clipping_geometry;
}

QVector<Polyline> printableLines(const QVector<Polyline>& lines, Distance min_path_length, Distance min_segment_length,
                                 bool closed) {
    QVector<Polyline> printable;
    printable.reserve(lines.size());

    for (const Polyline& line : lines) {
        Polyline cleaned_line = line.removeShortSegments(min_segment_length, closed);
        if (cleaned_line.size() > (closed ? 2 : 1) && cleaned_line.length() >= min_path_length)
            printable.push_back(cleaned_line);
    }

    return printable;
}

PolygonList printableFootprint(const QVector<Polyline>& lines, Distance bead_width,
                               const PolygonList& clipping_geometry) {
    PolygonList footprint;
    for (const Polyline& line : lines) footprint += polylineFootprint(line, bead_width, clipping_geometry);

    return footprint;
}

PolygonList futurePerimeterSupport(const PolygonList& current_geometry, const PolygonList& upper_geometry,
                                   Distance support_width) {
    if (support_width <= 0 || upper_geometry.isEmpty()) return PolygonList();

    // Extract the upper layer's perimeter band so top skin can support future walls.
    PolygonList support_geometry = upper_geometry - upper_geometry.offset(-support_width);
    return support_geometry & current_geometry;
}
}  // namespace

Skin::Skin(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, index, settings_polygons, PolygonList(), RegionType::kSkin) {
    // NOP
}

QString Skin::writeGCode(QSharedPointer<WriterBase> writer) { return writeRegionGCode(writer, RegionType::kSkin); }

void Skin::compute(uint layer_num) {
    m_paths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kSkinNum));

    Distance overlap = m_sb->setting<Distance>(PS::Skin::kOverlap);
    m_geometry       = m_geometry.offset(overlap);

    Distance beadWidth        = m_sb->setting<Distance>(PS::Skin::kBeadWidth);
    Distance minPathLength    = m_sb->setting<Distance>(PS::Skin::kMinPathLength);
    Distance minSegmentLength = m_sb->setting<Distance>(PS::Skin::kMinSegmentLength);
    Angle patternAngle        = m_sb->setting<Angle>(PS::Skin::kAngle);

    int top_count     = m_sb->setting<int>(PS::Skin::kTopCount);
    int bottom_count  = m_sb->setting<int>(PS::Skin::kBottomCount);
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
    if (m_sb->setting<bool>(PS::Skin::kInfillEnable)) computeGradualSkinSteps(gradual_count);

    if (!m_skin_geometry.isEmpty()) {
        PolygonList skin_offset           = m_skin_geometry.offset(-beadWidth / 2);
        const InfillPatterns skin_pattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern));
        QVector<Polyline> lines = createPatternForArea(skin_pattern, skin_offset, beadWidth, beadWidth, patternAngle);
        m_computed_geometry =
            printableLines(lines, minPathLength, minSegmentLength, skin_pattern == InfillPatterns::kConcentric);
        m_geometry -= printableFootprint(m_computed_geometry, beadWidth, m_skin_geometry);
    }

    if (!m_gradual_skin_geometry.isEmpty()) {
        Angle infillPatternAngle      = m_sb->setting<Angle>(PS::Skin::kInfillAngle);
        InfillPatterns gradualPattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kInfillPattern));
        double percentage             = 0;
        if (m_sb->setting<bool>(PS::Infill::kEnable))
            percentage =
                m_sb->setting<double>(PS::Infill::kBeadWidth) / m_sb->setting<double>(PS::Infill::kLineSpacing);

        double densityStep = (1.0 - percentage) / (gradual_count + 1);
        for (int i = 0, end = m_gradual_skin_geometry.size(); i < end; ++i) {
            if (!m_gradual_skin_geometry[i].isEmpty()) {
                PolygonList skin_offset = m_gradual_skin_geometry[i].offset(-beadWidth / 2);
                QVector<Polyline> lines =
                    createPatternForArea(gradualPattern, skin_offset, beadWidth,
                                         beadWidth / (percentage + densityStep * (end - i)), infillPatternAngle);
                m_gradual_computed_geometry.push_back(printableLines(lines, minPathLength, minSegmentLength,
                                                                     gradualPattern == InfillPatterns::kConcentric));
                m_geometry -=
                    printableFootprint(m_gradual_computed_geometry.back(), beadWidth, m_gradual_skin_geometry[i]);
            }
        }
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
    if (top_count <= 0) {
        // A zero top count disables both regular top skin and future-perimeter support.
        return;
    }

    PolygonList temp_geometry = m_geometry;
    const Distance perimeter_support_width =
        m_sb->setting<bool>(PS::Perimeter::kEnable)
            ? m_sb->setting<Distance>(PS::Perimeter::kBeadWidth) * m_sb->setting<int>(PS::Perimeter::kCount)
            : Distance(0);

    //! If skin is within top_count of top layer, compute common geometry
    if (m_upper_geometry_includes_top && m_upper_geometry.size() < top_count)
        for (const PolygonList& poly : m_upper_geometry) temp_geometry &= poly;
    else
        temp_geometry.clear();

    //! Compute difference geometry
    for (const PolygonList& poly : m_upper_geometry) {
        temp_geometry += m_geometry - poly;
        temp_geometry += futurePerimeterSupport(m_geometry, poly, perimeter_support_width);
    }

    m_skin_geometry += temp_geometry;
}

void Skin::computeBottomSkin(const int& bottom_count) {
    if (bottom_count <= 0) return;

    PolygonList temp_geometry = m_geometry;

    //! If skin is within bottom_count of botton layer, compute common geometry
    if (m_lower_geometry_includes_bottom && m_lower_geometry.size() < bottom_count)
        for (const PolygonList& poly : m_lower_geometry) temp_geometry &= poly;
    else
        temp_geometry.clear();

    //! Compute difference geometry
    for (const PolygonList& poly : m_lower_geometry) temp_geometry += m_geometry - poly;

    m_skin_geometry += temp_geometry;
}

void Skin::computeGradualSkinSteps(const int& gradual_count) {
    PolygonList currentGradual;
    for (const PolygonList& poly : m_gradual_geometry) {
        m_gradual_skin_geometry.push_back(m_geometry - m_skin_geometry - currentGradual - poly);
        currentGradual += m_gradual_skin_geometry[m_gradual_skin_geometry.size() - 1];
    }
    if (m_gradual_geometry_includes_top && m_gradual_geometry.size() < gradual_count) {
        m_gradual_skin_geometry.push_back(m_geometry - m_skin_geometry - currentGradual);

        while (m_gradual_skin_geometry.size() < gradual_count) m_gradual_skin_geometry.push_back(PolygonList());
    }
}

void Skin::optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) {
    PolylineOrderOptimizer poo(current_location, layerNumber);

    PathOrderOptimization pathOrderOptimization = this->pathOrderOptimization();
    if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
        Point startOverride = customPathOrderPoint();

        poo.setStartOverride(startOverride);
    }

    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPointOrder));

    if (usesCustomPointLocation(pointOrderOptimization)) {
        Point startOverride = customPointOrderPoint();

        poo.setStartPointOverride(startOverride);
    }

    poo.setPointParameters(pointOrderOptimization, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                           getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                           getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                           getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius),
                           getSb()->setting<bool>(PS::Optimizations::kEnablePointOrderSegmentBreaking));
    poo.setConsecutiveReferencePoint(getPreviousLayerStartPoint());

    m_paths.clear();
    bool supportsG3            = m_sb->setting<bool>(PRS::MachineSetup::kSupportG3);
    InfillPatterns skinPattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern));
    optimizeHelper(poo, supportsG3, current_location, skinPattern, m_computed_geometry, m_skin_geometry,
                   pathOrderOptimization);

    InfillPatterns gradualPattern = InfillPatterns::kLines;
    for (int i = 0, end = m_gradual_computed_geometry.size(); i < end; ++i) {
        optimizeHelper(poo, supportsG3, current_location, gradualPattern, m_gradual_computed_geometry[i],
                       m_gradual_skin_geometry[i], pathOrderOptimization);
    }
}

void Skin::optimizeHelper(PolylineOrderOptimizer poo, bool supportsG3, Point& current_location, InfillPatterns pattern,
                          QVector<Polyline> lines, PolygonList geometry, PathOrderOptimization path_order) {
    const bool order_open_lines =
        pattern == InfillPatterns::kLines && getSb()->setting<int>(PS::Optimizations::kSkinPathOrder) != 0;
    poo.setInfillParameters(pattern, geometry, getSb()->setting<Distance>(PS::Skin::kMinPathLength),
                            getSb()->setting<Distance>(PS::Travel::kInfillMinLength), order_open_lines);

    poo.setGeometryToEvaluate(lines, RegionType::kSkin, path_order);

    QVector<Polyline> previouslyLinkedLines;
    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline(previouslyLinkedLines);
        if (result.size() > 0) {
            Path newPath = createPath(result, pattern);
            if (newPath.size() > 0) {
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3));
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
    const InfillPatterns pattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern));
    return createPath(line, pattern);
}

Path Skin::createPath(Polyline line, InfillPatterns pattern) {
    const bool is_closed_path = pattern == InfillPatterns::kConcentric;

    line = line.removeShortSegments(m_sb->setting<Distance>(PS::Skin::kMinSegmentLength), is_closed_path);
    if (line.size() < (is_closed_path ? 3 : 2)) { return Path(); }

    Distance width                 = m_sb->setting<Distance>(PS::Skin::kBeadWidth);
    Distance height                = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity speed                 = m_sb->setting<Velocity>(PS::Skin::kSpeed);
    Acceleration acceleration      = m_sb->setting<Acceleration>(PRS::Acceleration::kInfill);
    AngularVelocity extruder_speed = m_sb->setting<AngularVelocity>(PS::Skin::kExtruderSpeed);
    int material_number            = m_sb->setting<int>(MS::MultiMaterial::kSkinNum);

    Path newPath;
    for (int i = 0; i < line.size() - 1; i++) {
        QSharedPointer<LineSegment> line_segment = QSharedPointer<LineSegment>::create(line[i], line[i + 1]);

        line_segment->getSb()->setSetting(MS::Extruder::kInitialSpeed, m_sb->setting<int>(MS::Extruder::kInitialSpeed));
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
    if (is_closed_path) {
        QSharedPointer<LineSegment> line_segment = QSharedPointer<LineSegment>::create(line.last(), line.first());

        line_segment->getSb()->setSetting(MS::Extruder::kInitialSpeed, m_sb->setting<int>(MS::Extruder::kInitialSpeed));
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

void Skin::addUpperGeometry(const PolygonList& poly_list) {
    m_upper_geometry.push_back(poly_list);
}

void Skin::addLowerGeometry(const PolygonList& poly_list) {
    m_lower_geometry.push_back(poly_list);
}

void Skin::addGradualGeometry(const PolygonList& poly_list) {
    m_gradual_geometry.push_back(poly_list);
}

void Skin::setGeometryIncludes(bool top, bool bottom, bool gradual) {
    m_upper_geometry_includes_top    = top;
    m_lower_geometry_includes_bottom = bottom;
    m_gradual_geometry_includes_top  = gradual;
}

void Skin::calculateModifiers(Path& path, bool supportsG3) {
    PathModifierGenerator::GenerateSharpCornerExtension(path, m_sb);

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
        // if Forward OR (if Optimal AND (Perimeter OR Inset)) OR (if Optimal AND Concentric)
        if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                TipWipeDirection::kForward ||
            (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                 TipWipeDirection::kOptimal &&
             (m_sb->setting<int>(PS::Perimeter::kEnable) || m_sb->setting<int>(PS::Inset::kEnable))) ||
            (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkinDirection)) ==
                 TipWipeDirection::kOptimal &&
             (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) == InfillPatterns::kConcentric))) {
            if (static_cast<InfillPatterns>(m_sb->setting<int>(PS::Skin::kPattern)) == InfillPatterns::kConcentric)
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kSkinDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kSkinSpeed), m_sb->setting<Angle>(MS::TipWipe::kSkinAngle),
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
}  // namespace ORNL
