#include "widgets/settings/setting_header.h"

#include <QFile>
#include <QHBoxLayout>
#include <qframe.h>
#include <qhashfunctions.h>
#include <qicon.h>
#include <qlabel.h>
#include <qlayoutitem.h>
#include <qnamespace.h>
#include <qpicture.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qsharedpointer.h>
#include <qsizepolicy.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"

namespace ORNL {
SettingHeader::SettingHeader(QWidget* parent, QString name, QIcon icon, bool canDelete)
    : QFrame(parent), m_name(name), m_icon(icon), m_can_delete(canDelete) {

    setupWidget();
    setupSubWidgets();
}

void SettingHeader::updateBackground() { this->update(); }

void SettingHeader::setName(QString new_name) {
    m_name = new_name;
    this->update();
}

void SettingHeader::setIcon(QIcon new_icon) {
    m_picture->setPixmap(new_icon.pixmap(QSize(28, 28), QIcon::Normal, QIcon::On));
    this->update();
}

void SettingHeader::setStatus(bool status) {
    m_status = status;
    this->update();
}

void SettingHeader::setupStyle() {
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "setting_headers.qss"));
    style->open(QIODevice::ReadOnly);
    this->setStyleSheet(style->readAll());
    style->close();
}

void SettingHeader::setupWidget() {
    this->setupStyle();
    m_status = false;
    this->setFixedHeight(55);
    this->setMinimumWidth(this->parentWidget()->width());
    this->setFrameStyle(QFrame::Panel | QFrame::Raised);
}

void SettingHeader::setupSubWidgets() {
    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    QSpacerItem* hspacer = new QSpacerItem(0, 0, QSizePolicy::Expanding);
    m_picture = new QLabel(this);
    m_picture->setSizeIncrement(28, 28);
    m_picture->setPixmap(m_icon.pixmap(QSize(28, 28), QIcon::Normal, QIcon::On));
    m_label = new QLabel(this);
    m_label->setText(m_name);
    m_arrow = new QLabel(this);
    QPixmap arrow(":/icons/down_black.png");
    m_arrow->setPixmap(arrow);
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFixedSize(1, 32);
    line->setFrameShadow(QFrame::Sunken);
    m_remove = new RemoveButton(m_can_delete, this);

    hlayout->addWidget(m_picture);
    hlayout->addWidget(m_label);
    hlayout->addSpacerItem(hspacer);
    hlayout->addWidget(m_arrow);
    hlayout->addWidget(line);
    hlayout->addWidget(m_remove);
    this->setLayout(hlayout);

    connect(m_remove, &QPushButton::clicked, this, &SettingHeader::deleteOrHideHeader);
}

void SettingHeader::mousePressEvent(QMouseEvent* event) {
    m_status = !m_status;

    if (m_status) {
        emit expand();
        QPixmap arrow(":/icons/up_black.png");
        m_arrow->setPixmap(arrow);
    }
    else {
        QPixmap arrow(":/icons/down_black.png");
        m_arrow->setPixmap(arrow);
        emit shrink();
    }

    this->update();
}

void SettingHeader::showHeader() { this->show(); }

void SettingHeader::deleteOrHideHeader() {
    if (m_can_delete)
        emit deleteHeader();
    else {
        this->hide();
        emit hideHeader();
    }
}

RemoveButton::RemoveButton(bool isDelete, QWidget* parent) : QPushButton(parent) {
    QPixmap hideOrDelete;
    if (isDelete) {
        hideOrDelete = QPixmap(":/icons/delete_black.png");
        this->setToolTip("Click here to delete");
    }
    else {
        hideOrDelete = QPixmap(":/icons/hide-mono.png");
        this->setToolTip("Click here to hide");
    }

    m_inside = false;

    this->setIcon(QIcon(hideOrDelete.scaled(22, 22, Qt::KeepAspectRatio)));
}

bool RemoveButton::isInside() { return m_inside; }
} // Namespace ORNL
