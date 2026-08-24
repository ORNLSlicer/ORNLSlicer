#include <QCoreApplication>
#include <QFile>
#include <QStringBuilder>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>
#include <optional>

#include "gcode/gcode_settings_importer.h"
#include "utilities/constants.h"

namespace {
bool writeFile(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;

    file.write(text.toUtf8());
    return true;
}

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QTemporaryDir temp_dir;
    if (!expect(temp_dir.isValid(), "Could not create temporary directory.")) return EXIT_FAILURE;

    const QString semicolon_path = temp_dir.path() + "/semicolon.gcode";
    const QString semicolon_gcode =
        "G1 X0 Y0\n"
        ";Settings Footer\n"
        ";layer_height 200\n"
        ";default_width 400\n";
    if (!expect(writeFile(semicolon_path, semicolon_gcode), "Could not write semicolon fixture.")) return EXIT_FAILURE;

    const ORNL::GcodeSettingsImporter::ImportResult semicolon_result =
        ORNL::GcodeSettingsImporter::importFile(semicolon_path, true);

    if (!expect(semicolon_result.errors.isEmpty(), qPrintable(semicolon_result.errors.join("\n")))) return EXIT_FAILURE;

    const auto semicolon_settings =
        semicolon_result.settings_file[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!expect(semicolon_settings.at(ORNL::PS::Layer::kLayerHeight.toStdString()).get<double>() == 200.0,
                "Did not import layer height from semicolon footer."))
        return EXIT_FAILURE;
    if (!expect(semicolon_settings.at(ORNL::PS::Layer::kBeadWidth.toStdString()).get<double>() == 400.0,
                "Did not import default width from semicolon footer."))
        return EXIT_FAILURE;

    const QString paren_path = temp_dir.path() + "/paren.nc";
    const QString paren_gcode =
        "(Settings Footer)\n"
        "(layer_height 300)\n"
        "(default_width 500)\n";
    if (!expect(writeFile(paren_path, paren_gcode), "Could not write parenthesized fixture.")) return EXIT_FAILURE;

    const ORNL::GcodeSettingsImporter::ImportResult paren_result =
        ORNL::GcodeSettingsImporter::importFile(paren_path, true);

    if (!expect(paren_result.errors.isEmpty(), qPrintable(paren_result.errors.join("\n")))) return EXIT_FAILURE;

    const auto paren_settings = paren_result.settings_file[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!expect(paren_settings.at(ORNL::PS::Layer::kLayerHeight.toStdString()).get<double>() == 300.0,
                "Did not import layer height from parenthesized footer."))
        return EXIT_FAILURE;
    if (!expect(paren_settings.at(ORNL::PS::Layer::kBeadWidth.toStdString()).get<double>() == 500.0,
                "Did not import default width from parenthesized footer."))
        return EXIT_FAILURE;

    const QString legacy_path = temp_dir.path() + "/legacy.gcode";
    const QString legacy_gcode =
        ";Settings Footer\n"
        ";layer_height 200\n"
        ";default_width 400\n"
        ";slicer_type 0\n"
        ";slicing_vector_x 0.25\n"
        ";slicing_vector_y 0.5\n"
        ";slicing_vector_z 0.75\n"
        ";image_resolution_x 0.8\n"
        ";image_resolution_y 0.9\n";
    if (!expect(writeFile(legacy_path, legacy_gcode), "Could not write legacy key fixture.")) return EXIT_FAILURE;

    const ORNL::GcodeSettingsImporter::ImportResult legacy_result =
        ORNL::GcodeSettingsImporter::importFile(legacy_path, true);

    if (!expect(legacy_result.errors.isEmpty(), qPrintable(legacy_result.errors.join("\n")))) return EXIT_FAILURE;

    const auto legacy_settings = legacy_result.settings_file[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    using Slicing              = ORNL::Constants::ProfileSettings::Slicing;
    if (!expect(legacy_settings.at(Slicing::kSlicingMode.toStdString()).get<int>() == 0,
                "Did not migrate legacy slicer_type footer key."))
        return EXIT_FAILURE;
    if (!expect(legacy_settings.at(Slicing::kSlicePlaneNormalX.toStdString()).get<double>() == 0.25,
                "Did not migrate legacy slicing_vector_x footer key."))
        return EXIT_FAILURE;
    if (!expect(legacy_settings.at(Slicing::kSlicePlaneNormalY.toStdString()).get<double>() == 0.5,
                "Did not migrate legacy slicing_vector_y footer key."))
        return EXIT_FAILURE;
    if (!expect(legacy_settings.at(Slicing::kSlicePlaneNormalZ.toStdString()).get<double>() == 0.75,
                "Did not migrate legacy slicing_vector_z footer key."))
        return EXIT_FAILURE;
    if (!expect(legacy_settings.at(Slicing::kImagePixelSizeX.toStdString()).get<double>() == 0.8,
                "Did not migrate legacy image_resolution_x footer key."))
        return EXIT_FAILURE;
    if (!expect(legacy_settings.at(Slicing::kImagePixelSizeY.toStdString()).get<double>() == 0.9,
                "Did not migrate legacy image_resolution_y footer key."))
        return EXIT_FAILURE;
    if (!expect(legacy_result.unknown_keys.isEmpty(), "Migrated legacy footer keys were still reported as unknown."))
        return EXIT_FAILURE;

    const QString cancel_path = temp_dir.path() + "/cancel.gcode";
    const QString cancel_gcode =
        ";Settings Footer\n"
        ";layer_height 200\n";
    if (!expect(writeFile(cancel_path, cancel_gcode), "Could not write cancel fixture.")) return EXIT_FAILURE;

    int prompt_count = 0;
    const ORNL::GcodeSettingsImporter::ImportResult cancel_result =
        ORNL::GcodeSettingsImporter::importFile(cancel_path, false, [&prompt_count](const QString&, const fifojson&) {
            ++prompt_count;
            return std::optional<fifojson>();
        });

    if (!expect(!cancel_result.errors.isEmpty(), "Canceling a missing setting prompt did not fail the import."))
        return EXIT_FAILURE;
    if (!expect(prompt_count == 1, "Canceling a missing setting prompt did not stop further prompts."))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
