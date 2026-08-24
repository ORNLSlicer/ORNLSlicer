#pragma once

#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

#include "utilities/qt_json_conversion.h"

namespace ORNL {

class GcodeSettingsImporter {
   public:
    struct ImportResult {
        fifojson settings_file;
        QStringList errors;
        QStringList warnings;
        QStringList imported_keys;
        QStringList defaulted_keys;
        QStringList prompted_keys;
        QStringList missing_keys;
        QStringList unknown_keys;
    };

    using MissingValueCallback =
        std::function<std::optional<fifojson>(const QString& key, const fifojson& master_entry)>;

    static ImportResult importFile(const QString& gcode_path, bool use_defaults_for_missing,
                                   const MissingValueCallback& missing_value_callback = MissingValueCallback());

    static bool validateValue(const QString& key, const fifojson& master_entry, const fifojson& value,
                              fifojson& normalized_value, QString& error, bool enforce_ranges = true);

    static QString displayName(const fifojson& master_entry);
    static QStringList settingOptions(const fifojson& master_entry);
};

}  // namespace ORNL
