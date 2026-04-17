#include "widgets/part_widget/part_toolbar.h"

#include <QComboBox>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QInputDialog>
#include <QLayout>
#include <QMenu>
#include <qcontainerfwd.h>
#include <qevent.h>
#include <qicon.h>
#include <qnamespace.h>
#include <qsharedpointer.h>
#include <qsizepolicy.h>
#include <qtmetamacros.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "widgets/part_widget/input/tool_bar_align_input.h"
#include "widgets/part_widget/input/tool_bar_input.h"

namespace ORNL {
PartToolbar::PartToolbar(QSharedPointer<PartMetaModel> model, QWidget* parent)
    : m_model(model), m_parent(parent), QToolBar(parent) {
    m_translation_controls = new ToolbarInput(m_parent, false);
    m_rotation_controls = new ToolbarInput(m_parent, false);
    m_scale_controls = new ToolbarInput(m_parent, true);

    setupWidget();
    setupSubWidgets();
}

void PartToolbar::updateTranslationUnits(Distance new_unit, Distance old_unit) {
    const QVector<Axis> axes = {Axis::kX, Axis::kY, Axis::kZ};
    QVector<Distance> values;

    // Get the old values.
    for (Axis cur_axis : axes) {
        double spinbox_val = m_translation_controls->getValue(cur_axis);
        Distance val;

        val.from(spinbox_val, old_unit);
        values.push_back(val);
    }

    m_translation_controls->setMaximumValue(Constants::Limits::Maximums::kMaxDistance.to(new_unit));
    m_translation_controls->setMinimumValue(Constants::Limits::Minimums::kMinLocation.to(new_unit));

    // Set the values after changing the maximum.
    for (int i = 0, end = values.size(); i < end; ++i) {
        m_translation_controls->setValue(static_cast<Axis>(i), values[i].to(new_unit));
    }

    m_translation_controls->setUnitText(new_unit.toString());
    m_translation_controls->setIncrement(1 / (new_unit() / cm()));
}

void PartToolbar::updateAngleUnits(Angle new_unit, Angle old_unit) {
    const QVector<Axis> axes = {Axis::kX, Axis::kY, Axis::kZ};
    QVector<Angle> values;

    // Get the old values.
    for (Axis cur_axis : axes) {
        double spinbox_val = m_rotation_controls->getValue(cur_axis);
        Angle val;

        val.from(spinbox_val, old_unit);
        values.push_back(val);
    }

    // Constrain the angle spin boxes to valid euler angles
    Angle min_pitch, max_pitch, min_yaw, max_yaw, min_roll, max_roll;
    min_pitch.from(-180.0f, deg);
    max_pitch.from(180.0f, deg);
    min_yaw.from(-180.0f, deg);
    max_yaw.from(180.0f, deg);
    min_roll.from(-180.0f, deg);
    max_roll.from(180.0f, deg);
    m_rotation_controls->setMinimumXValue(min_pitch.to(new_unit));
    m_rotation_controls->setMaximumXValue(max_pitch.to(new_unit));
    m_rotation_controls->setMinimumYValue(min_yaw.to(new_unit));
    m_rotation_controls->setMaximumYValue(max_yaw.to(new_unit));
    m_rotation_controls->setMinimumZValue(min_roll.to(new_unit));
    m_rotation_controls->setMaximumZValue(max_roll.to(new_unit));

    // Set the values after changing the maximum.
    for (int i = 0, end = values.size(); i < end; ++i) {
        m_rotation_controls->setValue(static_cast<Axis>(i), values[i].to(new_unit));
    }

    m_rotation_controls->setUnitText(new_unit.toString());
}

void PartToolbar::resize(QSize new_size) {
    // Update position
    auto parent_size = m_parent->size();

    // Prefer keeping a third of the way down the page. If there is not enough room to support the main tool bar above,
    // force this down
    double y_offset = (parent_size.height() / 3.0) - (Constants::UI::PartToolbar::kHeight / 2.0);
    if (y_offset <= Constants::UI::PartToolbar::kMinTopOffset) {
        this->move(Constants::UI::PartToolbar::kLeftOffset, Constants::UI::PartToolbar::kMinTopOffset);
    }
    else {
        this->move(Constants::UI::PartToolbar::kLeftOffset, y_offset);
    }

    // Update size
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->setMinimumHeight(Constants::UI::PartToolbar::kHeight);
    this->setMaximumWidth(Constants::UI::PartToolbar::kWidth);

    // Close + Update Controls
    closeAllControls();
    m_translation_controls->setPos(this->pos() + m_translation_btn->pos());
    m_rotation_controls->setPos(this->pos() + m_rotation_btn->pos());
    m_scale_controls->setPos(this->pos() + m_scale_btn->pos());
    m_align_controls->setPos(this->pos() + m_align_btn->pos());
}

void PartToolbar::closeAllControls() {
    m_translation_controls->closeInput();
    m_rotation_controls->closeInput();
    m_scale_controls->closeInput();
    m_align_controls->closeInput();
}

void PartToolbar::setEnabled(bool status) {
    if (!status) // If we are disabling, close all the controls
        closeAllControls();

    m_translation_btn->setEnabled(status);
    m_rotation_btn->setEnabled(status);
    m_scale_btn->setEnabled(status);
    // Align works even if a part is not selected. It should always be enabled.
    // m_align_btn->setEnabled(status);
    m_center_btn->setEnabled(status);
    m_drop_to_floor_btn->setEnabled(status);
}

void PartToolbar::setupWidget() {
    // Add drop shadow
    auto* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);

