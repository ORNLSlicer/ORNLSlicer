#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "utilities/qt_json_conversion.h"

namespace ORNL {

/*! \brief Compares the setting values stored in two ORNLSlicer settings files. */
class SettingsFileComparator {
   public:
    //! \brief A setting that occurs in both files with different values.
    struct ChangedSetting {
        QString key;
        int settings_index = 0;
        fifojson first_value;
        fifojson second_value;
    };

    //! \brief A setting that occurs in only one of the files.
    struct UniqueSetting {
        QString key;
        int settings_index = 0;
        fifojson value;
    };

    //! \brief Complete comparison result. Errors are empty after a successful comparison.
    struct Result {
        QVector<ChangedSetting> changed_settings;
        QVector<UniqueSetting> only_in_first;
        QVector<UniqueSetting> only_in_second;
        QStringList errors;
    };

    //! \brief Read and compare two settings files.
    static Result compareFiles(const QString& first_path, const QString& second_path);

    //! \brief Compare two parsed settings-file documents.
    static Result compare(const fifojson& first, const fifojson& second);
};

}  // namespace ORNL
