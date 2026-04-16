#include "widgets/graphics_view/polyline_item.h"

#include <cstdint>

#include <QPainter>
#include <qgraphicsitem.h>
#include <qnamespace.h>
#include <qobject.h>

#include "geometry/point.h"
#include "geometry/polyline.h"

namespace ORNL {
PolylineGraphicsItem::PolylineGraphicsItem(QGraphicsItem* parent) : QGraphicsItem(parent) { m_color = Qt::green; }

PolylineGraphicsItem::PolylineGraphicsItem(const Polyline& polyline, QGraphicsItem* parent) : QGraphicsItem(parent) {
    this->setPolyline(polyline);
    m_color = Qt::green;
}

void PolylineGraphicsItem::setPolyline(const Polyline& polyline) {
    m_polyline = polyline;
    m_bounding_rect = QRect(m_polyline.min().toQPoint() + QPoint(2, 2), m_polyline.max().toQPoint() + QPoint(2, 2));

    m_draw_to = polyline.size();
}

void PolylineGraphicsItem::setSegmentDraw(uint32_t segment_no) { m_draw_to = segment_no; }

Polyline PolylineGraphicsItem::getPolyline() const { return m_polyline; }

void PolylineGraphicsItem::setColor(QColor color) { m_color = color; }

QRectF PolylineGraphicsItem::boundingRect() const { return m_bounding_rect; }

void PolylineGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    if (m_polyline.count() <= 1)
        return;

    QPen line_pen;
    line_pen.setColor(m_color);
    line_pen.setWidth(10);

    QPen vertex_pen;
    vertex_pen.setColor(Qt::black);
    vertex_pen.setWidth(12);

    painter->setPen(line_pen);

    Point previous = m_polyline.first();
    for (int i = 1; i < m_draw_to; i++) {
        Point current = m_polyline[i];
        painter->drawLine(previous.toQPoint(), current.toQPoint());

        previous = current;
    }

    painter->setPen(vertex_pen);
    for (int i = 0; i < m_draw_to; i++) {
        painter->drawPoint(m_polyline[i].toQPoint());
    }
}
} // namespace ORNL
