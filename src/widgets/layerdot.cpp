#include "widgets/layerdot.h"

#include <qbrush.h>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qcoreevent.h>
#include <qevent.h>
#include <qhashfunctions.h>
#include <qnamespace.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qpropertyanimation.h>
#include <qtmetamacros.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "widgets/layerbar.h"

namespace ORNL {
LayerDot::LayerDot(QWidget* parent, int new_layer, QVector<QColor> m_colors, bool from_template)
    : QWidget(parent), m_layer(new_layer), m_range(nullptr), m_group(nullptr) {
    // this->setCursor(Qt::SizeVerCursor);
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedSize(20, 20);

    m_move_ani = new QPropertyAnimation(this, "pos");
    m_move_ani->setDuration(0);

    m_dot_colors = m_colors;
    if (!from_template) // Default color orange if dot from template. Green if dot entered by user.
        m_default_color = m_dot_colors.at(
            0); // dot colors = [0]base_color, [1]hover_color, [2]selected_color, [3]group_color, [4]label_color
    else
        m_default_color = m_dot_colors.at(2);
    m_color = m_default_color;
    m_shrink = 0;
    m_selected = false;
    m_from_template = false;
}

LayerDot::~LayerDot() { delete m_move_ani; }

void LayerDot::setSelected(bool status) {
    m_selected = status;
    if (m_selected)
        this->setCursor(Qt::SizeVerCursor);
    else
        this->setCursor(Qt::PointingHandCursor);

    this->update();
}

void LayerDot::setLayer(int layer) {
    m_layer = m_display_layer = layer;
    this->updateTooltip();
    emit layerChanged(m_layer);
}

void LayerDot::setRange(LayerBar::dot_range* range) {
    m_range = range;
    this->updateTooltip();
}

void LayerDot::setGroup(LayerBar::dot_group* group) {
    m_group = group;
    if (group != nullptr) {
        m_color = m_dot_colors.at(3);
        m_color.setAlpha(50);
        this->updateTooltip();
    }
    else {
        m_color = m_default_color;
        this->updateTooltip();
    }
}

void LayerDot::setDisplayLayer(int layer) {
    m_display_layer = layer;
    this->update();
}

void LayerDot::smoothMove(int x, int y) {
    m_move_ani->setStartValue(this->pos());
    m_move_ani->setEndValue(QPoint(x, y));

    m_move_ani->start();
}

void LayerDot::setShrink(int shrink) {
    if (shrink > 17 || shrink < 0)
        shrink = 17;
    m_shrink = shrink;
}

int LayerDot::getLayer() { return m_layer; }

int LayerDot::getDisplayLayer() { return m_display_layer; }

LayerBar::dot_range* LayerDot::getRange() { return m_range; }

LayerDot* LayerDot::getPair() {
    if (m_range == nullptr)
        return nullptr;
    return (m_range->a == this) ? m_range->b : m_range->a;
}

QString LayerDot::getGroupName() {
    if (m_group != nullptr) {
        return m_group->group_name;
    }
    return "";
}

LayerBar::dot_group* LayerDot::getGroup() { return m_group; }

bool LayerDot::isSelected() { return m_selected; }

void LayerDot::isFromTemplate() {
    m_from_template = true;
    m_color = m_dot_colors.at(2);
}

void LayerDot::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);
    this->paintLayerDot(&painter);
}

void LayerDot::enterEvent(QEvent* event) {
    m_color = m_dot_colors.at(1);
    this->update();
}

void LayerDot::leaveEvent(QEvent* event) {
    if (m_group == nullptr) {
        m_color = m_default_color;
        this->update();
    }
    else {
        m_color = m_dot_colors.at(3);
        m_color.setAlpha(50);
        this->update();
    }
}

void LayerDot::paintLayerDot(QPainter* painter) {
    const QPoint upper_corner(0, m_shrink), lower_corner(19, 19 - m_shrink);

    // Setup the fill and the circle dimensions.
    QBrush filler((m_selected) ? m_dot_colors.at(3) : m_color, Qt::SolidPattern); // blue if selected
    QRect circle_bounds(upper_corner, lower_corner);

    // Draw the circle.
    painter->setBrush(filler);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(circle_bounds);

    // Draw the number.
    painter->setPen(m_dot_colors.at(4));
    painter->drawText(circle_bounds, Qt::AlignCenter, QString::number(m_display_layer + 1));
}

void LayerDot::updateTooltip() {
    if (m_range == nullptr && m_group == nullptr)
        this->setToolTip("Layer Selection " + QString::number(m_layer + 1));
    else if (m_group != nullptr)
        this->setToolTip("Layer Selection " + QString::number(m_layer + 1) + " of " + this->getGroupName());
    else
        this->setToolTip("Range Selection Layer " + QString::number(m_layer + 1) + " to " +
                         QString::number(this->getPair()->getLayer() + 1));
}
} // Namespace ORNL
