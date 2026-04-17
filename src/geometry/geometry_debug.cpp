#include "geometry/geometry_debug.h"

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <qcontainerfwd.h>
#include <qlogging.h>

#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"

namespace {
QMutex& desmosOutputMutex() {
    static QMutex output_mutex;
    return output_mutex;
}

void printDesmosSegment(const ORNL::Point& start, const ORNL::Point& end) {
    qDebug().noquote().nospace() << Qt::fixed << qSetRealNumberPrecision(1) << "polygon((" << start.x() << ","
                                 << start.y() << "),(" << end.x() << "," << end.y() << "))";
}

void printDesmosPolyline(const ORNL::Polyline& polyline) {
    if (polyline.size() < 2) {
        return;
    }

    for (int i = 0; i < polyline.size() - 1; ++i) {
        printDesmosSegment(polyline[i], polyline[i + 1]);
    }
}

void printDesmosPolygon(const ORNL::Polygon& polygon) {
    if (polygon.size() < 2) {
        return;
    }

    for (int i = 0; i < polygon.size() - 1; ++i) {
        printDesmosSegment(polygon[i], polygon[i + 1]);
    }
    printDesmosSegment(polygon.last(), polygon.first());
}

void printDesmosEdgeList(const ORNL::GeometryDebug::EdgeList& edge_list) {
    for (const QPair<ORNL::Point, ORNL::Point>& edge : edge_list) {
        printDesmosSegment(edge.first, edge.second);
    }
}

} // namespace

namespace ORNL {
namespace GeometryDebug {
void printDesmos(const Polyline& polyline, QString label) {
    QMutexLocker locker(&desmosOutputMutex());
    if (!label.isEmpty()) {
        qDebug() << label;
    }
    printDesmosPolyline(polyline);
}

void printDesmos(const QVector<Polyline>& polylines, QString label) {
    QMutexLocker locker(&desmosOutputMutex());
    if (!label.isEmpty()) {
        qDebug() << label;
    }
    for (const Polyline& polyline : polylines) {
        printDesmosPolyline(polyline);
    }
}

void printDesmos(const QVector<QVector<Polyline>>& geometry, QString label) {
    QMutexLocker locker(&desmosOutputMutex());
    if (!label.isEmpty()) {
        qDebug() << label;
    }
    for (const QVector<Polyline>& polyline_group : geometry) {
        for (const Polyline& polyline : polyline_group) {
            printDesmosPolyline(polyline);
        }
    }
}

void printDesmos(const Polygon& polygon, QString label) {
    QMutexLocker locker(&desmosOutputMutex());
    if (!label.isEmpty()) {
        qDebug() << label;
    }
    printDesmosPolygon(polygon);
}

void printDesmos(const PolygonList& polygon_list, QString label) {
    QMutexLocker locker(&desmosOutputMutex());
    if (!label.isEmpty()) {
        qDebug() << label;
    }
    for (const Polygon& polygon : polygon_list) {
        printDesmosPolygon(polygon);
    }
}

void printDesmos(const EdgeList& edge_list, QString label) {
    QMutexLocker locker(&desmosOutputMutex());
    if (!label.isEmpty()) {
        qDebug() << label;
    }
    printDesmosEdgeList(edge_list);
}

} // namespace GeometryDebug
} // namespace ORNL
