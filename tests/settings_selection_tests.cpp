#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "utilities/theme_tool.h"
#include "widgets/settings/setting_bar.h"
#include "widgets/settings/setting_spin_box.h"
#include "windows/preferences_window.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const auto preferences = ORNL::PreferencesManager::getInstance();
    ORNL::PreferencesWindow preferences_window;
    auto* preference_tabs = preferences_window.findChild<QTabWidget*>();
    for (int index = 0; index < preference_tabs->count(); ++index)
        if (preference_tabs->tabText(index) == "Settings") preference_tabs->setCurrentIndex(index);
    preferences_window.show();
    app.processEvents();
    if (argc > 1)
        expect(preferences_window.grab().save(QString(argv[1]) + "/preferences.png"),
               "Could not save preferences screenshot.");

    auto* checkbox = preferences_window.findChild<QCheckBox*>("prompt_settings_for_all_categories");
    expect(checkbox != nullptr, "Settings preferences must offer an all-category settings-file prompt toggle.");
    if (checkbox == nullptr) return EXIT_FAILURE;
    expect(!checkbox->isChecked(), "The all-category prompt must default to disabled.");

    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create settings fixture directory.")) return EXIT_FAILURE;
    const auto manager = ORNL::SettingsManager::getInstance();
    QFile versions(":/configs/versions.conf");
    if (!ORNL::Testing::expect(versions.open(QIODevice::ReadOnly), "Could not read settings version."))
        return EXIT_FAILURE;
    const auto version = fifojson::parse(versions.readAll().toStdString()).at("master_version");
    const QStringList categories {"Printer", "Material", "Profile", "Experimental"};
    const QStringList keys {"tool_number", "printing_material", "perimeter_count", "tormach_mode"};
    const auto load_fixture = [&](const QString& name, const QList<int>& indices, int value,
                                  fifojson settings = fifojson::object()) {
        for (int index : indices) settings[keys[index].toStdString()] = value;
        if (name == "old") settings["individual_layer_output"] = true;
        fifojson document  = {{"header", {{"version", version}}}, {"settings", fifojson::array({settings})}};
        const QString path = temp_dir.filePath(name + ".s2c");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(document.dump().c_str());
        file.close();
        return manager->loadGlobalJson(path);
    };
    if (!ORNL::Testing::expect(load_fixture("old", {0, 1, 2, 3}, 1) && load_fixture("new", {0, 1, 2, 3}, 2) &&
                                   load_fixture("partial", {0, 2}, 3) && load_fixture("single", {1}, 4) &&
                                   load_fixture("reload", {0, 1, 2, 3}, 2),
                               "Could not load settings fixtures."))
        return EXIT_FAILURE;

    QHash<QString, QString> initial_history;
    for (const QString& category : categories) {
        initial_history[category] = "old";
        ORNL::CSM->setMostRecentSettingHistory(category, "old");
    }
    manager->constructActiveGlobal(initial_history);
    ORNL::SettingBar bar(initial_history);
    bar.resize(650, 800);
    auto* tabs  = bar.findChild<QTabWidget*>();
    auto* combo = bar.findChild<QComboBox*>(QString(), Qt::FindDirectChildrenOnly);
    bar.show();
    const auto select_category = [&](int category_index) {
        for (int index = 0; index < tabs->count(); ++index)
            if (tabs->tabText(index) == categories[category_index]) tabs->setCurrentIndex(index);
    };
    const auto select_file = [&](const QString& name) {
        expect(combo->findText(name) >= 0, "The selected file must be available in the current category.");
        combo->setCurrentText(name);
        QMetaObject::invokeMethod(combo, "textActivated", Qt::DirectConnection, Q_ARG(QString, name));
    };
    int prompts                = 0;
    auto answer                = QMessageBox::No;
    bool expect_partial_prompt = false;
    QTimer responder;
    QObject::connect(&responder, &QTimer::timeout, [&] {
        if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            ++prompts;
            expect(prompt->text().contains("Printer") && prompt->text().contains("Profile"),
                   "The prompt must identify the categories being changed.");
            if (expect_partial_prompt)
                expect(!prompt->text().contains("Material") && !prompt->text().contains("Experimental"),
                       "The prompt must exclude categories removed when reloading the file.");
            if (argc > 1)
                expect(prompt->grab().save(QString(argv[1]) + QString("/prompt-%1.png").arg(prompts)),
                       "Could not save prompt.");
            prompt->button(answer)->click();
        }
    });
    responder.start(10);
    int changes = 0;
    QObject::connect(&bar, &ORNL::SettingBar::settingsBaseChanged, [&] { ++changes; });

    select_category(0);
    select_file("new");
    expect(prompts == 0 && changes == 1, "Disabled prompts must preserve a single category change.");
    expect(manager->getGlobal()->setting<int>(keys[0]) == 2 && manager->getGlobal()->setting<int>(keys[1]) == 1,
           "Changing the printer file must preserve the material file when prompts are disabled.");
    manager->getGlobal()->setSetting(keys[0], 9);
    select_file("new");
    expect(prompts == 0 && changes == 1 && manager->getGlobal()->setting<int>(keys[0]) == 9,
           "Reselecting the active file with prompts disabled must preserve manual edits.");

    checkbox->click();
    if (argc > 1)
        expect(preferences_window.grab().save(QString(argv[1]) + "/preferences-enabled.png"),
               "Could not save enabled preference.");
    select_category(2);
    select_file("new");
    expect(prompts == 1 && changes == 2, "Enabling the preference must prompt once per user selection.");
    expect(manager->getGlobal()->setting<int>(keys[2]) == 2 && manager->getGlobal()->setting<int>(keys[3]) == 1,
           "Declining must apply only the current category.");
    expect(manager->getGlobal()->setting<bool>("individual_layer_output"),
           "Declining must preserve settings omitted from the file in other categories.");
    manager->getGlobal()->setSetting(keys[2], 9);
    select_file("new");
    expect(prompts == 2 && changes == 2 && manager->getGlobal()->setting<int>(keys[2]) == 9,
           "Declining when reselecting the active file must preserve manual edits.");

    answer = QMessageBox::Yes;
    // Reselect the current file to apply it to the remaining categories.
    select_file("new");
    expect(prompts == 3 && changes == 3, "Applying all categories must emit one settings change notification.");
    for (int index = 0; index < categories.size(); ++index) {
        expect(manager->getGlobal()->setting<int>(keys[index]) == 2, "Accepting must update all four category values.");
        expect(ORNL::CSM->getMostRecentSettingHistory().value(categories[index]) == "new",
               "Accepting must persist the file selection for every category.");
        select_category(index);
        expect(combo->currentText() == "new", "Each category dropdown must display the applied file.");
    }
    expect(!manager->getGlobal()->setting<bool>("individual_layer_output"),
           "Applying all categories must reset settings omitted from the replacement file in other categories.");
    bar.reloadDisplayedList();
    bar.displayNewSetting(categories, "new");
    app.processEvents();
    expect(prompts == 3 && changes == 3, "Tab changes and programmatic list refreshes must not prompt or apply files.");

    select_category(0);
    select_file("partial");
    expect(prompts == 4, "Files containing multiple categories must support the prompt.");
    for (int index = 0; index < categories.size(); ++index) {
        const bool included = index == 0 || index == 2;
        expect(manager->getGlobal()->setting<int>(keys[index]) == (included ? 3 : 2),
               "Applying a partial file must preserve values in categories absent from the file.");
        select_category(index);
        expect(combo->currentText() == (included ? "partial" : "new"),
               "Applying a partial file must preserve selections in categories absent from the file.");
        expect(ORNL::CSM->getMostRecentSettingHistory().value(categories[index]) == (included ? "partial" : "new"),
               "Applying a partial file must persist only the categories it contains.");
    }
    select_category(1);
    select_file("single");
    expect(prompts == 4 && manager->getGlobal()->setting<int>(keys[1]) == 4,
           "A file containing only one category must apply without a redundant prompt.");
    checkbox->click();
    select_category(3);
    select_file("old");
    expect(prompts == 4 && manager->getGlobal()->setting<int>(keys[3]) == 1 &&
               manager->getGlobal()->setting<int>(keys[0]) == 3,
           "Disabling the preference must immediately restore category-only selection.");

    const auto previous_settings = manager->getGlobal()->json();
    const auto previous_history  = ORNL::CSM->getMostRecentSettingHistory();
    expect(load_fixture("reload", {0, 2}, 6), "Could not reload a full settings file with only two categories.");
    bar.reloadDisplayedList();
    expect(manager->getGlobal()->json() == previous_settings &&
               ORNL::CSM->getMostRecentSettingHistory() == previous_history,
           "Reloading a file must preserve active settings and selection history until it is selected.");
    checkbox->click();
    expect_partial_prompt = true;
    select_category(0);
    const int previous_changes = changes;
    select_file("reload");
    expect(prompts == 5 && changes == previous_changes + 1,
           "Applying a reloaded file must prompt once and emit one settings change notification.");
    for (int index = 0; index < categories.size(); ++index) {
        const bool included = index == 0 || index == 2;
        expect(manager->getGlobal()->setting<int>(keys[index]) ==
                   (included ? 6 : previous_settings[0][keys[index].toStdString()].get<int>()),
               "Applying a reloaded file must preserve values in categories removed from the file.");
        const QString expected_file = included ? "reload" : previous_history.value(categories[index]);
        select_category(index);
        expect(combo->currentText() == expected_file &&
                   ORNL::CSM->getMostRecentSettingHistory().value(categories[index]) == expected_file,
               "Applying a reloaded file must preserve selections and history in removed categories.");
    }
    checkbox->click();
    expect(load_fixture("active_reload", {0, 1, 2, 3}, 2, {{"tool_coordinate", 2}, {"individual_layer_output", true}}),
           "Could not load the active-template reload fixture.");
    for (int index = 0; index < categories.size(); ++index) {
        select_category(index);
        bar.reloadDisplayedList();
        select_file("active_reload");
    }
    const auto active_before_reload  = manager->getGlobal()->json();
    const auto history_before_reload = ORNL::CSM->getMostRecentSettingHistory();
    const int changes_before_reload  = changes;
    expect(load_fixture("active_reload", {0, 2}, 3) && load_fixture("active_reload", {0, 2}, 4),
           "Could not reload the active template without its old keys and categories.");
    bar.reloadDisplayedList();
    expect(manager->getGlobal()->json() == active_before_reload &&
               ORNL::CSM->getMostRecentSettingHistory() == history_before_reload && changes == changes_before_reload,
           "Repeated active-template reloads must preserve active values and history until selection.");

    select_category(0);
    select_file("partial");
    expect(manager->getGlobal()->setting<int>("tool_coordinate") == 0,
           "Switching files must reset keys removed from the active template by a reload.");
    expect(manager->getGlobal()->setting<bool>("individual_layer_output"),
           "Switching Printer must preserve old active keys in Experimental.");
    select_category(3);
    select_file("new");
    expect(!manager->getGlobal()->setting<bool>("individual_layer_output"),
           "Switching files must reset old active keys even when their category disappeared from the cache.");

    manager->getGlobal()->setSetting("tool_coordinate", 7);
    select_category(0);
    select_file("new");
    expect(manager->getGlobal()->setting<int>("tool_coordinate") == 7,
           "Later selections must preserve manual edits to keys absent from the newly active template.");

    auto global = manager->getGlobal();
    global->setSetting("slicing_mode", 3);
    global->setSetting("dlp_pixel_width", 4096);
    bar.restoreSettingValue("slicing_mode");
    bar.restoreSettingValue("dlp_pixel_width");
    ORNL::SettingSpinBox* fixed_width = nullptr;
    QDoubleSpinBox* projection_width  = nullptr;
    QSpinBox* ordinary_integer        = nullptr;
    for (auto* pane : bar.getPanes()) {
        for (auto* tab : pane->getTabs()) {
            if (auto row = tab->getRow("dlp_pixel_width")) fixed_width = dynamic_cast<ORNL::SettingSpinBox*>(row.get());
            if (auto row = tab->getRow("dlp_projection_width"))
                projection_width = dynamic_cast<QDoubleSpinBox*>(row.get());
            if (auto row = tab->getRow("tool_coordinate")) ordinary_integer = dynamic_cast<QSpinBox*>(row.get());
        }
    }
    expect(fixed_width != nullptr && projection_width != nullptr && ordinary_integer != nullptr,
           "DLP width and ordinary numeric controls must exist.");
    if (fixed_width == nullptr || projection_width == nullptr || ordinary_integer == nullptr) return EXIT_FAILURE;
    expect(fixed_width->value() == 4096 && fixed_width->minimum() == 4096 && fixed_width->maximum() == 4096 &&
               fixed_width->isReadOnly() && fixed_width->buttonSymbols() == QAbstractSpinBox::NoButtons &&
               fixed_width->text().contains("fixed"),
           "DLP pixel width must visibly remain fixed at 4096 without editing arrows.");
    fixed_width->setValue(1024);
    fixed_width->stepUp();
    expect(fixed_width->value() == 4096 && global->setting<int>("dlp_pixel_width") == 4096,
           "Attempting to edit fixed pixel width must preserve its value.");
    expect(!projection_width->isReadOnly() && !ordinary_integer->isReadOnly(),
           "Fixing DLP pixel width must leave physical width and other numeric settings editable.");
    int fixed_changes = 0;
    QObject::connect(&bar, &ORNL::SettingBar::settingAboutToChange,
                     [&](const QString& key, const QList<QSharedPointer<ORNL::SettingsBase>>&) {
                         if (key == "dlp_pixel_width") ++fixed_changes;
                     });
    global->setSetting("dlp_pixel_width", 1024);
    bar.restoreSettingValue("dlp_pixel_width");
    expect(fixed_width->value() == 1024 && fixed_width->text().contains("requires 4096") &&
               fixed_width->toolTip().contains("unsupported") && fixed_width->isReadOnly() &&
               global->setting<int>("dlp_pixel_width") == 1024 && fixed_changes == 0,
           "Loading an unsupported saved width must expose it without silently rewriting the project.");
    select_category(3);
    bar.findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly)->setText("Projection Width in Pixels");
    app.processEvents();
    QToolButton* reset_width = nullptr;
    for (auto* button : bar.findChildren<QToolButton*>())
        if (button->toolTip() == "Use required value (4096)") reset_width = button;
    expect(reset_width && reset_width->isVisible() && reset_width->isEnabled(),
           "An unsupported saved fixed width must offer an explicit recovery button.");
    if (!reset_width) return EXIT_FAILURE;
    reset_width->click();
    expect(global->setting<int>("dlp_pixel_width") == 4096 && fixed_changes == 1 && reset_width->isHidden(),
           "Clicking the recovery button must restore 4096 through normal settings-change notifications.");
    fixed_width->setEnabled(false);
    fixed_width->setEnabled(true);
    expect(fixed_width->value() == 4096 && fixed_width->minimum() == 4096 && fixed_width->maximum() == 4096 &&
               fixed_width->isReadOnly() && fixed_width->text().contains("fixed"),
           "Reloading supported settings and toggling enablement must preserve fixed width behavior.");

    QCheckBox* stitching           = nullptr;
    QDoubleSpinBox* stitch_overlap = nullptr;
    QSharedPointer<ORNL::SettingRowBase> overlap_row;
    for (auto* pane : bar.getPanes()) {
        for (auto* tab : pane->getTabs()) {
            if (auto row = tab->getRow("dlp_interlaced_stitching")) stitching = dynamic_cast<QCheckBox*>(row.get());
            if (auto row = tab->getRow("dlp_stitch_overlap")) {
                overlap_row    = row;
                stitch_overlap = dynamic_cast<QDoubleSpinBox*>(row.get());
            }
        }
    }
    auto* search = bar.findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
    expect(stitching && stitch_overlap && overlap_row && search,
           "DLP stitching must expose a checkbox and physical overlap control.");
    if (!stitching || !stitch_overlap || !overlap_row || !search) return EXIT_FAILURE;
    search->setText("Stitch");
    app.processEvents();
    expect(!stitching->isChecked() && !global->setting<bool>("dlp_interlaced_stitching") && !overlap_row->isShown(),
           "Interlaced stitching must default off and hide its overlap field.");
    const double original_overlap = global->setting<double>("dlp_stitch_overlap");
    stitching->click();
    app.processEvents();
    expect(global->setting<bool>("dlp_interlaced_stitching") && overlap_row->isShown() && stitch_overlap->isVisible() &&
               stitch_overlap->isEnabled() && !stitch_overlap->isReadOnly(),
           "Checking stitching must enable its editable physical overlap field.");
    stitching->click();
    app.processEvents();
    expect(!global->setting<bool>("dlp_interlaced_stitching") && !overlap_row->isShown() &&
               global->setting<double>("dlp_stitch_overlap") == original_overlap,
           "Disabling stitching must hide its overlap field without changing the chosen width.");

    QApplication::setStyle(QStyleFactory::create("fusion"));
    expect(ORNL::Theme::initializeFonts(), "Could not initialize application fonts for DLP settings.");
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode}) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        QFile style(preferences->getTheme().getFolderPath() + "style.qss");
        if (!ORNL::Testing::expect(style.open(QIODevice::ReadOnly), "Could not read the root stylesheet."))
            return EXIT_FAILURE;
        const QString stylesheet = style.readAll();
        const QString prefix     = theme == ORNL::ThemeName::kLightMode ? "light-" : "dark-";
        bar.setStyleSheet(stylesheet);
        bar.resize(700, 600);
        bar.show();
        select_category(3);
        search->setText("Stitch");
        for (bool enabled : {false, true}) {
            if (stitching->isChecked() != enabled) stitching->click();
            app.processEvents();
            expect(stitching->isVisible() && stitching->isChecked() == enabled &&
                       global->setting<bool>("dlp_interlaced_stitching") == enabled &&
                       overlap_row->isShown() == enabled && QFontInfo(stitching->font()).family() == "Noto Sans",
                   "Stitching controls must retain their state, visibility, and application font in both themes.");
            if (argc > 1)
                expect(bar.grab().save(
                           QDir(QString::fromUtf8(argv[1]))
                               .filePath(prefix + (enabled ? "stitching-enabled.png" : "stitching-disabled.png"))),
                       "Could not save the DLP stitching settings screenshot.");
        }
        stitching->click();
        app.processEvents();
        for (bool fixed : {false, true}) {
            QWidget root;
            root.setStyleSheet(stylesheet);
            auto* layout  = new QVBoxLayout(&root);
            auto settings = QSharedPointer<ORNL::SettingsBase>::create();
            auto* tab     = new ORNL::SettingTab(&root, "DLP Slicer", QIcon(), 0, false, settings);
            auto metadata = manager->getMaster()->json().at("dlp_pixel_width");
            if (!fixed) metadata.erase("fixed");
            tab->addRow("dlp_pixel_width", metadata);
            tab->addRow("dlp_projection_width", manager->getMaster()->json().at("dlp_projection_width"));
            tab->expandTab();
            layout->addWidget(tab);
            root.resize(590, 150);
            root.show();
            app.processEvents();
            auto* width = dynamic_cast<QSpinBox*>(tab->getRow("dlp_pixel_width").get());
            expect(width != nullptr && width->isVisible() && width->isReadOnly() == fixed &&
                       QFontInfo(width->font()).family() == "Noto Sans",
                   "DLP pixel width must render with application fonts and the expected editability in both themes.");
            if (argc > 1) {
                const QString name = prefix + (fixed ? "fixed-width.png" : "fixed-width-before.png");
                expect(root.grab().save(QDir(QString::fromUtf8(argv[1])).filePath(name)),
                       "Could not save DLP fixed-width screenshot.");
            }
            if (fixed) {
                settings->setSetting("dlp_pixel_width", 1024);
                tab->getRow("dlp_pixel_width")->reloadValue();
                app.processEvents();
                QToolButton* reset = nullptr;
                for (auto* button : root.findChildren<QToolButton*>())
                    if (button->toolTip() == "Use required value (4096)") reset = button;
                expect(reset && reset->isVisible() && reset->isEnabled() &&
                           settings->setting<int>("dlp_pixel_width") == 1024 && width->value() == 1024,
                       "Unsupported fixed widths must remain visible and unchanged until explicitly restored in both "
                       "themes.");
                if (!reset) return EXIT_FAILURE;
                if (argc > 1)
                    expect(root.grab().save(
                               QDir(QString::fromUtf8(argv[1])).filePath(prefix + "fixed-width-unsupported.png")),
                           "Could not save unsupported DLP width screenshot.");
                reset->click();
                app.processEvents();
                expect(settings->setting<int>("dlp_pixel_width") == 4096 && width->value() == 4096 &&
                           width->isReadOnly() && reset->isHidden(),
                       "Explicit recovery must restore the required fixed width in both themes.");
                if (argc > 1)
                    expect(root.grab().save(
                               QDir(QString::fromUtf8(argv[1])).filePath(prefix + "fixed-width-recovered.png")),
                           "Could not save recovered DLP width screenshot.");
            }
        }
    }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
