#include "windows/settings_file_compare.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
const QString kSettingsFileFilter = "ORNLSlicer Configuration/Template File (*.s2c);;Any Files (*)";

QString valueText(const fifojson& value) {
    return QString::fromStdString(value.dump());
}

QTableWidgetItem* textItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setToolTip(text);
    return item;
}

QTableWidgetItem* settingsIndexItem(int settings_index) {
    auto* item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, settings_index + 1);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

void configureTable(QTableWidget* table) {
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSortingEnabled(true);
    table->verticalHeader()->setVisible(false);
}
}  // namespace

SettingsFileCompareDialog::SettingsFileCompareDialog(QWidget* parent) : QDialog(parent, Qt::Window) {
    setWindowTitle(QApplication::applicationDisplayName() + ": Compare Settings Files");
    setMinimumSize(850, 500);
    resize(1100, 700);

    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/ornlslicer_logo.png"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    auto* file_layout = new QGridLayout();
    m_first_file_edit = new QLineEdit(this);
    m_first_file_edit->setReadOnly(true);
    m_first_file_edit->setPlaceholderText("Select the first .s2c file");
    auto* first_browse = new QPushButton("Browse...", this);

    m_second_file_edit = new QLineEdit(this);
    m_second_file_edit->setReadOnly(true);
    m_second_file_edit->setPlaceholderText("Select the second .s2c file");
    auto* second_browse = new QPushButton("Browse...", this);

    file_layout->addWidget(new QLabel("First settings file:", this), 0, 0);
    file_layout->addWidget(m_first_file_edit, 0, 1);
    file_layout->addWidget(first_browse, 0, 2);
    file_layout->addWidget(new QLabel("Second settings file:", this), 1, 0);
    file_layout->addWidget(m_second_file_edit, 1, 1);
    file_layout->addWidget(second_browse, 1, 2);
    file_layout->setColumnStretch(1, 1);

    m_summary_label = new QLabel("Select two settings files to compare.", this);
    m_summary_label->setWordWrap(true);

    m_changed_table = new QTableWidget(this);
    m_changed_table->setColumnCount(5);
    configureTable(m_changed_table);

    m_unique_table = new QTableWidget(this);
    m_unique_table->setColumnCount(5);
    configureTable(m_unique_table);

    m_result_tabs = new QTabWidget(this);
    m_result_tabs->addTab(m_changed_table, "Different Values (0)");
    m_result_tabs->addTab(m_unique_table, "Only in One File (0)");

    auto* buttons    = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_compare_button = buttons->addButton("Compare", QDialogButtonBox::ActionRole);
    m_compare_button->setEnabled(false);
    m_compare_button->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(file_layout);
    layout->addWidget(m_summary_label);
    layout->addWidget(m_result_tabs, 1);
    layout->addWidget(buttons);

    connect(first_browse, &QPushButton::clicked, this, &SettingsFileCompareDialog::browseFirstFile);
    connect(second_browse, &QPushButton::clicked, this, &SettingsFileCompareDialog::browseSecondFile);
    connect(m_first_file_edit, &QLineEdit::textChanged, this, &SettingsFileCompareDialog::refreshCompareState);
    connect(m_second_file_edit, &QLineEdit::textChanged, this, &SettingsFileCompareDialog::refreshCompareState);
    connect(m_compare_button, &QPushButton::clicked, this, &SettingsFileCompareDialog::compareSelectedFiles);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsFileCompareDialog::close);

    clearResult();
}

void SettingsFileCompareDialog::browseFirstFile() {
    browseForFile(m_first_file_edit, "Select First Settings File");
}

void SettingsFileCompareDialog::browseSecondFile() {
    browseForFile(m_second_file_edit, "Select Second Settings File");
}

void SettingsFileCompareDialog::compareSelectedFiles() {
    const SettingsFileComparator::Result result =
        SettingsFileComparator::compareFiles(m_first_file_edit->text(), m_second_file_edit->text());

    if (!result.errors.isEmpty()) {
        clearResult();
        m_summary_label->setText("The files could not be compared.");
        QMessageBox::critical(this, "Compare Settings Files", result.errors.join("\n\n"));
        return;
    }

    displayResult(result);
}

void SettingsFileCompareDialog::refreshCompareState() {
    const bool ready = !m_first_file_edit->text().isEmpty() && !m_second_file_edit->text().isEmpty();
    m_compare_button->setEnabled(ready);
    clearResult();
    m_summary_label->setText(ready ? "Choose Compare to view the differences."
                                   : "Select two settings files to compare.");
}

void SettingsFileCompareDialog::browseForFile(QLineEdit* target, const QString& title) {
    QString initial_path = target->text();
    if (initial_path.isEmpty()) {
        QLineEdit* other = target == m_first_file_edit ? m_second_file_edit : m_first_file_edit;
        if (!other->text().isEmpty()) initial_path = QFileInfo(other->text()).absolutePath();
    }

    const QString path = QFileDialog::getOpenFileName(this, title, initial_path, kSettingsFileFilter);
    if (!path.isEmpty()) target->setText(path);
}

