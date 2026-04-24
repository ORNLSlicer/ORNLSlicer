#include "widgets/scrollable_graphics_view.h"

#include <QDebug>
#include <QLabel>
#include <QResizeEvent>
#include <QWheelEvent>
#include <qgraphicsview.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qsize.h>
#include <qwidget.h>

namespace ORNL {
ScrollableGraphicsView::ScrollableGraphicsView(QWidget* parent) : QGraphicsView(parent) {
    this->setViewportUpdateMode(ViewportUpdateMode::SmartViewportUpdate);
    this->setDragMode(QGraphicsView::ScrollHandDrag);
    this->setMouseTracking(true);

    m_pos_label = new QLabel("(0, 0)", this);
    m_scroll_factor = 0.1;
}

void ScrollableGraphicsView::setScrollFactor(double scroll_factor) { m_scroll_factor = scroll_factor; }

double ScrollableGraphicsView::scrollFactor() { return m_scroll_factor; }

void ScrollableGraphicsView::resizeEvent(QResizeEvent* event) {
    QSize new_size = event->size();
    int new_width = new_size.width();
    int new_height = new_size.height();

    // Position label.
    int pos_width = m_pos_label->width();
    int pos_height = m_pos_label->height();
    int pos_new_x = new_width - pos_width - 10;
    int pos_new_y = new_height - pos_height - 10;

    m_pos_label->move(pos_new_x, pos_new_y);
}

void ScrollableGraphicsView::wheelEvent(QWheelEvent* event) {
    // Disallow scrolling beyond bounds.
    QRectF visible_rect = this->mapToScene(this->viewport()->rect()).boundingRect();
    QRectF scene_rect = this->sceneRect();

    if (event->angleDelta().y() > 0) {
        double plus = 1.0 + m_scroll_factor;
        this->scale(plus, plus);
    }
    else {
        if (visible_rect.width() > scene_rect.width() && visible_rect.height() > scene_rect.height()) {
            return;
        }
        double minus = 1.0 - m_scroll_factor;
        this->scale(minus, minus);
    }
}

void ScrollableGraphicsView::mouseMoveEvent(QMouseEvent* event) {
    this->QGraphicsView::mouseMoveEvent(event);

    QPoint mouse_pos = event->pos();

    QPointF scene_pos = this->mapToScene(mouse_pos);

    m_pos_label->setText("(" + QString::number(scene_pos.x()) + ", " + QString::number(scene_pos.y()) + ")");
    m_pos_label->adjustSize();

    int width = this->viewport()->width();
    int height = this->viewport()->height();

    // Position label.
    int pos_width = m_pos_label->width();
    int pos_height = m_pos_label->height();
    int pos_new_x = width - pos_width - 10;
    int pos_new_y = height - pos_height - 10;

    m_pos_label->move(pos_new_x, pos_new_y);
}
} // namespace ORNL
