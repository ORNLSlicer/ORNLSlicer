#include "dlp/dlp_storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <cmath>

namespace ORNL {
namespace {
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}
}  // namespace

bool DlpStorage::temporaryRoot(QString& path, QString* error) {
    // Qt canonicalizes Unix temp paths and can return an empty path after the configured root disappears.
    path = QDir::tempPath();
#ifdef Q_OS_UNIX
    if (path.isEmpty()) path = QFile::decodeName(qgetenv("TMPDIR"));
#endif
    if (path.isEmpty())
        return fail(error,
                    "Could not resolve the DLP temporary output directory. "
                    "Check the application's temporary-directory environment and restart it.");
    path = QDir(path).absolutePath();
    return true;
}

bool DlpStorage::checkSpace(const QString& path, double required_bytes, QString* error) {
    if (path.isEmpty() || !std::isfinite(required_bytes) || required_bytes < 0)
        return fail(error, "Could not determine the DLP disk-space requirement.");
    const QString destination = QFileInfo(path).absoluteFilePath();
    QFileInfo existing(destination);
    while (!existing.exists()) {
        const QString parent = existing.absolutePath();
        if (parent == existing.absoluteFilePath()) break;
        existing.setFile(parent);
    }
    if (existing.isFile()) existing.setFile(existing.absolutePath());
    QStorageInfo storage(existing.absoluteFilePath());
    storage.refresh();
    const QString location = QDir::toNativeSeparators(destination);
    if (!storage.isValid() || !storage.isReady() || storage.bytesAvailable() < 0)
        return fail(error, QString("Could not determine available disk space for DLP output at \"%1\".").arg(location));
    if (storage.isReadOnly()) return fail(error, QString("The DLP output drive is read-only: \"%1\".").arg(location));
    if (std::ceil(required_bytes) > double(storage.bytesAvailable()))
        return fail(error, QString("Not enough disk space for DLP output at \"%1\". "
                                   "Required: %2 GiB; available: %3 GiB. Free space or choose another location.")
                               .arg(location)
                               .arg(required_bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 3)
                               .arg(storage.bytesAvailable() / (1024.0 * 1024.0 * 1024.0), 0, 'f', 3));
    return true;
}
}  // namespace ORNL
