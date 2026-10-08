#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QList>
#include <QMenu>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <cstdlib>

#include "test_utils.h"
#include "widgets/main_toolbar.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    QWidget parent;
    parent.resize(360, 200);

    ORNL::MainToolbar toolbar(&parent);
    toolbar.resize(parent.size());
    parent.show();
    app.processEvents();

    bool passed = true;

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
