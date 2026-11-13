#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QList>
#include <QMenu>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/theme_tool.h"
#include "widgets/main_toolbar.h"

namespace {
double luminance(const QColor& color) {
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

QColor buttonBackground(ORNL::MainToolbar& toolbar, QAction* action) {
    const QImage button = toolbar.widgetForAction(action)->grab().toImage();
    return button.pixelColor(button.width() / 2, 2);
}

bool checkIconContrast(ORNL::MainToolbar& toolbar) {
    bool passed = true;
    int count   = 0;
    for (QAction* action : toolbar.actions()) {
        if (action->icon().isNull() || action->text() == "Show Segment Info") continue;
        ++count;
        const double background = luminance(buttonBackground(toolbar, action));
        double enabled_contrast = 0.0;
        for (auto mode : {QIcon::Normal, QIcon::Disabled}) {
            const QImage icon =
                action->icon().pixmap(QSize(24, 24), mode, action->isChecked() ? QIcon::On : QIcon::Off).toImage();
            double foreground = 0.0;
            int opaque_pixels = 0;
            for (int y = 0; y < icon.height(); ++y) {
                for (int x = 0; x < icon.width(); ++x) {
                    const QColor pixel = icon.pixelColor(x, y);
                    if (pixel.alpha() < 240) continue;
                    foreground += luminance(pixel);
                    ++opaque_pixels;
                }
            }
            if (!ORNL::Testing::expect(opaque_pixels > 0, "Toolbar icons must contain visible pixels.")) {
                passed = false;
                continue;
            }
            foreground /= opaque_pixels;
            const double contrast =
                (std::max(foreground, background) + 0.05) / (std::min(foreground, background) + 0.05);
            if (mode == QIcon::Normal)
                enabled_contrast = contrast;
            else {
                passed &= ORNL::Testing::expect(contrast <= enabled_contrast * 0.5,
                                                QString("%1 disabled icon should be clearly muted relative to enabled.")
                                                    .arg(action->text())
                                                    .toStdString());
            }
            passed &= ORNL::Testing::expect(contrast >= (mode == QIcon::Normal ? 4.5 : 1.5),
                                            QString("%1 icon mode %2 has insufficient contrast: %3.")
                                                .arg(action->text())
                                                .arg(mode)
                                                .arg(contrast)
                                                .toStdString());
        }
    }
    return ORNL::Testing::expect(count == 10, "Check every monochrome toolbar icon.") && passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));

    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    bool passed            = ORNL::Theme::initializeFonts();
    const auto preferences = ORNL::PreferencesManager::getInstance();
    const auto apply_theme = [&preferences](QWidget& parent, ORNL::ThemeName theme) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        QFile style(preferences->getTheme().getFolderPath() + "style.qss");
        if (!style.open(QIODevice::ReadOnly)) return false;
        parent.setStyleSheet(style.readAll());
        return true;
    };
    for (const char* resource :
         {":/icons/file_black.png", ":/icons/translate_black.png", ":/styles/qdarkstyle/rc/up_arrow.png",
          ":/styles/qdarkstyle/rc/down_arrow.png", ":/styles/qdarkstyle/rc/right_arrow.png"}) {
        passed &= ORNL::Testing::expect(!QImage(QString::fromUtf8(resource)).isNull(),
                                        QString("Bundled GUI image must decode: %1").arg(resource).toStdString());
    }

    QWidget startup_parent;
    startup_parent.resize(ORNL::Constants::UI::MainWindow::kViewWidgetSize);
    passed &= apply_theme(startup_parent, ORNL::ThemeName::kDarkMode);

    ORNL::MainToolbar startup_toolbar(&startup_parent);
    startup_toolbar.resize(startup_parent.size());
    startup_parent.show();
    app.processEvents();
    passed &= checkIconContrast(startup_toolbar);
    QAction* slicing_action = nullptr;
    QAction* load_action    = nullptr;
    for (auto* action : startup_toolbar.actions()) {
        if (action->text() == "Show Slicing Geometry") slicing_action = action;
        if (action->text() == "Load Model") load_action = action;
    }
    if (!ORNL::Testing::expect(slicing_action != nullptr && load_action != nullptr,
                               "The toolbar must expose slicing geometry and load commands."))
        return EXIT_FAILURE;
    slicing_action->setChecked(true);
    app.processEvents();
    passed &= checkIconContrast(startup_toolbar);

    QAction* export_action = nullptr;
    for (auto* action : startup_toolbar.actions()) {
        if (action->text() == "Export G-code") export_action = action;
    }
    if (!ORNL::Testing::expect(export_action != nullptr, "The toolbar must expose the G-Code export command."))
        return EXIT_FAILURE;
    startup_toolbar.setExportAbility(true);
    startup_toolbar.setView(1);
    passed &= ORNL::Testing::expect(export_action->isEnabled(), "Completed G-Code must remain exportable.");
    startup_toolbar.setExportAbility(false);
    startup_toolbar.setView(0);
    startup_toolbar.setView(1);
    passed &= ORNL::Testing::expect(!export_action->isEnabled(),
                                    "Switching views after a DLP slice must not re-enable stale G-Code export.");
    startup_toolbar.setView(0);

    int iteration = 0;
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode, ORNL::ThemeName::kLightMode}) {
        passed &= apply_theme(startup_parent, theme);
        const auto actions = startup_toolbar.actions();
        QList<bool> checked, enabled;
        for (auto* action : actions) {
            checked.append(action->isChecked());
            enabled.append(action->isEnabled());
        }
        startup_toolbar.setupStyle();
        app.processEvents();
        passed &= checkIconContrast(startup_toolbar);
        for (int i = 0; i < actions.size(); ++i) {
            if (actions[i]->text() == "Show Segment Info") {
                passed &= ORNL::Testing::expect(actions[i]->icon().pixmap(QSize(24, 24)).toImage() ==
                                                    QIcon(":/icons/info.png").pixmap(QSize(24, 24)).toImage(),
                                                "Theme changes must preserve the colored information icon.");
            }
            passed &=
                ORNL::Testing::expect(actions[i]->isChecked() == checked[i] && actions[i]->isEnabled() == enabled[i],
                                      "Theme changes must preserve toolbar action states.");
        }
        for (int view : {0, 1}) {
            startup_toolbar.setView(view);
            app.processEvents();
            passed &= checkIconContrast(startup_toolbar);
            passed &= ORNL::Testing::expect(slicing_action->isChecked() && slicing_action->isEnabled() == (view == 0),
                                            "Switching views must preserve the checked state and update availability.");
            const bool neutral_background =
                buttonBackground(startup_toolbar, slicing_action) == buttonBackground(startup_toolbar, load_action);
            passed &= ORNL::Testing::expect(view == 0 ? !neutral_background : neutral_background,
                                            "Only enabled checked buttons should retain their active background.");
            if (argc > 1) {
                passed &= ORNL::Testing::expect(
                    startup_toolbar.grab().save(QString("%1/%2-view-%3.png").arg(argv[1]).arg(iteration).arg(view)),
                    "Could not save the toolbar screenshot.");
            }
        }
        ++iteration;
    }

    for (QAction* action : startup_toolbar.actions()) {
        if (action->isSeparator()) continue;

        QWidget* action_widget = startup_toolbar.widgetForAction(action);
        passed &= ORNL::Testing::expect(action_widget != nullptr && action_widget->isVisible(),
                                        QString("The %1 toolbar item should be visible at the startup view width.")
                                            .arg(action->text())
                                            .toStdString());
    }

    QToolButton* startup_extension_button = startup_toolbar.findChild<QToolButton*>("qt_toolbar_ext_button");
    passed &= ORNL::Testing::expect(startup_extension_button != nullptr && !startup_extension_button->isVisible(),
                                    "The startup view width should display every toolbar command without overflow.");

    QWidget parent;
    parent.resize(360, 200);

    ORNL::MainToolbar toolbar(&parent);
    toolbar.resize(parent.size());
    parent.show();
    app.processEvents();

    const QList<QAction*> actions = toolbar.actions();
    int widget_action_count       = 0;
    QAction* slice_action         = nullptr;
    for (QAction* action : actions) {
        if (qobject_cast<QWidgetAction*>(action) != nullptr) ++widget_action_count;
        if (action->text() == "SLICE") slice_action = action;
    }

    passed &= ORNL::Testing::expect(widget_action_count == 1,
                                    "Only the view tabs should require a QWidgetAction; toolbar commands must be "
                                    "QActions so Qt can place them in the overflow menu.");
    passed &= ORNL::Testing::expect(slice_action != nullptr, "The slice command should be exposed as an action.");

    QToolButton* extension_button = toolbar.findChild<QToolButton*>("qt_toolbar_ext_button");
    passed &= ORNL::Testing::expect(extension_button != nullptr && extension_button->isVisible(),
                                    "A constrained toolbar should display its overflow button.");

    if (slice_action != nullptr) {
        QWidget* slice_button = toolbar.widgetForAction(slice_action);
        passed &=
            ORNL::Testing::expect(slice_button != nullptr && !slice_button->isVisible(),
                                  "The constrained toolbar should move the trailing slice command into overflow.");

        if (extension_button != nullptr) {
            QMenu* overflow_menu = extension_button->menu();
            passed &= ORNL::Testing::expect(overflow_menu != nullptr && overflow_menu->actions().contains(slice_action),
                                            "The overflow menu should expose the hidden slice command.");
            if (overflow_menu != nullptr) overflow_menu->hide();
        }
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
