#include "widgets/settings/setting_line_edit.h"

#include <QToolTip>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {

SettingLineEdit::SettingLineEdit(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                                 QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QLineEdit() {
    QString cur;
    m_warn = false;
    if (m_sb->contains(m_key))
        cur = m_sb->setting<QString>(m_key);
    else {
        cur = json.operator[](Constants::Settings::Master::kDefault).get<QString>();
        m_sb->setSetting(m_key, cur);
    }

    this->setAlignment(Qt::AlignRight);

    connect(this, &QLineEdit::textChanged, this, &SettingLineEdit::valueChanged);
    connect(this, &SettingLineEdit::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingLineEdit::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
    registerRowWidget(this);
}

SettingRowBase* SettingLineEdit::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                fifojson json, QGridLayout* layout, int index) {
    return new SettingLineEdit(parent, sb, key, json, layout, index);
}

void SettingLineEdit::setEnabled(bool enabled) {
    SettingRowBase::setEnabled(enabled);
    applyWidgetState(static_cast<QLineEdit*>(this));
}

void SettingLineEdit::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingLineEdit::clearNotification() {
    // clear notification information
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingLineEdit::hide() {
    SettingRowBase::hide();
    applyWidgetState(static_cast<QLineEdit*>(this));
}

void SettingLineEdit::show() {
    SettingRowBase::show();
    applyWidgetState(static_cast<QLineEdit*>(this));
}

void SettingLineEdit::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<QString>(val.toString());
    emit modified(m_key);
}

void SettingLineEdit::reloadValue() {
    this->blockSignals(true);
    bool consistent = true;
    QString cur = reloadValueHelper<QString>(consistent);
    if (consistent)
        setText(cur);

    this->blockSignals(false);
    emit modified(m_key);
    emit warnParent(warningCountDelta(!consistent, m_warn));
}
} // Namespace ORNL
