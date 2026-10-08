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

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
