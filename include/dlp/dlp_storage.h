#pragma once

#include <QString>

namespace ORNL {
class DlpStorage {
   public:
    //! Resolves the configured temporary root without creating directories.
    static bool temporaryRoot(QString& path, QString* error = nullptr);

    //! Checks user-available space on the path's volume, using its nearest existing parent if necessary.
    static bool checkSpace(const QString& path, double required_bytes, QString* error = nullptr);
};
}  // namespace ORNL
