#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <cstdlib>
#include <limits>

#include "dlp/dlp_storage.h"
#include "test_utils.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    using ORNL::DlpStorage;
    using ORNL::Testing::expect;
    QTemporaryDir directory;
    if (!expect(directory.isValid(), "Could not create the DLP disk-space fixture")) return EXIT_FAILURE;
    QString error, root;
    bool passed               = expect(DlpStorage::temporaryRoot(root, &error) && QDir::isAbsolutePath(root),
                                       "The DLP temporary root must resolve to an absolute path");
    const QString destination = directory.filePath("missing/parent/Build");
    passed &= expect(DlpStorage::checkSpace(destination, 1, &error) && !QFileInfo::exists(destination) &&
                         QDir(directory.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty(),
                     "Space checks must use a missing destination's existing volume without creating folders");
    error.clear();
    passed &= expect(!DlpStorage::checkSpace(destination, double(std::numeric_limits<qint64>::max()), &error) &&
                         error.contains("Not enough disk space") && error.contains(destination) &&
                         error.contains("Required:") && error.contains("available:") &&
                         !QFileInfo::exists(directory.filePath("missing")),
                     "An impossible storage requirement must report required/available space before creating output");
    for (double invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        passed &= expect(!DlpStorage::checkSpace(destination, invalid, &error),
                         "Invalid storage requirements must be rejected");
    passed &= expect(!DlpStorage::checkSpace(QString(), 1, &error), "An unresolved output path must be rejected");
    QFile file(directory.filePath("cached.bmp"));
    if (!expect(file.open(QIODevice::WriteOnly) && file.write("cached") == 6,
                "Could not create an existing path fixture"))
        return EXIT_FAILURE;
    file.close();
    passed &= expect(DlpStorage::checkSpace(file.fileName(), 1, &error),
                     "An existing file path must resolve its containing volume");
#ifdef Q_OS_UNIX
    QStorageInfo read_only("/sys");
    if (read_only.isValid() && read_only.isReady() && read_only.isReadOnly())
        passed &=
            expect(!DlpStorage::checkSpace("/sys/ornlslicer-dlp-test/Build", 1, &error) && error.contains("read-only"),
                   "A read-only output volume must be rejected without creating files");
#endif
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