    // Set to vertical layout
    this->setOrientation(Qt::Vertical);

    this->setFloatable(false);
    this->setMovable(false);
    this->raise();
}

void PartToolbar::setupSubWidgets() {
    this->makeSpace();
    setupTranslation();
    this->makeSpace();
    this->addSeparator();
    this->makeSpace();

    setupRotation();
    this->makeSpace();
    this->addSeparator();
    this->makeSpace();

    setupScale();
    this->makeSpace();
    this->addSeparator();
    this->makeSpace();

    setupAlign();
    this->makeSpace();
    this->addSeparator();
    this->makeSpace();

    setupCenter();
    this->makeSpace();
    this->addSeparator();
    this->makeSpace();

    setupDropToFloor();
    this->makeSpace();

    // Load stylesheet
    // Needs to be done last so all subwidgets have been populated
    this->setupStyle();
}

void PartToolbar::setupStyle() {
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "/part_toolbar.qss"));
    style->open(QIODevice::ReadOnly);

    auto partToolbarStyle = style->readAll();
    this->setStyleSheet(partToolbarStyle);
    m_translation_controls->setStyleSheet(partToolbarStyle);
    m_rotation_controls->setStyleSheet(partToolbarStyle);
    m_scale_controls->setStyleSheet(partToolbarStyle);
    m_align_controls->setStyleSheet(partToolbarStyle);

    style->close();
}

void PartToolbar::makeSpace() {
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->addWidget(spacer);
}

void PartToolbar::setupTranslation() {

    m_translation_btn = buildIconButton(":/icons/translate_black.png", "Translation Controls", false);
    this->addWidget(m_translation_btn);
    m_translation_controls->setMaximumValue(
        Constants::Limits::Maximums::kMaxDistance.to(PreferencesManager::getInstance()->getDistanceUnit()));
    m_translation_controls->setMinimumValue(
        Constants::Limits::Minimums::kMinLocation.to(PreferencesManager::getInstance()->getDistanceUnit()));
    m_translation_controls->setUnitText(PreferencesManager::getInstance()->getDistanceUnitText());
    m_translation_controls->setIncrement(1 / (PreferencesManager::getInstance()->getDistanceUnit()() / cm()));
    m_translation_controls->setModel(m_model, ToolbarInput::ModelTrackingType::kTranslation);
    m_translation_controls->setPrecision(4);
    connect(m_translation_btn, &QToolButton::pressed, m_translation_controls, &ToolbarInput::toggleInput);
}

