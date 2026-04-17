#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qobject.h>
#include <qset.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "widgets/programmatic_check_box.h"

namespace ORNL {
/*!
 * \class TemplateSaveDialog
 * \brief Allows a group of settings to be saved in the settings manager.
 */
class TemplateSaveDialog : public QDialog {
    Q_OBJECT
  public:
    //! \brief Constructor.
    explicit TemplateSaveDialog(QWidget* parent = nullptr);

    //! \brief Get the keys from the dialog.
    QStringList& keys();

    //! \brief Get the filename from the dialog.
    QString& filename();

    //! \brief Get the optional name from the dialog.
    QString name();

  private slots:
    //! \brief Checks all items in a selection.
    void checkSelection(QTableWidgetItem* item);

    //! \brief File dialog for selecting a file.
    void fileDialog();

    //! \brief Search for a setting.
    void filter(QString str);

    //! \brief Accept the changes.
    void accept() override;

    //! \brief Update checkbox status
    void updateSelection();

  private:
    //! \brief Setup the static widgets and their layouts.
    void setupUi();

    //! \brief 1. Setup the window properties.
    void setupWindow();

    //! \brief 2. Setup the widgets.
    void setupWidgets();

    //! \brief 3. Setup the layouts and insert their children.
    void setupLayouts();

    //! \brief 4. Setup the table from the master json.
    void setupTable();

    //! \brief 5. Setup the insertions for all UI elements.
    void setupInsert();

    //! \brief 6. Setup the events for the various widgets.
    void setupEvents();

    //! \brief 7. check if there is at least one item checked.
    bool itemChecked();

    //! \brief Converts raw values for specific types to preferred unit
    //! \param type: type of value
    //! \param value: raw value
    //! \return Converted value
    QString convertRawValue(QString type, double value);

    //! \brief Map of display name to underlying key.
    QMap<QString, QString> m_keys_dis;

    //! \brief Major categories.
    QSet<QString> m_maj;

    //! \brief Copy of boolean values for all settings to easily determine
    //! if they should be added or not
    QHash<QString, QHash<QString, bool>> m_setting_status;

    // Table
    QTableWidget* m_table;

    // Layout
    QGridLayout* m_layout;
    QGridLayout* m_file_layout;

    // Container
    QWidget* m_file_container;

    // Button container
    QDialogButtonBox* m_button_container;

    // Widgets
    QLabel* m_dir_label;
    QLabel* m_sel_label;
    QLabel* m_save_label;
    QLabel* m_name_label;
    QToolButton* m_browse_btn;
    QLineEdit* m_search;
    QLineEdit* m_edit;
    QLineEdit* m_name_edit;

    //! \brief Result filename.
    QString m_filename;

    //! \brief Result keys.
    QStringList m_keys;

    //! \brief Check boxes to control categories
    QHash<QString, ProgrammaticCheckBox*> m_category_check_boxes;
    ProgrammaticCheckBox* m_all_check_box;

    //! \brief List of types to convert to user preferred unit
    QList<QString> m_convertable_types;
};
} // namespace ORNL
