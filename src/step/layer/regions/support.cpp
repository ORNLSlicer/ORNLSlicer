#include "step/layer/regions/support.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/pattern_generator.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
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
Support::Support(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, settings_polygons) {
    // NOP
}

QString Support::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kSupport);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kSupport);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kSupport);
    }
    gcode += writer->writeAfterRegion(RegionType::kSupport);
    return gcode;
}

void Support::compute(uint layer_num) {
    m_paths.clear();
    m_computed_perimeter_geometry.clear();
    m_computed_infill_geometry.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));

    Distance bead_width = m_sb->setting<Distance>(PS::Layer::kBeadWidth);
    Distance line_spacing = m_sb->setting<Distance>(PS::Support::kLineSpacing);
    Area min_infill_area = m_sb->setting<Area>(PS::Support::kMinInfillArea);
    InfillPatterns infill_pattern = static_cast<InfillPatterns>(m_sb->setting<int>(PS::Support::kPattern));
    const bool is_interface = m_sb->setting<bool>(PS::Support::kInterfaceRegion);
    const bool is_base = m_sb->setting<bool>(PS::Support::kBaseRegion);
    const bool is_tube_wall = m_sb->setting<bool>(PS::Support::kTubeWallRegion);
    const bool is_dense = is_interface || is_base;
    const Angle interface_rotation = is_dense && layer_num % 2 == 1 ? 90 * deg : 0 * deg;

    if (bead_width <= 0)
        return;

    if (is_tube_wall) {
        const int contour_count = qMax(1, m_sb->setting<int>(PS::Support::kTaperWallContours));
        const PolygonList outer_boundaries = m_geometry.externalPolygonBoundaries();
        for (int contour = 0; contour < contour_count; ++contour) {
            const Distance inset = bead_width / 2.0 + bead_width * contour;
            const PolygonList contour_geometry = outer_boundaries.offset(-inset);
            for (Polygon polygon : contour_geometry) {
                Polyline line = polygon.toPolyline();
                if (line.size() > 1)
                    m_computed_perimeter_geometry.push_back(line);
            }
        }
        return;
    }

    // Invalid spacing previously caused an endless loop in support pattern
    // generation.  Falling back to two bead widths keeps old/incomplete
    // profiles printable while preserving sparse support.
    if (line_spacing <= 0)
        line_spacing = bead_width * 2.0;

    const PolygonList support_geometry = m_geometry;
    const int wall_contours = is_dense ? 1 : qMax(1, m_sb->setting<int>(PS::Support::kWallContours));
    for (int contour = 0; contour < wall_contours; ++contour) {
        const Distance inset = bead_width / 2.0 + bead_width * contour;
        for (Polygon poly : support_geometry.offset(-inset)) {
            Polyline line = poly.toPolyline();
            if (line.size() > 1)
                m_computed_perimeter_geometry.push_back(line);
        }
    }

    m_geometry = support_geometry.offset(-(bead_width / 2.0 + bead_width * (wall_contours - 1)));

    // Determine whether or not to generate support infill
    if (m_geometry.netArea() > min_infill_area) {
        switch (infill_pattern) {
            case InfillPatterns::kLines:
                computeLine(line_spacing, interface_rotation);
                break;
            case InfillPatterns::kGrid:
                computeGrid(line_spacing); // Default rotation angle = 0 deg
                break;
            default:
                // Support exposes Lines and Grid.  Treat stale or malformed
                // enum values as Grid, the standard support style.
                computeGrid(line_spacing);
                break;
        }
    }
}

void Support::computeLine(Distance line_spacing, Angle rotation) {
    const Distance bead_width = m_sb->setting<Distance>(PS::Layer::kBeadWidth);
    PolygonList border_polygons = m_geometry.offset(-bead_width);
    if (!border_polygons.isEmpty()) {
        QVector<Polyline> lines = PatternGenerator::GenerateLines(border_polygons, line_spacing, rotation);
        if (!lines.isEmpty())
            m_computed_infill_geometry.push_back(lines);
    }
}

void Support::computeGrid(Distance line_spacing, Angle rotation) {
    //! Call computeLine with our base rotation
    computeLine(line_spacing, rotation);

    //! Call computeLine with our base rotation plus 90 deg
    computeLine(line_spacing, rotation + 90 * deg);
}

void Support::optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) {
    PolylineOrderOptimizer poo(current_location, layerNumber);

    PathOrderOptimization pathOrderOptimization =
        static_cast<PathOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPathOrder));
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

    m_paths.clear();

    poo.setGeometryToEvaluate(m_computed_perimeter_geometry, RegionType::kSupport,
                              static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline();
        if (result.size() > 2 && result.first() != result.last())
            result.push_back(result.first());
        Path newPath = createPath(result);

        if (newPath.size() > 0) {
            PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                  m_sb->setting<Velocity>(PS::Travel::kSpeed));
            current_location = newPath.back()->end();
            m_paths.push_back(newPath);
        }
    }

    poo.setInfillParameters(static_cast<InfillPatterns>(m_sb->setting<int>(PS::Support::kPattern)), m_geometry,
                            Distance(0), getSb()->setting<Distance>(PS::Travel::kInfillMinLength));

    // The optimizer's region type selects its linking algorithm.  Dense support
    // interfaces and sparse fill use the same boundary-checked extrusion links
    // as skin.  The resulting paths and segments remain support for settings
    // and G-code.  Grid hatch directions are optimized independently so their
    // intentional crossings do not block safe connectors.
    for (const QVector<Polyline>& infill_geometry : m_computed_infill_geometry) {
        poo.setGeometryToEvaluate(
            infill_geometry, RegionType::kSkin,
            static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

        QVector<Polyline> previouslyLinkedLines;
        while (poo.getCurrentPolylineCount() > 0) {
            Polyline result = poo.linkNextPolyline(previouslyLinkedLines);
            if (result.size() > 0) {
                Path newPath = createPath(result);
                if (newPath.size() > 0) {
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

void Support::calculateModifiers(Path& path, bool supportsG3) {
    // NOP
}

Path Support::createPath(Polyline line) {
    const bool is_closed_path = line.size() > 2 && line.first() == line.last();
    line = line.removeShortSegments(m_sb->setting<Distance>(PS::Support::kMinSegmentLength), is_closed_path);
    if (line.size() < (is_closed_path ? 4 : 2)) {
        return Path();
    }

    Distance bead_width = m_sb->setting<Distance>(PS::Layer::kBeadWidth);
    Distance layer_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity speed = m_sb->setting<Velocity>(PS::Layer::kSpeed);
    Acceleration acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kSupport);
    AngularVelocity extruder_speed = m_sb->setting<AngularVelocity>(PS::Layer::kExtruderSpeed);
    int material_number = m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum);

    Path newPath;

    for (int i = 0; i + 1 < line.size(); ++i) {
        if (line[i] == line[i + 1]) {
            continue;
        }

        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line[i], line[i + 1]);
        segment->getSb()->setSetting(MS::Extruder::kInitialSpeed, m_sb->setting<int>(MS::Extruder::kInitialSpeed));
        segment->getSb()->setSetting(SS::kWidth, bead_width);
        segment->getSb()->setSetting(SS::kHeight, layer_height);
        segment->getSb()->setSetting(SS::kSpeed, speed);
        segment->getSb()->setSetting(SS::kAccel, acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
        segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kSupport);
        newPath.append(segment);
    }

    if (newPath.calculateLength() > m_sb->setting<Distance>(PS::Layer::kMinExtrudeLength)) {
        return newPath;
    }
    else {
        return Path();
    }
}
} // namespace ORNL
