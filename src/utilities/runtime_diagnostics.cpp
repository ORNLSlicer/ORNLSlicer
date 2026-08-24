#include "utilities/runtime_diagnostics.h"

#include <iostream>

#include <qglobal.h>

#include "ornlslicer/build_info.h"

namespace ORNL::Diagnostics {
QString runtimeSummary(const QString& mode) {
    return QString("runtime mode=%1 version=%2 git=%3 build=%4 package=%5 qt_runtime=%6 qt_build=%7")
        .arg(mode)
        .arg(QString::fromUtf8(ORNL::BuildInfo::Version))
        .arg(QString::fromUtf8(ORNL::BuildInfo::GitRevision))
        .arg(QString::fromUtf8(ORNL::BuildInfo::BuildConfiguration))
        .arg(QString::fromUtf8(ORNL::BuildInfo::PackageType))
        .arg(QString::fromUtf8(qVersion()))
        .arg(QString::fromUtf8(QT_VERSION_STR));
}

void logLine(const QString& message) {
    std::cerr << "[ornlslicer] " << message.toStdString() << std::endl;
}

void logRuntimeSummary(const QString& mode) {
    logLine(runtimeSummary(mode));
}
}  // namespace ORNL::Diagnostics
