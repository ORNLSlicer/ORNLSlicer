#include <QApplication>
#include <QLabel>
#include <QSemaphore>
#include <QStandardPaths>
#include <QThread>
#include <cstdlib>

#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "widgets/settings/setting_bar.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const QString key = ORNL::PS::Layer::kLayerHeight;
    auto global       = ORNL::GSM->getGlobal();
    global->setSetting(key, 200.0);
    ORNL::SettingBar bar({});
    bar.resize(650, 800);
    auto* tabs = bar.findChild<QTabWidget*>();
    for (int index = 0; index < tabs->count(); ++index)
        if (tabs->tabText(index) == "Profile") tabs->setCurrentIndex(index);
    auto* filter = bar.findChild<QLineEdit*>();
    filter->setText("Layer Height");
    bar.show();
    app.processEvents();
    auto* indicator       = bar.findChild<QLabel*>("part_override_indicator_" + key);
    auto* layer_indicator = bar.findChild<QLabel*>("layer_override_indicator_" + key);
    expect(indicator != nullptr && layer_indicator != nullptr, "Global setting must have both override indicators.");
    if (indicator == nullptr || layer_indicator == nullptr) return EXIT_FAILURE;
    expect(indicator->isHidden() && layer_indicator->isHidden(),
           "Global settings without overrides must not show either indicator.");
    if (argc > 1) expect(bar.grab().save(QString(argv[1]) + "/before.png"), "Could not save before screenshot.");

    auto part = QSharedPointer<ORNL::Part>::create();
    part->setName("override-test");
    part->getSb()->setSetting(key, 300.0);
    bar.setOverrideIndicatorsSuspended(true);
    QSemaphore part_added;
    QSemaphore continue_loading;
    auto* loader = QThread::create([&] {
        ORNL::CSM->addPart(part);
        part_added.release();
        continue_loading.acquire();
        part->createSettingsRange(1, 2);
        part->getSettingsRange(1, 2)->getSb()->setSetting(key, 400.0);
    });
    QObject::connect(loader, &QThread::finished, &bar, [&bar] { bar.setOverrideIndicatorsSuspended(false); });
    loader->start();
    part_added.acquire();
    app.processEvents();
    expect(indicator->isHidden() && layer_indicator->isHidden(),
           "Queued part additions must not refresh indicators while project loading is suspended.");
    bar.refreshOverrideIndicators();
    expect(indicator->isHidden(), "Direct refreshes must also defer scans while project loading is suspended.");
    continue_loading.release();
    loader->wait();
    app.processEvents();
    delete loader;
    expect(indicator->isVisible() && layer_indicator->isVisible(),
           "Finishing project loading must refresh indicators using the final part and layer settings.");
    part->removeSettingsRange(1, 2);
    bar.refreshOverrideIndicators();
    expect(indicator->isVisible(), "Adding a part with a different value must mark the global setting.");
    expect(layer_indicator->isHidden(), "Part-only overrides must not show the layer indicator.");
    expect(!indicator->pixmap().isNull() && indicator->toolTip() == "Overridden by part settings.",
           "Part indicator must have an icon and identify part overrides.");
    expect(!layer_indicator->pixmap().isNull() && layer_indicator->toolTip() == "Overridden by layer settings.",
           "Layer indicator must have an icon and identify layer overrides.");
    expect(indicator->pixmap().toImage() != layer_indicator->pixmap().toImage(),
           "Part and layer indicators must use different icons.");
    if (argc > 1) expect(bar.grab().save(QString(argv[1]) + "/part-only.png"), "Could not save part screenshot.");

    bar.settingsBasesSelected(qMakePair(part->name(), QList {part->getSb()}), {});
    expect(indicator->isHidden(), "The global indicator must be hidden while editing part settings.");
    bar.settingsBasesSelected(qMakePair(QString(), QList<QSharedPointer<ORNL::SettingsBase>>()), {});
    expect(indicator->isVisible(), "Returning to Global must restore the override indicator.");

    global->setSetting(key, 300.0);
    bar.restoreSettingValue(key);
    expect(indicator->isHidden(), "Matching Global to the part value must clear the indicator.");
    global->setSetting(key, 200.0);
    bar.restoreSettingValue(key);
    expect(indicator->isVisible(), "Restoring a different global value must restore the indicator.");

    part->getSb()->remove(key);
    part->createSettingsRange(1, 2);
    auto range = part->getSettingsRange(1, 2)->getSb();
    range->setSetting(key, 400.0);
    bar.refreshOverrideIndicators();
    expect(layer_indicator->isVisible() && indicator->isHidden(),
           "Layer-only overrides must show only the layer indicator.");
    app.processEvents();
    if (argc > 1) expect(bar.grab().save(QString(argv[1]) + "/layer-only.png"), "Could not save layer screenshot.");
    part->getSb()->setSetting(key, 300.0);
    bar.refreshOverrideIndicators();
    expect(indicator->isVisible() && layer_indicator->isVisible(),
           "Part and layer overrides of the same setting must show both indicators.");
    app.processEvents();
    expect(!indicator->geometry().intersects(layer_indicator->geometry()), "Override icons must not overlap.");
    if (argc > 1) expect(bar.grab().save(QString(argv[1]) + "/after.png"), "Could not save combined screenshot.");
    bar.settingsBasesSelected(qMakePair(part->name(), QList {range}), {part->getSb()});
    expect(indicator->isHidden() && layer_indicator->isHidden(),
           "Both global indicators must hide while editing layer settings.");
    bar.settingsBasesSelected(qMakePair(QString(), QList<QSharedPointer<ORNL::SettingsBase>>()), {});
    expect(indicator->isVisible() && layer_indicator->isVisible(), "Returning to Global must restore both indicators.");
    part->getSb()->remove(key);
    range->setSetting(key, 200.0);
    bar.refreshOverrideIndicators();
    expect(layer_indicator->isHidden() && indicator->isHidden(),
           "A stored layer value equal to Global must not show either indicator.");
    range->setSetting(key, 400.0);
    bar.refreshOverrideIndicators();
    filter->setText("unmatched setting filter");
    expect(layer_indicator->isHidden(), "Filtering out a row must hide its indicator.");
    filter->setText("Layer Height");
    expect(layer_indicator->isVisible(), "Showing the row again must restore its indicator.");
    part->removeSettingsRange(1, 2);
    bar.refreshOverrideIndicators();
    expect(layer_indicator->isHidden(), "Removing the last layer override must clear the indicator.");

    // A secondary component must mark its grouped row as overridden.
    const QString secondary = "image_pixel_size_y";
    global->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kImage));
    bar.restoreSettingValue(ORNL::PS::Slicing::kSlicingMode);
    part->getSb()->setSetting(secondary, global->setting<double>(secondary) + 1.0);
    filter->setText("Image Pixel Size");
    bar.refreshOverrideIndicators();
    auto* grouped = bar.findChild<QLabel*>("part_override_indicator_image_pixel_size_x");
    expect(grouped != nullptr && !grouped->isHidden(), "Overrides of secondary components must mark grouped rows.");

    part->getSb()->setSetting(key, 300.0);
    filter->setText("Layer Height");
    bar.refreshOverrideIndicators();
    ORNL::CSM->removePart(part->name());
    expect(indicator->isHidden(), "Removing the overriding part must clear the indicator.");
    ORNL::CSM->addPart(part);
    app.processEvents();
    expect(indicator->isVisible(), "Adding the overriding part again must restore the indicator.");
    ORNL::CSM->clearParts();
    expect(indicator->isHidden(), "Clearing the session must clear override indicators.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
