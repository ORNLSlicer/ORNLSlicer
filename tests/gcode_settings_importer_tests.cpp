#include <cstdlib>
#include <iostream>

#include <QCoreApplication>
#include <QFile>
#include <QStringBuilder>
#include <QTemporaryDir>

#include "gcode/gcode_settings_importer.h"
#include "utilities/constants.h"

namespace {
bool writeFile(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    file.write(text.toUtf8());
    return true;
}

bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QTemporaryDir temp_dir;
    if (!expect(temp_dir.isValid(), "Could not create temporary directory."))
        return EXIT_FAILURE;

    const QString semicolon_path = temp_dir.path() + "/semicolon.gcode";
    const QString semicolon_gcode = "G1 X0 Y0\n"
                                    ";Settings Footer\n"
                                    ";layer_height 200\n"
                                    ";default_width 400\n";
    if (!expect(writeFile(semicolon_path, semicolon_gcode), "Could not write semicolon fixture."))
        return EXIT_FAILURE;

    const ORNL::GcodeSettingsImporter::ImportResult semicolon_result =
        ORNL::GcodeSettingsImporter::importFile(semicolon_path, true);

    if (!expect(semicolon_result.errors.isEmpty(), qPrintable(semicolon_result.errors.join("\n"))))
        return EXIT_FAILURE;

    const auto semicolon_settings =
        semicolon_result.settings_file[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!expect(semicolon_settings.at(ORNL::PS::Layer::kLayerHeight.toStdString()).get<double>() == 200.0,
                "Did not import layer height from semicolon footer."))
        return EXIT_FAILURE;
    if (!expect(semicolon_settings.at(ORNL::PS::Layer::kBeadWidth.toStdString()).get<double>() == 400.0,
                "Did not import default width from semicolon footer."))
        return EXIT_FAILURE;

    const QString paren_path = temp_dir.path() + "/paren.nc";
    const QString paren_gcode = "(Settings Footer)\n"
                                "(layer_height 300)\n"
                                "(default_width 500)\n";
    if (!expect(writeFile(paren_path, paren_gcode), "Could not write parenthesized fixture."))
        return EXIT_FAILURE;

    const ORNL::GcodeSettingsImporter::ImportResult paren_result =
        ORNL::GcodeSettingsImporter::importFile(paren_path, true);

    if (!expect(paren_result.errors.isEmpty(), qPrintable(paren_result.errors.join("\n"))))
        return EXIT_FAILURE;

    const auto paren_settings = paren_result.settings_file[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!expect(paren_settings.at(ORNL::PS::Layer::kLayerHeight.toStdString()).get<double>() == 300.0,
                "Did not import layer height from parenthesized footer."))
        return EXIT_FAILURE;
    if (!expect(paren_settings.at(ORNL::PS::Layer::kBeadWidth.toStdString()).get<double>() == 500.0,
                "Did not import default width from parenthesized footer."))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
