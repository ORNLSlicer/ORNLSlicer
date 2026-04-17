#pragma once

#include <QColor>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPropertyAnimation>
#include <QString>
#include <QWidget>
#include <qcontainerfwd.h>
#include <qcoreevent.h>
#include <qnamespace.h>
#include <qtmetamacros.h>

#include "widgets/layerbar.h"

namespace ORNL {
/*!
 * \class LayerDot
 * \brief Visual representation of a layer selection.
 */
class LayerDot : public QWidget {
    Q_OBJECT
    friend class LayerBar;

  public:
    //! \brief Constructor.
    explicit LayerDot(QWidget* parent = nullptr, int new_layer = -1,
                      QVector<QColor> m_colors = {Qt::darkGreen, Qt::darkYellow, QColor(255, 172, 28), Qt::darkBlue,
                                                  Qt::white},
                      bool from_template = false);

    //! \brief Destructor.
    ~LayerDot();

    //! \brief Public function to obtain internal layer.
    int getLayer();

    //! \brief Get the currently displayed layer.
    int getDisplayLayer();

    //! \brief Get the range for this dot.
    LayerBar::dot_range* getRange();

    //! \brief If this dot is in a range, get its pair.
    LayerDot* getPair();

    //! \brief Get dot group number
    QString getGroupName();

    //! \brief Check if dot is in a group and, if so, return the group
    LayerBar::dot_group* getGroup();

    //! \brief Get the selection status of the dot.
    bool isSelected();

    //! \brief Sets dot color to red (all dots from template are red).
    void isFromTemplate();

  signals:
    //! \brief Signal that the layer was updated.
    void layerChanged(int layer);

  public slots:
    //! \brief Slot to set the status of the dot (the color).
    void setSelected(bool status);
    //! \brief Slot that sets the layer.
    void setLayer(int layer);
    //! \brief Slot that sets the range for this dot.
    void setRange(LayerBar::dot_range* range);
    //! \brief Slot to put this dot in a group.
    void setGroup(LayerBar::dot_group* group);
    //! \brief Slot that allows the dot to display any int without changing the internal layer.
    void setDisplayLayer(int layer);
    //! \brief Animated move function.
    void smoothMove(int x, int y);
    //! \brief Set the shrink factor.
    void setShrink(int shrink);

  private:
    // -- Qt Overrides --

    void paintEvent(QPaintEvent* event);
    void enterEvent(QEvent* event);
    void leaveEvent(QEvent* event);

    // -- Helper Functions --

    //! \brief Paint the dot with the number inside. Used by paintEvent().
    void paintLayerDot(QPainter* painter);

    //! \brief Updates the tooltip.
    void updateTooltip();

    // -- Member Variables --

    //! \brief Layer.
    int m_layer;

    //! \brief Display layer. Desynced from actual layer, changes as dot moves.
    int m_display_layer;

    //! \brief Number of pixels to vertically shrink.
    int m_shrink;

    //! \brief Range that this dot belongs to.
    LayerBar::dot_range* m_range;

    //! \brief the group that this dot belongs to
    LayerBar::dot_group* m_group;

    //! \brief Animation for moving.
    QPropertyAnimation* m_move_ani;

    //! \brief Selection status.
    bool m_selected;

    //! \brief Whether dot is from template.
    bool m_from_template;

    // --- Colors ---

    //! \brief Current color.
    QColor m_color;
    QColor m_default_color; // default color is red if from template, green is added by user.
    QVector<QColor> m_dot_colors;
};
} // Namespace ORNL
