#pragma once

#include <cstdint>

#include <QGraphicsItem>
#include <qcolor.h>
#include <qobject.h>

#include "geometry/polyline.h"

namespace ORNL {
class PolylineGraphicsItem : public QGraphicsItem {
  public:
    PolylineGraphicsItem(QGraphicsItem* parent = nullptr);

    PolylineGraphicsItem(const Polyline& polyline, QGraphicsItem* parent = nullptr);

    void setPolyline(const Polyline& polyline);

    void setSegmentDraw(uint32_t segment_no);

    Polyline getPolyline() const;

    void setColor(QColor color);

    QRectF boundingRect() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  private:
    Polyline m_polyline;

    uint32_t m_draw_to;

    QRect m_bounding_rect;

    QColor m_color;
};
} // namespace ORNL
