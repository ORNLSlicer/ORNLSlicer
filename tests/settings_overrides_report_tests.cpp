#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTableWidget>
#include <QTemporaryDir>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "windows/settings_overrides.h"

namespace {
int findRow(QTableWidget* table, const QString& part, const QString& key, const QString& scope = "Part") {
    for (int row = 0; row < table->rowCount(); ++row)
        if (table->item(row, 0)->text() == part && table->item(row, 3)->data(Qt::UserRole).toString() == key &&
            table->item(row, 1)->text() == scope)
            return row;
    return -1;
}

QList<QStringList> tableRows(QTableWidget* table, bool selected_only = false) {
    QList<QStringList> rows;
    QStringList headers;
    for (int column = 0; column < table->columnCount(); ++column)
        headers.append(table->horizontalHeaderItem(column)->text());
    rows.append(headers);
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->isRowHidden(row) || (selected_only && !table->selectionModel()->isRowSelected(row, QModelIndex())))
            continue;
        QStringList values;
        for (int column = 0; column < table->columnCount(); ++column) values.append(table->item(row, column)->text());
        rows.append(values);
    }
    return rows;
}

// Parse records independently so quoted delimiters and embedded CR/LF must round-trip as cell contents.
QList<QStringList> parseRecords(const QString& text, QChar delimiter) {
    QList<QStringList> records;
    QStringList record;
    QString field;
    bool quoted = false;
    for (int index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (quoted) {
            if (character == '"' && index + 1 < text.size() && text.at(index + 1) == '"') {
                field += '"';
                ++index;
            }
            else if (character == '"')
                quoted = false;
            else
                field += character;
        }
        else if (character == '"')
            quoted = true;
        else if (character == delimiter) {
            record.append(field);
            field.clear();
        }
        else if (character == '\r' && index + 1 < text.size() && text.at(index + 1) == '\n')
            continue;
        else if (character == '\n') {
            record.append(field);
            records.append(record);
            record.clear();
            field.clear();
        }
        else
            field += character;
    }
    if (quoted) return {};
    if (!field.isEmpty() || !record.isEmpty()) {
        record.append(field);
        records.append(record);
    }
    return records;
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QSharedPointer<ORNL::Part> addPart(const QString& name) {
    auto part = QSharedPointer<ORNL::Part>::create();
    part->setName(name);
    ORNL::CSM->addPart(part);
    return part;
}
}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    bool passed       = true;
    const auto expect = [&](bool condition, const auto& message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const auto settle = [&] {
        app.processEvents();
        QCoreApplication::sendPostedEvents();
        app.processEvents();
    };
    expect(ORNL::Theme::initializeFonts(), "Could not load application fonts.");
    const auto preferences  = ORNL::PreferencesManager::getInstance();
    const auto old_theme    = preferences->getThemeText();
    const auto old_distance = preferences->getDistanceUnit();
    const auto old_velocity = preferences->getVelocityUnit();
    const auto old_angle    = preferences->getAngleUnit();
    preferences->setDistanceUnit(ORNL::mm);
    preferences->setVelocityUnit(ORNL::mm / ORNL::s);
    preferences->setAngleUnit(ORNL::deg);
    QWidget root;
    const auto apply_theme = [&](ORNL::ThemeName theme) {
        preferences->setTheme(theme);
        QFile stylesheet(preferences->getTheme().getFolderPath() + "style.qss");
        if (!ORNL::Testing::expect(stylesheet.open(QIODevice::ReadOnly),
                                   "Could not open the application stylesheet.")) {
            passed = false;
            return;
        }
        QApplication::setPalette(preferences->getTheme().getPalette());
        root.setStyleSheet(stylesheet.readAll());
        settle();
    };
    apply_theme(ORNL::ThemeName::kLightMode);
    ORNL::CSM->clearParts();
    auto global = ORNL::GSM->getGlobal();
    global->reset();
    global->setSetting("printing_material", 2);
    global->setSetting("slicing_mode", 0);
    global->setSetting("perimeter", true);
    auto part10 = addPart("Part 10");
    part10->getSb()->setSetting("perimeter_count", 10);
    part10->getSb()->setSetting("printing_material", 1);
    auto part2 = addPart("Part 2");
    part2->getSb()->setSetting("perimeter_count", 3);
    part2->getSb()->setSetting("printing_material", 0);
    for (const auto& bounds : {qMakePair(1, 1), qMakePair(1, 2), qMakePair(1, 9), qMakePair(9, 9)}) {
        part2->createSettingsRange(bounds.first, bounds.second);
        part2->getSettingsRange(bounds.first, bounds.second)->getSb()->setSetting("perimeter_count", bounds.second + 4);
    }

    ORNL::SettingsOverridesDialog dialog(&root);
    dialog.show();
    settle();
    auto* table         = dialog.findChild<QTableWidget*>("settings_overrides_table");
    auto* filter        = dialog.findChild<QLineEdit*>("settings_overrides_filter");
    auto* part_filter   = dialog.findChild<QComboBox*>("settings_overrides_part_filter");
    auto* scope_filter  = dialog.findChild<QComboBox*>("settings_overrides_scope_filter");
    auto* copy_button   = dialog.findChild<QPushButton*>("settings_overrides_copy");
    auto* export_button = dialog.findChild<QPushButton*>("settings_overrides_export");
    auto* show_button   = dialog.findChild<QPushButton*>("settings_overrides_show_setting");
    if (!ORNL::Testing::expect(
            table && filter && part_filter && scope_filter && copy_button && export_button && show_button,
            "The report must expose filters, copy/export actions, and navigation."))
        return EXIT_FAILURE;
    expect(table->columnCount() == 8 && table->horizontalHeaderItem(7)->text() == "Status",
           "The report must append a Status column while retaining the original seven columns.");
    expect(table->selectionMode() == QAbstractItemView::ExtendedSelection,
           "The report must permit multiple selected rows.");
    filter->setText("perimeter_count");
    table->sortItems(0, Qt::AscendingOrder);
    auto rows = tableRows(table);
    expect(rows.size() == 7 && rows.at(1).at(0) == "Part 2" && rows.last().at(0) == "Part 10",
           "Part names must sort naturally so Part 2 precedes Part 10.");
    part_filter->setCurrentIndex(part_filter->findData(part2->getId()));
    table->sortItems(1, Qt::AscendingOrder);
    rows = tableRows(table);
    QStringList scopes;
    for (int row = 1; row < rows.size(); ++row) scopes.append(rows.at(row).at(1));
    expect(scopes == QStringList {"Part", "Layer 2", "Layers 2-3", "Layers 2-10", "Layer 10"},
           "Scope sorting must compare numeric lower and upper bounds with part overrides first.");
    part_filter->setCurrentIndex(0);
    scope_filter->setCurrentIndex(1);
    filter->setText("printing_material");
    table->sortItems(6, Qt::AscendingOrder);
    rows = tableRows(table);
    expect(rows.size() == 3 && rows.at(1).at(6) == "ABS" && rows.at(2).at(6) == "ABS20CF",
           "Enumeration values must sort by their visible labels rather than stored indices.");

    ORNL::CSM->clearParts();
    global->reset();
    global->setSetting("infill_speed", 100000.0);
    auto numbers = addPart("Numbers");
    numbers->getSb()->setSetting("cylinder_axis_x", -10000.0);
    numbers->getSb()->setSetting("extension_decimal", -2.5);
    numbers->getSb()->setSetting("cylinder_axis_y", -2000.0);
    numbers->getSb()->setSetting("layer_height", 1500.0);
    numbers->getSb()->setSetting("perimeter_speed", 10000.0);
    numbers->getSb()->setSetting("infill_angle", 270 * ORNL::deg);
    numbers->getSb()->setSetting("infill_speed", fifojson());
    numbers->getSb()->setSetting("extension_text", QString("text"));
    filter->clear();
    scope_filter->setCurrentIndex(0);
    dialog.refresh();
    table->sortItems(6, Qt::AscendingOrder);
    QStringList keys;
    for (int row = 0; row < table->rowCount(); ++row) keys.append(table->item(row, 3)->data(Qt::UserRole).toString());
    const QStringList numeric_order {"infill_speed", "cylinder_axis_x", "extension_decimal", "cylinder_axis_y",
                                     "layer_height", "perimeter_speed", "infill_angle",      "extension_text"};
    expect(keys == numeric_order,
           "Value sorting must place Not set first, compare negative/decimal displayed magnitudes across units, then "
           "text.");
    table->sortItems(6, Qt::DescendingOrder);
    QStringList descending_keys;
    for (int row = table->rowCount() - 1; row >= 0; --row)
        descending_keys.append(table->item(row, 3)->data(Qt::UserRole).toString());
    expect(descending_keys == numeric_order, "Descending value sorting must reverse the full natural order.");

    ORNL::CSM->clearParts();
    global->reset();
    const QString file_part = "File, \"β\"\tplate\n2";
    const QString file_name = "/tmp/schedule, \"γ\"\tfixture\r\n.csv";
    const QString code      = "G1 X2, Y3\t; \"δοκιμή\"\r\nM30";
    auto file               = addPart(file_part);
    file->getSb()->setSetting("start_code", code);
    file->getSb()->setSetting("arc_specialties_g80_weld_schedule_file", file_name);
    file->getSb()->setSetting("layer_height", 2500.0);
    file->getSb()->setSetting("extension_value", QString("α,\"β\"\tγ\nδ"));
    file->createSettingsRange(1, 1);
    file->getSettingsRange(1, 1)->getSb()->setSetting("layer_height", 3500.0);
    file->createSettingsRange(9, 9);
    file->getSettingsRange(9, 9)->getSb()->setSetting("start_code", QString("G4 P10"));
    auto other = addPart("File 10");
    other->getSb()->setSetting("start_code", QString("Other code"));
    other->getSb()->setSetting("layer_height", 10000.0);
    dialog.refresh();
    table->sortItems(0, Qt::AscendingOrder);
    const auto select_rows = [&](const QList<int>& selected) {
        table->clearSelection();
        for (int row : selected) {
            expect(row >= 0, "The selected report row must exist.");
            if (row >= 0)
                table->selectionModel()->select(table->model()->index(row, 0),
                                                QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    };
    const auto select_report = [&] {
        select_rows({findRow(table, other->name(), "start_code"),
                     findRow(table, file->name(), "layer_height", "Layer 2"),
                     findRow(table, file->name(), "arc_specialties_g80_weld_schedule_file"),
                     findRow(table, file->name(), "start_code")});
    };
    select_report();
    expect(copy_button->isEnabled() && !show_button->isEnabled(),
           "Multiple selected rows must enable copy while disabling single-setting navigation.");
    const auto selected_rows = tableRows(table, true);
    dialog.copySelectedRows();
    const QString copied = QApplication::clipboard()->text();
    expect(selected_rows.size() == 5 && parseRecords(copied, '\t') == selected_rows,
           "Copy must include all eight headers and selected rows in table order with lossless TSV escaping.");
    expect(copied.contains("\"File, \"\"β\"\"\tplate\n2\"") &&
               copied.contains("\"/tmp/schedule, \"\"γ\"\"\tfixture\r\n.csv\""),
           "TSV must quote and escape tabs, double quotes, and embedded line breaks.");
    part_filter->setCurrentIndex(part_filter->findData(file->getId()));
    dialog.copySelectedRows();
    expect(parseRecords(QApplication::clipboard()->text(), '\t') == tableRows(table, true) &&
               tableRows(table, true).size() == 4,
           "Copy must omit selected rows hidden by the current filters.");
    part_filter->setCurrentIndex(0);
    select_report();
    other->getSb()->setSetting("layer_height", 11000.0);
    dialog.scheduleRefresh();
    settle();
    expect(table->selectionModel()->selectedRows().size() == 4 &&
               table->selectionModel()->isRowSelected(findRow(table, file->name(), "layer_height", "Layer 2"), {}) &&
               table->selectionModel()->isRowSelected(findRow(table, other->name(), "start_code"), {}) &&
               table->selectionModel()->isRowSelected(findRow(table, file->name(), "start_code"), {}) &&
               table->selectionModel()->isRowSelected(
                   findRow(table, file->name(), "arc_specialties_g80_weld_schedule_file"), {}),
           "Refresh must restore every selected override by part UUID, range, and key.");

    int navigations = 0;
    QUuid requested_part;
    int requested_low  = -1;
    int requested_high = -1;
    QString requested_key;
    QObject::connect(&dialog, &ORNL::SettingsOverridesDialog::showSettingRequested,
                     [&](QUuid part_id, int low, int high, QString key) {
                         ++navigations;
                         requested_part = part_id;
                         requested_low  = low;
                         requested_high = high;
                         requested_key  = key;
                     });
    table->selectionModel()->setCurrentIndex(table->model()->index(findRow(table, other->name(), "start_code"), 0),
                                             QItemSelectionModel::NoUpdate);
    const int clicked = findRow(table, file->name(), "layer_height", "Layer 2");
    if (clicked >= 0)
        QMetaObject::invokeMethod(table, "itemDoubleClicked", Qt::DirectConnection,
                                  Q_ARG(QTableWidgetItem*, table->item(clicked, 3)));
    expect(navigations == 1 && requested_part == file->getId() && requested_low == 1 && requested_high == 1 &&
               requested_key == "layer_height",
           "Double-click navigation from a multi-selection must use the clicked row rather than the current row.");

    select_report();
    QAction competing_copy("Main window copy", &dialog);
    competing_copy.setShortcut(QKeySequence::Copy);
    competing_copy.setShortcutContext(Qt::ApplicationShortcut);
    dialog.addAction(&competing_copy);
    int competing_requests = 0;
    QObject::connect(&competing_copy, &QAction::triggered, [&] { ++competing_requests; });
    table->setFocus();
    QKeyEvent shortcut_override(QEvent::ShortcutOverride, Qt::Key_C, Qt::ControlModifier);
    shortcut_override.setAccepted(false);
    QApplication::sendEvent(table, &shortcut_override);
    expect(shortcut_override.isAccepted(), "The report table must claim Ctrl+C before application-wide shortcuts.");
    QApplication::clipboard()->setText("unchanged");
    QKeyEvent copy_key(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
    QApplication::sendEvent(table, &copy_key);
    expect(competing_requests == 0 && parseRecords(QApplication::clipboard()->text(), '\t') == tableRows(table, true),
           "Ctrl+C in the table must copy the report without triggering the application's part-copy action.");
    filter->setText("copy only this search");
    filter->selectAll();
    QApplication::sendEvent(filter, &copy_key);
    expect(QApplication::clipboard()->text() == "copy only this search",
           "Ctrl+C in the search field must retain ordinary text-field behavior.");
    filter->clear();
    table->clearSelection();
    QApplication::clipboard()->setText("no selection");
    dialog.copySelectedRows();
    expect(!copy_button->isEnabled() && QApplication::clipboard()->text() == "no selection",
           "Copy without visible selected rows must be disabled and leave the clipboard unchanged.");

    QTemporaryDir directory;
    if (!ORNL::Testing::expect(directory.isValid(), "Could not create the CSV export fixture directory."))
        return EXIT_FAILURE;
    const QString csv_path = directory.filePath("overrides-β.csv");
    QString error;
    part_filter->setCurrentIndex(part_filter->findData(file->getId()));
    scope_filter->setCurrentIndex(1);
    table->sortItems(6, Qt::AscendingOrder);
    bool exported = dialog.exportCsv(csv_path, &error);
    expect(exported, QString("Could not export filtered report: %1").arg(error).toStdString());
    const QByteArray csv_bytes = readFile(csv_path);
    const QString csv_text     = QString::fromUtf8(csv_bytes);
    expect(csv_text.toUtf8() == csv_bytes && parseRecords(csv_text, ',') == tableRows(table) &&
               tableRows(table).size() == 5,
           "CSV export must preserve UTF-8, displayed units, all eight headers, current sorting, and active filters.");
    expect(csv_text.contains("\"File, \"\"β\"\"\tplate\n2\"") &&
               csv_text.contains("\"/tmp/schedule, \"\"γ\"\"\tfixture\r\n.csv\"") && csv_text.contains("2.5 mm"),
           "CSV must quote commas/quotes/newlines and export displayed physical units.");
    scope_filter->setCurrentIndex(2);
    table->sortItems(1, Qt::DescendingOrder);
    expect(dialog.exportCsv(csv_path, &error), "CSV export must safely replace an existing report.");
    rows = parseRecords(QString::fromUtf8(readFile(csv_path)), ',');
    expect(
        rows == tableRows(table) && rows.size() == 3 && rows.at(1).at(1) == "Layer 10" && rows.at(2).at(1) == "Layer 2",
        "CSV must export all visible layers in the current numeric scope sort order.");
    filter->setText("no matching report row");
    expect(dialog.exportCsv(csv_path, &error) &&
               parseRecords(QString::fromUtf8(readFile(csv_path)), ',') == tableRows(table) &&
               tableRows(table).size() == 1,
           "Empty filtered results must still export the eight-column header.");
    error.clear();
    const QString invalid_path = directory.filePath("missing/report.csv");
    expect(!dialog.exportCsv(invalid_path, &error) && !error.isEmpty() && !QFile::exists(invalid_path),
           "An unwritable destination must report an error without creating a partial CSV file.");
    const QByteArray before_loading = readFile(csv_path);
    dialog.setRefreshSuspended(true);
    error.clear();
    QApplication::clipboard()->setText("loading");
    dialog.copySelectedRows();
    expect(!copy_button->isEnabled() && !export_button->isEnabled() && QApplication::clipboard()->text() == "loading" &&
               !dialog.exportCsv(csv_path, &error) && !error.isEmpty() && readFile(csv_path) == before_loading,
           "Loading must disable report actions and preserve an existing export file.");
    dialog.setRefreshSuspended(false);
    filter->clear();
    scope_filter->setCurrentIndex(0);
    ORNL::CSM->clearParts();
    dialog.refresh();
    expect(dialog.exportCsv(csv_path, &error) && parseRecords(QString::fromUtf8(readFile(csv_path)), ',').size() == 1,
           "An empty project must export a header-only report.");

    global->reset();
    global->setSetting("slicing_mode", 0);
    global->setSetting("layer_height", 1000.0);
    auto build = addPart("Build object");
    build->setModifierEnabled(false);
    build->getSb()->setSetting("perimeter_count", 3);
    build->getSb()->setSetting("layer_height", 0.0);
    build->getSb()->setSetting("start_code", QString("M117 Ready"));
    build->getSb()->setSetting("extension_unknown", 1);
    build->createSettingsRange(0, 0);
    build->getSettingsRange(0, 0)->getSb()->setSetting("perimeter", true);
    build->getSettingsRange(0, 0)->getSb()->setSetting("perimeter_count", 5);
    auto modifier = addPart("Disabled settings mesh");
    modifier->setMeshType(ORNL::MeshType::kSettings);
    modifier->setModifierEnabled(false);
    modifier->getSb()->setSetting("start_code", QString("M117 Modifier"));
    auto clipper = addPart("Disabled clipping mesh");
    clipper->setMeshType(ORNL::MeshType::kClipping);
    clipper->setModifierEnabled(false);
    clipper->getSb()->setSetting("start_code", QString("M117 Clipper"));
    const bool height_hidden = preferences->isSettingHidden("Profile", "Layer");
    if (!height_hidden) preferences->addHiddenSetting("Profile", "Layer");
    dialog.refresh();
    const auto status = [&](const QString& part, const QString& key, const QString& scope = "Part") {
        const int row = findRow(table, part, key, scope);
        expect(row >= 0, "The override being checked for activity must exist.");
        return row < 0 ? QString() : table->item(row, 7)->text();
    };
    expect(status(build->name(), "perimeter_count") == "Inactive" &&
               status(build->name(), "perimeter_count", "Layer 1") == "Active",
           "Status must evaluate dependencies against the effective global, part, and owning layer settings.");
    expect(status(build->name(), "start_code") == "Active" && status(build->name(), "layer_height") == "Active",
           "Build objects, local:false metadata, hidden UI preferences, and warning-only values must not become "
           "inactive.");
    expect(status(build->name(), "extension_unknown") == "Unknown", "Missing metadata must produce an Unknown status.");
    const int disabled_row        = findRow(table, modifier->name(), "start_code");
    const QString disabled_reason = disabled_row < 0 ? QString() : table->item(disabled_row, 7)->toolTip();
    expect(status(modifier->name(), "start_code") == "Inactive" &&
               status(clipper->name(), "start_code") == "Inactive" &&
               disabled_reason.contains("disabled", Qt::CaseInsensitive),
           "Disabled settings and clipping meshes must explain their inactive overrides.");
    filter->setText("Inactive");
    rows = tableRows(table);
    expect(rows.size() == 4, "Searching Inactive must retain the disabled dependency and both disabled modifiers.");
    filter->setText(disabled_reason);
    expect(disabled_row >= 0 && !table->isRowHidden(disabled_row), "Search must include the inactive reason tooltip.");
    filter->setText("Unknown");
    rows = tableRows(table);
    expect(rows.size() == 2 && rows.at(1).at(7) == "Unknown", "The Unknown status must be searchable.");
    filter->clear();
    table->sortItems(0, Qt::AscendingOrder);
    if (argc > 1) {
        expect(QDir().mkpath(argv[1]), "Could not create the report screenshot directory.");
        int image_index = 0;
        for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode}) {
            apply_theme(theme);
            const int widths[] {115, 90, 155, 200, 105, 140, 130, 85};
            for (int column = 0; column < table->columnCount(); ++column) table->setColumnWidth(column, widths[column]);
            expect(dialog.grab().save(QString(argv[1]) + QString("/report-status-%1.png").arg(image_index++)),
                   "Could not capture active/inactive report rows.");
        }
    }
    build->getSb()->setSetting("perimeter", true);
    modifier->setModifierEnabled(true);
    clipper->setModifierEnabled(true);
    dialog.scheduleRefresh();
    settle();
    expect(status(build->name(), "perimeter_count") == "Active" && status(modifier->name(), "start_code") == "Active" &&
               status(clipper->name(), "start_code") == "Active",
           "Activity status must update when dependencies and modifier enablement change.");
    global->setSetting("slicing_mode", 1);
    dialog.refresh();
    expect(status(build->name(), "perimeter_count") == "Inactive",
           "False visibility gates must mark overrides inactive.");
    build->getSettingsRange(0, 0)->getSb()->setSetting("slicing_mode", 0);
    dialog.refresh();
    expect(status(build->name(), "perimeter_count", "Layer 1") == "Active",
           "Layer visibility must use the layer's effective slicing mode rather than the global mode.");

    if (!height_hidden) preferences->removeHiddenSetting("Profile", "Layer");
    ORNL::CSM->clearParts();
    preferences->setDistanceUnit(old_distance);
    preferences->setVelocityUnit(old_velocity);
    preferences->setAngleUnit(old_angle);
    preferences->setTheme(old_theme);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
