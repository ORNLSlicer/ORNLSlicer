#include "windows/dialogs/template_save.h"

#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"

namespace ORNL {

TemplateSaveDialog::TemplateSaveDialog(QWidget* parent)
    : QDialog(parent), m_filename(QDir::homePath() + "/template.s2c") {
    m_convertable_types = QList<QString> {"location", "distance", "voltage",     "speed", "accel", "density",
                                          "ang_vel",  "time",     "temperature", "angle", "area"};

    this->setupUi();
}

QStringList& TemplateSaveDialog::keys() { return m_keys; }

QString& TemplateSaveDialog::filename() { return m_filename; }

QString TemplateSaveDialog::name() { return m_name_edit->text(); }

void TemplateSaveDialog::checkSelection(QTableWidgetItem* item) {

    QString major_key = m_table->item(item->row(), 2)->text();
    m_setting_status[major_key][item->text()] = (bool)item->checkState();
    QList<bool> valuesToCheck = m_setting_status[major_key].values();

    if (item->checkState() == Qt::CheckState::Checked) {
        if (std::all_of(valuesToCheck.begin(), valuesToCheck.end(), [](bool b) { return b; })) {
            m_category_check_boxes[major_key]->setCheckState(Qt::CheckState::Checked);

            if (std::all_of(m_category_check_boxes.begin(), m_category_check_boxes.end(),
                            [](ProgrammaticCheckBox* b) { return b->checkState(); }))
                m_all_check_box->setCheckState(Qt::CheckState::Checked);
        }
        else {
            m_all_check_box->setCheckState(Qt::CheckState::PartiallyChecked);
            m_category_check_boxes[major_key]->setCheckState(Qt::CheckState::PartiallyChecked);
        }

        m_browse_btn->setEnabled(true);
        m_button_container->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
    else {
        if (std::all_of(valuesToCheck.begin(), valuesToCheck.end(), [](bool b) { return !b; })) {
            m_category_check_boxes[major_key]->setCheckState(Qt::CheckState::Unchecked);
            if (std::none_of(m_category_check_boxes.begin(), m_category_check_boxes.end(),
                             [](ProgrammaticCheckBox* b) { return b->checkState(); }))
                m_all_check_box->setCheckState(Qt::CheckState::Unchecked);
        }
        else {
            m_all_check_box->setCheckState(Qt::CheckState::PartiallyChecked);
            m_category_check_boxes[major_key]->setCheckState(Qt::CheckState::PartiallyChecked);
        }

        for (QHash<QString, bool> major_category : m_setting_status) {
            for (bool b : major_category.values())
                if (b)
                    goto skip;
        }

        m_browse_btn->setEnabled(false);
        m_button_container->button(QDialogButtonBox::Ok)->setEnabled(false);

    skip:;
    }

    // If the clicked item is not in the selection, only select the clicked item.
    if (!m_table->selectedItems().contains(item))
        return;

    m_table->blockSignals(true);
    for (QTableWidgetItem* cur : m_table->selectedItems()) {
        cur->setCheckState(item->checkState());
    }
    m_table->blockSignals(false);
}

void TemplateSaveDialog::updateSelection() {
    QCheckBox* sender = qobject_cast<QCheckBox*>(QObject::sender());

    m_table->blockSignals(true);
    // If all, select everything.
    if (sender->text() == "All") {
        for (QCheckBox* box : m_category_check_boxes.values())
            box->setCheckState(sender->checkState());

        for (int i = 0; i < m_table->rowCount(); i++) {
            m_table->item(i, 0)->setCheckState(sender->checkState());
            m_setting_status[m_table->item(i, 2)->text()][m_table->item(i, 0)->text()] = (bool)sender->checkState();
        }
    }
    // Otherwise, select the category.
    else {
        if (sender->checkState() == Qt::CheckState::Checked) {
            if (std::all_of(m_category_check_boxes.begin(), m_category_check_boxes.end(),
                            [](ProgrammaticCheckBox* b) { return b->checkState(); }))
                m_all_check_box->setCheckState(Qt::CheckState::Checked);
            else
                m_all_check_box->setCheckState(Qt::CheckState::PartiallyChecked);
        }
        else {
            if (std::none_of(m_category_check_boxes.begin(), m_category_check_boxes.end(),
                             [](ProgrammaticCheckBox* b) { return b->checkState(); }))
                m_all_check_box->setCheckState(Qt::CheckState::Unchecked);
            else
                m_all_check_box->setCheckState(Qt::CheckState::PartiallyChecked);
        }

        for (int i = 0; i < m_table->rowCount(); i++) {
            if (m_table->item(i, 2)->text() == sender->text()) {
                m_table->item(i, 0)->setCheckState(sender->checkState());
                m_setting_status[m_table->item(i, 2)->text()][m_table->item(i, 0)->text()] = (bool)sender->checkState();
            }
        }
    }

    if (std::any_of(m_category_check_boxes.begin(), m_category_check_boxes.end(),
                    [](ProgrammaticCheckBox* b) { return !b->checkState(); }) ||
        std::any_of(m_setting_status.begin(), m_setting_status.end(), [](QHash<QString, bool> category) {
            return std::any_of(category.begin(), category.end(), [](bool b) { return b; });
        })) {
        m_browse_btn->setEnabled(true);
        m_button_container->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
    else {
        m_browse_btn->setEnabled(false);
        m_button_container->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    m_table->blockSignals(false);
}

void TemplateSaveDialog::fileDialog() {
    QFileDialog save_dialog;
    save_dialog.setWindowTitle("Save Template Location");
    save_dialog.setAcceptMode(QFileDialog::AcceptSave);
    save_dialog.setNameFilters(QStringList() << "Slicer-2 Configuration/Template File (*.s2c)" << "Any Files (*)");
    save_dialog.setDirectory(m_filename);
    save_dialog.setDefaultSuffix("s2c");

    if (!save_dialog.exec())
        return;

    m_filename = save_dialog.selectedFiles().first();
    m_edit->setText(m_filename);
}

void TemplateSaveDialog::filter(QString str) {
    for (int i = 0; i < m_table->rowCount(); i++) {
        bool match = (m_table->item(i, 0)->text().contains(str, Qt::CaseInsensitive));
        m_table->setRowHidden(i, !match);
    }
}

void TemplateSaveDialog::accept() {
    for (int i = 0; i < m_table->rowCount(); i++) {
        if (m_table->item(i, 0)->checkState()) {
            QString key = m_keys_dis[m_table->item(i, 0)->text()];
            m_keys.push_back(key);
        }
    }

    QMessageBox::information(this, "Save Template", "Template been succesfully exported.");

    this->QDialog::accept();
}

bool TemplateSaveDialog::itemChecked() {
    for (int i = 0; i < m_table->rowCount(); i++) {
        if (m_table->item(i, 0)->checkState()) {
            return true;
        }
    }
    return false;
}

void TemplateSaveDialog::setupUi() {
    this->setupWindow();
    this->setupWidgets();
    this->setupLayouts();
    this->setupTable();
    this->setupInsert();
    this->setupEvents();
    m_browse_btn->setEnabled(false);
    m_button_container->button(QDialogButtonBox::Ok)->setEnabled(false);
}

void TemplateSaveDialog::setupWindow() {
    this->setWindowTitle("Save As Template");
    this->setMinimumSize(900, 900);
}

void TemplateSaveDialog::setupWidgets() {
    m_table = new QTableWidget(this);

    m_dir_label = new QLabel(this);
    m_dir_label->setText("Select the settings to include in the template below:");

    m_sel_label = new QLabel(this);
    // A couple of spaces to line up with the container.
    m_sel_label->setText("Select:  ");

    m_button_container = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    m_save_label = new QLabel(this);
    m_save_label->setText("Save to:");

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search for setting...");

    m_edit = new QLineEdit(this);
    m_edit->setText(m_filename);
    m_edit->setReadOnly(true);
    m_edit->setEnabled(false);

    m_browse_btn = new QToolButton(this);
    m_browse_btn->setText("...");

    m_name_label = new QLabel(this);
    m_name_label->setText("Saved by:");

    m_name_edit = new QLineEdit(this);
    m_name_edit->setPlaceholderText("Optional name to be placed in header information");

    m_file_container = new QWidget(this);

    m_all_check_box = new ProgrammaticCheckBox("All", this);
}

void TemplateSaveDialog::setupLayouts() {
    m_layout = new QGridLayout(this);
    m_file_layout = new QGridLayout(m_file_container);
}

void TemplateSaveDialog::setupTable() {
    fifojson j = GSM->getMaster()->json();

    // Create the table with our desired dimensions.
    m_table->setColumnCount(4);
    m_table->setRowCount(j.size());

    // Setup the header.
    m_table->setHorizontalHeaderLabels(QStringList() << "Setting" << "Category" << "Major" << "Current");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // For each item in the master, fetch the display name, category, and current value.
    int pos = 0;
    int index = 0;
    bool isFound = true;

    while (true) {
        for (auto it : j.items()) {
            if (!GSM->getGlobal()->contains(QString::fromStdString(it.key()), index)) {
                isFound = false;
                break;
            }
            QString val;
            QString type = QString::fromStdString(it.value()["type"]);
            if (m_convertable_types.contains(type)) {
                double rawVal = GSM->getGlobal()->setting<double>(QString::fromStdString(it.key()), index);
                val = convertRawValue(type, rawVal);
            }
            else
                val = QString::fromStdString(GSM->getGlobal()->json()[0][it.key()].dump());

            if (val == "null")
                val = "(unset)";

            QTableWidgetItem* dis =
                new QTableWidgetItem(QString::fromStdString(it.value()[Constants::Settings::Master::kDisplay]));
            QTableWidgetItem* cat =
                new QTableWidgetItem(QString::fromStdString(it.value()[Constants::Settings::Master::kMinor]));
            QTableWidgetItem* maj =
                new QTableWidgetItem(QString::fromStdString(it.value()[Constants::Settings::Master::kMajor]));
            QTableWidgetItem* cur = new QTableWidgetItem(val);

            // Add the major category to our list if required.
            if (Q_UNLIKELY(!m_maj.contains(maj->text())))
                m_maj.insert(maj->text());

            // Adds checkboxes to display name.
            dis->setCheckState(Qt::Unchecked);
            cur->setTextAlignment(Qt::AlignRight);

            // Only first col is selectable.
            dis->setFlags(Qt::ItemFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable));
            cat->setFlags(Qt::NoItemFlags);
            maj->setFlags(Qt::NoItemFlags);
            cur->setFlags(Qt::NoItemFlags);

            // Insert
            m_table->setItem(pos, 0, dis);
            m_table->setItem(pos, 1, cat);
            m_table->setItem(pos, 2, maj);
            m_table->setItem(pos, 3, cur);

            m_keys_dis[dis->text()] = QString::fromStdString(it.key());
            // m_deselected_settings[maj->text()].insert(dis->text());
            m_setting_status[maj->text()][dis->text()] = false;
            pos++;
            //  }
            // ++index;
        }
        ++index;
        if (!isFound)
            break;
    }

    // Sort by category.
    m_table->setSortingEnabled(true);
    m_table->sortItems(1);
}

void TemplateSaveDialog::setupInsert() {
    m_file_layout->addWidget(m_save_label, 0, 0);
    m_file_layout->addWidget(m_edit, 0, 1);
    m_file_layout->addWidget(m_browse_btn, 0, 2);
    m_file_layout->addWidget(m_name_label, 1, 0);
    m_file_layout->addWidget(m_name_edit, 1, 1);

    m_layout->addWidget(m_dir_label, 0, 0, 1, 4);

    m_layout->addWidget(m_table, 1, 0, 1, 4);

    m_layout->addWidget(m_sel_label, 2, 0, 1, 1);

    m_layout->addWidget(m_all_check_box, 2, 1, 1, 1);

    int index = 3;
    for (QString str : m_maj) {
        m_category_check_boxes.insert(str, new ProgrammaticCheckBox(str, this));
        m_layout->addWidget(m_category_check_boxes[str], index, 1, 1, 1);
        ++index;
    }

    m_layout->addWidget(m_search, 2, 2, 1, 1);

    m_layout->addWidget(m_file_container, ++index, 0, 1, 3);

    // m_layout->addWidget();
    m_layout->addWidget(m_button_container, ++index, 0, 1, 3);
}

void TemplateSaveDialog::setupEvents() {
    // Connect filter to table.
    connect(m_search, &QLineEdit::textChanged, this, &TemplateSaveDialog::filter);

    // Connect the table and the combo box up
    connect(m_table, &QTableWidget::itemChanged, this, &TemplateSaveDialog::checkSelection);

    // Connect the browse button to the savedialog.
    connect(m_browse_btn, &QToolButton::clicked, this, &TemplateSaveDialog::fileDialog);

    // Connect check boxes
    connect(m_all_check_box, &QCheckBox::clicked, this, &TemplateSaveDialog::updateSelection);
    for (QCheckBox* box : m_category_check_boxes.values())
        connect(box, &QCheckBox::clicked, this, &TemplateSaveDialog::updateSelection);

    // Connect the ok and cancel buttons.
    QPushButton* ok = m_button_container->button(QDialogButtonBox::Ok);
    QPushButton* cancel = m_button_container->button(QDialogButtonBox::Cancel);
    connect(ok, &QPushButton::pressed, this, &TemplateSaveDialog::accept);
    connect(cancel, &QPushButton::pressed, this, &TemplateSaveDialog::reject);
}

QString TemplateSaveDialog::convertRawValue(QString type, double value) {
    if (type == "location" || type == "distance") {
        Distance base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getDistanceUnit()));
    }
    else if (type == "voltage") {
        Voltage base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getVoltageUnit()));
    }
    else if (type == "speed") {
        Velocity base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getVelocityUnit()));
    }
    else if (type == "accel") {
        Acceleration base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getAccelerationUnit()));
    }
    else if (type == "density") {
        Density base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getMassUnit() /
                                             (PreferencesManager::getInstance()->getDistanceUnit() *
                                              PreferencesManager::getInstance()->getDistanceUnit() *
                                              PreferencesManager::getInstance()->getDistanceUnit())));
    }
    else if (type == "ang_vel") {
        AngularVelocity base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getAngleUnit() /
                                             PreferencesManager::getInstance()->getTimeUnit()));
    }
    else if (type == "time") {
        Time base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getTimeUnit()));
    }
    else if (type == "temperature") {
        Temperature base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getTemperatureUnit()));
    }
    else if (type == "angle") {
        Angle base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getAngleUnit()));
    }
    else if (type == "area") {
        Area base_value(value);
        return QString::number(base_value.to(PreferencesManager::getInstance()->getDistanceUnit() *
                                             PreferencesManager::getInstance()->getDistanceUnit()));
    }
    else {
        return QString::number(value);
    }
}
} // namespace ORNL
