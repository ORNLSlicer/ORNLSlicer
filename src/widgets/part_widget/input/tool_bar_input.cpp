#include "widgets/part_widget/input/tool_bar_input.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qevent.h>
#include <qframe.h>
#include <qhashfunctions.h>
#include <qicon.h>
#include <qlabel.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qoverload.h>
#include <qpoint.h>
#include <qpropertyanimation.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qsizepolicy.h>
#include <qtypes.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {
ToolbarInput::ToolbarInput(QWidget* parent, bool optionalCombo)
    : m_parent(parent), QFrame(parent), m_combo_enabled(optionalCombo) {
    // Build widget
    this->setupWidget();

    m_conversion.from(1, mm);
}

void ToolbarInput::setWrapping(bool w) {
    m_input_x->setWrapping(w);
    m_input_y->setWrapping(w);
    m_input_z->setWrapping(w);
}

void ToolbarInput::setModel(QSharedPointer<PartMetaModel> m, ModelTrackingType type) {
    m_model = m;
    m_type = type;

    switch (type) {
        case ModelTrackingType::kTranslation: {
            QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this,
                             &ToolbarInput::modelTranslationUpdate);
            break;
        }
        case ModelTrackingType::kRotation: {
            QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelRotationUpdate);
            break;
        }
        case ModelTrackingType::kScale: {
            QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelScaleUpdate);
            break;
        }
    }

    QObject::connect(m_model.get(), &PartMetaModel::selectionUpdate, this, &ToolbarInput::modelSelectionUpdate);
    QObject::connect(m_model.get(), &PartMetaModel::itemRemovedUpdate, this, &ToolbarInput::modelRemovalUpdate);
}

void ToolbarInput::setPos(QPoint pos) {
    m_pos = pos;
    setupAnimations();
}

void ToolbarInput::closeInput() {
    this->hide();
    m_close_ani->start();
    is_open = false;
}

void ToolbarInput::toggleInput() {
    if (is_open) {
        m_close_ani->start();
        is_open = false;
    }
    else {
        this->show();
        m_open_ani->start();
        is_open = true;
    }
}

void ToolbarInput::setValue(Axis axis, double val) {
    if (axis == Axis::kX)
        m_input_x->setValue(val);
    else if (axis == Axis::kY)
        m_input_y->setValue(val);
    else
        m_input_z->setValue(val);
}

void ToolbarInput::setIndex(int index) {
    if (m_combo != nullptr)
        m_combo->setCurrentIndex(index);
}

double ToolbarInput::getValue(Axis axis) {
    if (axis == Axis::kX)
        return m_input_x->value();
    else if (axis == Axis::kY)
        return m_input_y->value();
    else
        return m_input_z->value();
}

void ToolbarInput::setIncrement(double inc) {
    m_input_x->setSingleStep(inc);
    m_input_y->setSingleStep(inc);
    m_input_z->setSingleStep(inc);
}

void ToolbarInput::setMaximumValue(double val) {
    m_input_x->setMaximum(val);
    m_input_y->setMaximum(val);
    m_input_z->setMaximum(val);
}

void ToolbarInput::setMaximumXValue(double val) { m_input_x->setMaximum(val); }

void ToolbarInput::setMaximumYValue(double val) { m_input_y->setMaximum(val); }

void ToolbarInput::setMaximumZValue(double val) { m_input_z->setMaximum(val); }

void ToolbarInput::setMinimumValue(double val) {
    m_input_x->setMinimum(val);
    m_input_y->setMinimum(val);
    m_input_z->setMinimum(val);
}

void ToolbarInput::setMinimumXValue(double val) { m_input_x->setMinimum(val); }

void ToolbarInput::setMinimumYValue(double val) { m_input_y->setMinimum(val); }

void ToolbarInput::setMinimumZValue(double val) { m_input_z->setMinimum(val); }

void ToolbarInput::setUnitText(QString unit) { m_label_unit->setText(unit); }

void ToolbarInput::setXText(QString x) { m_label_x->setText(x); }

void ToolbarInput::setYText(QString y) { m_label_y->setText(y); }

void ToolbarInput::setZText(QString z) { m_label_z->setText(z); }

