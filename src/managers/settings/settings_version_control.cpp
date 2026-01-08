#include "managers/settings/settings_version_control.h"

#include "QDateTime"
#include "QRegularExpression"
#include "utilities/constants.h"

namespace ORNL {
void SettingsVersionControl::rollSettingsForward(double& version, fifojson& settings) {
    if (version < 1)
        pre_1_0To1_0(version, settings);
    if (version < 2) // all versions converted to Version 2.0
        pre_2_0To2_0(version, settings);
}

void SettingsVersionControl::formatSettings(double version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedBy] = "ORNL Slicer-2";
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedOn] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = version;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLock] = "false";

    new_format[Constants::SettingFileStrings::kSettings] = settings;
    settings = new_format;
}

void SettingsVersionControl::pre_1_0To1_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedBy] = "";
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedOn] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 1.0;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLock] = "false";

    new_format[Constants::SettingFileStrings::kSettings] = settings;

    std::list<std::string> keys;
    for (auto& el : settings.items())
        keys.push_back(el.key());

    for (std::string key : keys) {
        // get iterator to old key; TODO: error handling if key is not present
        fifojson::iterator it = new_format[Constants::SettingFileStrings::kSettings].find(key);
        // create null value for new key and swap value from old key
        std::swap(new_format[Constants::SettingFileStrings::kSettings][key + "_0"], it.value());
        // delete value at old key (cheap, because the value is null after swap)
        new_format[Constants::SettingFileStrings::kSettings].erase(it);
    }

    settings = new_format;
}

void SettingsVersionControl::pre_2_0To2_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 2.0;

    fifojson settings_array = fifojson::array({});
    fifojson current_index_settings;
    int index = 0; // Last index denotes suffix
    for (auto& el : new_format[Constants::SettingFileStrings::kSettings].items()) {
        QString key_root = QString::fromStdString(el.key());
        int last_index = key_root.lastIndexOf(QRegularExpression("_\\d+")); // index of suffix
        if (last_index >= 0) {
            key_root.chop(key_root.size() - last_index);                               // remove suffix
            int key_suffix = key_root.right(key_root.size() - last_index - 1).toInt(); // get suffix
            // If suffix matches index, add to json
            if (key_suffix == index)
                current_index_settings[key_root.toStdString()] = el.value();
            // otherwise increment index and add current json to json array
            else {
                index++;
                settings_array.push_back(current_index_settings);
                // Empty current json to be filled again with next index items
                for (auto it : current_index_settings.items()) {
                    current_index_settings.erase(current_index_settings.begin(), current_index_settings.end());
                }
                current_index_settings[key_root.toStdString()] = el.value();
            }
        }
        else {
            current_index_settings[key_root.toStdString()] = el.value();
        }
    }
    settings_array.push_back(current_index_settings);

    new_format[Constants::SettingFileStrings::kSettings] = settings_array;
    version = 2.0;
    settings = new_format;
}
} // namespace ORNL
