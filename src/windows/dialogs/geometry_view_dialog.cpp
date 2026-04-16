#include "windows/dialogs/geometry_view_dialog.h"

#include <qboxlayout.h>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qdialog.h>
#include <qgraphicsscene.h>
#include <qpolygon.h>
#include <qwidget.h>

#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "widgets/graphics_view/grid_scene.h"
#include "widgets/graphics_view/polyline_item.h"
#include "widgets/scrollable_graphics_view.h"

namespace ORNL {

GeometryView::GeometryView(QWidget* parent) : QDialog(parent) {
    m_layout = new QHBoxLayout(this);
    m_polygon_view = new ScrollableGraphicsView(this);
    m_polygon_view->setScene(new GridScene(m_polygon_view));
    m_polygon_view->scene()->setSceneRect(-1000000, -1000000, 2000000, 2000000);
    // Flip y-axis so view is more standard.
    m_polygon_view->scale(1, -1);

    m_layout->addWidget(m_polygon_view);
}

void GeometryView::addPolyline(Polyline polyline, QColor color) {
    QGraphicsScene* gs = m_polygon_view->scene();

    static const QVector<QColor> colors = {
        QColor(255, 0, 0),  QColor(0, 255, 0),     QColor(0, 0, 255),   QColor(100, 0, 50),   QColor(255, 255, 0),
        QColor(0, 128, 64), QColor(127, 0, 255),   QColor(255, 128, 0), QColor(255, 64, 0),   QColor(255, 128, 44),
        QColor(64, 128, 0), QColor(255, 255, 122), QColor(50, 50, 50),  QColor(100, 128, 128)};

    if (!color.isValid()) {
        color = colors[gs->items().size() % colors.size()];
    }

    PolylineGraphicsItem* poly_graphic = new PolylineGraphicsItem(polyline);
    poly_graphic->setColor(color);

    gs->addItem(poly_graphic);

    m_polygon_view->update();
}

void GeometryView::addPolygon(PolygonList polygon, QColor color) {
    QGraphicsScene* gs = m_polygon_view->scene();

    static const QVector<QColor> colors = {
        QColor(255, 0, 0),  QColor(0, 255, 0),     QColor(0, 0, 255),   QColor(100, 0, 50),   QColor(255, 255, 0),
        QColor(0, 128, 64), QColor(127, 0, 255),   QColor(255, 128, 0), QColor(255, 64, 0),   QColor(255, 128, 44),
        QColor(64, 128, 0), QColor(255, 255, 122), QColor(50, 50, 50),  QColor(100, 128, 128)};

    if (!color.isValid()) {
        color = colors[gs->items().size() % colors.size()];
    }

    QVector<QPolygon> polygons = polygon.toQPolygons();

    for (const QPolygon& poly : polygons) {
        gs->addPolygon(poly, color, color);
    }
}

void GeometryView::jumpTo(Point location) { m_polygon_view->centerOn(location.x(), location.y()); }

void GeometryView::clear() { m_polygon_view->scene()->clear(); }
} // namespace ORNL
