#pragma once

#include <QSharedPointer>
#include <QString>

class QFileDialog;
class QMessageBox;
class QWidget;

namespace ORNL {
struct DlpBuild;
struct DlpPreflight;

void configureDlpFolderDialog(QFileDialog& dialog, const QString& parent_directory);
void configureDlpSummaryDialog(QMessageBox& dialog, const DlpPreflight& summary);

//! Runs file export on a worker while the modal progress dialog remains responsive.
bool exportDlpWithProgress(QWidget* parent, QSharedPointer<const DlpBuild> build, const QString& destination,
                           QString* error, bool* canceled);
}  // namespace ORNL