void ToolbarInput::readValue(double val) {
    if (m_model.isNull() || m_selected_item.isNull())
        return;

    QVector3D new_val;

    new_val.setX(m_input_x->value());
    new_val.setY(m_input_y->value());
    new_val.setZ(m_input_z->value());

    switch (m_type) {
        case ModelTrackingType::kTranslation: {
            Distance conv;
            Distance pm_dist = PreferencesManager::getInstance()->getDistanceUnit();
            conv.from(1, pm_dist);

            QObject::disconnect(m_model.get(), &PartMetaModel::transformUpdate, this,
                                &ToolbarInput::modelTranslationUpdate);
            m_selected_item->setTranslation(new_val * conv() * Constants::OpenGL::kObjectToView);
            QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this,
                             &ToolbarInput::modelTranslationUpdate);

            break;
        }

        case ModelTrackingType::kRotation: {
            Angle conv = deg;
            Angle pm_ang = PreferencesManager::getInstance()->getAngleUnit();
            double conv_d = conv.to(pm_ang);

            QObject::disconnect(m_model.get(), &PartMetaModel::transformUpdate, this,
                                &ToolbarInput::modelRotationUpdate);
            m_selected_item->setRotation(QQuaternion::fromEulerAngles(new_val / conv_d));
            QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelRotationUpdate);
            break;
        }

        case ModelTrackingType::kScale: {
            QObject::disconnect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelScaleUpdate);
            m_selected_item->setScale(new_val * (m_conversion() / mm()));
            QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelScaleUpdate);

            break;
        }

        default: {
            break;
        }
    }
}

void ToolbarInput::readUnit(int index) {
    if (m_model.isNull() || m_selected_item.isNull())
        return;

    Distance conversion;
    switch (index) {
        case 0:
            conversion.from(1, mm);
            break;
        case 1:
            conversion.from(1, cm);
            break;
        case 2:
            conversion.from(1, m);
            break;
        case 3:
            conversion.from(1, inch);
            break;
        case 4:
            conversion.from(1, feet);
            break;
        default:
            conversion.from(1, mm);
            break;
    }

    m_conversion = conversion;

    QVector3D new_val;

    new_val.setX(m_input_x->value());
    new_val.setY(m_input_y->value());
    new_val.setZ(m_input_z->value());

    QObject::disconnect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelScaleUpdate);
    m_selected_item->setScale(new_val * (m_conversion() / mm()));
    m_selected_item->setScaleUnitIndex(index);
    QObject::connect(m_model.get(), &PartMetaModel::transformUpdate, this, &ToolbarInput::modelScaleUpdate);
}

void ToolbarInput::lockScale(bool toggle) {
    if (toggle) {
        setupValueEditEvents(true);

        m_input_y->setDisabled(true);
        m_input_z->setDisabled(true);

        double val = m_input_x->value();
        m_input_y->setValue(val);
        m_input_z->setValue(val);

        connect(m_input_x, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this,
                &ToolbarInput::lockScaleUpdate);

        this->lockScaleUpdate(val);
    }
    else {
        setupValueEditEvents();

        m_input_y->setDisabled(false);
        m_input_z->setDisabled(false);

        disconnect(m_input_x, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this,
                   &ToolbarInput::lockScaleUpdate);
    }
}

void ToolbarInput::lockScaleUpdate(double val) {
    m_input_y->setValue(val);
    m_input_z->setValue(val);

    this->readValue(val);
}

void ToolbarInput::modelSelectionUpdate(QSharedPointer<PartMetaItem> item) {
    QList<QSharedPointer<PartMetaItem>> selected_items = m_model->selectedItems();
    QSharedPointer<PartMetaItem> manip_item;

    if (item->isSelected())
        manip_item = item;
    else if (selected_items.count())
        manip_item = selected_items.back();

    if (manip_item.isNull()) {
        m_selected_item = nullptr;
        return;
    }

    m_selected_item = item;

    // Update boxes.
    switch (m_type) {
        case ORNL::ToolbarInput::ModelTrackingType::kTranslation:
            this->modelTranslationUpdate(item);
            break;
        case ORNL::ToolbarInput::ModelTrackingType::kRotation:
            this->modelRotationUpdate(item);
            break;
        case ORNL::ToolbarInput::ModelTrackingType::kScale:
            this->modelScaleUpdate(item);
            break;
    }
}

