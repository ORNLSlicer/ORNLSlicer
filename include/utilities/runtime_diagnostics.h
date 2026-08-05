#pragma once

#include <QString>

namespace ORNL::Diagnostics {
QString runtimeSummary(const QString& mode);
void logLine(const QString& message);
void logRuntimeSummary(const QString& mode);
} // namespace ORNL::Diagnostics
