#include "utilities/dialog_utils.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QObject>

#include "managers/session_manager.h"
#include "part/part.h"

namespace ORNL {
namespace DialogUtils {

QString promptForName(QWidget* parent, const QString& title, const QString& current_name,
                      QSharedPointer<Part> current_part) {
    bool ok;
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

}  // namespace DialogUtils
}  // namespace ORNL