void ToolbarInput::modelTranslationUpdate(QSharedPointer<PartMetaItem> item) {
    if (m_selected_item != item)
        return;

    Distance conv;
    Distance pm_dist = PreferencesManager::getInstance()->getDistanceUnit();
    conv.from(1, pm_dist);

    QVector3D pos = m_selected_item->translation();
    pos *= Constants::OpenGL::kViewToObject;
    pos /= conv();

    m_input_x->blockSignals(true);
    m_input_y->blockSignals(true);
    m_input_z->blockSignals(true);

    m_input_x->setValue(pos.x());
    m_input_y->setValue(pos.y());
    m_input_z->setValue(pos.z());

    m_input_x->blockSignals(false);
    m_input_y->blockSignals(false);
    m_input_z->blockSignals(false);
}

void ToolbarInput::modelRotationUpdate(QSharedPointer<PartMetaItem> item) {
    if (m_selected_item != item)
        return;

    Angle conv = deg;
    Angle pm_ang = PreferencesManager::getInstance()->getAngleUnit();
    double conv_d = conv.to(pm_ang);

    QVector3D r = m_selected_item->rotation().toEulerAngles();

    m_input_x->blockSignals(true);
    m_input_y->blockSignals(true);
    m_input_z->blockSignals(true);

    m_input_x->setValue(r.x() * conv_d);
    m_input_y->setValue(r.y() * conv_d);
    m_input_z->setValue(r.z() * conv_d);

    m_input_x->blockSignals(false);
    m_input_y->blockSignals(false);
    m_input_z->blockSignals(false);
}

void ToolbarInput::modelScaleUpdate(QSharedPointer<PartMetaItem> item) {
    if (item != m_selected_item)
        return;

    Distance conversion;
    switch (item->scaleUnitIndex()) {
        case 0:
            conversion.from(1, mm);
            break;
        case 1:
            conversion.from(1, cm);
            break;
        case 2:
            conversion.from(1, m);
            break;
        case 3:
            conversion.from(1, inch);
            break;
        case 4:
            conversion.from(1, feet);
            break;
        default:
            conversion.from(1, mm);
            break;
    }

    m_conversion = conversion;

    m_combo->blockSignals(true);
    m_combo->setCurrentIndex(item->scaleUnitIndex());
    m_combo->blockSignals(false);

    QVector3D s = item->scale() / (m_conversion() / mm());

    m_input_x->blockSignals(true);
    m_input_y->blockSignals(true);
    m_input_z->blockSignals(true);

    m_input_x->setValue(s.x());
    m_input_y->setValue(s.y());
    m_input_z->setValue(s.z());

    m_input_x->blockSignals(false);
    m_input_y->blockSignals(false);
    m_input_z->blockSignals(false);
}

void ToolbarInput::modelRemovalUpdate(QSharedPointer<PartMetaItem> item) {
    if (item == m_selected_item)
        m_selected_item = nullptr;
}

void ToolbarInput::setupWidget() {
    if (m_combo_enabled)
        this->setMinimumWidth(Constants::UI::PartToolbar::Input::kBoxWidth +
                              Constants::UI::PartToolbar::Input::kExtraButtonWidth);
    else
        this->setMinimumWidth(Constants::UI::PartToolbar::Input::kBoxWidth);

    this->setupSubWidgets();
    this->setupLayouts();
    this->setupInsert();
    this->setupAnimations();
    this->setupEvents();

    // Styling
    this->setFrameStyle(QFrame::Panel);
    this->setObjectName("tool_bar_input_frame");
    this->resize(Constants::UI::PartToolbar::Input::kBoxWidth, Constants::UI::PartToolbar::Input::kBoxHeight);
    this->setWrapping(false);
    this->raise();
    this->hide();

    // Drop shadow
    auto* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);
}

void ToolbarInput::setupSubWidgets() {
    // Input Labels
    m_label_x = new QLabel("X: ", this);
    m_label_x->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    m_label_y = new QLabel("Y: ", this);
    m_label_y->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    m_label_z = new QLabel("Z: ", this);
    m_label_z->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    m_label_unit = new QLabel("", this);
    m_label_unit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    // Input Boxes
    m_input_x = new QSlicerDoubleSpinBox(this);
    m_input_x->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_input_x->setDecimals(Constants::UI::PartToolbar::Input::kPrecision);
    m_input_y = new QSlicerDoubleSpinBox(this);
    m_input_y->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_input_y->setDecimals(Constants::UI::PartToolbar::Input::kPrecision);
    m_input_z = new QSlicerDoubleSpinBox(this);
    m_input_z->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_input_z->setDecimals(Constants::UI::PartToolbar::Input::kPrecision);

    if (m_combo_enabled) {
        // optional combobox
        m_combo = new QComboBox(this);
        m_combo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        m_combo->addItem("mm");
        m_combo->addItem("cm");
        m_combo->addItem("m");
        m_combo->addItem("inch");
        m_combo->addItem("feet");

        m_check = new QCheckBox(this);
        m_check->setIcon(QIcon(":/icons/lock.png"));
        m_check->setToolTip("Locks the X/Y/Z scales together.");
    }

    // Load stylesheet
    this->setupStyle();
}

