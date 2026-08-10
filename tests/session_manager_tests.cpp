#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <zip/zip.h>

#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/session_loader.h"
#include "utilities/constants.h"

namespace {
bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

bool writeZipEntry(zip_t* zip, const std::string& name, const std::string& text) {
    if (zip_entry_open(zip, name.c_str()) < 0)
        return false;

    int result = zip_entry_write(zip, text.c_str(), text.size());
    zip_entry_close(zip);
    return result >= 0;
}

bool writeProjectWithOldGlobal(const QString& path) {
    QByteArray path_bytes = path.toUtf8();
    zip_t* zip = zip_open(path_bytes.constData(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (zip == nullptr)
        return false;

    fifojson global = fifojson::object();
    global[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 2.0;
    global[ORNL::Constants::SettingFileStrings::kSettings] = fifojson::array(
        {fifojson::object({{"slicer_type", 0}, {"slicing_vector_x", 0.25}})});

    fifojson session = fifojson::object();
    session[ORNL::Constants::Settings::Session::kParts] = fifojson::object();

    bool success =
        writeZipEntry(zip, ORNL::Constants::Settings::Session::Files::kGlobal, global.dump(4)) &&
        writeZipEntry(zip, ORNL::Constants::Settings::Session::Files::kSession, session.dump(4)) &&
        writeZipEntry(zip, ORNL::Constants::Settings::Session::Files::kLocal, fifojson::array().dump(4));

    zip_close(zip);
    return success;
}

std::optional<fifojson> readGlobalFromProject(const QString& path) {
    QByteArray path_bytes = path.toUtf8();
    zip_t* zip = zip_open(path_bytes.constData(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    if (zip == nullptr)
        return std::nullopt;

    void* buffer = nullptr;
    size_t buffer_size = 0;
    if (zip_entry_open(zip, ORNL::Constants::Settings::Session::Files::kGlobal.c_str()) < 0 ||
        zip_entry_read(zip, &buffer, &buffer_size) < 0) {
        zip_close(zip);
        return std::nullopt;
    }

    zip_entry_close(zip);
    std::string text(static_cast<char*>(buffer), buffer_size);
    free(buffer);
    zip_close(zip);

    return fifojson::parse(text);
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QTemporaryDir temp_dir;
    if (!expect(temp_dir.isValid(), "Could not create temporary directory."))
        return EXIT_FAILURE;

    QString project_path = temp_dir.path() + "/old-settings-project.s2p";
    if (!expect(writeProjectWithOldGlobal(project_path), "Could not write project fixture."))
        return EXIT_FAILURE;

    ORNL::SessionLoader* loader = ORNL::CSM->loadSession(false, project_path, false);
    if (!expect(loader != nullptr, "CLI-style session load rejected the old project before auto-updating settings."))
        return EXIT_FAILURE;

    QEventLoop loop;
    bool finished = false;
    bool succeeded = false;
    QObject::connect(loader, &ORNL::SessionLoader::loadSucceeded, &loop, [&succeeded]() { succeeded = true; });
    QObject::connect(loader, &ORNL::SessionLoader::finished, &loop, [&finished, &loop]() {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, [&loop]() { loop.quit(); });
    loop.exec();

    if (!expect(finished, "Timed out waiting for CLI-style session load to finish."))
        return EXIT_FAILURE;
    if (!expect(succeeded, "CLI-style session load did not complete successfully."))
        return EXIT_FAILURE;

    std::optional<fifojson> archived_global = readGlobalFromProject(project_path);
    if (!expect(archived_global.has_value(), "Could not read archived global settings from project."))
        return EXIT_FAILURE;

    double archived_version = (*archived_global)[ORNL::Constants::SettingFileStrings::kHeader]
                                               [ORNL::Constants::SettingFileStrings::kVersion];
    if (!expect(archived_version == 2.0, "CLI-style session load unexpectedly modified the project archive."))
        return EXIT_FAILURE;

    double loaded_slice_normal_x = ORNL::GSM->getGlobal()->setting<double>(
        ORNL::Constants::ProfileSettings::Slicing::kSlicePlaneNormalX);
    if (!expect(loaded_slice_normal_x == 0.25,
                "CLI-style session load did not use migrated global settings in memory."))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
