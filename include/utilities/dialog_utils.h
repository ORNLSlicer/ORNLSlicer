#pragma once

#include <QSharedPointer>
#include <QString>

class QWidget;

namespace ORNL {
class Part;

namespace DialogUtils {

//! \brief Prompts the user for a part or mesh name with duplicate validation.
//!        Re-prompts if the entered name is already used by a different part.
//! \param parent The parent widget for the modal input dialog.
//! \param title The window title for the dialog.
//! \param current_name The initial text pre-filled in the dialog (used when renaming to allow keeping the same name).
//! \param current_part The part being renamed (optional), so keeping its own name is not treated as a collision.
//! \return The name entered by the user, or empty if the user canceled.
QString promptForName(QWidget* parent, const QString& title, const QString& current_name = "",
                      QSharedPointer<Part> current_part = nullptr);

}  // namespace DialogUtils
}  // namespace ORNL
