#include <QAbstractButton>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSemaphore>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionButton>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrentRun>
#include <cstdlib>
#include <functional>

#include "dlp/dlp_build.h"
#include "managers/preferences_manager.h"
#include "test_utils.h"
#include "threading/slicers/dlp_slicer.h"
#include "utilities/theme_tool.h"
#include "windows/dialogs/dlp_dialogs.h"
#include "windows/dialogs/slice_dialog.h"

namespace {
using namespace ORNL;

bool waitFor(const std::function<bool()>& condition) {
    QElapsedTimer deadline;
    deadline.start();
    while (!condition() && deadline.elapsed() < 5000) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

QAbstractButton* button(QFileDialog& dialog, QDialogButtonBox::ButtonRole role) {
    auto* box = dialog.findChild<QDialogButtonBox*>();
    if (box)
        for (auto* candidate : box->buttons())
            if (box->buttonRole(candidate) == role) return candidate;
    return nullptr;
}

bool capture(QWidget& widget, const QString& directory, const QString& name) {
    return directory.isEmpty() || widget.grab().save(QDir(directory).filePath(name + ".png"));
}

bool testFolder(QWidget& root, const QString& screenshots, const QString& prefix) {
    QTemporaryDir directory;
    if (!Testing::expect(directory.isValid() && QDir(directory.path()).mkdir("Existing Folder"),
                         "Could not create the DLP folder-dialog fixture"))
        return false;
    QFile existing(directory.filePath("existing-output"));
    if (!Testing::expect(existing.open(QIODevice::WriteOnly) && existing.write("keep") == 4,
                         "Could not create the existing-file dialog fixture"))
        return false;
    existing.close();
    bool passed = true;
    {
        QFileDialog dialog(&root);
        configureDlpFolderDialog(dialog, directory.path());
        dialog.show();
        auto* name   = dialog.findChild<QLineEdit*>("fileNameEdit");
        auto* accept = button(dialog, QDialogButtonBox::AcceptRole);
        if (!Testing::expect(name && accept, "DLP folder dialog must expose its name field and create button"))
            return false;
        name->setText("Build with spaces.v1");
        if (!Testing::expect(waitFor([&] { return accept->isEnabled(); }), "The create-folder button must enable"))
            return false;
        QApplication::processEvents();
        QStyleOptionButton option;
        option.initFrom(accept);
        option.text = accept->text();
        if (auto* push = qobject_cast<QPushButton*>(accept)) {
            if (push->autoDefault()) option.features |= QStyleOptionButton::AutoDefaultButton;
            if (push->isDefault()) option.features |= QStyleOptionButton::DefaultButton;
        }
        const QRect contents         = accept->style()->subElementRect(QStyle::SE_PushButtonContents, &option, accept);
        const QRect visible_contents = contents.intersected(accept->visibleRegion().boundingRect());
        passed &=
            Testing::expect(visible_contents.width() >= accept->fontMetrics().horizontalAdvance("Create Folder") &&
                                visible_contents.height() >= accept->fontMetrics().height(),
                            "The Create Folder label must fit inside its styled button without clipping");
        passed &= Testing::expect(
            dialog.labelText(QFileDialog::FileName) == "Folder name:" &&
                dialog.labelText(QFileDialog::Accept) == "Create Folder" && dialog.defaultSuffix().isEmpty() &&
                dialog.testOption(QFileDialog::DontUseNativeDialog) &&
                QFontInfo(name->font()).family() == "Noto Sans" && QFontInfo(name->font()).pointSizeF() == 9,
            "The named-folder prompt must use application styling without a filename suffix");
        passed &=
            Testing::expect(capture(dialog, screenshots, prefix + "-folder"), "Could not capture the folder dialog");
        accept->click();
        passed &=
            Testing::expect(waitFor([&] { return dialog.result() == QDialog::Accepted; }) &&
                                dialog.selectedFiles() == QStringList {directory.filePath("Build with spaces.v1")} &&
                                !QFileInfo::exists(directory.filePath("Build with spaces.v1")),
                            "Accepting a new folder name must preserve spaces and dots; export creates the folder");
    }
    {
        QFileDialog dialog(&root);
        configureDlpFolderDialog(dialog, directory.path());
        dialog.show();
        auto* name   = dialog.findChild<QLineEdit*>("fileNameEdit");
        auto* cancel = button(dialog, QDialogButtonBox::RejectRole);
        if (!Testing::expect(name && cancel, "The DLP folder dialog must expose cancellation")) return false;
        name->setText("Canceled Build");
        cancel->click();
        passed &= Testing::expect(!dialog.isVisible() && dialog.result() == QDialog::Rejected &&
                                      !QFileInfo::exists(directory.filePath("Canceled Build")),
                                  "Canceling the folder prompt must create nothing");
    }
    {
        QFileDialog dialog(&root);
        configureDlpFolderDialog(dialog, directory.path());
        dialog.show();
        auto* name   = dialog.findChild<QLineEdit*>("fileNameEdit");
        auto* accept = button(dialog, QDialogButtonBox::AcceptRole);
        if (!Testing::expect(name && accept, "The existing-folder fixture must expose the folder controls"))
            return false;
        name->setText("Existing Folder");
        if (!Testing::expect(waitFor([&] { return accept->isEnabled(); }), "Existing-folder navigation must enable"))
            return false;
        accept->click();
        passed &= Testing::expect(
            waitFor([&] { return dialog.directory().absolutePath() == directory.filePath("Existing Folder"); }) &&
                dialog.isVisible() && dialog.result() != QDialog::Accepted,
            "Choosing an existing folder must navigate into it instead of replacing it");
        dialog.reject();
    }
    {
        QFileDialog dialog(&root);
        configureDlpFolderDialog(dialog, directory.path());
        dialog.show();
        auto* name   = dialog.findChild<QLineEdit*>("fileNameEdit");
        auto* accept = button(dialog, QDialogButtonBox::AcceptRole);
        if (!Testing::expect(name && accept, "The existing-file fixture must expose the folder controls")) return false;
        name->setText("existing-output");
        if (!Testing::expect(waitFor([&] { return accept->isEnabled(); }), "Existing-file handling must enable"))
            return false;
        bool overwrite_prompt = false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &dialog, [&] {
            if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                overwrite_prompt = true;
                prompt->reject();
            }
        });
        watchdog.start(1000);
        accept->click();
        watchdog.stop();
        passed &=
            Testing::expect(!overwrite_prompt && dialog.result() == QDialog::Accepted &&
                                dialog.selectedFiles() == QStringList {existing.fileName()} &&
                                existing.open(QIODevice::ReadOnly) && existing.readAll() == "keep",
                            "An existing-file path must return without an overwrite prompt or modifying the file");
    }
    return passed;
}

