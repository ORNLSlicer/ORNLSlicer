#include "widgets/settings/setting_row_base.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <qgridlayout.h>
#include <qhashfunctions.h>
#include <qlabel.h>
#include <qlist.h>
#include <qlogging.h>
#include <qnamespace.h>
#include <qpicture.h>
#include <qsharedpointer.h>
#include <qwidget.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {

SettingRowBase::SettingRowBase(QWidget* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                               QGridLayout* layout, int index)
    : m_sb(sb), m_key(key), m_json(json), m_layout(layout), m_index(index) {
    m_theme_path = PreferencesManager::getInstance()->getTheme().getFolderPath();
    m_key_label.reset(new QLabel());
    m_key_label->setText(json.at(Constants::Settings::Master::kDisplay));
    m_key_label->setToolTip("<html><body><p>" + QString::fromStdString(json.at(Constants::Settings::Master::kToolTip)) +
                            "</p></body></html>");
    m_key_label->setCursor(Qt::WhatsThisCursor);
    m_key_label->setMinimumHeight(25);
    m_key_label->setIndent(25);
    this->styleLabelFromFile(m_theme_path + "setting_rows_normal.qss");
    layout->addWidget(m_key_label.get(), index, 0);
    layout->setContentsMargins(0, 0, 20, 0);
}

SettingRowBase::~SettingRowBase() {
    // NOP - all widgets inherit from this class and a QObject-derived class
    // Skip destruction and allow QObject-derivation to handle it
}

void SettingRowBase::clearDependencyLogic() {
    m_rows_to_notify.clear();
    m_dependency_logic.dependentRow.reset();
    m_dependency_logic.children.clear();
}

QString SettingRowBase::getLabelText() { return m_key_label->text(); }

// This is to style inheritents of setting_row_base (i.e., all the different types of settings)
bool SettingRowBase::setStyleFromFile(QWidget* target, QString file) {
    m_style_file = QSharedPointer<QFile>(new QFile(file));

    if (!m_style_file->open(QIODevice::ReadOnly)) {
        qDebug("Could not open style resource file '%s'.\n", file.toStdString().c_str());
        m_style_file.clear();
        return false;
    }

    target->setStyleSheet(m_style_file->readAll());
    m_style_file->close();
    return true;
}

// Label has to be styled independently
bool SettingRowBase::styleLabelFromFile(QString file) {
    m_style_file = QSharedPointer<QFile>(new QFile(file));

    if (!m_style_file->open(QIODevice::ReadOnly)) {
        qDebug("Could not open style resource file '%s'.\n", file.toStdString().c_str());
        m_style_file.clear();
        return false;
    }

    m_key_label->setStyleSheet(m_style_file->readAll());
    m_style_file->close();
    return true;
}

void SettingRowBase::styleLabel(bool isConsistent) {
    if (isConsistent) {
        this->styleLabelFromFile(m_theme_path + "setting_rows_normal.qss");
        m_key_label->setToolTip("<html><body><p>" +
                                QString::fromStdString(m_json.at(Constants::Settings::Master::kToolTip)) +
                                "</p></body></html>");
    }
    else {
        this->styleLabelFromFile(m_theme_path + "setting_rows_warning.qss");
        m_key_label->setToolTip("Inconsistent settings between selected layers. <p>" % m_key_label->toolTip());
    }
}

bool SettingRowBase::isLocal() { return m_json[Constants::Settings::Master::kLocal]; }

fifojson SettingRowBase::getDependencies() { return m_json[Constants::Settings::Master::kDepends]; }

void SettingRowBase::addRowToNotify(QSharedPointer<SettingRowBase> row) { m_rows_to_notify.push_back(row); }

void SettingRowBase::setBases(QList<QSharedPointer<SettingsBase>> settings_bases) { m_settings_bases = settings_bases; }

QList<QSharedPointer<SettingsBase>> SettingRowBase::getBases() { return m_settings_bases; }

void SettingRowBase::checkDependencies() { setEnabled(checkLogic(m_dependency_logic)); }

void SettingRowBase::hide() {
    m_key_label->hide();
    m_unit_label->hide();
}

void SettingRowBase::show() {
    m_key_label->show();
    m_unit_label->show();
}

void SettingRowBase::setEnabled(bool enabled) {
    m_key_label->setEnabled(enabled);
    m_unit_label->setEnabled(enabled);
}

void SettingRowBase::setDependencyLogic(DependencyNode root) { m_dependency_logic = root; }

void SettingRowBase::setSettingsBase(QSharedPointer<SettingsBase> sb) { m_sb = sb; }

bool SettingRowBase::checkLogic(DependencyNode root) {
    if (root.key == "AND") {
        return checkLogic(root.children[0]) && checkLogic(root.children[1]);
    }
    else if (root.key == "OR") {
        return checkLogic(root.children[0]) || checkLogic(root.children[1]);
    }
    else if (root.key == "NOT") {
        return !checkLogic(root.children[0]);
    }
    else {
        if (root.dependentRow.isNull())
            return true;

        if (QCheckBox* checkBox = dynamic_cast<QCheckBox*>(root.dependentRow.get())) {
            for (auto& el : root.val.items()) {
                if (checkBox->isChecked() != static_cast<bool>(el.value()))
                    return false;
                else
                    return true;
            }
        }
        else if (QComboBox* comboBox = dynamic_cast<QComboBox*>(root.dependentRow.get())) {
            for (auto& el : root.val.items()) {
                if (comboBox->currentIndex() != el.value())
                    return false;
                else
                    return true;
            }
        }
        else if (QSpinBox* spinBox = dynamic_cast<QSpinBox*>(root.dependentRow.get())) {
            for (auto& el : root.val.items()) {
                if (spinBox->value() != el.value())
                    return false;
                else
                    return true;
            }
        }
    }

    // Default return in case none of the conditions match
    return false;
}

void SettingRowBase::checkDynamicDependencies() {}
} // Namespace ORNL