void ToolbarInput::setupStyle() {
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "tool_bar_input.qss"));
    style->open(QIODevice::ReadOnly);
    this->setStyleSheet(style->readAll());
    style->close();
}

void ToolbarInput::setupLayouts() { m_layout = new QHBoxLayout(this); }

void ToolbarInput::setupInsert() {
    this->setLayout(m_layout);

    m_layout->addWidget(m_label_x);
    m_layout->addWidget(m_input_x);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_label_y);
    m_layout->addWidget(m_input_y);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_label_z);
    m_layout->addWidget(m_input_z);
    m_layout->addWidget(m_label_unit);

    if (m_combo_enabled) {
        m_layout->addWidget(m_check);
        m_layout->addWidget(m_combo);
    }
}

void ToolbarInput::setupAnimations() {
    m_open_ani = new QPropertyAnimation(this, "pos", this);
    m_open_ani->setEasingCurve(QEasingCurve::InOutCubic);
    m_open_ani->setDuration(Constants::UI::PartToolbar::Input::kAnimationInTime);
    m_open_ani->setStartValue(QPoint(-(this->width() + 10), m_pos.y()));
    m_open_ani->setEndValue(QPoint(m_pos.x() + Constants::UI::PartToolbar::kWidth, m_pos.y()));

    m_close_ani = new QPropertyAnimation(this, "pos", this);
    m_close_ani->setEasingCurve(QEasingCurve::InOutCubic);
    m_close_ani->setDuration(Constants::UI::PartToolbar::Input::kAnimationOutTime);
    m_close_ani->setStartValue(QPoint(m_pos.x() + Constants::UI::PartToolbar::kWidth, m_pos.y()));
    m_close_ani->setEndValue(QPoint(-(this->width() + 10), m_pos.y()));
}

void ToolbarInput::setupEvents() {
    // Hide the container when the close animation completes.
    connect(m_close_ani, &QPropertyAnimation::finished, this, &ToolbarInput::hide);

    setupValueEditEvents();

    if (m_combo_enabled) {
        connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ToolbarInput::readUnit);
        connect(m_check, &QCheckBox::toggled, this, &ToolbarInput::lockScale);
    }
}

void ORNL::ToolbarInput::setupValueEditEvents(bool disconect) {
    if (disconect) {
        disconnect(m_input_x, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this,
                   &ToolbarInput::readValue);
        disconnect(m_input_y, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this,
                   &ToolbarInput::readValue);
        disconnect(m_input_z, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this,
                   &ToolbarInput::readValue);
    }
    else {
        connect(m_input_x, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this, &ToolbarInput::readValue);
        connect(m_input_y, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this, &ToolbarInput::readValue);
        connect(m_input_z, QOverload<double>::of(&QSlicerDoubleSpinBox::valueChanged), this, &ToolbarInput::readValue);
    }
}

QFrame* ToolbarInput::buildSeparator() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setObjectName("separator");
    return line;
}

void ToolbarInput::disableX(bool has_settings_part) { m_input_x->setDisabled(has_settings_part); }

void ToolbarInput::disableY(bool has_settings_part) { m_input_y->setDisabled(has_settings_part); }

void ToolbarInput::disableZ(bool has_settings_part) { m_input_z->setDisabled(has_settings_part); }

void ToolbarInput::disableCombo(bool has_settings_part) {
    m_combo->setDisabled(has_settings_part);
    m_combo_enabled = !has_settings_part;
}

void ToolbarInput::setPrecision(uint decimals) {
    m_input_x->setDecimals(decimals);
    m_input_y->setDecimals(decimals);
    m_input_z->setDecimals(decimals);
}
} // namespace ORNL
