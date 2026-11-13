#include "capture_common.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QOpenGLWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <algorithm>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "graphics/base_view.h"
#include "graphics/view/gcode_view.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "widgets/cmd_widget.h"
#include "widgets/gcode_widget.h"
#include "widgets/gcodebar.h"
#include "widgets/layerbar.h"
#include "widgets/main_toolbar.h"
#include "widgets/part_widget/part_control/part_control.h"
#include "widgets/part_widget/part_toolbar.h"
#include "widgets/part_widget/part_widget.h"
#include "widgets/settings/setting_bar.h"
#include "windows/dialogs/slice_dialog.h"
#include "windows/dialogs/template_save.h"
#include "windows/gcode_to_s2c.h"
#include "windows/layer_times_window.h"
#include "windows/main_window.h"
#include "windows/preferences_window.h"
#include "windows/settings_file_compare.h"

namespace ORNL {

QString toSlug(const QString& name) {
    QString slug = name.toLower();
    slug.replace(QRegularExpression("[^a-z0-9]+"), "_");
    slug.replace(QRegularExpression("^_+|_+$"), "");
    return slug;
}

QImage captureWidget(QWidget* widget) {
    QWidget* target = requireNonNull(widget, "captureWidget target");
    if (auto* glWidget = qobject_cast<QOpenGLWidget*>(target)) {
        QImage img = glWidget->grabFramebuffer();
        glWidget->makeCurrent();
        if (img.isNull() || img.size().isEmpty()) {
            throw std::runtime_error("OpenGL framebuffer capture returned an empty image");
        }
        return img;
    }
    QImage img = target->grab().toImage();
    if (img.isNull() || img.size().isEmpty()) { throw std::runtime_error("Widget capture returned an empty image"); }
    return img;
}

void saveWidget(QWidget* widget, const QString& filePath) {
    QDir().mkpath(QFileInfo(filePath).path());
    QImage img = captureWidget(widget);
    if (!img.save(filePath, "PNG")) {
        throw std::runtime_error(("Failed to save captured image to " + filePath).toStdString());
    }
}

void clearScene(MainWindow* window) {
    requireChild<PartWidget>(window)->clear();
    CSM->clearParts();
}

QSharedPointer<PartMetaItem> addScenePart(MainWindow* window, const QSharedPointer<MeshBase>& mesh, const QString& name,
                                          MeshType mt) {
    mesh->setType(mt);
    CSM->addPart(mesh, name, mt);
    auto part = requireNonNull(CSM->parts().value(name), QString("CSM part '%1'").arg(name));
    auto item = requireNonNull(requireChild<PartWidget>(window)->getPartMeta()->lookupByPointer(part),
                               QString("PartMetaItem '%1'").arg(name));
    item->setMeshType(mt);
    return item;
}

QVector3D offsetFrom(const QSharedPointer<PartMetaItem>& baseItem, float dx_mm, float dy_mm, float dz_mm) {
    return baseItem->translation() +
           QVector3D(dx_mm * mm(), dy_mm * mm(), dz_mm * mm()) * Constants::OpenGL::kObjectToView;
}

void ensureSampleParts(MainWindow* window) {
    PreferencesManager::getInstance()->setFileShiftPreference(PreferenceChoice::kSkipAutomatically);

    auto base     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    auto baseItem = addScenePart(window, base, "Base_Bracket", MeshType::kBuild);
    auto clip     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(150.0 * mm, 300.0 * mm));
    auto clipItem = addScenePart(window, clip, "Cavity_Void", MeshType::kClipping);

    baseItem->adoptChild(clipItem);
    clipItem->setTranslation(offsetFrom(baseItem, 450.0f, 250.0f));
    baseItem->setSelected(true);
    requireChild<PartControl>(window)->expandAll();
    requireChild<PartToolbar>(window)->setEnabled(true);
    requireChild<LayerTimesWindow>(window)->setEnabled(true);

