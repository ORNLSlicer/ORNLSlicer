#include "windows/dialogs/dlp_dialogs.h"

#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>
#include <atomic>

#include "dlp/dlp_build.h"
#include "threading/slicers/dlp_slicer.h"
#include "units/unit.h"

namespace ORNL {
namespace {
class ExportProgressDialog : public QDialog {
   public:
    ExportProgressDialog(QWidget* parent, std::atomic_bool& canceled) : QDialog(parent), m_canceled(canceled) {
        setWindowTitle(tr("Export DLP Build"));
        setMinimumWidth(380);
        auto* layout = new QVBoxLayout(this);
        m_label      = new QLabel(tr("Copying DLP images…"), this);
        layout->addWidget(m_label);
        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 1000);
        layout->addWidget(m_progress);
        m_cancel = new QPushButton(tr("Cancel Export"), this);
        layout->addWidget(m_cancel, 0, Qt::AlignRight);
        connect(m_cancel, &QPushButton::clicked, this, &ExportProgressDialog::reject);
    }

    void updateProgress(qint64 completed, qint64 total) {
        if (m_canceled) return;
        m_progress->setValue(total > 0 ? int(1000.0 * completed / total) : 0);
        m_label->setText(tr("Exporting images: %1 of %2 MiB")
                             .arg(completed / (1024.0 * 1024.0), 0, 'f', 1)
                             .arg(total / (1024.0 * 1024.0), 0, 'f', 1));
    }

    void reject() override {
        m_canceled = true;
        m_cancel->setEnabled(false);
        m_label->setText(tr("Canceling export…"));
    }

   private:
    std::atomic_bool& m_canceled;
    QLabel* m_label;
    QProgressBar* m_progress;
    QPushButton* m_cancel;
};
}  // namespace

void configureDlpFolderDialog(QFileDialog& dialog, const QString& parent_directory) {
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setWindowTitle(QObject::tr("Create DLP Build Folder"));
    dialog.setDirectory(parent_directory);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    dialog.setLabelText(QFileDialog::FileName, QObject::tr("Folder name:"));
    dialog.setLabelText(QFileDialog::Accept, QObject::tr("Create Folder"));
    dialog.setOption(QFileDialog::DontConfirmOverwrite);
}

void configureDlpSummaryDialog(QMessageBox& dialog, const DlpPreflight& summary) {
    const auto& build = summary.layout;
    dialog.setWindowTitle(QObject::tr("DLP Slice Summary"));
    dialog.setIcon(summary.warnings.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setText(QObject::tr("%1 layers · %2 lanes per layer · %3 image files")
                       .arg(summary.layer_count)
                       .arg(build.lane_count)
                       .arg(summary.file_count - 1));
    QString details = QObject::tr(
                          "%1 output · %2-bit BMP\n"
                          "Pixel pitch: %3 mm/pixel (square pixels)\n"
                          "Full layer: %4 × %5 pixels\n"
                          "Each lane: %6 × %7 pixels\n"
                          "Black padding below Y: %8 rows\n"
                          "Estimated image working memory: %9 MiB\n"
                          "Estimated image storage: %10 GiB temporary + %10 GiB exported\n"
                          "Memory estimate excludes models and geometry processing.")
                          .arg(build.settings.output_mode == DlpOutputMode::kStatic ? QObject::tr("Static")
                                                                                    : QObject::tr("Scrolling"))
                          .arg(build.settings.bit_depth)
                          .arg(Distance(build.pixel_pitch).to(mm), 0, 'g', 6)
                          .arg(build.image_width)
                          .arg(build.image_height)
                          .arg(build.settings.lane_pixel_width)
                          .arg(build.lane_image_height)
                          .arg(build.lane_image_height - build.image_height)
                          .arg(summary.working_bytes / (1024.0 * 1024.0), 0, 'f', 1)
                          .arg(summary.output_bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    if (!summary.warnings.isEmpty()) details += "\n\n" + summary.warnings.join("\n\n");
    dialog.setInformativeText(details);
    dialog.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    dialog.button(QMessageBox::Ok)->setText(QObject::tr("Slice"));
    dialog.setDefaultButton(QMessageBox::Ok);
}

bool exportDlpWithProgress(QWidget* parent, QSharedPointer<const DlpBuild> build, const QString& destination,
                           QString* error, bool* canceled) {
    std::atomic_bool cancel_requested {false};
    std::atomic<qint64> completed {0}, total {0};
    bool was_canceled = false;
    QString failure;
    ExportProgressDialog dialog(parent, cancel_requested);
    QFutureWatcher<bool> watcher;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &dialog, [&] { dialog.updateProgress(completed.load(), total.load()); });
    QObject::connect(&watcher, &QFutureWatcher<bool>::finished, &dialog, &QDialog::accept);
    watcher.setFuture(QtConcurrent::run([&, build, destination] {
        return DlpExporter::exportBuild(
            *build, destination, &failure,
            [&](qint64 done, qint64 size) {
                total     = size;
                completed = done;
                return !cancel_requested.load();
            },
            &was_canceled);
    }));
    timer.start(50);
    dialog.exec();
    watcher.waitForFinished();
    if (error) *error = failure;
    if (canceled) *canceled = was_canceled;
    return watcher.result();
}
}  // namespace ORNL