void PartToolbar::setupRotation() {

    m_rotation_btn = buildIconButton(":/icons/rotate_black.png",
                                     "Rotation Controls\r\nBefore applying any roation, previous scaling, if any,\r\n"
                                     "will be applied permenantly to current transformation and\r\n"
                                     "scaling factors will be reset to 100%",
                                     false);
    this->addWidget(m_rotation_btn);
    m_rotation_controls->setUnitText(PreferencesManager::getInstance()->getAngleUnitText());

    // Constrain the angle spin boxes to valid euler angles
    Angle min_pitch, max_pitch, min_yaw, max_yaw, min_roll, max_roll;
    Angle default_unit = PreferencesManager::getInstance()->getAngleUnit();
    min_pitch.from(-180.0f, deg);
    max_pitch.from(180.0f, deg);
    min_yaw.from(-180.0f, deg);
    max_yaw.from(180.0f, deg);
    min_roll.from(-180.0f, deg);
    max_roll.from(180.0f, deg);
    m_rotation_controls->setMinimumXValue(min_pitch.to(default_unit));
    m_rotation_controls->setMaximumXValue(max_pitch.to(default_unit));
    m_rotation_controls->setMinimumYValue(min_yaw.to(default_unit));
    m_rotation_controls->setMaximumYValue(max_yaw.to(default_unit));
    m_rotation_controls->setMinimumZValue(min_roll.to(default_unit));
    m_rotation_controls->setMaximumZValue(max_roll.to(default_unit));
    m_rotation_controls->setModel(m_model, ToolbarInput::ModelTrackingType::kRotation);
    m_rotation_controls->setPrecision(4);
    connect(m_rotation_btn, &QToolButton::pressed, m_rotation_controls, &ToolbarInput::toggleInput);
    ;
}

void PartToolbar::setupScale() {
    m_scale_btn = buildIconButton(":/icons/scale_black.png", "Scaling Controls", false);
    this->addWidget(m_scale_btn);
    m_scale_controls->setMaximumValue(Constants::Limits::Maximums::kMaxFloat);
    m_scale_controls->setMinimumValue(0.0001f); // Shouldn't be able to scale a dimension to 0! Unphysical
    m_scale_controls->setIncrement(0.1);
    m_scale_controls->setPrecision(4);
    m_scale_controls->setValue(Axis::kX, 1.0);
    m_scale_controls->setValue(Axis::kY, 1.0);
    m_scale_controls->setValue(Axis::kZ, 1.0);
    m_scale_controls->setModel(m_model, ToolbarInput::ModelTrackingType::kScale);
    connect(m_scale_btn, &QToolButton::pressed, m_scale_controls, &ToolbarInput::toggleInput);
}

void PartToolbar::setupAlign() {
    m_align_btn = buildIconButton(":/icons/align.png", "Align Controls", false);
    this->addWidget(m_align_btn);
    m_align_controls = new ToolbarAlignInput(m_parent);
    connect(m_align_btn, &QToolButton::pressed, m_align_controls, &ToolbarAlignInput::toggleInput);
    connect(m_align_controls, &ToolbarAlignInput::setAlignment, this,
            [this](QVector3D dir) { emit setupAlignment(dir); });
}

void PartToolbar::setupCenter() {
    m_center_btn = buildIconButton(":/icons/center.png", "Center Part in Build Volume", false);
    this->addWidget(m_center_btn);
    connect(m_center_btn, &QToolButton::pressed, [this]() { emit centerParts(); });
}

void PartToolbar::setupDropToFloor() {
    m_drop_to_floor_btn = buildIconButton(":/icons/floor.png", "Drop Part to Floor", false);
    this->addWidget(m_drop_to_floor_btn);
    connect(m_drop_to_floor_btn, &QToolButton::pressed, this, [this]() { emit dropPartsToFloor(); });
}

QToolButton* PartToolbar::buildIconButton(const QString& icon_loc, const QString& tooltip, bool toggle) {
    auto* button = new QToolButton(this);
    button->setIcon(QIcon(icon_loc));
    button->setToolTip(tooltip);
    button->setCheckable(toggle);
    return button;
}
} // namespace ORNL