    for (QLabel* label : requireChild<SettingBar>(window)->findChildren<QLabel*>()) {
        if (label->text().contains("Currently searching in")) {
            label->setText("Currently searching in: <settings-path>");
            label->hide();
        }
    }
}

void setupCaptureCameras(MainWindow* window, const QVector2D& angle, float zoomDelta,
                         std::optional<QVector3D> panTarget) {
    for (auto* view : window->findChildren<BaseView*>()) {
        view->resetCamera();
        view->camera()->rotateAbsolute(angle);
        if (panTarget.has_value()) { view->camera()->panAbsolute(*panTarget); }
        else {
            QVector3D center = view->camera()->getPan();
            center += QVector3D(550.0f * mm(), 380.0f * mm(), 125.0f * mm()) * Constants::OpenGL::kObjectToView;
            view->camera()->panAbsolute(center);
        }
        view->camera()->zoom(zoomDelta);
    }
}

void setGCodeLayerRange(MainWindow* window, int low, int high) {
    auto* gcodeBar  = requireChild<GcodeBar>(window);
    auto* gcodeView = requireNonNull(requireChild<GCodeWidget>(window)->view(), "GCodeWidget::view");
    int maxL        = gcodeBar->maxLayer();
    low             = std::clamp(low, 0, maxL);
    high            = std::clamp(high, low, maxL);
    gcodeBar->setLowerLayer(low);
    gcodeBar->setUpperLayer(high);
    gcodeView->setLowLayer(low);
    gcodeView->setHighLayer(high);
}

void sliceAndWait(MainWindow* window) {
    auto* textEdit = requireChild<QPlainTextEdit>(requireChild<GcodeBar>(window));
    textEdit->clear();
    QEventLoop loop;
    QObject::connect(textEdit, &QPlainTextEdit::textChanged, &loop, [textEdit, &loop]() {
        if (!textEdit->toPlainText().isEmpty()) { QTimer::singleShot(150, &loop, &QEventLoop::quit); }
    });
    emit requireChild<MainToolbar>(window)->slice();
    for (auto* dlg : window->findChildren<SliceDialog*>()) {
        QObject::disconnect(dlg, &SliceDialog::cancelSlice, nullptr, nullptr);
        for (auto* btn : dlg->findChildren<QPushButton*>()) { btn->setEnabled(false); }
        dlg->hide();
    }
    loop.exec();
    setGCodeLayerRange(window, 0, requireChild<GcodeBar>(window)->maxLayer());
    QApplication::sendPostedEvents();
}

void sliceSampleParts(MainWindow* window) {
    sliceAndWait(window);

    auto* cmd    = requireChild<QTextEdit>(requireChild<CmdWidget>(window));
    QString text = cmd->toPlainText().replace(QDir::homePath(), "/home/user");
    text.replace(QRegularExpression("Total Slice Time.*"),
                 "Total Slice Time (excluding gcode writing/parsing): 00:00:02.000");
    cmd->setPlainText(text);
    requireChild<LayerBar>(window)->addRange(10, 40);

    setupCaptureCameras(window);
    requireChild<MainToolbar>(window)->setView(0);
}

QWidget* selectSettingsTab(MainWindow* window, int tabIndex) {
    auto* bar = requireChild<SettingBar>(window);
    bar->settingsBasesSelected({"", {}}, {});
    requireChild<QTabWidget>(bar)->setCurrentIndex(tabIndex);
    QApplication::sendPostedEvents();
    return bar;
}

QWidget* preparePreferencesWindow(MainWindow* window) {
    auto* w           = requireChild<PreferencesWindow>(window);
    auto* tabWidget   = requireChild<QTabWidget>(w);
    auto* layout      = requireNonNull(w->layout(), "PreferencesWindow::layout");
    const int margins = layout->contentsMargins().left() + layout->contentsMargins().right();
    w->resize(std::max(w->width(), tabWidget->tabBar()->sizeHint().width() + margins), w->height());
    tabWidget->setCurrentIndex(0);
    return w;
}

