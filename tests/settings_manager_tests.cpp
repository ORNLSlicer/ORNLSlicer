#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <cstdlib>

#include <nlohmann/json.hpp>

#include "managers/settings/settings_manager.h"
#include "test_utils.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    Q_INIT_RESOURCE(configs);

    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create temporary directory.")) return EXIT_FAILURE;

    const auto dismiss_upgrade_prompt = [] {
        // If schema validation regresses, dismiss the legacy-upgrade prompt so the test fails instead of hanging.
        QTimer::singleShot(0, [] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* message_box = qobject_cast<QMessageBox*>(widget)) {
                    message_box->done(QMessageBox::Yes);
                    return;
                }
            }
        });
    };

    const fifojson project_local = fifojson::array(
        {fifojson::object({{"name", "part.stl"}, {"settings", fifojson::array()}, {"ranges", nullptr}})});
    const QByteArray original_contents = QByteArray::fromStdString(project_local.dump(4));
    const QString path                 = temp_dir.filePath("local.s2c");

    QFile project_local_file(path);
    if (!ORNL::Testing::expect(project_local_file.open(QIODevice::WriteOnly | QIODevice::Truncate),
                               "Could not create project-local settings fixture."))
        return EXIT_FAILURE;
    project_local_file.write(original_contents);
    project_local_file.close();

    dismiss_upgrade_prompt();

    bool loaded = false;
    bool threw  = false;
    try {
        loaded = ORNL::SettingsManager::getInstance()->loadGlobalJson(path);
    } catch (const nlohmann::json::exception&) { threw = true; }

    bool passed = true;
    passed &= ORNL::Testing::expect(!threw, "Project-local settings JSON should not throw during template loading.");
    passed &= ORNL::Testing::expect(!loaded, "Project-local settings JSON should not be loaded as a template.");

    if (!ORNL::Testing::expect(project_local_file.open(QIODevice::ReadOnly),
                               "Could not reopen project-local settings fixture."))
        return EXIT_FAILURE;
    passed &= ORNL::Testing::expect(project_local_file.readAll() == original_contents,
                                    "Rejected project-local settings JSON should not be rewritten.");
    project_local_file.close();

    const fifojson project_session    = fifojson::object({{"parts", fifojson::object()}});
    const QByteArray session_contents = QByteArray::fromStdString(project_session.dump(4));
    const QString session_path        = temp_dir.filePath("session.s2c");

    QFile project_session_file(session_path);
    if (!ORNL::Testing::expect(project_session_file.open(QIODevice::WriteOnly | QIODevice::Truncate),
                               "Could not create project-session fixture."))
        return EXIT_FAILURE;
    project_session_file.write(session_contents);
    project_session_file.close();

    dismiss_upgrade_prompt();

    bool session_loaded = false;
    bool session_threw  = false;
    try {
        session_loaded = ORNL::SettingsManager::getInstance()->loadGlobalJson(session_path);
    } catch (const nlohmann::json::exception&) { session_threw = true; }

    passed &= ORNL::Testing::expect(!session_threw, "Project-session JSON should not throw during template loading.");
    passed &= ORNL::Testing::expect(!session_loaded, "Project-session JSON should not be loaded as a template.");

    if (!ORNL::Testing::expect(project_session_file.open(QIODevice::ReadOnly),
                               "Could not reopen project-session fixture."))
        return EXIT_FAILURE;
    passed &= ORNL::Testing::expect(project_session_file.readAll() == session_contents,
                                    "Rejected project-session JSON should not be rewritten.");

    const auto manager = ORNL::SettingsManager::getInstance();
    QFile versions(":/configs/versions.conf");
    if (!ORNL::Testing::expect(versions.open(QIODevice::ReadOnly), "Could not read settings version."))
        return EXIT_FAILURE;
    const auto version       = fifojson::parse(versions.readAll().toStdString()).at("master_version");
    const auto write_fixture = [&](const QString& name, const QByteArray& contents) {
        QFile file(temp_dir.filePath(name + ".s2c"));
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
    };
    const fifojson full_settings = {{"tool_number", 1},
                                    {"tool_coordinate", 2},
                                    {"printing_material", 1},
                                    {"perimeter_count", 1},
                                    {"tormach_mode", 1}};
    fifojson document            = {{"header", {{"version", version}}}, {"settings", fifojson::array({full_settings})}};
    const QByteArray full_contents = QByteArray::fromStdString(document.dump());
    const QString reload_path      = temp_dir.filePath("reload.s2c");
    if (!ORNL::Testing::expect(write_fixture("reload", full_contents) && manager->loadGlobalJson(reload_path) &&
                                   write_fixture("unrelated", full_contents) &&
                                   manager->loadGlobalJson(temp_dir.filePath("unrelated.s2c")),
                               "Could not load reload fixtures."))
        return EXIT_FAILURE;

    const QStringList categories {"Printer", "Material", "Profile", "Experimental"};
    QMap<QString, fifojson> original_settings;
    for (const QString& category : categories)
        original_settings[category] = manager->getAllGlobals().value(category).value("reload")->json();
    manager->getGlobal()->setSetting("tool_number", 9);
    const fifojson active_settings = manager->getGlobal()->json();

    for (const QByteArray& invalid_contents : {QByteArray("{"), session_contents}) {
        if (!ORNL::Testing::expect(write_fixture("reload", invalid_contents),
                                   "Could not write invalid reload fixture."))
            return EXIT_FAILURE;
        passed &= ORNL::Testing::expect(!manager->loadGlobalJson(reload_path), "Invalid reloads must fail.");
        for (const QString& category : categories) {
            const auto cached = manager->getAllGlobals().value(category).value("reload");
            passed &= ORNL::Testing::expect(!cached.isNull() && cached->json() == original_settings.value(category),
                                            "Invalid reloads must preserve cached settings in every category.");
        }
    }
    if (!ORNL::Testing::expect(QFile::remove(reload_path), "Could not remove reload fixture.")) return EXIT_FAILURE;
    passed &= ORNL::Testing::expect(!manager->loadGlobalJson(reload_path), "Missing-file reloads must fail.");
    for (const QString& category : categories) {
        const auto cached = manager->getAllGlobals().value(category).value("reload");
        passed &= ORNL::Testing::expect(!cached.isNull() && cached->json() == original_settings.value(category),
                                        "Missing-file reloads must preserve cached settings in every category.");
    }

    document["settings"] = fifojson::array({{{"tool_number", 3}, {"perimeter_count", 4}}});
    if (!ORNL::Testing::expect(
            write_fixture("reload", QByteArray::fromStdString(document.dump())) && manager->loadGlobalJson(reload_path),
            "Could not reload a partial settings file."))
        return EXIT_FAILURE;
    const auto reloaded = manager->getAllGlobals();
    const auto printer  = reloaded.value("Printer").value("reload");
    const auto profile  = reloaded.value("Profile").value("reload");
    passed &= ORNL::Testing::expect(!printer.isNull() && printer->setting<int>("tool_number") == 3 &&
                                        !printer->contains("tool_coordinate") && !profile.isNull() &&
                                        profile->setting<int>("perimeter_count") == 4,
                                    "Reloading must replace retained categories, including removing omitted keys.");
    passed &= ORNL::Testing::expect(
        !reloaded.value("Material").contains("reload") && !reloaded.value("Experimental").contains("reload"),
        "Reloading must remove categories absent from the current file.");
    for (const QString& category : categories) {
        const auto unrelated = reloaded.value(category).value("unrelated");
        passed &= ORNL::Testing::expect(!unrelated.isNull() && unrelated->json() == original_settings.value(category),
                                        "Reloading must preserve settings from unrelated files.");
    }
    passed &= ORNL::Testing::expect(manager->getGlobal()->json() == active_settings,
                                    "Loading files must preserve active settings until a selection is applied.");

    const auto load_settings = [&](const QString& name, const fifojson& settings) {
        document["settings"] = fifojson::array({settings});
        return write_fixture(name, QByteArray::fromStdString(document.dump())) &&
               manager->loadGlobalJson(temp_dir.filePath(name + ".s2c"));
    };
    const int default_tool_number     = manager->getMaster()->json().at("tool_number").at("default").get<int>();
    const int default_tool_coordinate = manager->getMaster()->json().at("tool_coordinate").at("default").get<int>();
    const QString saved_path          = temp_dir.filePath("saved.s2c");

    if (!ORNL::Testing::expect(load_settings("reapplied", full_settings),
                               "Could not load the template before reapplying it."))
        return EXIT_FAILURE;
    manager->constructActiveGlobal("Printer", "reapplied");
    if (!ORNL::Testing::expect(load_settings("reapplied", {{"tool_number", 3}}),
                               "Could not reload the template before reapplying it."))
        return EXIT_FAILURE;
    manager->constructActiveGlobal("Printer", "reapplied");
    manager->removeCurrentSettings("Printer", "reapplied");
    passed &= ORNL::Testing::expect(
        manager->getGlobal()->setting<int>("tool_coordinate") == default_tool_coordinate,
        "Reapplying the same file after reloading must retain its previously applied reset keys.");

    manager->constructActiveGlobal("Printer", "unrelated");
    manager->getGlobal()->setSetting("tool_coordinate", 7);
    manager->saveTemplate({"tool_number"}, saved_path, "");
    if (!ORNL::Testing::expect(manager->loadGlobalJson(saved_path), "Could not load the saved template."))
        return EXIT_FAILURE;
    manager->removeCurrentSettings("Printer", "saved");
    passed &= ORNL::Testing::expect(manager->getGlobal()->setting<int>("tool_number") == default_tool_number &&
                                        manager->getGlobal()->setting<int>("tool_coordinate") == 7,
                                    "Selecting a saved template must not reset keys from a different active file.");

    manager->constructActiveGlobal("Printer", "saved");
    manager->getGlobal()->setSetting("tool_coordinate", 6);
    manager->saveTemplate({"tool_number", "tool_coordinate"}, saved_path, "");
    if (!ORNL::Testing::expect(manager->loadGlobalJson(saved_path), "Could not reload the overwritten template."))
        return EXIT_FAILURE;
    manager->removeCurrentSettings("Printer", "saved");
    passed &= ORNL::Testing::expect(
        manager->getGlobal()->setting<int>("tool_coordinate") == default_tool_coordinate,
        "Leaving an overwritten template must reset manually added keys saved under the same name.");

    manager->constructActiveGlobal("Printer", "unrelated");
    if (!ORNL::Testing::expect(load_settings("unrelated", {{"tool_number", 3}}),
                               "Could not replace the active template before session loading."))
        return EXIT_FAILURE;
    document["settings"] = fifojson::array({{{"tool_number", 4}, {"tool_coordinate", 7}}});
    if (!ORNL::Testing::expect(manager->loadGlobalJson(document), "Could not load replacement session settings."))
        return EXIT_FAILURE;
    const QString unrelated_path = temp_dir.filePath("unrelated.s2c");
    manager->saveTemplate({"tool_number"}, unrelated_path, "");
    if (!ORNL::Testing::expect(manager->loadGlobalJson(unrelated_path), "Could not save the session's template."))
        return EXIT_FAILURE;
    manager->removeCurrentSettings("Printer", "unrelated");
    passed &=
        ORNL::Testing::expect(manager->getGlobal()->setting<int>("tool_coordinate") == 7,
                              "Replacing session settings must discard the previously applied template's reset keys.");

    if (!ORNL::Testing::expect(load_settings("unrelated", full_settings),
                               "Could not restore the template before clearing global settings."))
        return EXIT_FAILURE;
    manager->constructActiveGlobal("Printer", "unrelated");
    if (!ORNL::Testing::expect(load_settings("unrelated", {{"tool_number", 3}}),
                               "Could not replace the active template before clearing global settings."))
        return EXIT_FAILURE;
    manager->clearGlobal();
    manager->getGlobal()->setSetting("tool_coordinate", 7);
    manager->removeCurrentSettings("Printer", "unrelated");
    passed &=
        ORNL::Testing::expect(manager->getGlobal()->setting<int>("tool_coordinate") == 7,
                              "Clearing global settings must discard the previously applied template's reset keys.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
