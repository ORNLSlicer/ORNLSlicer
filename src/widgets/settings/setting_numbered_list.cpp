#include "widgets/settings/setting_numbered_list.h"

#include <QDropEvent>
#include <QHeaderView>
#include <QToolTip>
#include <qabstractitemview.h>
#include <qcontainerfwd.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtablewidget.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {
SettingNumberedList::SettingNumberedList(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                         fifojson json, QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QTableWidget() {
    m_warn = false;
    QList<QString> entries;
    if (m_sb->contains(m_key))
        entries = m_sb->setting<QList<QString>>(m_key);
    else {
        entries = m_json.operator[](Constants::Settings::Master::kDefault).get<QList<QString>>();
        m_sb->setSetting(key, entries);
    }

    this->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->setSelectionMode(QAbstractItemView::SingleSelection);
    this->setDragEnabled(true);
    this->setDropIndicatorShown(true);
    this->setDragDropMode(QAbstractItemView::InternalMove);
    this->insertColumn(0);
    this->horizontalHeader()->hide();
    this->setDragDropOverwriteMode(false);
    this->setAcceptDrops(true);

    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    this->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    for (int i = 0, end = entries.size(); i < end; ++i) {
        this->insertRow(i);
        this->setItem(i, 0, new QTableWidgetItem(entries[i]));
    }

    this->setMinimumHeight(this->rowHeight(0) * entries.size());
    this->setMaximumHeight(this->rowHeight(0) * entries.size());
    this->setMinimumWidth(this->columnWidth(0));
    this->setMaximumWidth(this->columnWidth(0));

    connect(this, &SettingNumberedList::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingNumberedList::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

SettingRowBase* SettingNumberedList::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                    fifojson json, QGridLayout* layout, int index) {
    return new SettingNumberedList(parent, sb, key, json, layout, index);
}

void SettingNumberedList::setEnabled(bool enabled) {
    dynamic_cast<QTableWidget*>(this)->setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void SettingNumberedList::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingNumberedList::clearNotification() {
    // clear notification information
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingNumberedList::hide() {
    dynamic_cast<QTableWidget*>(this)->hide();
    SettingRowBase::hide();
}

void SettingNumberedList::show() {
    dynamic_cast<QTableWidget*>(this)->show();
    SettingRowBase::show();
}

void SettingNumberedList::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<QList<QString>>(val.value<QStringList>());
    emit modified(m_key);
}

void SettingNumberedList::reloadValue() {
    this->blockSignals(true);

    bool consistent = true;
    QList<QString> cur = reloadValueHelper<QList<QString>>(consistent);
    if (consistent)
        setEntries(cur);

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

void SettingNumberedList::setEntries(QList<QString> entries) {
    for (int i = 0, end = entries.size(); i < end; ++i) {
        this->item(i, 0)->setText(entries[i]);
    }
}

void SettingNumberedList::dropEvent(QDropEvent* event) {
    if (event->source() == this) {
        int newRow = this->indexAt(event->pos()).row();
        if (newRow != -1) {
            QTableWidgetItem* selectedItem = this->selectedItems()[0];
            int originalRow = selectedItem->row();
            this->takeItem(originalRow, 0);

            if (newRow > originalRow) {
                for (int i = originalRow; i < newRow; ++i) {
                    this->setItem(i, 0, this->takeItem(i + 1, 0));
                }
            }
            else {
                for (int i = originalRow; i > newRow; --i) {
                    this->setItem(i, 0, this->takeItem(i - 1, 0));
                }
            }
            this->setItem(newRow, 0, selectedItem);
            this->clearSelection();

            QList<QString> entries;
            for (int i = 0, end = this->rowCount(); i < end; ++i)
                entries.push_back(this->item(i, 0)->text());

            valueChanged(QVariant(entries));
        }
    }
}
} // namespace ORNL