bool testSummary(QWidget& root, const QString& screenshots, const QString& prefix) {
    DlpPreflight summary;
    summary.layer_count                      = 53;
    summary.file_count                       = 266;
    summary.working_bytes                    = 192.0 * 1024 * 1024;
    summary.output_bytes                     = 2.5 * 1024 * 1024 * 1024;
    summary.layout.lane_count                = 4;
    summary.layout.image_width               = 16384;
    summary.layout.image_height              = 12288;
    summary.layout.lane_image_height         = 13056;
    summary.layout.pixel_pitch               = 31.005859375;
    summary.layout.settings.lane_pixel_width = 4096;
    summary.layout.settings.bit_depth        = 1;
    summary.layout.settings.output_mode      = DlpOutputMode::kScrolling;
    summary.warnings                         = {"Geometry extends beyond the printable area and will be clipped."};
    QMessageBox dialog(&root);
    configureDlpSummaryDialog(dialog, summary);
    dialog.show();
    QApplication::processEvents();
    const auto details = dialog.informativeText();
    bool passed        = Testing::expect(
        dialog.text().contains("53 layers") && dialog.text().contains("4 lanes per layer") &&
            dialog.text().contains("265 image files") && details.contains("Scrolling output · 1-bit BMP") &&
            details.contains("0.0310059 mm/pixel") && details.contains("16384 × 12288 pixels") &&
            details.contains("4096 × 13056 pixels") && details.contains("768 rows") && details.contains("192.0 MiB") &&
            details.contains("2.50 GiB temporary + 2.50 GiB exported") && details.contains(summary.warnings.first()) &&
            dialog.icon() == QMessageBox::Warning && dialog.button(QMessageBox::Ok)->text() == "Slice" &&
            dialog.button(QMessageBox::Cancel) && !details.contains("Interlaced") &&
            capture(dialog, screenshots, prefix + "-summary"),
        "DLP summary must show dimensions, pitch, layers, storage, padding, and clipping warnings");
    dialog.hide();
    summary.layout.settings.interlaced_stitching = true;
    summary.layout.settings.stitch_overlap       = 1000;
    summary.layout.stitch_overlap_pixels         = 33;
    summary.layout.lane_count                    = 5;
    summary.file_count                           = 319;
    summary.working_bytes                        = 249.375 * 1024 * 1024;
    summary.output_bytes                         = 53.0 * ((62 + 2048 * 12288) + 5.0 * (62 + 512 * 13056));
    QMessageBox stitching(&root);
    configureDlpSummaryDialog(stitching, summary);
    stitching.show();
    QApplication::processEvents();
    passed &= Testing::expect(
        stitching.text().contains("5 lanes per layer") && stitching.text().contains("318 image files") &&
            stitching.informativeText().contains("Interlaced stitching: enabled") &&
            stitching.informativeText().contains("33 px (1.02319 mm)") &&
            stitching.informativeText().contains("alternates each layer") &&
            stitching.informativeText().contains("exported lane X origins") &&
            capture(stitching, screenshots, prefix + "-stitching-summary"),
        "Stitching summaries must expose quantized width, alternating patterns, and the increased lane count");
    stitching.hide();
    summary.layout.lane_count   = 1;
    summary.layout.image_width  = 4096;
    summary.layout.image_height = summary.layout.lane_image_height = 2160;
    summary.layout.pixel_pitch                                     = 50;
    summary.layout.stitch_overlap_pixels                           = 20;
    summary.layout.settings.output_mode                            = DlpOutputMode::kStatic;
    summary.file_count                                             = 107;
    summary.working_bytes                                          = 4096.0 * 2160 + 4096.0 * 2160 + 512.0 * 2160;
    summary.output_bytes                                           = 53.0 * 2 * (62 + 512 * 2160);
    QMessageBox single_lane(&root);
    configureDlpSummaryDialog(single_lane, summary);
    single_lane.show();
    QApplication::processEvents();
    passed &= Testing::expect(single_lane.informativeText().contains("enabled (single lane; no seam)") &&
                                  !single_lane.informativeText().contains("Pattern alternates") &&
                                  capture(single_lane, screenshots, prefix + "-single-lane-stitching-summary"),
                              "A single-lane stitching summary must not imply that a joining seam will be generated");
    return passed;
}

