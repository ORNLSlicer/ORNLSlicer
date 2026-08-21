#include "step/layer/helical_layer.h"

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
#include "step/layer/layer.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! @brief Segment setting key used by the radial writer to recover the cylinder center X.
const QString kRadialCenterX = "radial_center_x";

//! @brief Segment setting key used by the radial writer to recover the cylinder center Y.
const QString kRadialCenterY = "radial_center_y";

//! @brief Returns the first printable helical segment in a path, or nullptr when the path only contains travels.
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

//! @brief Restores helical print metadata and gives optimizer-created travels center settings.
void restoreHelicalPathSettings(Path& path, const QSharedPointer<SettingsBase>& layer_settings) {
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

HelicalLayer::HelicalLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb) : Layer(layer_nr, sb) {}

void HelicalLayer::addPath(const Path& path) {
    if (path.size() > 0) { m_paths.push_back(path); }
}

QString HelicalLayer::writeGCode(QSharedPointer<WriterBase> writer) {
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

void HelicalLayer::compute() {
    // Paths are generated up front by HelicalSlicer.
}

void HelicalLayer::calculateModifiers(Point& currentLocation) {
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
        Path next_path = path_optimizer.linkNextHelicalPath();
        if (next_path.size() > 0) {
            restoreHelicalPathSettings(next_path, m_sb);
            ordered_paths.push_back(next_path);
        }
    }

    m_paths = ordered_paths;
}

Point HelicalLayer::getStartLocation() const {
    if (m_paths.isEmpty() || m_paths.first().size() == 0) { return Point(0, 0, 0); }

    return m_paths.first().front()->start();
}

float HelicalLayer::getMinZ() {
    float min_z = std::numeric_limits<float>::max();
    for (const Path& path : m_paths) {
        for (const QSharedPointer<SegmentBase>& segment : path) { min_z = std::min(min_z, segment->getMinZ()); }
    }

    return min_z == std::numeric_limits<float>::max() ? 0.0f : min_z;
}

Point HelicalLayer::getEndLocation() {
    if (m_paths.isEmpty() || m_paths.last().size() == 0) { return Point(0, 0, 0); }

    return m_paths.last().back()->end();
}

bool HelicalLayer::hasPaths() const {
    return !m_paths.isEmpty();
}
}  // namespace ORNL
