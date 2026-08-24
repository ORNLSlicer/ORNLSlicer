#include "step/layer/radial_layer.h"

#include <cmath>
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
#include "step/layer/layer.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! @brief Segment setting key used by the radial writer to recover the cylinder center X.
const QString kRadialCenterX = "radial_center_x";

//! @brief Segment setting key used by the radial writer to recover the cylinder center Y.
const QString kRadialCenterY = "radial_center_y";

//! @brief Group of disconnected arcs that belong to the same horizontal radial circle.
struct CirclePathGroup {
    long long z_key;
    QVector<Path> paths;
};

//! @brief Returns the first printable radial segment in a path, or nullptr when the path only contains travels.
QSharedPointer<SegmentBase> firstPrintSegment(const Path& path) {
    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (dynamic_cast<TravelSegment*>(segment.data()) == nullptr) { return segment; }
    }

    return nullptr;
}

//! @brief Returns segment settings from the first printable segment in a path.
QSharedPointer<SettingsBase> firstPrintSettings(const Path& path, const QSharedPointer<SettingsBase>& fallback) {
    QSharedPointer<SegmentBase> print_segment = firstPrintSegment(path);
    if (print_segment != nullptr) { return print_segment->getSb(); }

    return fallback;
}

//! @brief Returns the radial center stored on the path's printable segment settings.
Point radialCenter(const Path& path) {
    QSharedPointer<SettingsBase> settings = firstPrintSettings(path, QSharedPointer<SettingsBase>::create());
    return Point(settings->setting<Distance>(kRadialCenterX), settings->setting<Distance>(kRadialCenterY), 0);
}

//! @brief Key used to group arcs that share the same radial circle.
long long circleKey(const Path& path) {
    QSharedPointer<SegmentBase> print_segment = firstPrintSegment(path);
    return print_segment == nullptr ? 0 : std::llround(print_segment->start().z());
}

//! @brief Adds a path to the group matching its bead Z, preserving first-seen group order.
void addToCircleGroup(QVector<CirclePathGroup>& groups, const Path& path) {
    const long long key = circleKey(path);
    for (CirclePathGroup& group : groups) {
        if (group.z_key == key) {
            group.paths.push_back(path);
            return;
        }
    }

    groups.push_back(CirclePathGroup {key, QVector<Path> {path}});
}

//! @brief Restores radial print metadata and gives optimizer-created travels radial center settings.
void restoreRadialPathSettings(Path& path, const QSharedPointer<SettingsBase>& layer_settings) {
    QSharedPointer<SettingsBase> print_settings = firstPrintSettings(path, layer_settings);

    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (dynamic_cast<TravelSegment*>(segment.data()) != nullptr) {
            QSharedPointer<SettingsBase> travel_settings = QSharedPointer<SettingsBase>::create(*print_settings);
            travel_settings->setSetting(SS::kSpeed, layer_settings->setting<Velocity>(PS::Travel::kSpeed));
            travel_settings->setSetting(SS::kIsRegionStartSegment, true);
            travel_settings->setSetting(SS::kRegionType, RegionType::kPerimeter);
            segment->setSb(travel_settings);
        }
        else { segment->getSb()->setSetting(SS::kRegionType, RegionType::kPerimeter); }
    }
}
}  // namespace

RadialLayer::RadialLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb) : Layer(layer_nr, sb) {}

void RadialLayer::addPath(const Path& path) {
    if (path.size() > 0) { m_paths.push_back(path); }
}

QString RadialLayer::writeGCode(QSharedPointer<WriterBase> writer) {
    if (m_paths.isEmpty()) { return writer->writeEmptyStep(); }

    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kPerimeter, m_paths.size());
    for (Path& path : m_paths) {
        if (path.size() == 0) { continue; }

        gcode += writer->writeBeforePath(RegionType::kPerimeter);
        for (const QSharedPointer<SegmentBase>& segment : path) { gcode += segment->writeGCode(writer); }
        gcode += writer->writeAfterPath(RegionType::kPerimeter);
    }
    gcode += writer->writeAfterRegion(RegionType::kPerimeter);

    return gcode;
}

void RadialLayer::compute() {
    // Paths are generated up front by RadialSlicer.
}

void RadialLayer::calculateModifiers(Point& currentLocation) {
    QVector<CirclePathGroup> circle_groups;
    for (Path path : m_paths) {
        path.removeTravels();
        if (firstPrintSegment(path) != nullptr) { addToCircleGroup(circle_groups, path); }
    }

    if (circle_groups.isEmpty()) {
        m_paths.clear();
        return;
    }

    QVector<Path> optimized_paths;
    optimized_paths.reserve(m_paths.size());
    for (CirclePathGroup& group : circle_groups) {
        PathOrderOptimizer path_optimizer(currentLocation, getLayerNumber(), m_sb);
        path_optimizer.setPathsToEvaluate(group.paths);
        const Point center = radialCenter(group.paths.front());

        while (path_optimizer.getCurrentPathCount() > 0) {
            Path next_path = path_optimizer.linkNextRadialPath(center);
            if (next_path.size() > 0) {
                restoreRadialPathSettings(next_path, m_sb);
                optimized_paths.push_back(next_path);
            }
        }
    }

    m_paths = optimized_paths;
}

Point RadialLayer::getStartLocation() const {
    if (m_paths.isEmpty() || m_paths.first().size() == 0) { return Point(0, 0, 0); }

    return m_paths.first().front()->start();
}

float RadialLayer::getMinZ() {
    float min_z = std::numeric_limits<float>::max();
    for (const Path& path : m_paths) {
        for (const QSharedPointer<SegmentBase>& segment : path) { min_z = std::min(min_z, segment->getMinZ()); }
    }

    return min_z == std::numeric_limits<float>::max() ? 0.0f : min_z;
}

Point RadialLayer::getEndLocation() {
    if (m_paths.isEmpty() || m_paths.last().size() == 0) { return Point(0, 0, 0); }

    return m_paths.last().back()->end();
}

bool RadialLayer::hasPaths() const {
    return !m_paths.isEmpty();
}
}  // namespace ORNL
