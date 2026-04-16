#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QToolButton>
#include <QWheelEvent>
#include <QWidget>
#include <qhashfunctions.h>
#include <qnamespace.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class QSlicerDoubleSpinBox
 * \brief a custom spin box control that supports double values
 */
class QSlicerDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent required qwidget pointer
    QSlicerDoubleSpinBox(QWidget* parent = 0) : QDoubleSpinBox(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setKeyboardTracking(false);
    }

  protected:
    //! \brief mousePressEvent override
    //! \param QMouseEvent pointer
    virtual void mousePressEvent(QMouseEvent* event) {
        blockSignals(true);
        QDoubleSpinBox::mousePressEvent(event);
    }

    //! \brief mouseReleaseEvent override
    //! \param QMouseEvent pointer
    virtual void mouseReleaseEvent(QMouseEvent* event) {
        QDoubleSpinBox::mouseReleaseEvent(event);
        blockSignals(false);
    }

    //! \brief wheelEvent override
    //! \param QWheelEvent pointer
    virtual void wheelEvent(QWheelEvent* event) {
        if (!hasFocus()) {
            event->ignore();
            return;
        }

        blockSignals(true);
        QDoubleSpinBox::wheelEvent(event);
        blockSignals(false);
    }

    //! \brief keyPressEvent override
    //! \param QKeyEvent pointer
    virtual void keyPressEvent(QKeyEvent* event) {
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
            blockSignals(true);
        QDoubleSpinBox::keyPressEvent(event);
    }

    //! \brief keyReleaseEvent override
    //! \param QKeyEvent pointer
    virtual void keyReleaseEvent(QKeyEvent* event) {
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
            if (m_last_value != value()) {
                m_last_value = value();
                Q_EMIT valueChanged(value());
            }
        }
        QDoubleSpinBox::keyReleaseEvent(event);
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
            blockSignals(false);
    }

    //! \brief focusInEvent override
    //! \param QFocusEvent pointer
    virtual void focusInEvent(QFocusEvent* event) {
        m_last_value = value();
        QDoubleSpinBox::focusInEvent(event);
    }

    //! \brief focusOutEvent override
    //! \param QFocusEvent pointer
    virtual void focusOutEvent(QFocusEvent* event) {
        if (m_last_value != value()) {
            m_last_value = value();
            Q_EMIT valueChanged(value());
        }
        QDoubleSpinBox::focusOutEvent(event);
    }

  private:
    // Keeps track of the last value
    double m_last_value = 0;
};

// Forward
class PartMetaItem;
class PartMetaModel;

/*!
 * \class ToolbarInput
 * \brief a box that animates open to display part controls
 */
class ToolbarInput : public QFrame {
    Q_OBJECT
  public:
    //! \enum ModelTrackingType
    //! \brief the type of change this box should track
    enum class ModelTrackingType { kTranslation, kRotation, kScale };

    //! \brief Constructor
    //! \param parent required qwidget pointer
    //! \param optionalCombo should an extra units combo box be added
    explicit ToolbarInput(QWidget* parent, bool optionalCombo = false);

    //! \brief should wrapping be enabled
    //! \param w flag
    void setWrapping(bool w);

    //! \brief sets he model to track
    //! \param m model
    //! \param type type of change to track
    void setModel(QSharedPointer<PartMetaModel> m, ModelTrackingType type);

    //! \brief sets the position of the widget
    //! \param pos the new position
    void setPos(QPoint pos);

  public slots:
    //! \brief closes this input immediately(no animation)
    void closeInput();

    //! \brief toggles the input with animation
    void toggleInput();

    //! \brief sets a value
    //! \param axis the axis to set
    //! \param val the new value
    void setValue(Axis axis, double val);

    //! \brief gets the value of the axis
    //! \param axis the desired axis
    //! \return the value
    double getValue(Axis axis);

    //! \brief sets the combo box index
    //! \param index in the combo box
    void setIndex(int index);

    //! \brief sets the increment values of the spinbox
    //! \param inc the increment value
    void setIncrement(double inc);

    //! \brief sets the max value of all spin boxes
    //! \param val the new value
    void setMaximumValue(double val);