QWidget* prepareSliceDialog(MainWindow* w) {
    auto* dlg = new SliceDialog(w);
    dlg->updateStatus(StatusUpdateStepType::kPreProcess, 100);
    dlg->updateStatus(StatusUpdateStepType::kCompute, 100);
    dlg->updateStatus(StatusUpdateStepType::kPostProcess, 100);
    dlg->updateStatus(StatusUpdateStepType::kGcodeGeneraton, 30);
    return dlg;
}

QWidget* prepareTemplateSaveDialog(MainWindow* w) {
    auto* dlg = new TemplateSaveDialog(w);
    for (auto* edit : dlg->findChildren<QLineEdit*>()) {
        if (edit->text().contains("template.s2c")) {
            edit->setText("/home/user/template.s2c");
            break;
        }
    }
    return dlg;
}

QWidget* prepareGcodeToS2CDialog(MainWindow* w) {
    auto* dlg  = new GcodeToS2CDialog(w);
    auto edits = dlg->findChildren<QLineEdit*>();
    if (edits.size() < 2) { throw std::runtime_error("GcodeToS2CDialog missing expected QLineEdit fields"); }
    edits[0]->setText("/home/user/sample_toolpath.gcode");
    edits[1]->setText("/home/user/extracted_settings.s2c");
    return dlg;
}

QWidget* prepareSettingsFileCompareDialog(MainWindow* w) {
    static QTemporaryDir tempDir;
    const QString fileA = tempDir.filePath("BAAM_20in_Baseline.s2c");
    const QString fileB = tempDir.filePath("BAAM_20in_HighThroughput.s2c");

    {
        QFile f1(fileA);
        if (!f1.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error(("Failed to create temp settings file: " + fileA).toStdString());
        }
        f1.write(
            R"({"header":{"created_by":"User","created_on":"2026-01-15","last_modified":"2026-01-15","version":2.0},"settings":[{"syntax":15,"machine_type":0,"layer_height":5080.0,"default_width":10160.0,"default_speed":101600.0,"perimeter_count":1,"inset_count":2,"infill_pattern":0,"infill_density":35.0,"support":false}]})");
        QFile f2(fileB);
        if (!f2.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error(("Failed to create temp settings file: " + fileB).toStdString());
        }
        f2.write(
            R"({"header":{"created_by":"User","created_on":"2026-02-01","last_modified":"2026-02-01","version":2.0},"settings":[{"syntax":15,"machine_type":0,"layer_height":6350.0,"default_width":12700.0,"default_speed":152400.0,"perimeter_count":2,"inset_count":2,"infill_pattern":1,"infill_density":50.0,"raft":true,"variable_layer_height":true}]})");
    }

    auto* dlg  = new SettingsFileCompareDialog(w);
    auto edits = dlg->findChildren<QLineEdit*>();
    if (edits.size() < 2) { throw std::runtime_error("SettingsFileCompareDialog missing expected QLineEdit fields"); }
    edits[0]->setText(fileA);
    edits[1]->setText(fileB);

    bool clickedCompare = false;
    for (auto* btn : dlg->findChildren<QPushButton*>()) {
        if (btn->text() == "Compare") {
            btn->click();
            clickedCompare = true;
            break;
        }
    }
    if (!clickedCompare) { throw std::runtime_error("SettingsFileCompareDialog missing 'Compare' button"); }

    const QSignalBlocker b0(edits[0]);
    const QSignalBlocker b1(edits[1]);
    edits[0]->setText("/home/user/templates/BAAM_20in_Baseline.s2c");
    edits[1]->setText("/home/user/templates/BAAM_20in_HighThroughput.s2c");
    dlg->resize(980, 420);
    return dlg;
}

}  // namespace ORNL
