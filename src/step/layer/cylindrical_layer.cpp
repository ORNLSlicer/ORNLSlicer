#include "step/layer/cylindrical_layer.h"

#include <algorithm>
#include <limits>

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "geometry/segments/travel.h"
#include "optimizers/path_order_optimizer.h"
#include "slicing/helical_tool_start_angle.h"
#include "step/layer/layer.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! @brief Returns the first printable cylindrical segment in a path, or nullptr when the path only contains travels.
QSharedPointer<SegmentBase> firstPrintSegment(const Path& path) {
    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (dynamic_cast<TravelSegment*>(segment.data()) == nullptr) { return segment; }
    }

    return nullptr;
}

//! @brief Returns whether a segment is a travel move.
bool isTravelSegment(const QSharedPointer<SegmentBase>& segment) {
    return dynamic_cast<TravelSegment*>(segment.data()) != nullptr;
}

//! @brief Returns a segment's print region metadata, or Unknown when absent.
RegionType segmentRegionType(const QSharedPointer<SegmentBase>& segment) {
    if (segment == nullptr || segment->getSb() == nullptr || !segment->getSb()->contains(SS::kRegionType)) {
        return RegionType::kUnknown;
    }

    return segment->getSb()->setting<RegionType>(SS::kRegionType);
}

//! @brief Returns segment settings from the first printable segment in a path.
QSharedPointer<SettingsBase> firstPrintSettings(const Path& path, const QSharedPointer<SettingsBase>& fallback) {
    QSharedPointer<SegmentBase> print_segment = firstPrintSegment(path);
    if (print_segment != nullptr) { return print_segment->getSb(); }

    return fallback;
}

//! @brief Returns the cylindrical path-order mode supported by direct cylindrical layers.
PathOrderOptimization resolvedCylindricalPathOrder(const QSharedPointer<SettingsBase>& settings) {
    const int path_order = settings->setting<int>(PS::Optimizations::kCylindricalPathOrder);
    return path_order == static_cast<int>(PathOrderOptimization::kNextFarthest) ? PathOrderOptimization::kNextFarthest
                                                                                : PathOrderOptimization::kNextClosest;
}

//! @brief Applies the helical tool offset sign that matches the ordered print direction.
void applyHelicalToolOffsetForOrderedDirection(Path& path, const QSharedPointer<SettingsBase>& layer_settings,
                                               bool starts_from_generated_end) {
    const HelicalPathZClipRounding z_clip_rounding =
        static_cast<HelicalPathZClipRounding>(layer_settings->setting<int>(PS::Helical::kHelicalPathZClipRounding));
    const PathOrderOptimization path_order = resolvedCylindricalPathOrder(layer_settings);
    const Angle configured_offset          = layer_settings->setting<Angle>(PS::Helical::kHelicalToolStartAngleOffset);
    const Angle effective_offset = HelicalToolStartAngle::effectiveOffset(configured_offset, starts_from_generated_end,
                                                                          z_clip_rounding, path_order);

    layer_settings->setSetting(PS::Helical::kHelicalToolStartAngleOffset, effective_offset);
    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (segment != nullptr && segment->getSb() != nullptr) {
            segment->getSb()->setSetting(PS::Helical::kHelicalToolStartAngleOffset, effective_offset);
        }
    }
}

//! @brief Recomputes region-start flags after optimizer ordering or reversal.
void recomputeRegionStartFlags(Path& path) {
    bool has_previous_print_region = false;
    RegionType previous_region     = RegionType::kUnknown;

    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (isTravelSegment(segment)) { continue; }

        QSharedPointer<SettingsBase> settings = segment->getSb();
        if (settings == nullptr) { continue; }

        const RegionType current_region = segmentRegionType(segment);
        settings->setSetting(SS::kIsRegionStartSegment,
                             !has_previous_print_region || current_region != previous_region);
        previous_region           = current_region;
        has_previous_print_region = true;
    }
}

//! @brief Restores cylindrical print metadata and gives optimizer-created travels center settings.
void restoreCylindricalPathSettings(Path& path, const QSharedPointer<SettingsBase>& layer_settings) {
    QSharedPointer<SettingsBase> print_settings = firstPrintSettings(path, layer_settings);
    if (print_settings == nullptr) { print_settings = layer_settings; }

    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (isTravelSegment(segment)) {
            QSharedPointer<SettingsBase> travel_settings = QSharedPointer<SettingsBase>::create(*print_settings);
            travel_settings->setSetting(SS::kSpeed, layer_settings->setting<Velocity>(PS::Travel::kSpeed));
            travel_settings->setSetting(SS::kIsRegionStartSegment, true);
            segment->setSb(travel_settings);
        }
        else if (segment->getSb() == nullptr) { segment->setSb(QSharedPointer<SettingsBase>::create(*print_settings)); }
    }

    recomputeRegionStartFlags(path);
}

