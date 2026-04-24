#include "geometry/settings_polygon.h"

#include <clipper.hpp>
#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/point.h"
#include "geometry/polygon.h"

namespace ORNL {
SettingsPolygon::SettingsPolygon(QVector<Polygon> geometry, QSharedPointer<SettingsBase>& sb) {
    m_sb = sb;

    // Set geometry from cs.
    if (!geometry.isEmpty()) {
        this->addAll(geometry);
    }
}

QVector<Point> SettingsPolygon::clipLine(Point start, Point end) const {
    ClipperLib2::PolyTree poly_tree;
    ClipperLib2::Paths paths;

    ClipperLib2::Path line;
    line.push_back(start.toIntPoint());
    line.push_back(end.toIntPoint());

    ClipperLib2::Clipper clipper;
    clipper.AddPath(line, ClipperLib2::ptSubject, false);
    clipper.AddPaths((*this)(), ClipperLib2::ptClip, true);

    clipper.Execute(ClipperLib2::ctIntersection, poly_tree, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    ClipperLib2::OpenPathsFromPolyTree(poly_tree, paths);

    QVector<Point> rv;
    for (ClipperLib2::Path path : paths) {
        for (auto int_point : path) {
            Point p(int_point);

            if (p != start && p != end) {
                p.setSettings(m_sb);
                rv.push_back(p);
            }
        }
    }
    return rv;
}

QSharedPointer<SettingsBase> SettingsPolygon::getSettings() { return m_sb; }

QSharedPointer<SettingsBase> SettingsPolygon::getSettings() const { return m_sb; }
} // namespace ORNL