bool testProgress(QWidget& root, QSharedPointer<const DlpBuild> build, const QString& screenshots,
                  const QString& prefix, bool cancel) {
    QTemporaryDir directory;
    if (!Testing::expect(directory.isValid(), "Could not create the progress-dialog fixture")) return false;
    const QString destination  = directory.filePath("Progress Build");
    auto* pool                 = QThreadPool::globalInstance();
    const int previous_threads = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore started, release;
    auto blocker = QtConcurrent::run(pool, [&] {
        started.release();
        release.acquire();
    });
    if (!started.tryAcquire(1, 5000)) {
        release.release();
        blocker.waitForFinished();
        pool->setMaxThreadCount(previous_threads);
        return Testing::expect(false, "Could not hold the export worker for deterministic progress checks");
    }
    bool responsive = false, controls_valid = false, cancellation_visible = false;
    QTimer::singleShot(0, &root, [&] {
        auto* dialog   = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        auto* progress = dialog ? dialog->findChild<QProgressBar*>() : nullptr;
        auto* button   = dialog ? dialog->findChild<QPushButton*>() : nullptr;
        controls_valid = dialog && dialog->windowTitle() == "Export DLP Build" && progress && button &&
                         button->isEnabled() && button->text() == "Cancel Export";
        if (dialog && cancel) {
            controls_valid &= capture(*dialog, screenshots, prefix + "-export");
            if (button) button->click();
            const auto labels = dialog->findChildren<QLabel*>();
            for (auto* label : labels) cancellation_visible |= label->text() == "Canceling export…";
            cancellation_visible &= button && !button->isEnabled() && dialog->isVisible() &&
                                    capture(*dialog, screenshots, prefix + "-export-canceling");
        }
        QTimer::singleShot(0, &root, [&] {
            responsive = true;
            release.release();
        });
    });
    QString error;
    bool canceled     = false;
    const bool result = exportDlpWithProgress(&root, build, destination, &error, &canceled);
    blocker.waitForFinished();
    pool->setMaxThreadCount(previous_threads);
    const auto contents =
        QDir(directory.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    return Testing::expect(
        responsive && controls_valid &&
            (cancel ? !result && canceled && cancellation_visible && contents.isEmpty()
                    : result && !canceled && error.isEmpty() && QFile::exists(destination + "/build.csv")),
        cancel ? "Cancel must update the responsive progress dialog and leave no partial export"
               : "Export must finish on its worker while the progress dialog processes UI events");
}

bool testSliceProgress(QWidget& root, const QString& screenshots, const QString& prefix) {
    bool passed = true;
    {
        SliceDialog before(&root);
        before.updateStatus(StatusUpdateStepType::kPreProcess, 50);
        before.show();
        QApplication::processEvents();
        passed &= Testing::expect(before.findChildren<QProgressBar*>().size() == 6 &&
                                      capture(before, screenshots, prefix + "-slice-progress-before"),
                                  "The normal slice dialog must preserve its six existing progress stages");
    }
    SliceDialog dialog(&root, true);
    const auto bars = dialog.findChildren<QProgressBar*>();
    auto* cancel    = dialog.findChild<QPushButton*>();
    if (!Testing::expect(bars.size() == 1 && cancel, "DLP slice progress must expose one layer bar and cancellation"))
        return false;
    dialog.updateStatus(StatusUpdateStepType::kPreProcess, 50);
    dialog.updateMessage("Rasterizing layer 27 of 53…");
    dialog.show();
    QApplication::processEvents();
    bool heading = false, layers = false, status = false;
    for (auto* label : dialog.findChildren<QLabel*>()) {
        heading |= label->text() == "DLP Exposure Images";
        layers |= label->text() == "Layers";
        status |= label->text() == "Rasterizing layer 27 of 53…";
    }
    passed &=
        Testing::expect(heading && layers && status && bars[0]->value() == 50 &&
                            capture(dialog, screenshots, prefix + "-slice-progress"),
                        "DLP slice progress must show its exposure heading, current layer status, and percentage");
    bool canceled = false;
    QObject::connect(&dialog, &SliceDialog::cancelSlice, [&] { canceled = true; });
    cancel->click();
    passed &=
        Testing::expect(canceled && !dialog.isVisible(), "Cancel Slice must emit cancellation and close the dialog");
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    using namespace ORNL;
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    if (!Testing::expect(Theme::initializeFonts(), "Could not initialize application fonts")) return EXIT_FAILURE;
    const QString screenshots = argc > 1 ? QString::fromUtf8(argv[1]) : QString();
    if (!screenshots.isEmpty() && !QDir().mkpath(screenshots)) return EXIT_FAILURE;

    auto build         = QSharedPointer<DlpBuild>::create();
    build->storage     = QSharedPointer<QTemporaryDir>::create();
    build->image_width = build->settings.lane_pixel_width = 16;
    build->image_height = build->lane_image_height = 8;
    build->lane_count                              = 1;
    build->lane_starts                             = {0};
    build->pixel_pitch                             = 50;
    DlpLayer layer;
    layer.number     = 1;
    layer.image_path = build->storage->filePath("full.bmp");
    layer.lanes.append({1, build->storage->filePath("lane.bmp"), false});
    QImage image(16, 8, QImage::Format_Mono);
    image.setColorTable({qRgb(0, 0, 0), qRgb(255, 255, 255)});
    image.fill(1);
    if (!Testing::expect(build->storage->isValid() && image.save(layer.image_path, "BMP") &&
                             image.save(layer.lanes[0].image_path, "BMP"),
                         "Could not create the cached build for export-dialog tests"))
        return EXIT_FAILURE;
    build->layers.append(layer);
    QWidget root;
    bool passed = true;
    for (int number : {0, 1}) {
        PreferencesManager::getInstance()->setTheme(number == 0 ? ThemeName::kLightMode : ThemeName::kDarkMode);
        Theme theme(number);
        QApplication::setPalette(theme.getPalette());
        QFile style(theme.getFolderPath() + "style.qss");
        if (!Testing::expect(style.open(QIODevice::ReadOnly), "Could not read the application root stylesheet"))
            return EXIT_FAILURE;
        root.setStyleSheet(style.readAll());
        const QString prefix = number == 0 ? "light" : "dark";
        passed &= testFolder(root, screenshots, prefix);
        passed &= testSummary(root, screenshots, prefix);
        passed &= testProgress(root, build, screenshots, prefix, true);
        passed &= testProgress(root, build, screenshots, prefix, false);
        passed &= testSliceProgress(root, screenshots, prefix);
    }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