void SettingsFileCompareDialog::displayResult(const SettingsFileComparator::Result& result) {
    QString first_label  = fileLabel(m_first_file_edit->text());
    QString second_label = fileLabel(m_second_file_edit->text());
    if (first_label == second_label) {
        first_label += " (First)";
        second_label += " (Second)";
    }

    m_changed_table->setSortingEnabled(false);
    m_changed_table->setRowCount(result.changed_settings.size());
    m_changed_table->setHorizontalHeaderLabels(
        {"Setting", "Key", "Settings Set", first_label + " Value", second_label + " Value"});

    for (int row = 0; row < result.changed_settings.size(); ++row) {
        const SettingsFileComparator::ChangedSetting& setting = result.changed_settings.at(row);
        m_changed_table->setItem(row, 0, textItem(displayName(setting.key)));
        m_changed_table->setItem(row, 1, textItem(setting.key));
        m_changed_table->setItem(row, 2, settingsIndexItem(setting.settings_index));
        m_changed_table->setItem(row, 3, textItem(valueText(setting.first_value)));
        m_changed_table->setItem(row, 4, textItem(valueText(setting.second_value)));
    }

    m_changed_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_changed_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_changed_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_changed_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_changed_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_changed_table->horizontalHeaderItem(3)->setToolTip(m_first_file_edit->text());
    m_changed_table->horizontalHeaderItem(4)->setToolTip(m_second_file_edit->text());
    m_changed_table->setSortingEnabled(true);
    m_changed_table->sortItems(0);

    const int unique_count = result.only_in_first.size() + result.only_in_second.size();
    m_unique_table->setSortingEnabled(false);
    m_unique_table->setRowCount(unique_count);
    m_unique_table->setHorizontalHeaderLabels({"Setting", "Key", "Settings Set", "Present In", "Value"});

    int row                    = 0;
    const auto add_unique_rows = [this, &row](const QVector<SettingsFileComparator::UniqueSetting>& settings,
                                              const QString& file_label, const QString& file_path) {
        for (const SettingsFileComparator::UniqueSetting& setting : settings) {
            m_unique_table->setItem(row, 0, textItem(displayName(setting.key)));
            m_unique_table->setItem(row, 1, textItem(setting.key));
            m_unique_table->setItem(row, 2, settingsIndexItem(setting.settings_index));
            QTableWidgetItem* file_item = textItem(file_label);
            file_item->setToolTip(file_path);
            m_unique_table->setItem(row, 3, file_item);
            m_unique_table->setItem(row, 4, textItem(valueText(setting.value)));
            ++row;
        }
    };
    add_unique_rows(result.only_in_first, first_label, m_first_file_edit->text());
    add_unique_rows(result.only_in_second, second_label, m_second_file_edit->text());

    m_unique_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_unique_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_unique_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_unique_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_unique_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_unique_table->setSortingEnabled(true);
    m_unique_table->sortItems(0);

    m_result_tabs->setTabText(0, QString("Different Values (%1)").arg(result.changed_settings.size()));
    m_result_tabs->setTabText(1, QString("Only in One File (%1)").arg(unique_count));

    if (result.changed_settings.isEmpty() && unique_count == 0) {
        m_summary_label->setText("No settings differences found.");
    }
    else {
        m_summary_label->setText(
            QString("%1 setting(s) have different values. %2 setting(s) appear in only one file (%3 only in %4; "
                    "%5 only in %6).")
                .arg(result.changed_settings.size())
                .arg(unique_count)
                .arg(result.only_in_first.size())
                .arg(first_label)
                .arg(result.only_in_second.size())
                .arg(second_label));
    }
}

void SettingsFileCompareDialog::clearResult() {
    m_changed_table->clearContents();
    m_changed_table->setRowCount(0);
    m_changed_table->setHorizontalHeaderLabels(
        {"Setting", "Key", "Settings Set", "First File Value", "Second File Value"});
    m_unique_table->clearContents();
    m_unique_table->setRowCount(0);
    m_unique_table->setHorizontalHeaderLabels({"Setting", "Key", "Settings Set", "Present In", "Value"});
    m_result_tabs->setTabText(0, "Different Values (0)");
    m_result_tabs->setTabText(1, "Only in One File (0)");
}

QString SettingsFileCompareDialog::displayName(const QString& key) const {
    const fifojson& master = GSM->getMaster()->json();
    const auto setting     = master.find(key.toStdString());
    if (setting == master.end() || !setting->is_object()) return key;

    const auto display = setting->find(Constants::Settings::Master::kDisplay);
    if (display == setting->end() || !display->is_string()) return key;

    return QString::fromStdString(display->get<std::string>());
}

QString SettingsFileCompareDialog::fileLabel(const QString& path) const {
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? path : name;
}

}  // namespace ORNL
