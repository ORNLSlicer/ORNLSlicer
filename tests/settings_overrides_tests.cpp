#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTableWidget>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "windows/settings_overrides.h"

namespace {
int findRow(QTableWidget* table, const QString& part, const QString& key, const QString& scope) {
    for (int row = 0; row < table->rowCount(); ++row)
        if (table->item(row, 0)->text() == part && table->item(row, 3)->data(Qt::UserRole).toString() == key &&
            table->item(row, 1)->text().startsWith(scope))
            return row;
    return -1;
}

int visibleRows(QTableWidget* table) {
    int count = 0;
    for (int row = 0; row < table->rowCount(); ++row) count += !table->isRowHidden(row);
    return count;
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
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const auto settle = [&] {
        app.processEvents();
        QCoreApplication::sendPostedEvents();
        app.processEvents();
    };
    expect(ORNL::Theme::initializeFonts(), "Could not load the bundled application fonts.");
    const auto preferences  = ORNL::PreferencesManager::getInstance();
    const auto old_theme    = preferences->getThemeText();
    const auto old_distance = preferences->getDistanceUnit();
    const auto old_velocity = preferences->getVelocityUnit();
    preferences->setDistanceUnit(ORNL::mm);
    preferences->setVelocityUnit(ORNL::mm / ORNL::s);
    QWidget root;
    const auto apply_theme = [&](ORNL::ThemeName theme) {
        preferences->setTheme(theme);
        QFile stylesheet(preferences->getTheme().getFolderPath() + "style.qss");
        if (!ORNL::Testing::expect(stylesheet.open(QIODevice::ReadOnly), "Could not read the root stylesheet.")) {
            passed = false;
            return;
        }
        QApplication::setPalette(preferences->getTheme().getPalette());
        root.setStyleSheet(stylesheet.readAll());
        app.processEvents();
    };
    apply_theme(ORNL::ThemeName::kLightMode);
    if (argc > 1) expect(QDir().mkpath(argv[1]), "Could not create the screenshot directory.");

    ORNL::CSM->clearParts();
    const auto global = ORNL::GSM->getGlobal();
    global->reset();
    global->setSetting("perimeter_count", 2);
    global->setSetting("perimeter", false);
    global->setSetting("perimeter_boundary_selection", 0);
    global->setSetting("layer_height", 1000.0);
    global->setSetting("perimeter_speed", 100000.0);
    global->setSetting("image_pixel_size_y", 1000.0);

    ORNL::SettingsOverridesDialog dialog(&root);
    dialog.show();
    app.processEvents();
    auto* table        = dialog.findChild<QTableWidget*>("settings_overrides_table");
    auto* filter       = dialog.findChild<QLineEdit*>("settings_overrides_filter");
    auto* summary      = dialog.findChild<QLabel*>("settings_overrides_summary");
    auto* part_filter  = dialog.findChild<QComboBox*>("settings_overrides_part_filter");
    auto* scope_filter = dialog.findChild<QComboBox*>("settings_overrides_scope_filter");
    auto* show_setting = dialog.findChild<QPushButton*>("settings_overrides_show_setting");
    if (!ORNL::Testing::expect(table != nullptr && filter != nullptr && summary != nullptr && part_filter != nullptr &&
                                   scope_filter != nullptr && show_setting != nullptr,
                               "The overview must expose its table, search, filters, summary, and navigation button."))
        return EXIT_FAILURE;
    expect(table->rowCount() == 0 && !summary->text().isEmpty(), "An empty project must have a clear empty state.");
    expect(table->editTriggers() == QAbstractItemView::NoEditTriggers, "The overview must be read-only.");
    expect(table->horizontalHeaderItem(5)->text() == "Value Being Replaced",
           "The replacement column must explain the value an override replaces.");
    expect(!show_setting->isEnabled(), "Navigation must be disabled without a selected override.");
    expect(part_filter->count() == 1 && part_filter->currentData().toUuid().isNull() && scope_filter->count() == 3,
           "Empty projects must retain the all-parts and all/part/layer filters.");
    if (argc > 1)
        expect(dialog.grab().save(QString(argv[1]) + "/empty-light.png"), "Could not save the empty screenshot.");

    auto bracket = QSharedPointer<ORNL::Part>::create();
    bracket->setName("Bracket");
    bracket->getSb()->setSetting("perimeter_count", 4);
    bracket->getSb()->setSetting("perimeter", false);
    bracket->getSb()->setSetting("perimeter_boundary_selection", 2);
    bracket->getSb()->setSetting("layer_height", 2000.0);
    bracket->createSettingsRange(0, 2);
    const auto first_layers = bracket->getSettingsRange(0, 2)->getSb();
    first_layers->setSetting("perimeter_count", 2);
    first_layers->setSetting("perimeter", true);
    first_layers->setSetting("perimeter_speed", 150000.0);
    bracket->createSettingsRange(6, 6, "Finish");
    bracket->getSettingsRange(6, 6)->getSb()->setSetting("perimeter_count", 6);
    bracket->createSettingsRange(9, 11, "Finish");
    ORNL::CSM->addPart(bracket);

    auto housing = QSharedPointer<ORNL::Part>::create();
    housing->setName("Housing");
    housing->getSb()->setSetting("perimeter_count", 3);
    housing->getSb()->setSetting("variable_layer_height", true);
    housing->getSb()->setSetting("image_pixel_size_y", 2000.0);
    housing->getSb()->setSetting("extension_calibration", QString("custom"));
    ORNL::CSM->addPart(housing);
    const auto global_before  = global->json();
    const auto bracket_before = bracket->getSb()->json();
    const auto range_before   = first_layers->json();
    settle();
    expect(table->rowCount() == 12, "Only effective differences across every part and layer range must be listed.");
    expect(part_filter->count() == 3 && part_filter->findData(bracket->getId()) >= 0 &&
               part_filter->findData(housing->getId()) >= 0,
           "The part filter must identify every project part using its persistent UUID.");

    const auto cell = [&](const QString& part, const QString& key, const QString& scope, int column) {
        const int row = findRow(table, part, key, scope);
        expect(row >= 0, "An expected override row is missing.");
        return row < 0 ? QString() : table->item(row, column)->text();
    };
    const auto replaced_source = [&](const QString& key, const QString& scope) {
        const int row = findRow(table, "Bracket", key, scope);
        expect(row >= 0, "An expected replacement source row is missing.");
        return row < 0 ? QString() : table->item(row, 5)->toolTip();
    };
    expect(cell("Bracket", "perimeter_count", "Part", 4) == "2" &&
               cell("Bracket", "perimeter_count", "Part", 5) == "2" &&
               cell("Bracket", "perimeter_count", "Part", 6) == "4",
           "Part overrides must show the global, inherited, and overriding values.");
    expect(replaced_source("perimeter_count", "Part").contains("Global") &&
               replaced_source("perimeter_count", "Layers 1").contains("Bracket") &&
               replaced_source("perimeter_speed", "Layers 1").contains("Global"),
           "Replacement tooltips must identify whether the value comes from Global or the owning part.");
    expect(cell("Bracket", "perimeter_count", "Layers 1", 4) == "2" &&
               cell("Bracket", "perimeter_count", "Layers 1", 5) == "4" &&
               cell("Bracket", "perimeter_count", "Layers 1", 6) == "2",
           "A layer that restores Global must still override its differing part value.");
    expect(cell("Bracket", "perimeter_count", "Layers 1", 1).contains("3"),
           "Layer ranges must display their inclusive upper bound using one-based numbers.");
    expect(findRow(table, "Bracket", "perimeter", "Part") < 0 &&
               findRow(table, "Bracket", "layer_height", "Layers 1") < 0 &&
               findRow(table, "Bracket", "perimeter_boundary_selection", "Layers 1") < 0,
           "Redundant part values and copied layer values must not appear as overrides.");
    expect(
        cell("Bracket", "perimeter", "Layers 1", 5) == "false" && cell("Bracket", "perimeter", "Layers 1", 6) == "true",
        "Boolean values must remain readable.");
    expect(cell("Bracket", "perimeter_boundary_selection", "Part", 6) == "External Boundaries",
           "Enumerations must display their option labels.");
    expect(cell("Bracket", "layer_height", "Part", 4) == "1 mm" &&
               cell("Bracket", "layer_height", "Part", 6) == "2 mm" &&
               cell("Bracket", "perimeter_speed", "Layers 1", 5) == "100 " + preferences->getVelocityUnitText() &&
               cell("Bracket", "perimeter_speed", "Layers 1", 6) == "150 " + preferences->getVelocityUnitText(),
           "Physical values must use preferred units, including Global fallback for layers without a part value.");
    expect(cell("Housing", "variable_layer_height", "Part", 5) == "false",
           "Missing Global values must inherit the master default.");
    expect(cell("Housing", "extension_calibration", "Part", 3) == "extension_calibration" &&
               cell("Housing", "extension_calibration", "Part", 5) == "Not set" &&
               cell("Housing", "extension_calibration", "Part", 6) == "custom",
           "Unknown settings must retain their key and value without inventing an inherited value.");
    expect(cell("Housing", "image_pixel_size_y", "Part", 6) == "2 mm",
           "Secondary components of composite settings must be included.");
    expect(cell("Bracket", "perimeter_count", "Layer 7", 1).contains("Finish") &&
               cell("Bracket", "perimeter_count", "Layers 10", 1).contains("Finish") &&
               cell("Bracket", "perimeter_count", "Layers 10", 1).contains("12"),
           "Grouped settings must retain every disjoint range and its group name.");
    for (int row = 0; row < table->rowCount(); ++row)
        for (int column = 0; column < table->columnCount(); ++column)
            expect(!(table->item(row, column)->flags() & Qt::ItemIsEditable), "Override cells must not be editable.");
    expect(global->json() == global_before && bracket->getSb()->json() == bracket_before &&
               first_layers->json() == range_before,
           "Building the overview must not materialize defaults or otherwise change project settings.");

    filter->setText("housing");
    expect(visibleRows(table) == 4, "Search must match part names without case sensitivity.");
    filter->setText("perimeter_boundary_selection");
    expect(visibleRows(table) == 1, "Search must include the underlying setting key.");
    filter->setText("Finish");
    expect(visibleRows(table) == 2, "Search must include layer group names.");
    filter->setText("no matching override");
    expect(visibleRows(table) == 0 && !summary->text().isEmpty(), "Unmatched searches must show a clear empty result.");
    filter->clear();
    expect(visibleRows(table) == 12, "Clearing search must restore every override.");

    const auto expect_counts = [&](int parts, int layers) {
        expect(visibleRows(table) == parts + layers && summary->text().contains(QString("%1 part").arg(parts)) &&
                   summary->text().contains(QString("%1 layer").arg(layers)),
               "The summary must count only part and layer overrides matching all active filters.");
    };
    struct FilterCase {
        QUuid part;
        int scope;
        int parts;
        int layers;
    };
    for (const auto& test : {FilterCase {{}, 0, 7, 5}, FilterCase {{}, 1, 7, 0}, FilterCase {{}, 2, 0, 5},
                             FilterCase {bracket->getId(), 0, 3, 5}, FilterCase {bracket->getId(), 1, 3, 0},
                             FilterCase {bracket->getId(), 2, 0, 5}, FilterCase {housing->getId(), 0, 4, 0},
                             FilterCase {housing->getId(), 1, 4, 0}, FilterCase {housing->getId(), 2, 0, 0}}) {
        part_filter->setCurrentIndex(part_filter->findData(test.part));
        scope_filter->setCurrentIndex(test.scope);
        expect_counts(test.parts, test.layers);
    }
    part_filter->setCurrentIndex(part_filter->findData(bracket->getId()));
    scope_filter->setCurrentIndex(2);
    filter->setText("Finish");
    expect_counts(0, 2);
    if (argc > 1)
        expect(dialog.grab().save(QString(argv[1]) + "/filtered-layers.png"), "Could not save filtered layers.");
    filter->setText("perimeter_boundary_selection");
    expect_counts(0, 0);
    filter->clear();
    part_filter->setCurrentIndex(0);
    scope_filter->setCurrentIndex(0);

    int requests = 0;
    QUuid requested_part;
    int requested_low  = -2;
    int requested_high = -2;
    QString requested_key;
    QObject::connect(&dialog, &ORNL::SettingsOverridesDialog::showSettingRequested,
                     [&](QUuid part_id, int low, int high, QString key) {
                         ++requests;
                         requested_part = part_id;
                         requested_low  = low;
                         requested_high = high;
                         requested_key  = key;
                     });
    const auto select = [&](const QString& part, const QString& key, const QString& scope) {
        const int row = findRow(table, part, key, scope);
        expect(row >= 0, "The override selected for navigation must exist.");
        if (row >= 0) {
            table->setCurrentCell(row, 0);
            table->selectRow(row);
        }
        return row;
    };
    select("Bracket", "perimeter_count", "Part");
    expect(show_setting->isEnabled(), "Known selected part overrides must enable navigation.");
    show_setting->click();
    expect(requests == 1 && requested_part == bracket->getId() && requested_low == -1 && requested_high == -1 &&
               requested_key == "perimeter_count",
           "The navigation button must request the exact part override and setting.");
    select("Bracket", "perimeter_count", "Layers 10");
    show_setting->click();
    expect(requests == 2 && requested_part == bracket->getId() && requested_low == 9 && requested_high == 11 &&
               requested_key == "perimeter_count",
           "Grouped layer navigation must retain the clicked range's exact zero-based bounds.");
    const int single_layer = select("Bracket", "perimeter_count", "Layer 7");
    if (single_layer >= 0)
        expect(QMetaObject::invokeMethod(table, "itemDoubleClicked", Qt::DirectConnection,
                                         Q_ARG(QTableWidgetItem*, table->item(single_layer, 3))),
               "The table must expose its double-click activation signal.");
    expect(requests == 3 && requested_part == bracket->getId() && requested_low == 6 && requested_high == 6,
           "Double-clicking a layer override must navigate to that exact layer.");
    bracket->removeSettingsRange(6, 6);
    show_setting->click();
    expect(requests == 3, "A range removed before the next refresh must never produce a stale navigation request.");
    bracket->createSettingsRange(6, 6, "Finish");
    dialog.scheduleRefresh();
    settle();
    select("Housing", "extension_calibration", "Part");
    expect(!show_setting->isEnabled(), "Unknown metadata must disable navigation to a nonexistent setting row.");
    show_setting->click();
    expect(requests == 3, "Unknown setting navigation must not emit a request.");
    const int filtered_row = select("Bracket", "perimeter_count", "Part");
    filter->setText("no matching override");
    expect(!show_setting->isEnabled(), "A selected override hidden by filters must disable navigation.");
    if (filtered_row >= 0)
        QMetaObject::invokeMethod(table, "itemDoubleClicked", Qt::DirectConnection,
                                  Q_ARG(QTableWidgetItem*, table->item(filtered_row, 3)));
    expect(requests == 3, "Hidden override rows must not emit navigation requests.");
    filter->clear();

    select("Housing", "perimeter_count", "Part");
    part_filter->setCurrentIndex(part_filter->findData(housing->getId()));
    ORNL::CSM->renamePart(housing, "Renamed Housing");
    dialog.scheduleRefresh();
    settle();
    expect(part_filter->currentData().toUuid() == housing->getId() && part_filter->currentText() == "Renamed Housing" &&
               visibleRows(table) == 4 &&
               table->currentRow() == findRow(table, "Renamed Housing", "perimeter_count", "Part"),
           "Renaming a part must retain filter and selected-row identity while updating the displayed name.");
    ORNL::CSM->renamePart(housing, "Housing");
    dialog.scheduleRefresh();
    settle();
    part_filter->setCurrentIndex(0);

    dialog.resize(850, 400);
    for (int column = 0; column < table->columnCount(); ++column) table->setColumnWidth(column, 300);
    table->setColumnWidth(3, 333);
    table->sortItems(3, Qt::DescendingOrder);
    select("Bracket", "perimeter_count", "Layer 7");
    settle();
    table->horizontalScrollBar()->setValue(100);
    table->verticalScrollBar()->setValue(1);
    const int horizontal_position = table->horizontalScrollBar()->value();
    const int vertical_position   = table->verticalScrollBar()->value();
    expect(horizontal_position > 0 && vertical_position > 0, "The preservation check must exercise both scrollbars.");
    int inserted_rows = 0;
    const auto inserted =
        QObject::connect(table->model(), &QAbstractItemModel::rowsInserted,
                         [&](const QModelIndex&, int first, int last) { inserted_rows += last - first + 1; });
    bracket->getSb()->setSetting("layer_height", 2500.0);
    // Keep copied layer heights inherited so that this edit changes a value without changing the row count.
    for (const auto& range : bracket->getSettingsRanges()) range->getSb()->setSetting("layer_height", 2500.0);
    dialog.scheduleRefresh();
    dialog.scheduleRefresh();
    dialog.scheduleRefresh();
    expect(cell("Bracket", "layer_height", "Part", 6) == "2 mm",
           "Scheduled refreshes must defer work until the event loop can coalesce them.");
    settle();
    expect(inserted_rows == 12 && cell("Bracket", "layer_height", "Part", 6) == "2.5 mm",
           "Multiple scheduled edits must produce one overview rebuild with the latest values.");
    QObject::disconnect(inserted);
    expect(table->columnWidth(3) == 333 && table->horizontalHeader()->sortIndicatorSection() == 3 &&
               table->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder &&
               table->currentRow() == findRow(table, "Bracket", "perimeter_count", "Layer 7") &&
               table->selectionModel()->selectedRows().size() == 1 &&
               table->horizontalScrollBar()->value() == horizontal_position &&
               table->verticalScrollBar()->value() == vertical_position,
           "Live refresh must preserve column widths, sorting, selected override, and scroll positions.");
    dialog.hide();
    dialog.show();
    settle();
    expect(table->columnWidth(3) == 333 &&
               table->currentRow() == findRow(table, "Bracket", "perimeter_count", "Layer 7") &&
               table->horizontalScrollBar()->value() == horizontal_position &&
               table->verticalScrollBar()->value() == vertical_position,
           "Reopening must preserve the overview's navigation and layout state.");
    dialog.resize(1200, 650);
    table->resizeColumnsToContents();
    table->horizontalScrollBar()->setValue(0);
    table->verticalScrollBar()->setValue(0);

    int image_index = 0;
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode, ORNL::ThemeName::kLightMode}) {
        apply_theme(theme);
        for (QWidget* widget :
             {static_cast<QWidget*>(table), static_cast<QWidget*>(filter), static_cast<QWidget*>(summary)}) {
            expect(widget->font().family() == "Noto Sans" && widget->font().pointSize() == 9 &&
                       widget->font().weight() == QFont::Normal,
                   "Overview fields and labels must retain the application's font across theme changes.");
        }
        const bool dark = theme == ORNL::ThemeName::kDarkMode;
        expect(dark ? table->palette().color(QPalette::Base).lightness() < 128
                    : table->palette().color(QPalette::Base).lightness() > 200,
               "The overview must follow the current application theme.");
        if (argc > 1)
            expect(dialog.grab().save(QString(argv[1]) + QString("/overrides-%1.png").arg(image_index++)),
                   "Could not save the themed overview screenshot.");
    }

    preferences->setDistanceUnit(ORNL::inch);
    preferences->setVelocityUnit(ORNL::inch / ORNL::minute);
    settle();
    expect(cell("Bracket", "layer_height", "Part", 6).contains(preferences->getDistanceUnitText()) &&
               cell("Bracket", "layer_height", "Part", 6) != "2 mm" &&
               cell("Bracket", "perimeter_speed", "Layers 1", 6).contains(preferences->getVelocityUnitText()),
           "Unit changes must automatically reformat physical values.");

    dialog.setRefreshSuspended(true);
    dialog.refresh();
    dialog.scheduleRefresh();
    auto loading_part = QSharedPointer<ORNL::Part>::create();
    loading_part->setName("Loading part");
    loading_part->getSb()->setSetting("perimeter_count", 8);
    ORNL::CSM->addPart(loading_part);
    settle();
    expect(table->rowCount() == 0 && !show_setting->isEnabled(),
           "Project loading must clear stale rows and suspend scheduled scans and navigation.");
    show_setting->click();
    expect(requests == 3, "Project loading must never emit a navigation request.");
    ORNL::CSM->removePart(loading_part->name());
    bracket->removeSettingsRange(0, 2);
    dialog.setRefreshSuspended(false);
    settle();
    expect(table->rowCount() == 9, "Resuming refresh must rebuild rows from the completed project state.");
    global->setSetting("perimeter_count", 4);
    expect(QMetaObject::invokeMethod(ORNL::GSM.get(), "globalLoaded", Qt::DirectConnection,
                                     Q_ARG(QString, QString("test global settings"))),
           "Global settings must expose their load notification.");
    settle();
    expect(findRow(table, "Bracket", "perimeter_count", "Part") < 0,
           "Loading changed Global values must automatically remove newly redundant part overrides.");
    dialog.hide();
    housing->getSb()->setSetting("perimeter_count", 5);
    dialog.scheduleRefresh();
    settle();
    expect(cell("Housing", "perimeter_count", "Part", 6) == "3",
           "Hidden overview windows must defer scheduled scans until they reopen.");
    dialog.show();
    settle();
    expect(cell("Housing", "perimeter_count", "Part", 6) == "5", "Reopening must show the latest project values.");
    select("Housing", "perimeter_count", "Part");
    ORNL::CSM->removePart(housing->name());
    show_setting->click();
    expect(requests == 3, "A part removed before the next refresh must never produce a stale navigation request.");
    settle();
    expect(table->rowCount() == 4 && part_filter->findData(housing->getId()) < 0,
           "Part removal must automatically discard its rows and filter option.");
    ORNL::CSM->clearParts();
    settle();
    expect(table->rowCount() == 0 && !show_setting->isEnabled(),
           "Clearing the project must automatically clear the overview.");

    preferences->setDistanceUnit(old_distance);
    preferences->setVelocityUnit(old_velocity);
    preferences->setTheme(old_theme);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
