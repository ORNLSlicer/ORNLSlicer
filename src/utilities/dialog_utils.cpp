#include "utilities/dialog_utils.h"

#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QObject>
#include <QWidget>

#include "managers/session_manager.h"
#include "part/part.h"

namespace ORNL {
namespace DialogUtils {

QString promptForName(QWidget* parent, const QString& title, const QString& current_name,
                      QSharedPointer<Part> current_part) {
    bool ok = false;
    QString name;
    QString label = "Enter a Name:";

    while (name.isEmpty()) {
        name = QInputDialog::getText(parent, title, label, QLineEdit::Normal, current_name, &ok);
        if (!ok) break;

        QSharedPointer<Part> existing = CSM->getPart(name);
        if (existing != nullptr && existing != current_part) {
            label = "Name already in use. Please enter another:";
            name  = "";
        }
    }

    return name;
}

double promptForDouble(QWidget* parent, const QString& label_text, const QString& current_text,
                       const QString& unit_text, double unit_conversion, std::optional<double> min_value) {
    QString str_value;
    QString label = label_text + "(" + unit_text + "):";
    bool ok       = false;
    double value  = 0.0;

    while (str_value.isEmpty()) {
        str_value = QInputDialog::getText(parent, label_text, label, QLineEdit::Normal, current_text, &ok);
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

int promptForInt(QWidget* parent, const QString& label_text, const QString& current_text, const QString& unit_text,
                 std::optional<int> min_value) {
    QString str_value;
    QString label = label_text + "(" + unit_text + "):";
    bool ok       = false;
    int value     = 0;

    while (str_value.isEmpty()) {
        str_value = QInputDialog::getText(parent, label_text, label, QLineEdit::Normal, current_text, &ok);
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
