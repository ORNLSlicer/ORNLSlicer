#include "widgets/view_controls_toolbar.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QLayout>
#include <QSignalBlocker>
#include <QToolButton>
#include <qfiledevice.h>
#include <qicon.h>
#include <qnamespace.h>
#include <qsharedpointer.h>
#include <qsizepolicy.h>
#include <qtmetamacros.h>
#include <qtoolbar.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
constexpr int kOrthographicButtonWidth = 70;
} // namespace

ViewControlsToolbar::ViewControlsToolbar(QWidget* parent, bool show_orthographic_button)
    : QToolBar(parent), m_parent(parent), m_show_orthographic_button(show_orthographic_button), m_iso_btn(nullptr),
      m_front_btn(nullptr), m_side_btn(nullptr), m_top_btn(nullptr), m_ortho_btn(nullptr) {
    setupWidget();
    setupSubWidgets();
}

void ViewControlsToolbar::setupWidget() {
    // Load stylesheet
    this->setupStyle();

    // Add drop shadow
    auto* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);

    // Set to horizontal layout
    this->setOrientation(Qt::Horizontal);

    this->setFloatable(false);
    this->setMovable(false);
    this->raise();
}

void ViewControlsToolbar::setupSubWidgets() {
    m_iso_btn = new QToolButton(this);
    m_iso_btn->setIcon(QIcon(":/icons/view_iso.png"));
    m_iso_btn->setToolTip("Isometric View");

    connect(m_iso_btn, &QToolButton::clicked, this, [this]() { emit setIsoView(); });

    this->makeSpace();
    this->addWidget(m_iso_btn);
    this->makeSpacedSeparator();

    m_front_btn = new QToolButton(this);
    m_front_btn->setIcon(QIcon(":/icons/view_front.png"));
    m_front_btn->setToolTip("Front Projection");
    connect(m_front_btn, &QToolButton::clicked, this, [this]() { emit setFrontView(); });

    this->addWidget(m_front_btn);
    this->makeSpacedSeparator();

    m_side_btn = new QToolButton(this);
    m_side_btn->setIcon(QIcon(":/icons/view_left.png"));
    m_side_btn->setToolTip("Side Projection");
    connect(m_side_btn, &QToolButton::clicked, this, [this]() { emit setSideView(); });

    this->addWidget(m_side_btn);
    this->makeSpacedSeparator();

    m_top_btn = new QToolButton(this);
    m_top_btn->setIcon(QIcon(":/icons/view_top.png"));
    m_top_btn->setToolTip("Top Projection");
    connect(m_top_btn, &QToolButton::clicked, this, [this]() { emit setTopView(); });

    this->addWidget(m_top_btn);
    this->makeSpace();

    if (m_show_orthographic_button) {
        this->addSeparator();
        this->makeSpace();

        m_ortho_btn = new QToolButton(this);
        m_ortho_btn->setIcon(QIcon(":/icons/orthogonal_view.png"));
        m_ortho_btn->setToolTip("Overhead Orthographic Projection");
        m_ortho_btn->setCheckable(true);
        connect(m_ortho_btn, &QToolButton::toggled, this, [this](bool checked) {
            emit setOrthographicView(checked);
        });

        this->addWidget(m_ortho_btn);
        this->makeSpace();
    }
}

void ViewControlsToolbar::setupStyle() {
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "view_controls_toolbar.qss"));
    style->open(QIODevice::ReadOnly);
    this->setStyleSheet(style->readAll());
    style->close();
}

void ViewControlsToolbar::makeSpace() {
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->addWidget(spacer);
}

void ViewControlsToolbar::makeSpacedSeparator() {
    this->makeSpace();
    this->addSeparator();
    this->makeSpace();
}

void ViewControlsToolbar::resize(QSize new_size) {
    // Update position
    auto parent_size = m_parent->size();
    const int toolbar_width = Constants::UI::ViewControlsToolbar::kWidth +
                              (m_show_orthographic_button ? kOrthographicButtonWidth : 0);

    this->move(parent_size.width() - toolbar_width - Constants::UI::ViewControlsToolbar::kRightOffset,
               parent_size.height() - Constants::UI::ViewControlsToolbar::kHeight -
                   Constants::UI::ViewControlsToolbar::kBottomOffset);

    // Update size
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->setMinimumHeight(Constants::UI::ViewControlsToolbar::kHeight);
    this->setMaximumHeight(Constants::UI::ViewControlsToolbar::kHeight);
    this->setMinimumWidth(toolbar_width);
    this->setMaximumWidth(toolbar_width);
}

void ViewControlsToolbar::setProjectionControlsEnabled(bool status) {
    m_iso_btn->setEnabled(status);
    m_front_btn->setEnabled(status);
    m_side_btn->setEnabled(status);
    m_top_btn->setEnabled(status);
}

void ViewControlsToolbar::setOrthographicViewChecked(bool status) {
    if (m_ortho_btn == nullptr) {
        return;
    }

    const QSignalBlocker blocker(m_ortho_btn);
    m_ortho_btn->setChecked(status);
}

void ViewControlsToolbar::setEnabled(bool status) {
    QToolBar::setEnabled(status);
    setProjectionControlsEnabled(status);

    if (m_ortho_btn != nullptr) {
        m_ortho_btn->setEnabled(status);
    }
}
} // namespace ORNL
