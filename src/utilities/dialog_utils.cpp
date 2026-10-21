#include "utilities/dialog_utils.h"

#include <QApplication>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QObject>
#include <QScreen>
#include <QWidget>
#include <QWindow>
#include <algorithm>

#include "managers/session_manager.h"
#include "part/part.h"

namespace ORNL {
namespace DialogUtils {

namespace {

QWidget* resolveHostWindow(QWidget* parent) {
    QWidget* top_window = parent;
    while (top_window != nullptr && top_window->isWindow() && top_window->windowType() == Qt::Popup) {
        top_window = top_window->parentWidget();
    }
    if (top_window != nullptr) { return top_window->window(); }
    return QApplication::activeWindow();
}

}  // namespace

void positionOnParentScreen(QWidget* widget, QWidget* parent) {
    if (widget == nullptr) return;

    QWidget* top_window    = resolveHostWindow(parent);
    QScreen* target_screen = nullptr;

    if (top_window != nullptr) {
        target_screen = QGuiApplication::screenAt(top_window->frameGeometry().center());
        if (target_screen == nullptr) { target_screen = top_window->screen(); }
    }
    if (target_screen == nullptr) { target_screen = QGuiApplication::primaryScreen(); }

    if (target_screen != nullptr) {
        widget->winId();
        if (widget->windowHandle() != nullptr) { widget->windowHandle()->setScreen(target_screen); }
    }

    if (top_window != nullptr || target_screen != nullptr) {
        widget->adjustSize();
        const QRect ref_rect = top_window != nullptr ? top_window->frameGeometry() : target_screen->availableGeometry();
        const QSize dlg_size = widget->sizeHint().expandedTo(widget->size());
        QPoint pos           = ref_rect.center() - QPoint(dlg_size.width() / 2, dlg_size.height() / 2);

        if (target_screen != nullptr) {
            const QRect avail = target_screen->availableGeometry();
            pos.setX(std::clamp(pos.x(), avail.left(),
                                std::max(avail.left(), avail.left() + avail.width() - dlg_size.width())));
            pos.setY(std::clamp(pos.y(), avail.top(),
                                std::max(avail.top(), avail.top() + avail.height() - dlg_size.height())));
        }

        widget->move(pos);
    }
}

QString getText(QWidget* parent, const QString& title, const QString& label, QLineEdit::EchoMode mode,
                const QString& text, bool* ok) {
    QWidget* dialog_parent = resolveHostWindow(parent);
    QInputDialog dialog(dialog_parent);
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setTextEchoMode(mode);
    dialog.setTextValue(text);

    positionOnParentScreen(&dialog, dialog_parent);

    const bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok != nullptr) { *ok = accepted; }
    return accepted ? dialog.textValue() : QString();
}

QString promptForName(QWidget* parent, const QString& title, const QString& current_name,
                      QSharedPointer<Part> current_part) {
    bool ok = false;
    QString name;
    QString label = "Enter a Name:";

    while (name.isEmpty()) {
        name = getText(parent, title, label, QLineEdit::Normal, current_name, &ok);

        if (!ok) break;

        QSharedPointer<Part> existing = CSM->getPart(name);
        if (existing != nullptr && existing != current_part) {
            label = "Name already in use. Please enter another:";
            name  = "";
        }
    }

    return name;
}

double promptForDouble(QWidget* parent, const QString& label_text, const QString& unit_text, double unit_conversion,
                       bool& ok, std::optional<double> min_value) {
    QString str_value;
    QString label = label_text + "(" + unit_text + "):";
    ok            = false;
    double value  = 0.0;

    while (str_value.isEmpty()) {
        str_value = getText(parent, QObject::tr("Input"), label, QLineEdit::Normal, "", &ok);

        if (!ok) return 0.0;

        bool conv_ok = false;
        value        = str_value.toDouble(&conv_ok);

        const bool out_of_range = min_value.has_value() && (value < *min_value);

        if (!conv_ok || out_of_range) {
            const QString condition =
                min_value.has_value() ? ("a number >= " + QString::number(*min_value)) : "a valid number";
            label     = "Error: value must be " + condition + ". " + label_text + "(" + unit_text + "):";
            str_value = "";
        }
    }

    return value * unit_conversion;
}

int promptForInt(QWidget* parent, const QString& label_text, const QString& unit_text, bool& ok,
                 std::optional<int> min_value) {
    QString str_value;
    QString label = label_text + "(" + unit_text + "):";
    ok            = false;
    int value     = 0;

    while (str_value.isEmpty()) {
        str_value = getText(parent, QObject::tr("Input"), label, QLineEdit::Normal, "", &ok);

        if (!ok) return 0;

        bool conv_ok = false;
        value        = str_value.toInt(&conv_ok);

        const bool out_of_range = min_value.has_value() && (value < *min_value);

        if (!conv_ok || out_of_range) {
            const QString condition =
                min_value.has_value() ? ("an integer >= " + QString::number(*min_value)) : "a valid integer";
            label     = "Error: value must be " + condition + ". " + label_text + "(" + unit_text + "):";
            str_value = "";
        }
    }

    return value;
}

}  // namespace DialogUtils
}  // namespace ORNL
