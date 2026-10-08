#include <QCoreApplication>
#include <cstdlib>
#include <stdexcept>

#include "managers/settings/settings_version_control.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
constexpr float kTolerance = 1.0e-3f;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const std::string inner_radius_key = ORNL::PS::Slicing::kCylinderInnerRadius.toStdString();
    const std::string layer_height_key = ORNL::PS::Layer::kLayerHeight.toStdString();
    const std::string arc_length_key   = ORNL::PS::Helical::kHelicalToolStartArcLengthOffset.toStdString();
    const double inner_radius          = (100.0 * ORNL::mm)();
    const double layer_height          = (2.0 * ORNL::mm)();
    const double first_radius          = inner_radius + (layer_height / 2.0);

    fifojson v10_settings;
    v10_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 10.0;
    v10_settings[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"helical_path_start_angle", 1.74532925},
                                           {inner_radius_key, inner_radius},
                                           {layer_height_key, layer_height}})});
    double v10_version = 10.0;
    ORNL::SettingsVersionControl::rollSettingsForward(v10_version, v10_settings);

    const fifojson v10_group = v10_settings[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!ORNL::Testing::expect(
            v10_group.contains(arc_length_key) && !v10_group.contains("helical_path_start_angle") &&
                !v10_group.contains("helical_tool_start_angle_offset") &&
                ORNL::Testing::near(v10_group.at(arc_length_key).get<double>(),
                                    first_radius * (1.74532925 - (90.0 * ORNL::degree)()), kTolerance),
            "Did not roll v10 helical_path_start_angle to a first-radius arc length."))
        return EXIT_FAILURE;

    fifojson v11_settings;
    v11_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 11.0;
    v11_settings[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"helical_start_angle_offset", -0.20943951},
                                           {inner_radius_key, inner_radius},
                                           {layer_height_key, layer_height}})});
    double v11_version = 11.0;
    ORNL::SettingsVersionControl::rollSettingsForward(v11_version, v11_settings);

    const fifojson v11_group = v11_settings[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!ORNL::Testing::expect(v11_group.contains(arc_length_key) &&
                                   !v11_group.contains("helical_start_angle_offset") &&
                                   ORNL::Testing::near(v11_group.at(arc_length_key).get<double>(),
                                                       first_radius * (-12.0 * ORNL::degree)(), kTolerance),
                               "Did not roll v11 helical_start_angle_offset to a first-radius arc length."))
        return EXIT_FAILURE;

    const std::string z_key = ORNL::PRS::Dimensions::kUseVariableForZ.toStdString();
    fifojson v12_settings;
    v12_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 12.0;
    v12_settings[ORNL::Constants::SettingFileStrings::kSettings] = fifojson::array({fifojson::object({{z_key, true}})});
    double v12_version                                           = 12.0;
    ORNL::SettingsVersionControl::rollSettingsForward(v12_version, v12_settings);

    const fifojson v12_group = v12_settings[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!ORNL::Testing::expect(v12_group.contains(z_key) && v12_group.at(z_key).is_number_integer() &&
                                   v12_group.at(z_key).get<int>() == static_cast<int>(ORNL::VariableZ::kVar200),
                               "Did not roll boolean variable_for_z forward to enumeration index kVar200."))
        return EXIT_FAILURE;

    fifojson v13_settings;
    v13_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 13.0;
    v13_settings[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"helical_tool_start_angle_offset", (-12.0 * ORNL::degree)()},
                                           {inner_radius_key, inner_radius},
                                           {layer_height_key, layer_height}})});
    double v13_version = 13.0;
    ORNL::SettingsVersionControl::rollSettingsForward(v13_version, v13_settings);
    const fifojson v13_group = v13_settings[ORNL::Constants::SettingFileStrings::kSettings].at(0);
    if (!ORNL::Testing::expect(v13_group.contains(arc_length_key) &&
                                   !v13_group.contains("helical_tool_start_angle_offset") &&
                                   ORNL::Testing::near(v13_group.at(arc_length_key).get<double>(),
                                                       first_radius * (-12.0 * ORNL::degree)(), kTolerance),
                               "Did not roll v13 helical tool angle offset to a first-radius arc length."))
        return EXIT_FAILURE;

    fifojson invalid_settings;
    invalid_settings[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] =
        13.0;
    invalid_settings[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"helical_tool_start_angle_offset", (5.0 * ORNL::degree)()}})});
    double invalid_version = 13.0;
    bool invalid_rejected  = false;
    try {
        ORNL::SettingsVersionControl::rollSettingsForward(invalid_version, invalid_settings);
    } catch (const std::invalid_argument&) { invalid_rejected = true; }
    if (!ORNL::Testing::expect(invalid_rejected && invalid_version == 13.0,
                               "Migrated a nonzero helical angle without reference geometry."))
        return EXIT_FAILURE;

    const double expected_version = 14.0;
    if (!ORNL::Testing::expect(v10_version == expected_version && v11_version == expected_version &&
                                   v12_version == expected_version && v13_version == expected_version,
                               "Did not roll setting files forward."))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
