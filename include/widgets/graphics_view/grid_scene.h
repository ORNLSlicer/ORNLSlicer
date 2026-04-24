#pragma once

#include <QGraphicsScene>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qwindowdefs.h>

namespace ORNL {
class GridScene : public QGraphicsScene {
    Q_OBJECT
  public:
    GridScene(QObject* parent = nullptr);

    void setGridStep(int step);

  protected:
    void drawBackground(QPainter* painter, const QRectF& rect);

    void drawForeground(QPainter* painter, const QRectF& rect);

  private:
    int m_grid_step;
};
} // namespace ORNL
