#include "widgets/graphics_view/grid_scene.h"

#include <QDebug>
#include <QPainter>
#include <qcolor.h>
#include <qgraphicsscene.h>
#include <qnamespace.h>
#include <qobject.h>

#include "utilities/mathutils.h"

namespace ORNL {
GridScene::GridScene(QObject* parent) : QGraphicsScene(parent) { m_grid_step = 20; }

void GridScene::setGridStep(int step) { m_grid_step = step; }

void GridScene::drawBackground(QPainter* painter, const QRectF& rect) {
    // Draw grid lines.
    QColor grid_color = QColor(Qt::blue);
    grid_color.setAlpha(128);

    QPen p;
    p.setColor(grid_color);
    p.setWidth(1);
    painter->setPen(p);

    QRectF scene_rect = this->sceneRect();
    int scene_left = MathUtils::snap(scene_rect.left(), m_grid_step);
    int scene_right = MathUtils::snap(scene_rect.right(), m_grid_step);
    int scene_top = MathUtils::snap(scene_rect.top(), m_grid_step);
    int scene_bottom = MathUtils::snap(scene_rect.bottom(), m_grid_step);

    for (int i = scene_left; i < scene_right; i += m_grid_step * 100) {
        painter->drawLine(i, scene_top, i, scene_bottom);
    }
    for (int i = scene_top; i < scene_bottom; i += m_grid_step * 100) {
        painter->drawLine(scene_left, i, scene_right, i);
    }
}

void GridScene::drawForeground(QPainter* painter, const QRectF& rect) {
    // If close enough, draw dots.
    if (!(rect.width() > m_grid_step * 100)) {
        QColor dot_color = QColor(Qt::red);
        dot_color.setAlpha(128);

        painter->setPen(dot_color);

        int left = MathUtils::snap(rect.left(), m_grid_step) - m_grid_step;
        int right = MathUtils::snap(rect.right(), m_grid_step) + m_grid_step;
        int top = MathUtils::snap(rect.top(), m_grid_step) - m_grid_step;
        int bottom = MathUtils::snap(rect.bottom(), m_grid_step) + m_grid_step;

        for (int i = left; i < right; i += m_grid_step) {
            for (int j = top; j < bottom; j += m_grid_step) {
                painter->drawPoint(i, j);
            }
        }
    }

    // Draw the origin
    QColor origin_color = Qt::red;
    origin_color.setAlpha(128);

    QPen p;
    p.setColor(origin_color);
    p.setWidthF(0.5);
    painter->setPen(p);

    painter->drawLine(-5, 0, 5, 0);
    painter->drawLine(0, 5, 0, -5);
}
} // namespace ORNL
