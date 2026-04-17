#include "widgets/part_widget/input/tool_bar_align_input.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <qboxlayout.h>
#include <qeasingcurve.h>
#include <qfiledevice.h>
#include <qframe.h>
#include <qicon.h>
#include <qpoint.h>
#include <qpropertyanimation.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtoolbutton.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "utilities/constants.h"

namespace ORNL {
ToolbarAlignInput::ToolbarAlignInput(QWidget* parent) : QFrame(parent) { this->setupWidget(); }

void ToolbarAlignInput::setPos(QPoint pos) {
    m_pos = pos;
    setupAnimations();
}

void ToolbarAlignInput::closeInput() {
    this->hide();
    m_close_ani->start();
    is_open = false;
}

void ToolbarAlignInput::toggleInput() {
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

void ToolbarAlignInput::setupWidget() {
    this->setMinimumWidth(Constants::UI::PartToolbar::Input::kBoxWidth);

    this->setupSubWidgets();
    this->setupLayouts();
    this->setupInsert();
    this->setupAnimations();
    this->setupEvents();

    // Styling
    this->setFrameStyle(QFrame::Box);
    this->setObjectName("tool_bar_input_frame");
    this->resize(Constants::UI::PartToolbar::Input::kBoxWidth, Constants::UI::PartToolbar::Input::kBoxHeight);
    this->hide();

    // Drop shadow
    auto* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);
}

void ToolbarAlignInput::setupSubWidgets() {
    // Tool buttons
    m_align_bottom_btn = new QToolButton(this);
    m_align_bottom_btn->setIcon(QIcon(":/icons/align_bottom.png"));
    m_align_bottom_btn->setToolTip("Align Face of Part to Bottom");

    m_align_top_btn = new QToolButton(this);
    m_align_top_btn->setIcon(QIcon(":/icons/align_top.png"));
    m_align_top_btn->setToolTip("Align Face of Part to Top");

    m_align_right_btn = new QToolButton(this);
    m_align_right_btn->setIcon(QIcon(":/icons/align_right.png"));
    m_align_right_btn->setToolTip("Align Face of Part Right");

    m_align_left_btn = new QToolButton(this);
    m_align_left_btn->setIcon(QIcon(":/icons/align_left.png"));
    m_align_left_btn->setToolTip("Align Face of Part Left");

    m_align_front_btn = new QToolButton(this);
    m_align_front_btn->setIcon(QIcon(":/icons/align_front.png"));
    m_align_front_btn->setToolTip("Align Face of Part to Front");

    m_align_back_btn = new QToolButton(this);
    m_align_back_btn->setIcon(QIcon(":/icons/align_back.png"));
    m_align_back_btn->setToolTip("Align Face of Part to Back");

    // Load stylesheet
    this->setupStyle();
}

void ToolbarAlignInput::setupLayouts() {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
}

void ToolbarAlignInput::setupStyle() {
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "tool_bar_input.qss"));
    style->open(QIODevice::ReadOnly);
    this->setStyleSheet(style->readAll());
    style->close();
}

void ToolbarAlignInput::setupInsert() {
    this->setLayout(m_layout);

    m_layout->addWidget(m_align_bottom_btn);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_align_top_btn);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_align_right_btn);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_align_left_btn);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_align_front_btn);
    m_layout->addWidget(buildSeparator());
    m_layout->addWidget(m_align_back_btn);
}

void ToolbarAlignInput::setupAnimations() {
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

void ToolbarAlignInput::setupEvents() {
    // Hide the container when the close animation completes.
    connect(m_close_ani, &QPropertyAnimation::finished, this, &ToolbarAlignInput::hide);

    // Re-emit signal from buttons
    connect(m_align_bottom_btn, &QToolButton::pressed, this, [this]() {
        emit setAlignment(QVector3D(0, 0, -1));
        this->hide();
    });
    connect(m_align_top_btn, &QToolButton::pressed, this, [this]() {
        emit setAlignment(QVector3D(0, 0, 1));
        this->hide();
    });
    connect(m_align_right_btn, &QToolButton::pressed, this, [this]() {
        emit setAlignment(QVector3D(1, 0, 0));
        this->hide();
    });
    connect(m_align_left_btn, &QToolButton::pressed, this, [this]() {
        emit setAlignment(QVector3D(-1, 0, 0));
        this->hide();
    });
    connect(m_align_front_btn, &QToolButton::pressed, this, [this]() {
        emit setAlignment(QVector3D(0, 1, 0));
        this->hide();
    });
    connect(m_align_back_btn, &QToolButton::pressed, this, [this]() {
        emit setAlignment(QVector3D(0, -1, 0));
        this->hide();
    });
}

QFrame* ToolbarAlignInput::buildSeparator() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setObjectName("separator");
    return line;
}

} // Namespace ORNL
