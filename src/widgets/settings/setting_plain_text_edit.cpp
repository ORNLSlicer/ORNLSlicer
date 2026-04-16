#include "widgets/settings/setting_plain_text_edit.h"

#include <QToolTip>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qplaintextedit.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {

SettingPlainTextEdit::SettingPlainTextEdit(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                           fifojson json, QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QPlainTextEdit() {
    QString cur;
    m_warn = false;
    if (m_sb->contains(key))
        cur = m_sb->setting<QString>(key);
    else {
        cur = json.operator[](Constants::Settings::Master::kDefault).get<QString>();
        m_sb->setSetting(key, cur);
    }

    this->setPlainText(cur);

    connect(this, &QPlainTextEdit::textChanged, [this]() { valueChanged(toPlainText()); });
    connect(this, &SettingPlainTextEdit::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingPlainTextEdit::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

SettingRowBase* SettingPlainTextEdit::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                     fifojson json, QGridLayout* layout, int index) {
    return new SettingPlainTextEdit(parent, sb, key, json, layout, index);
}

void SettingPlainTextEdit::setEnabled(bool enabled) {
    dynamic_cast<QPlainTextEdit*>(this)->setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void SettingPlainTextEdit::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingPlainTextEdit::clearNotification() {
    // clear notification information
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingPlainTextEdit::hide() {
    dynamic_cast<QPlainTextEdit*>(this)->hide();
    SettingRowBase::hide();
}

void SettingPlainTextEdit::show() {
    dynamic_cast<QPlainTextEdit*>(this)->show();
    SettingRowBase::show();
}

void SettingPlainTextEdit::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<QString>(val.value<QString>());
    emit modified(m_key);
}

void SettingPlainTextEdit::reloadValue() {
    this->blockSignals(true);
    bool consistent = true;
    QString cur = reloadValueHelper<QString>(consistent);
    if (consistent)
        setPlainText(cur);

    this->blockSignals(false);
    emit modified(m_key);
    // give (1) warning if there is a mismatch, otherwise give no (0) warning
    if (!consistent) {
        emit warnParent(1);
        m_warn = true;
    }
    else
        emit warnParent(0);
}
} // Namespace ORNL
