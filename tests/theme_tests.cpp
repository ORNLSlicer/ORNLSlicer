#include <QApplication>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QOpenGLContext>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "graphics/base_view.h"
#include "managers/preferences_manager.h"
#include "test_utils.h"
#include "windows/flowratecalc.h"
#include "windows/xtrudecalc.h"

namespace {
class ThemeView : public ORNL::BaseView {
    void initView() override {}
    void handleLeftClick(QPointF) override {}
};

QLabel* findLabel(QWidget& widget, const QString& prefix) {
    for (auto* label : widget.findChildren<QLabel*>())
        if (label->text().startsWith(prefix)) return label;
    return nullptr;
}

double luminance(const QColor& color) {
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

double contrast(const QColor& first, const QColor& second) {
    const double a = luminance(first);
    const double b = luminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}
}  // namespace

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QSurfaceFormat format;
    format.setVersion(4, 4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName("ornlslicer-theme-tests");
    QApplication::setStyle(QStyleFactory::create("fusion"));
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(shaders);

    bool passed       = true;
    const auto expect = [&passed](bool condition, const auto& message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const auto preferences = ORNL::PreferencesManager::getInstance();
    QApplication::setPalette(preferences->getTheme().getPalette());
    QTemporaryDir directory;
    if (!ORNL::Testing::expect(directory.isValid(), "Could not create a temporary directory.")) return EXIT_FAILURE;
    QWidget parent;
    ORNL::FlowrateCalcWindow flowrate(&parent);
    ORNL::XtrudeCalcWindow xtrude(&parent);
    QFileDialog dialog;
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setDirectory(directory.path());
    flowrate.show();
    xtrude.show();
    dialog.show();
    auto* flow_error   = findLabel(flowrate, "Please fill out");
    auto* xtrude_error = findLabel(xtrude, "Please fill out");
    auto* hint         = findLabel(xtrude, "(You can also use the FPR");
    if (!ORNL::Testing::expect(flow_error != nullptr && xtrude_error != nullptr && hint != nullptr,
                               "Calculators should expose their validation and help text."))
        return EXIT_FAILURE;

    ThemeView view;
    int iteration = 0;
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode, ORNL::ThemeName::kLightMode}) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        view.setupStyle();
        app.processEvents();
        expect(view.context() == nullptr, "Theme changes before a view is shown must not require an OpenGL context.");
        const bool dark = theme == ORNL::ThemeName::kDarkMode;
        for (auto* widget :
             {static_cast<QWidget*>(&flowrate), static_cast<QWidget*>(&xtrude), static_cast<QWidget*>(&dialog)}) {
            expect(widget->parentWidget() == nullptr, "The regression should cover standalone windows.");
            for (auto group : {QPalette::Active, QPalette::Inactive}) {
                const auto palette = widget->palette();
                expect(dark ? palette.color(group, QPalette::Window).lightness() < 128
                            : palette.color(group, QPalette::Window).lightness() > 200,
                       "Standalone windows should follow the selected theme.");
                for (auto roles :
                     {qMakePair(QPalette::WindowText, QPalette::Window), qMakePair(QPalette::Text, QPalette::Base),
                      qMakePair(QPalette::ButtonText, QPalette::Button),
                      qMakePair(QPalette::HighlightedText, QPalette::Highlight)}) {
                    const QColor foreground = palette.color(group, roles.first);
                    const QColor background = palette.color(group, roles.second);
                    expect(contrast(foreground, background) >= 4.5,
                           QString("Theme %1 role %2 in group %3 has insufficient contrast: %4 on %5.")
                               .arg(static_cast<int>(theme))
                               .arg(roles.first)
                               .arg(group)
                               .arg(foreground.name(), background.name())
                               .toStdString());
                }
            }
        }
        for (auto* label : {flow_error, xtrude_error, hint}) {
            const auto palette = label->palette();
            expect(contrast(palette.color(label->foregroundRole()), palette.color(QPalette::Window)) >= 4.5,
                   "Calculator status and help text should have at least 4.5:1 contrast.");
        }
        if (argc > 1) {
            const QString prefix = QString(argv[1]) + "/" + QString::number(iteration++);
            expect(flowrate.grab().save(prefix + "-flow.png"), "Could not save the flowrate screenshot.");
            expect(xtrude.grab().save(prefix + "-xtrude.png"), "Could not save the Xtrude screenshot.");
            expect(dialog.grab().save(prefix + "-dialog.png"), "Could not save the dialog screenshot.");
        }
    }

    // The default offscreen platform cannot render QOpenGLWidget; XCB permits a real-context check under Xvfb.
    if (QApplication::platformName() == "xcb") {
        view.resize(200, 160);
        for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode, ORNL::ThemeName::kLightMode}) {
            preferences->setTheme(theme);
            view.setupStyle();
            expect(QOpenGLContext::currentContext() == nullptr, "A hidden view should only queue a theme repaint.");
            view.show();
            app.processEvents();
            if (!ORNL::Testing::expect(view.isValid(), "The render check requires an initialized OpenGL view."))
                return EXIT_FAILURE;
            const QImage frame  = view.grabFramebuffer();
            const auto color    = preferences->getTheme().getBgColor();
            const auto expected = QColor::fromRgbF(color[0], color[1], color[2], color[3]);
            expect(!frame.isNull(), "The themed view should render a framebuffer.");
            if (!frame.isNull()) {
                const QColor actual = frame.pixelColor(0, 0);
                expect(std::abs(actual.red() - expected.red()) <= 1 &&
                           std::abs(actual.green() - expected.green()) <= 1 &&
                           std::abs(actual.blue() - expected.blue()) <= 1,
                       "The first frame after showing a view should use the latest theme background.");
            }
            view.hide();
            view.doneCurrent();
        }
    }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
