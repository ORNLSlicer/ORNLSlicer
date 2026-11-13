#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <cstdlib>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "widgets/layerbar.h"
#include "widgets/layerdot.h"
#include "widgets/settings/setting_bar.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    QApplication::setStyle(QStyleFactory::create("fusion"));

    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const QString key = ORNL::PS::Perimeter::kCount;
    auto global       = ORNL::GSM->getGlobal();
    global->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    global->setSetting(ORNL::PS::Perimeter::kEnable, true);
    ORNL::GSM->setCurrentTemplate("Current template");
    const auto preferences = ORNL::PreferencesManager::getInstance();
    expect(ORNL::Theme::initializeFonts(), "Could not load the bundled application fonts.");
    QApplication::setPalette(preferences->getTheme().getPalette());
    QFile stylesheet(preferences->getTheme().getFolderPath() + "style.qss");
    if (!ORNL::Testing::expect(stylesheet.open(QIODevice::ReadOnly), "Could not read the application stylesheet."))
        return EXIT_FAILURE;
    if (argc > 1) expect(QDir().mkpath(argv[1]), "Could not create navigation screenshot directory.");
    preferences->addHiddenSetting("Profile", "Perimeter");
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kHide);

    auto model          = QSharedPointer<ORNL::PartMetaModel>::create();
    const auto add_part = [&](const QString& name, ORNL::MeshType type = ORNL::MeshType::kBuild) {
        auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(10000, 10000, 12000));
        mesh->setName(name);
        mesh->setType(type);
        auto part = QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
        part->setCurrentPartTemplate("Original template");
        ORNL::CSM->addPart(part, false);
        return model->newItem(part);
    };
    auto other = add_part("Other");
    auto owner = add_part("Owner");
    auto part  = owner->part();
    part->getSb()->setSetting(key, 2);
    part->getSb()->setSetting("point_order_optimization", 4);
    part->getSb()->setSetting("seam_attractor_vector_y", 2.5);
    part->createSettingsRange(0, 0);
    auto first_layer = part->getSettingsRange(0, 0)->getSb();
    first_layer->setSetting(key, 7);
    part->createSettingsRange(2, 6);
    auto range = part->getSettingsRange(2, 6)->getSb();
    range->setSetting(key, 3);
    part->createSettingsRange(4, 4);
    part->getSettingsRange(4, 4)->getSb()->setSetting(key, 4);
    part->createSettingsRange(8, 8, "Shared group");
    part->createSettingsRange(10, 10, "Shared group");
    auto grouped = part->getSettingsRange(8, 8)->getSb();
    grouped->setSetting(key, 5);
    grouped->setSetting(ORNL::PS::Perimeter::kEnable, false);
    auto settings_mesh = add_part("Settings region", ORNL::MeshType::kSettings);
    settings_mesh->part()->getSb()->setSetting(key, 6);

    ORNL::SettingBar bar({});
    bar.setStyleSheet(stylesheet.readAll());
    bar.resize(650, 800);
    bar.show();
    ORNL::LayerBar layers(model);
    layers.resize(80, 600);
    QObject::connect(&layers, &ORNL::LayerBar::setSelectedSettings, &bar, &ORNL::SettingBar::settingsBasesSelected);
    QList<QSharedPointer<ORNL::SettingsBase>> selected;
    QList<QPair<int, int>> selected_ranges;
    QSharedPointer<ORNL::Part> selected_part;
    QObject::connect(&layers, &ORNL::LayerBar::setSelectedSettings, &app,
                     [&](QPair<QString, QList<QSharedPointer<ORNL::SettingsBase>>> named_bases,
                         QList<QSharedPointer<ORNL::SettingsBase>>) { selected = named_bases.second; });
    QObject::connect(&layers, &ORNL::LayerBar::selectedLayerSettingsRangesChanged, &app,
                     [&](QSharedPointer<ORNL::Part> visual_part, QList<QPair<int, int>> ranges) {
                         selected_part   = visual_part;
                         selected_ranges = ranges;
                     });

    ORNL::SettingTab* perimeter = nullptr;
    for (auto* pane : bar.getPanes())
        if (auto* tab = pane->getTab("Perimeter")) perimeter = tab;
    expect(perimeter != nullptr, "The existing settings editor must contain the Perimeter category.");
    if (perimeter == nullptr) return EXIT_FAILURE;
    auto row     = perimeter->getRow(key);
    auto* spin   = dynamic_cast<QSpinBox*>(row.get());
    auto* search = bar.findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
    expect(spin != nullptr && search != nullptr, "The existing row must have an integer editor and settings search.");
    if (spin == nullptr || search == nullptr) return EXIT_FAILURE;

    const auto global_before = global->json();
    const auto part_before   = part->getSb()->json();
    const auto ranges_before = part->SettingsRangesToJson();
    other->setSelected(true);
    owner->renamePart("Renamed owner");
    expect(layers.selectSettingsTarget(part->getId(), 0, 0) && selected == QList {first_layer} &&
               selected_ranges == QList {qMakePair(0, 0)},
           "The displayed first layer must navigate to the exact stored zero-based single-layer override.");
    const auto shift_click_layer = [&](int layer) {
        ORNL::LayerDot* target = nullptr;
        for (auto* dot : layers.findChildren<ORNL::LayerDot*>())
            if (dot->getLayer() == layer) target = dot;
        expect(target != nullptr, "Shift-click navigation must target an existing layer dot.");
        if (target == nullptr) return;
        const QPointF position = target->geometry().center();
        QMouseEvent press(QEvent::MouseButtonPress, position, layers.mapToGlobal(position.toPoint()), Qt::LeftButton,
                          Qt::LeftButton, Qt::ShiftModifier);
        QApplication::sendEvent(&layers, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, position, layers.mapToGlobal(position.toPoint()),
                            Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
        QApplication::sendEvent(&layers, &release);
    };
    shift_click_layer(4);
    expect(selected_ranges == QList {qMakePair(0, 0), qMakePair(2, 6)},
           "Shift-click after first navigation must extend selection from the navigation target.");
    expect(layers.selectSettingsTarget(part->getId(), 8, 8),
           "Repeated navigation must rebuild the layer dots and select the new target.");
    shift_click_layer(4);
    expect(selected_ranges == QList {qMakePair(2, 6), qMakePair(8, 8), qMakePair(10, 10)},
           "Shift-click after repeated navigation must use the new target and retain its grouped selection.");
    expect(layers.selectSettingsTarget(part->getId(), 2, 6),
           "Navigation must resolve the owner by UUID after renaming.");
    expect(model->selectedItems() == QList {owner} && selected == QList {range},
           "Navigation must select only the owner and the exact range, excluding a nested override.");
    expect(selected_part == part && selected_ranges == QList {qMakePair(2, 6)},
           "Range navigation must identify the exact zero-based range for visualization.");
    expect(bar.focusSetting(key), "A user-hidden category must be revealable for navigation.");
    app.processEvents();
    expect(spin->isVisible() && spin->value() == 3 && row->getBases() == QList {range},
           "The revealed editor must display the selected range override.");
    if (argc > 1)
        expect(bar.grab().save(QString(argv[1]) + "/navigation-range.png"),
               "Could not save range navigation screenshot.");
    expect(preferences->isSettingHidden("Profile", "Perimeter"),
           "Revealing a navigation target must preserve hidden-category preferences.");
    auto* header = perimeter->findChild<ORNL::SettingHeader*>();
    search->clear();
    app.processEvents();
    expect(header != nullptr && header->isHidden() && preferences->isSettingHidden("Profile", "Perimeter"),
           "Clearing navigation must hide the temporary category header, including after the search restores tabs.");
    expect(bar.focusSetting(key), "A temporarily hidden category must be revealable again.");
    bar.showHiddenSetting("Profile", "Perimeter");
    preferences->removeHiddenSetting("Profile", "Perimeter");
    search->clear();
    app.processEvents();
    expect(header != nullptr && !header->isHidden() && !preferences->isSettingHidden("Profile", "Perimeter") &&
               perimeter->parentWidget()->layout()->indexOf(perimeter) >= 0,
           "Permanently showing a category must survive navigation cleanup and retain its layout entry.");
    preferences->addHiddenSetting("Profile", "Perimeter");
    bar.hideSetting("Profile", "Perimeter");
    expect(part->getSb()->json() == part_before && part->SettingsRangesToJson() == ranges_before &&
               global->json() == global_before && part->getCurrentPartTemplate() == "Original template" &&
               ORNL::GSM->getCurrentTemplate() == "Current template",
           "Navigation must preserve all settings, copied range values, and layer-template state.");

    expect(layers.selectSettingsTarget(part->getId(), 8, 8) && selected == QList {grouped},
           "Grouped navigation must edit the group's shared settings base.");
    expect(selected_ranges == QList {qMakePair(8, 8), qMakePair(10, 10)},
           "Grouped navigation must retain the editor's normal shared-group visualization.");
    expect(bar.focusSetting(key), "Dependency-hidden overrides must remain reachable from the overview.");
    app.processEvents();
    expect(spin->isVisible() && !spin->isEnabled() && spin->value() == 5,
           "Revealing an inactive setting must show its value without enabling editing.");
    if (argc > 1)
        expect(bar.grab().save(QString(argv[1]) + "/navigation-inactive.png"),
               "Could not save inactive setting screenshot.");
    search->clear();
    app.processEvents();
    expect(!row->isShown(), "Changing the search must restore dependency-driven row visibility.");

    expect(layers.selectSettingsTarget(part->getId(), -1, -1) && selected == QList {part->getSb()} &&
               selected_part.isNull() && selected_ranges.isEmpty(),
           "Part navigation must clear selected layer ranges and edit only part overrides.");
    expect(bar.focusSetting(key) && spin->value() == 2,
           "Part navigation must restore the owning part's displayed value.");
    expect(bar.focusSetting("seam_attractor_vector_y"),
           "Composite component aliases must resolve to their existing row.");
    app.processEvents();
    ORNL::SettingTab* optimizations = nullptr;
    for (auto* pane : bar.getPanes())
        if (auto* tab = pane->getTab("Optimizations")) optimizations = tab;
    const auto composite   = optimizations == nullptr ? QSharedPointer<ORNL::SettingRowBase>()
                                                      : optimizations->getRow("seam_attractor_vector_y");
    auto* composite_widget = dynamic_cast<QWidget*>(composite.get());
    const auto components  = composite_widget == nullptr ? QList<QDoubleSpinBox*>()
                                                         : composite_widget->findChildren<QDoubleSpinBox*>(
                                                               QString(), Qt::FindDirectChildrenOnly);
    expect(components.size() == 3 && components.at(1)->value() == 2.5 && components.at(1)->hasFocus(),
           "Navigation to a composite alias must focus the requested component at its local override value.");
    expect(!bar.focusSetting(ORNL::PRS::MachineSetup::kSyntax),
           "Navigation must preserve settings that cannot be edited at local scope.");
    expect(
        !layers.selectSettingsTarget(part->getId(), 1, 1) && !layers.selectSettingsTarget(QUuid::createUuid(), -1, -1),
        "Missing ranges and removed owners must not redirect editing to another scope.");

    expect(layers.selectSettingsTarget(settings_mesh->part()->getId(), -1, -1) &&
               selected == QList {settings_mesh->part()->getSb()},
           "Settings-region navigation must retain local editing when no build layer bar is available.");
    expect(bar.focusSetting(key) && spin->value() == 6,
           "Settings-region navigation must focus the region's local override.");
    expect(part->getSb()->json() == part_before && part->SettingsRangesToJson() == ranges_before &&
               global->json() == global_before,
           "All navigation and focus operations must leave project settings unchanged.");
    preferences->removeHiddenSetting("Profile", "Perimeter");
    model->clearItems();
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
