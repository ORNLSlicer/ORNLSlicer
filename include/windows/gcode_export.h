#pragma once

#include <QCheckBox>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>
#include <qboxlayout.h>
#include <qhashfunctions.h>
#include <qtmetamacros.h>

#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class GcodeExport
 * \brief Window that allows users to export gcode and project information
 */
class GcodeExport : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    explicit GcodeExport(QWidget* parent);

    //! \brief Sets default name for file based on part control widget
    //! \param name: name of top-most part in control widget when window is opened
    void setDefaultName(QString name);

    //! \brief Destructor
    ~GcodeExport();

  public slots:
    //! \brief Handler to receive necessary information once the slice is complete and
    //! gcode generated.  Necessary to add final header comments if user opts to supply any
    //! \param tempLocation current location of temporary gcode file
    //! \param meta Most recent meta used
    void updateOutputInformation(QString tempLocation, GcodeMeta meta);

  protected:
    //! \brief Close event override to reset form values
    void closeEvent(QCloseEvent* event);

  private slots:
    //! \brief Handler for export button click
    void exportGcode();

    //! \brief Handler to show completion message at the end of export
    //! \param path Path file was saved to
    //! \param filename Name of the file saved
    void showComplete(QString path, QString filename);

  private:
    //! \brief Layout for widget
    QVBoxLayout* m_layout;

    //! \brief UI components in widget
    QLineEdit* m_operator_input;
    QTextEdit* m_description_input;
    QCheckBox* m_gcode_file_checkbox;
    QCheckBox* m_auxiliary_file_checkbox;
    QCheckBox* m_project_file_checkbox;
    QCheckBox* m_bundle_files_checkbox;

    //! \brief Most recently used meta
    GcodeMeta m_most_recent_meta;

    //! \brief The location of the current temporary file that holds the sliced gcode
    QString m_location;

    //! \brief Default file name
    QString m_default_name;

}; // class GcodeExport
} // namespace ORNL
