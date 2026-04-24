#pragma once

#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QQueue>
#include <QRadioButton>
#include <QSet>
#include <QString>
#include <QToolTip>
#include <QVector>
#include <QWidget>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qsharedpointer.h>
#include <qsize.h>
#include <qtmetamacros.h>

#include "configs/settings_base.h"
#include "configs/settings_range.h"
#include "part/part.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {
class LayerDot;

/*!
 * \class LayerBar
 * \brief Widget that allows for ranged selections.
 */
class LayerBar : public QWidget {
    Q_OBJECT
    friend class LayerDot;

  public:
    //! \brief Constructor.
    explicit LayerBar(QSharedPointer<PartMetaModel> pm, QWidget* parent = nullptr);

    //! \brief Destructor.
    ~LayerBar();

    //! \brief Struct to represent the range graphically.
    struct dot_range {
        LayerDot* a;
        LayerDot* b;
    };

    //! \brief Struct to hold grouped dots
    struct dot_group {
        QString group_name;
        QVector<LayerDot*> grouped;
    };

    //! \brief Size hint for containing layout.
    QSize sizeHint() const;

    //! \brief gets the number of layers in this part
    //! \return an int
    int getLayerCount();

  signals:
    //! \brief Signal that the selection has been altered.
    //! \param name_and_bases: Label plus list of ranges currently selected
    void setSelectedSettings(QPair<QString, QList<QSharedPointer<SettingsBase>>> name_and_bases);

    //! \brief Signal that a settings range needs to be deleted
    void deleteDot(LayerDot* dot);

  public slots:
    //! \brief Slot to add a layer range to a part.
    void addRange(int lower, int upper);

    //! \brief Slot to add a layer range to a part from a template.
    //! \param lower: lower range value
    //! \param upper: upper range value
    //! \param sb: the range's settings base
    void addRangeFromTemplate(int lower, int upper, QSharedPointer<SettingsBase> sb);

    //! \brief Slot to add a single layer to a part.
    void addSingle(int layer);

    //! \brief Slot to add a single layer to a part from a template.
    //! \param layer: the single layer
    //! \param sb: the layer's settings base
    void addSingleFromTemplate(int layer, QSharedPointer<SettingsBase> sb);

    //! \brief Slot to change the currently selected part.
    void changePart(QSharedPointer<PartMetaItem> item);

    //! \brief Version of changePart() to change template applied to part while it's still selected.
    void reselectPart();

    //! \brief Slot to outright remove a part.
    void removePart(QSharedPointer<Part> part);

    //! \brief Clears the m_curr_dots set and sets the status of the dots to unselected.
    void clearSelection();

    //! \brief Select all dots on some interval.
    void selectOnInterval(int min, int max);

    //! \brief Selects all dots.
    void selectAll();

    //! \brief Deletes a range.
    void deleteRange(dot_range* range);

    //! \brief Splits a range.
    void splitRange(dot_range* range);

    //! \brief Dialog prompt to remove a dot from its group.
    void removeFromGrp(LayerDot* dot);

    //! \brief called when a setting changes, triggers layer recalculation
    //!        if the changed setting requires it
    void handleModifiedSetting(QString key);

    //! \brief Deletes a dot.
    void deleteSingle(LayerDot* dot);

    //! \brief Update the current layer count.
    void updateLayers();

    //! \brief Store layers deleted due to number of layers decreasing.
    //! \param layer_number: the layer that's been removed due to layer bar size decrease
    //! \param pair: its pair if it's in a range. Will also be removed
    void storeDeletedLayers(int layer_number, int pair);

    //! \brief Bring back deleted layers if number of layers increases to make them fit again.
    void returnDeletedLayers();

    //! \brief Returns array of deleted layers.
    QVector<SettingsRange> getDeletedRanges();

    //! \brief Used to sort deleted ranges by the top value, since they will be brought back in that order.
    //! \param a,b: 2 settings ranges to be compared by top value
    static bool sortByTop(SettingsRange a, SettingsRange b);

    //! \brief Clear all container in the bar.
    void clear();

    //! \brief Clear dots and deleted ranges if <no template selected>.
    void clearTemplate();

  private slots:
    // -- QActions --

    //! \brief Slot to delete the currently selected ranges.
    void deleteSelection();

    //! \brief Slot to popup a dialog to add a new selection.
    void addSelection();

    //! \brief Slot to popup a dialog to add a group of selections.
    void addGroup();

    //! \brief Slot to popup a dialog to set the layer manually.
    void setLayer();

    //! \brief Slot to move a layer pair manually
    void setPairLayers();

