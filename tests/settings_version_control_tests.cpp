#include <QCoreApplication>
#include <cstdlib>

#include "managers/settings/settings_version_control.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
constexpr float kTolerance = 1.0e-6f;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    fifojson v10_settings;
    v10_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 10.0;
    v10_settings[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"helical_path_start_angle", 1.74532925}})});
    double v10_version = 10.0;
    ORNL::SettingsVersionControl::rollSettingsForward(v10_version, v10_settings);

    const fifojson v10_group      = v10_settings[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    const std::string helical_key = ORNL::PS::Helical::kHelicalToolStartAngleOffset.toStdString();
    if (!ORNL::Testing::expect(
            v10_group.contains(helical_key) && !v10_group.contains("helical_path_start_angle") &&
                ORNL::Testing::near(v10_group.at(helical_key).get<double>(), (10.0 * ORNL::degree)(), kTolerance),
            "Did not roll v10 helical_path_start_angle setting forward."))
        return EXIT_FAILURE;

    fifojson v11_settings;
    v11_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 11.0;
    v11_settings[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"helical_start_angle_offset", -0.20943951}})});
    double v11_version = 11.0;
    ORNL::SettingsVersionControl::rollSettingsForward(v11_version, v11_settings);

    const fifojson v11_group = v11_settings[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!ORNL::Testing::expect(
            v11_group.contains(helical_key) && !v11_group.contains("helical_start_angle_offset") &&
                ORNL::Testing::near(v11_group.at(helical_key).get<double>(), (-12.0 * ORNL::degree)(), kTolerance),
            "Did not roll v11 helical_start_angle_offset setting forward."))
        return EXIT_FAILURE;

    const double expected_version = 12.0;
    if (!ORNL::Testing::expect(
            v10_version == expected_version && v11_version == expected_version,
            "Did not roll setting files forward."))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
