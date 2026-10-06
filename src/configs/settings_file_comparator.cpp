#include "configs/settings_file_comparator.h"

#include <QFile>
#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "utilities/constants.h"

namespace ORNL {
namespace {
constexpr double kAbsoluteFloatTolerance = 1.0e-9;
constexpr double kRelativeFloatTolerance = 1.0e-12;

bool readSettingsFile(const QString& path, const QString& description, fifojson& document, QString& error) {
    if (path.trimmed().isEmpty()) {
        error = description + " has not been selected.";
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QString("Could not open %1:\n%2\n%3").arg(description, path, file.errorString());
        return false;
    }

    try {
        document = fifojson::parse(file.readAll().toStdString());
    } catch (const fifojson::parse_error& exception) {
        error = QString("%1 does not contain valid JSON:\n%2").arg(description, QString::fromUtf8(exception.what()));
        return false;
    }

    return true;
}

bool validateSettings(const fifojson& document, const QString& description, QString& error) {
    if (!document.is_object()) {
        error = description + " must contain a JSON object.";
        return false;
    }

    const auto settings = document.find(Constants::SettingFileStrings::kSettings);
    if (settings == document.end() || !settings->is_array()) {
        error = description + " must contain a \"settings\" array.";
        return false;
    }

    for (std::size_t index = 0; index < settings->size(); ++index) {
        if (!settings->at(index).is_object()) {
            error = QString("%1 contains a non-object entry at settings index %2.").arg(description).arg(index);
            return false;
        }
    }

    return true;
}

bool numbersEqual(const fifojson& first, const fifojson& second) {
    // Integer settings and enums retain exact comparison semantics.
    if (!first.is_number_float() && !second.is_number_float()) return first == second;

    const double first_value  = first.get<double>();
    const double second_value = second.get<double>();
    if (!std::isfinite(first_value) || !std::isfinite(second_value)) return first == second;

    const double difference = std::abs(first_value - second_value);
    const double magnitude  = std::max(std::abs(first_value), std::abs(second_value));
    return difference <= std::max(kAbsoluteFloatTolerance, kRelativeFloatTolerance * magnitude);
}

bool valuesEqual(const fifojson& first, const fifojson& second) {
    if (first.is_number() && second.is_number()) return numbersEqual(first, second);

    if (first.is_object() && second.is_object()) {
        if (first.size() != second.size()) return false;

        for (const auto& item : first.items()) {
            const auto other = second.find(item.key());
            if (other == second.end() || !valuesEqual(item.value(), *other)) return false;
        }
        return true;
    }

    if (first.is_array() && second.is_array()) {
        if (first.size() != second.size()) return false;

        for (std::size_t index = 0; index < first.size(); ++index) {
            if (!valuesEqual(first.at(index), second.at(index))) return false;
        }
        return true;
    }

    return first == second;
}
}  // namespace

SettingsFileComparator::Result SettingsFileComparator::compareFiles(const QString& first_path,
                                                                    const QString& second_path) {
    fifojson first;
    fifojson second;
    QString error;

    if (!readSettingsFile(first_path, "The first settings file", first, error)) {
        Result result;
        result.errors.append(error);
        return result;
    }
    if (!readSettingsFile(second_path, "The second settings file", second, error)) {
        Result result;
        result.errors.append(error);
        return result;
    }

    return compare(first, second);
}

SettingsFileComparator::Result SettingsFileComparator::compare(const fifojson& first, const fifojson& second) {
    Result result;
    QString error;
    if (!validateSettings(first, "The first settings file", error)) result.errors.append(error);
    if (!validateSettings(second, "The second settings file", error)) result.errors.append(error);
    if (!result.errors.isEmpty()) return result;

    const fifojson& first_settings  = first.at(Constants::SettingFileStrings::kSettings);
    const fifojson& second_settings = second.at(Constants::SettingFileStrings::kSettings);
    const std::size_t set_count     = std::max(first_settings.size(), second_settings.size());

    for (std::size_t settings_index = 0; settings_index < set_count; ++settings_index) {
        const fifojson empty_settings = fifojson::object();
        const fifojson& first_set =
            settings_index < first_settings.size() ? first_settings.at(settings_index) : empty_settings;
        const fifojson& second_set =
            settings_index < second_settings.size() ? second_settings.at(settings_index) : empty_settings;

        std::set<std::string> keys;
        for (const auto& item : first_set.items()) keys.insert(item.key());
        for (const auto& item : second_set.items()) keys.insert(item.key());

        for (const std::string& key : keys) {
            const auto first_value  = first_set.find(key);
            const auto second_value = second_set.find(key);

            if (first_value == first_set.end()) {
                result.only_in_second.append(
                    {QString::fromStdString(key), static_cast<int>(settings_index), *second_value});
            }
            else if (second_value == second_set.end()) {
                result.only_in_first.append(
                    {QString::fromStdString(key), static_cast<int>(settings_index), *first_value});
            }
            else if (!valuesEqual(*first_value, *second_value)) {
                result.changed_settings.append(
                    {QString::fromStdString(key), static_cast<int>(settings_index), *first_value, *second_value});
            }
        }
    }

    return result;
}

}  // namespace ORNL