//! @brief Writes a helical path as contiguous print-region runs without injecting region-boundary travel.
QString writeHelicalPathByRegionRuns(Path& path, const QSharedPointer<WriterBase>& writer) {
    QString gcode;
    bool path_open           = false;
    bool region_open         = false;
    RegionType active_region = RegionType::kUnknown;

    auto openRegion = [&gcode, &region_open, &active_region, &writer](RegionType region_type) {
        active_region = region_type;
        gcode += writer->writeBeforeRegion(active_region, 1);
        region_open = true;
    };

    for (int i = 0, end = path.size(); i < end; ++i) {
        const QSharedPointer<SegmentBase>& segment = path[i];

        if (isTravelSegment(segment)) {
            gcode += segment->writeGCode(writer);
            continue;
        }

        const RegionType segment_region = segmentRegionType(segment);
        if (!path_open) {
            openRegion(segment_region);
            gcode += writer->writeBeforePath(active_region);
            path_open = true;
        }
        else if (segment_region != active_region) {
            if (region_open) { gcode += writer->writeAfterRegion(active_region); }
            openRegion(segment_region);
        }

        gcode += segment->writeGCode(writer);
    }

    if (path_open) { gcode += writer->writeAfterPath(active_region); }
    if (region_open) { gcode += writer->writeAfterRegion(active_region); }

    return gcode;
}
}  // namespace

CylindricalLayer::CylindricalLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb,
                                   CylindricalPathPattern path_pattern)
    : Layer(layer_nr, sb), m_path_pattern(path_pattern) {}

void CylindricalLayer::addPath(const Path& path) {
    if (path.size() > 0) { m_paths.push_back(path); }
}

QString CylindricalLayer::writeGCode(QSharedPointer<WriterBase> writer) {
    if (m_paths.isEmpty()) { return writer->writeEmptyStep(); }

    QString gcode;
    if (m_path_pattern != CylindricalPathPattern::kHelical) {
        gcode += writer->writeBeforeRegion(RegionType::kPerimeter, m_paths.size());
    }
    for (Path& path : m_paths) {
        if (path.size() == 0) { continue; }

        if (m_path_pattern == CylindricalPathPattern::kHelical) { gcode += writeHelicalPathByRegionRuns(path, writer); }
        else {
            gcode += writer->writeBeforePath(RegionType::kPerimeter);
            for (const QSharedPointer<SegmentBase>& segment : path) { gcode += segment->writeGCode(writer); }
            gcode += writer->writeAfterPath(RegionType::kPerimeter);
        }
    }
    if (m_path_pattern != CylindricalPathPattern::kHelical) {
        gcode += writer->writeAfterRegion(RegionType::kPerimeter);
    }

    return gcode;
}

void CylindricalLayer::compute() {
    // Paths are generated up front by the cylindrical slicers.
}

void CylindricalLayer::calculateModifiers(Point& currentLocation) {
    QVector<Path> print_paths;
    print_paths.reserve(m_paths.size());
    for (Path path : m_paths) {
        path.removeTravels();
        if (firstPrintSegment(path) != nullptr) { print_paths.push_back(path); }
    }

    if (print_paths.isEmpty()) {
        m_paths.clear();
        return;
    }

    QVector<Path> ordered_paths;
    ordered_paths.reserve(print_paths.size());
    PathOrderOptimizer path_optimizer(currentLocation, getLayerNumber(), m_sb);
    path_optimizer.setPathsToEvaluate(print_paths);

    while (path_optimizer.getCurrentPathCount() > 0) {
        bool starts_from_generated_end = false;
        Path next_path                 = m_path_pattern == CylindricalPathPattern::kHelical
                                             ? path_optimizer.linkNextHelicalPath(&starts_from_generated_end)
                                             : path_optimizer.linkNextRadialPath();
        if (next_path.size() > 0) {
            restoreCylindricalPathSettings(next_path, m_sb);
            if (m_path_pattern == CylindricalPathPattern::kHelical) {
                applyHelicalToolOffsetForOrderedDirection(next_path, m_sb, starts_from_generated_end);
            }
            ordered_paths.push_back(next_path);
        }
    }

    m_paths = ordered_paths;
}

Point CylindricalLayer::getStartLocation() const {
    if (m_paths.isEmpty() || m_paths.first().size() == 0) { return Point(0, 0, 0); }

    return m_paths.first().front()->start();
}

float CylindricalLayer::getMinZ() {
    float min_z = std::numeric_limits<float>::max();
    for (const Path& path : m_paths) {
        for (const QSharedPointer<SegmentBase>& segment : path) { min_z = std::min(min_z, segment->getMinZ()); }
    }

    return min_z == std::numeric_limits<float>::max() ? 0.0f : min_z;
}

Point CylindricalLayer::getEndLocation() {
    if (m_paths.isEmpty() || m_paths.last().size() == 0) { return Point(0, 0, 0); }

    return m_paths.last().back()->end();
}

bool CylindricalLayer::hasPaths() const {
    return !m_paths.isEmpty();
}
}  // namespace ORNL
