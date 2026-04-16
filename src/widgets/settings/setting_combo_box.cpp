#include "widgets/settings/setting_combo_box.h"

#include <QToolTip>
#include <QWheelEvent>
#include <qcombobox.h>
#include <qcontainerfwd.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qoverload.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {
SettingComboBox::SettingComboBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                                 QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QComboBox() {
    this->setFocusPolicy(Qt::StrongFocus);
    m_warn = false;

    if (m_sb->contains(key)) {
        m_cur = m_sb->setting<int>(key);
    }
    else {
        m_cur = json.operator[](Constants::Settings::Master::kDefault).get<int>();
        m_sb->setSetting(key, m_cur);
    }

    // Get the QStringList from the list in the json.
    QStringList options = json.operator[]("options").get<QString>().split(',');

    // For each option, make sure it is trimmed.
    for (QString& curr : options)
        curr = curr.trimmed();

    this->addItems(options);
    this->setCurrentIndex(m_cur);
    m_prev = m_cur;

    connect(this, QOverload<int>::of(&QComboBox::activated), this, &SettingComboBox::valueChanged);
    connect(this, &SettingComboBox::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingComboBox::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

SettingRowBase* SettingComboBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                fifojson json, QGridLayout* layout, int index) {
    return new SettingComboBox(parent, sb, key, json, layout, index);
}

void SettingComboBox::setEnabled(bool enabled) {
    dynamic_cast<QComboBox*>(this)->setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void SettingComboBox::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingComboBox::clearNotification() {
    // clear notification information
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingComboBox::hide() {
    dynamic_cast<QComboBox*>(this)->hide();
    SettingRowBase::hide();
}

void SettingComboBox::show() {
    dynamic_cast<QComboBox*>(this)->show();
    SettingRowBase::show();
}

void SettingComboBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<int>(val.toInt());
    if (m_prev != this->currentIndex()) {
        emit modified(m_key);
    }
    m_prev = this->currentIndex();
}

void SettingComboBox::reloadValue() {
    this->blockSignals(true);
    bool consistent = true;
    int m_cur = reloadValueHelper<int>(consistent);
    if (consistent)
        setCurrentIndex(m_cur);

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

void SettingComboBox::wheelEvent(QWheelEvent* event) {
    if (!hasFocus())
        event->ignore();
    else
        QComboBox::wheelEvent(event);
}
} // namespace ORNL
