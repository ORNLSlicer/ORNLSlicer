#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>
#include <cstdlib>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "widgets/layerbar.h"
#include "widgets/layerdot.h"

namespace {
QRadioButton* radioButton(QDialog& dialog, const QString& text) {
    for (auto* button : dialog.findChildren<QRadioButton*>())
        if (button->text() == text) return button;
    return nullptr;
}
}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    QApplication::setStyle(QStyleFactory::create("fusion"));

    bool passed       = true;
    const auto expect = [&](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    expect(ORNL::Theme::initializeFonts(), "Could not load the application's fonts.");
    auto global = ORNL::GSM->getGlobal();
    global->setSetting(ORNL::PS::Layer::kLayerHeight, 1000.0);
    auto model = QSharedPointer<ORNL::PartMetaModel>::create();
    QWidget root;
    ORNL::LayerBar bar(model, &root);
    bar.resize(80, 600);
    auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(10000, 10000, 40000));
    auto part = QSharedPointer<ORNL::Part>::create(mesh);
    part->setCurrentPartTemplate(ORNL::GSM->getCurrentTemplate());
    ORNL::CSM->addPart(part, false);
    auto item = model->newItem(part);
    item->setSelected(true);

    const auto openGroup = [&](const auto& edit) {
        bool opened = false;
        QTimer::singleShot(0, &bar, [&]() {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            expect(dialog != nullptr, "Add Group must open its modal dialog.");
            if (dialog == nullptr) return;
            opened = true;
            edit(*dialog);
            if (dialog->isVisible()) dialog->reject();
        });
        expect(QMetaObject::invokeMethod(&bar, "addGroup", Qt::DirectConnection), "Could not invoke Add Group.");
        expect(opened, "The group dialog must be exercised.");
    };
    bool has_list_mode     = true;
    const auto preferences = ORNL::PreferencesManager::getInstance();
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode}) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        QFile style(preferences->getTheme().getFolderPath() + "style.qss");
        if (!ORNL::Testing::expect(style.open(QIODevice::ReadOnly), "Could not load the application stylesheet."))
            return EXIT_FAILURE;
        root.setStyleSheet(style.readAll());
        openGroup([&](QDialog& dialog) {
            const auto spins = dialog.findChildren<QDoubleSpinBox*>();
            expect(spins.size() == 3 && spins[1]->maximum() == 40, "The fixture must have forty selectable layers.");
            auto* custom = radioButton(dialog, "Make a custom group.");
            expect(custom != nullptr, "The existing custom group option must remain available.");
            if (custom) custom->click();
            if (argc > 1) {
                expect(QDir().mkpath(argv[1]), "Could not create the screenshot directory.");
                app.processEvents();
                const QString name = theme == ORNL::ThemeName::kLightMode ? "light" : "dark";
                expect(dialog.grab().save(QString(argv[1]) + "/" + name + "-interval.png"),
                       "Could not save the custom interval dialog.");
            }
            auto* list_mode = radioButton(dialog, "Layer list");
            has_list_mode &= list_mode != nullptr;
            if (list_mode == nullptr) return;
            list_mode->click();
            auto* input = dialog.findChild<QLineEdit*>("layerGroupLayers");
            expect(input != nullptr && input->isEnabled(), "List mode must enable the layer list editor.");
            if (input) input->setText("1, 2, 5-9, 12, 30-40");
            if (argc > 1) {
                app.processEvents();
                const QString name = theme == ORNL::ThemeName::kLightMode ? "light" : "dark";
                expect(dialog.grab().save(QString(argv[1]) + "/" + name + "-list.png"),
                       "Could not save the layer list dialog.");
            }
        });
    }
    if (!ORNL::Testing::expect(has_list_mode, "Custom groups must provide a Layer list mode (issue #398)."))
        return EXIT_FAILURE;

    const auto members = [&](const QString& name) {
        QSet<int> layers;
        for (auto range : part->getSettingsRanges()) {
            if (range->groupName() != name) continue;
            expect(range->isSingle(), "List ranges must expand into individual group members.");
            layers.insert(range->low() + 1);
        }
        return layers;
    };
    const auto configureList = [&](QDialog& dialog, const QString& text, const QString& name) {
        radioButton(dialog, "Make a custom group.")->click();
        radioButton(dialog, "Layer list")->click();
        dialog.findChild<QLineEdit*>("layerGroupLayers")->setText(text);
        dialog.findChild<QLineEdit*>("layerGroupName")->setText(name);
    };
    openGroup([&](QDialog& dialog) {
        configureList(dialog, "1, 2, 5-9, 12, 30-40", "Requested layers");
        auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        expect(ok->isEnabled(), "The issue's example must be accepted.");
        ok->click();
    });
    const QSet<int> requested {1, 2, 5, 6, 7, 8, 9, 12, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40};
    expect(members("Requested layers") == requested && part->getSettingsRanges().size() == requested.size(),
           "The issue's list must create exactly the requested layers, including both range endpoints.");
    auto shared = part->getSettingsRange(0, 0)->getSb();
    shared->setSetting(ORNL::PS::Perimeter::kCount, 7);
    for (auto range : part->getSettingsRanges())
        expect(range->getSb() == shared && range->getSb()->setting<int>(ORNL::PS::Perimeter::kCount) == 7,
               "Every new group member must share the same editable settings.");
    const auto saved = part->SettingsRangesToJson();
    part->loadRangesFromJson(saved);
    expect(members("Requested layers") == requested && part->SettingsRangesToJson() == saved,
           "The list's membership, name, and settings must survive the existing save format.");
    expect(bar.selectSettingsTarget(part->getId(), 0, 0), "Saved list groups must remain selectable.");
    ORNL::LayerBar::dot_group* restored_group = nullptr;
    for (auto* dot : bar.findChildren<ORNL::LayerDot*>()) {
        if (dot->getLayer() == 0) restored_group = dot->getGroup();
    }
    expect(restored_group != nullptr && restored_group->grouped.size() == requested.size(),
           "Rebuilding the layer bar must restore the full visual group.");

    const auto reset = [&]() {
        part->clearSettingsRanges();
        item->setSelected(true);
    };
    reset();
    openGroup([&](QDialog& dialog) {
        configureList(dialog, "", "Validation");
        auto* input = dialog.findChild<QLineEdit*>("layerGroupLayers");
        auto* ok    = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        for (const QString& invalid : {"", " ", "0", "-1", "41", "2-41", "8-5", "1.5", "1,,2", "1,", ",1", "1 2",
                                       "1-2-3", "1;2", "a", "1, a", "1-", "2147483648", "1-99999999999999999999"}) {
            input->setText(invalid);
            expect(!ok->isEnabled(), "Malformed, reversed, overflowing, or unavailable layers must disable OK.");
            expect(part->getSettingsRanges().isEmpty(), "Invalid input must not create any group members.");
        }
        input->setText(" 9, 2 - 4, 3, 9, 4-4 ");
        expect(ok->isEnabled(), "Whitespace, unsorted values, overlaps, and singleton ranges must be accepted.");
        ok->click();
    });
    expect(members("Validation") == QSet<int> {2, 3, 4, 9} && part->getSettingsRanges().size() == 4,
           "Repeated and overlapping list entries must produce one member per layer.");
    const auto before_cancel = part->SettingsRangesToJson();
    openGroup([&](QDialog& dialog) { configureList(dialog, "1-40", "Cancelled"); });
    expect(part->SettingsRangesToJson() == before_cancel, "Cancelling a valid list must leave settings unchanged.");
    openGroup([&](QDialog& dialog) {
        configureList(dialog, "2, 10", "Available layers");
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    expect(members("Validation") == QSet<int> {2, 3, 4, 9} && members("Available layers") == QSet<int> {10},
           "Lists must preserve existing settings at occupied layers, matching interval groups.");

    reset();
    openGroup([&](QDialog& dialog) {
        configureList(dialog, "40", "Last layer");
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    expect(members("Last layer") == QSet<int> {40}, "A list may contain just the last layer.");
    reset();
    openGroup([&](QDialog& dialog) {
        configureList(dialog, "invalid", "Interval group");
        radioButton(dialog, "Start/end/interval")->click();
        auto spins = dialog.findChildren<QDoubleSpinBox*>();
        expect(spins.size() == 3, "Existing interval controls must remain available.");
        if (spins.size() != 3) return;
        spins[0]->setValue(2);
        spins[1]->setValue(10);
        spins[2]->setValue(2);
        auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        expect(ok->isEnabled(), "An inactive invalid list must not block interval groups.");
        ok->click();
    });
    expect(members("Interval group") == QSet<int> {2, 5, 8},
           "Start/end/interval must retain its existing skip-count semantics.");
    for (bool even : {false, true}) {
        reset();
        openGroup([&](QDialog& dialog) {
            if (even) radioButton(dialog, "Select all even layers.")->click();
            dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        QSet<int> expected;
        for (int i = even ? 2 : 1; i <= 40; i += 2) expected.insert(i);
        expect(members(even ? "Even Layers" : "Odd Layers") == expected,
               "Even selection and default odd selection must retain their behavior.");
    }
    model->clearItems();
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