    //! \brief sets the max value of x spin box
    //! \param val the new value
    void setMaximumXValue(double val);

    //! \brief sets the max value of y spin box
    //! \param val the new value
    void setMaximumYValue(double val);

    //! \brief sets the max value of z spin box
    //! \param val the new value
    void setMaximumZValue(double val);

    //! \brief sets the min value of all spin boxes
    //! \param val the new value
    void setMinimumValue(double val);

    //! \brief sets the min value of x spin box
    //! \param val the new value
    void setMinimumXValue(double val);

    //! \brief sets the min value of y spin box
    //! \param val the new value
    void setMinimumYValue(double val);

    //! \brief sets the min value of z spin box
    //! \param val the new value
    void setMinimumZValue(double val);

    //! \brief sets the unit label text
    //! \param unit new text
    void setUnitText(QString unit);

    //! \brief sets the x label text
    //! \param unit new text
    void setXText(QString x);

    //! \brief sets the y label text
    //! \param unit new text
    void setYText(QString y);

    //! \brief sets the z label text
    //! \param unit new text
    void setZText(QString z);

    //! \brief Enables/ disables the input field
    //! \param a flag
    void disableX(bool has_settings_part);

    //! \brief Enables/ disables the input field
    //! \param a flag
    void disableY(bool has_settings_part);

    //! \brief Enables/ disables the input field
    //! \param a flag
    void disableZ(bool has_settings_part);

    //! \brief Enables/ disables the input field
    //! \param a flag
    void disableCombo(bool has_settings_part);

    //! \brief sets the precision of all spin boxes
    //! \param decimals the number of decimal points
    void setPrecision(uint decimals);

    //! \brief setup stylesheets for controlling theme
    void setupStyle();

  private slots:
    //! \brief reads a value
    //! \param val the value to read
    void readValue(double val);

    //! \brief reads the unit
    //! \param index the index to read
    void readUnit(int index);

    //! \brief locks scale controls
    //! \param toggle if scale should be locked
    void lockScale(bool toggle);

    //! \brief locks scale updates
    //! \param val the new value
    void lockScaleUpdate(double val);

    //! \brief updates based on model selection
    //! \param item the item in question
    void modelSelectionUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief updates based on model translation
    //! \param item the item in question
    void modelTranslationUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief updates based on model rotation
    //! \param item the item in question
    void modelRotationUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief updates based on model scale
    //! \param item the item in question
    void modelScaleUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief updates based on model removal
    //! \param item the item in question
    void modelRemovalUpdate(QSharedPointer<PartMetaItem> item);

  private:
    // Setup the static widgets and their layouts.
    void setupWidget();

    // 1. Setup the widgets.
    void setupSubWidgets();

    // 2. Setup the layouts.
    void setupLayouts();
    // 3. Setup the layouts by inserting all required widgets.
    void setupInsert();
    // 4. Setup Animations
    void setupAnimations();
    // 5. Setup the events for the various widets.
    void setupEvents();
    // 5.1. Setup the events for the spin box value edits.
    void setupValueEditEvents(bool disconect = false);

    //! \brief draws and returns a vertical separator using a QFrame
    //! \return a new vertical line
    QFrame* buildSeparator();

    // Required parent pointer
    QWidget* m_parent;

    // The layout.
    QHBoxLayout* m_layout;

    // Labels
    QLabel* m_label_x;
    QLabel* m_label_y;
    QLabel* m_label_z;
    QLabel* m_label_unit;

    // Input
    QSlicerDoubleSpinBox* m_input_x;
    QSlicerDoubleSpinBox* m_input_y;
    QSlicerDoubleSpinBox* m_input_z;

    // Optional Combo
    QComboBox* m_combo;
    QCheckBox* m_check;
    Distance m_conversion;
    bool m_combo_enabled;

    // Animations
    QPropertyAnimation* m_open_ani;
    QPropertyAnimation* m_close_ani;

    // Type
    ModelTrackingType m_type;

    // Model
    QSharedPointer<PartMetaModel> m_model;

    // Selected item.
    QSharedPointer<PartMetaItem> m_selected_item;

    // Location on parent
    QPoint m_pos;

    // Keeps track of the open status
    bool is_open = false;
};
} // Namespace ORNL
