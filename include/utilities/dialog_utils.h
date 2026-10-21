#pragma once

#include <QLineEdit>
#include <QSharedPointer>
#include <QString>
#include <optional>

class QWidget;

namespace ORNL {
class Part;

namespace DialogUtils {

//! \brief Opens a modal text input dialog on the window with the application open.
//! \param title The window title for the dialog.
//! \param label The label text shown inside the dialog.
//! \param mode The echo mode for the line edit.
//! \param text The initial pre-filled text.
//! \param ok Optional pointer to a bool set to true if accepted, false if canceled.
//! \param parent Optional parent widget; defaults to the active application window if null.
//! \return The text entered by the user, or empty if canceled.
QString getText(const QString& title, const QString& label, QLineEdit::EchoMode mode = QLineEdit::Normal,
                const QString& text = "", bool* ok = nullptr, QWidget* parent = nullptr);

//! \brief Prompts the user for a part or mesh name with duplicate validation.
//!        Re-prompts if the entered name is already used by a different part.
//! \param parent The parent widget for the modal input dialog.
//! \param title The window title for the dialog.
//! \param current_name The initial text pre-filled in the dialog (used when renaming to allow keeping the same name).
//! \param current_part The part being renamed (optional), so keeping its own name is not treated as a collision.
//! \return The name entered by the user, or empty if the user canceled.
QString promptForName(QWidget* parent, const QString& title, const QString& current_name = "",
                      QSharedPointer<Part> current_part = nullptr);

//! \brief Opens a dialog that asks for a floating-point number to be entered with units.
//! \param parent The parent widget for the modal input dialog.
//! \param label_text The label for the dialog.
//! \param current_text The initial pre-filled text.
//! \param unit_text The text to display for the unit.
//! \param unit_conversion The conversion value to apply to the input.
//! \param min_value Optional minimum allowable value (inclusive). Defaults to std::nullopt (no minimum).
//! \return The converted value (0.0 if canceled).
double promptForDouble(QWidget* parent, const QString& label_text, const QString& current_text,
                       const QString& unit_text, double unit_conversion,
                       std::optional<double> min_value = std::nullopt);

//! \brief Opens a dialog that asks for an integer to be entered.
//! \param parent The parent widget for the modal input dialog.
//! \param label_text The label for the dialog.
//! \param current_text The initial pre-filled text.
//! \param unit_text The text to display for the unit.
//! \param min_value Optional minimum allowable integer value (inclusive). Defaults to std::nullopt (no minimum).
//! \return The integer entered (0 if canceled).
int promptForInt(QWidget* parent, const QString& label_text, const QString& current_text, const QString& unit_text,
                 std::optional<int> min_value = std::nullopt);

}  // namespace DialogUtils
}  // namespace ORNL