    //! \brief Slot to make a pair from two layer dots.
    void makePair();

    //! \brief Slot to split up a pair.
    void splitPair();

    //! \brief Slot to make a pair from zero or 1 dots
    void addPair();

    //! \brief Slot to group dots together.
    void groupDots();

    //! \brief Slot to ungroup dots.
    void ungroupDots();

    //! \brief called when a part is selected/ deselected. Updates bar
    //! \param item - the part that was changed
    void selectionUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief determines if a scale or rotation event occurred. Updates bar
    //! \param item - the part that was changed
    void transformUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief determines if a visual change should change the layer bar (ie mesh type)
    //! \param item - the part that was changed
    void visualUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief disables layerbar when a part is removed
    //! \param item - the part that was removed
    void removalUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief Loads layers unto layer bar when new part is selected.
    void loadTemplateLayers();

  private:
    // -- Qt Overrides --
    void paintEvent(QPaintEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void contextMenuEvent(QContextMenuEvent* event);
    void resizeEvent(QResizeEvent* event);

    // -- Helper Functions --

    //! \brief Paint the division lines seen in the background. Used by paintEvent().
    void paintDivisions(QPainter* painter);

    //! \brief Paint the fill inbetween layer ranges. Used by paintEvent().
    void paintRanges(QPainter* painter);

    //! \brief Adds a dot to the selected layer.
    //! \param from_template - whether the dot is from a template or added by user.
    LayerDot* addDot(int layer, bool from_template);

    //! \brief Returns the current status of the layer being moved to.
    bool layerValid(int layer);

    //! \brief Returns the layer from a y position.
    int getLayerFromPosition(int y_coord);

    //! \brief Returns the y position from the layer.
    int getPositionFromLayer(int layer);

    //! \brief Move dot to layer specified.
    bool moveDotToLayer(LayerDot* dot, int layer);

    //! \brief Finds the next available layer and inserts the dot there.
    bool moveDotToNextLayer(LayerDot* dot);

    //! \brief Finds the next available layer and inserts the dot there.
    bool moveDotToNextLayer(LayerDot* dot, int layer);

    //! \brief Checks a dot against the layer to determine if this layer is occupied by this dot.
    bool onLayer(LayerDot* dot, int layer);

    //! \brief determines what ranges are selected and emits accordingly
    void changeSelectedSettings();

    //! \brief Select a given dot and its group/range
    void selectDot(LayerDot* dot);

    //! \brief Deselect a given dot and its group/range
    void deselectDot(LayerDot* dot);

    //! \brief Sets up the actions used by the context menu.
    void setupActions();

    // -- Member Variables --

    //! \brief Pointer to current part.
    QSharedPointer<Part> m_part;

    //! \brief the part meta model used for tracking changes
    QSharedPointer<PartMetaModel> m_model;

    //! \brief the number of layers displayed
    int m_layers;

    //! \brief Vector which keeps track of which dots are where.
    QVector<LayerDot*> m_position;

    //! \brief Set of dots currently selected.
    QVector<LayerDot*> m_selection;

    QVector<SettingsRange> m_deleted_ranges;

    //! \brief Float that saves the pixels BETWEEN divisions; used for painting.
    float m_px_divs;

    //! \brief Float that saves the pixels INCLUDING divisions; used for painting.
    float m_px_divs_inc;

    //! \brief Int to define how many divs to skip and stretch for the layerdot.
    int m_skip;

    //! \brief tracks the dot last clicked/selected by the user
    //! \note this is not inherently the same as dot most recently added to m_selection,
    //!       i.e. m_selection.back(), because clicking on a grouped/ranged layer will select
    //!       its group-mates and add additional dots to m_selection
    LayerDot* m_last_clicked_dot;

    //! \brief flag used to tell if we need to track the changes of a dot, used primarily
    //!        when user is dragging a dot to new location
    bool m_track_change;

    //! \brief flag to track if dots should be deselected on mouseRelease
    bool m_should_deselect;

    //! \brief location of dot before user drags dot to new location
    int m_original_y;

    // int m_top_value;

    // -- QActions --
    QAction* m_join_act;
    QAction* m_split_act;
    QAction* m_add_pair;
    QAction* m_pair_from_one;
    QAction* m_set_layer_act;
    QAction* m_set_range_layers_act;
    QAction* m_add_act;
    QAction* m_add_group;
    QAction* m_delete_act;
    QAction* m_clear_act;
    QAction* m_group_dots;
    QAction* m_ungroup_dots;
    QAction* m_select_all;
};
} // Namespace ORNL
