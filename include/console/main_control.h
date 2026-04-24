#pragma once

#include <qhashfunctions.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "gcode/gcode_meta.h"
#include "managers/settings/settings_manager.h"
#include "utilities/enums.h"

namespace ORNL {

/*!
 * \class MainControl
 * \brief The main control thread when running from console.
 */
class MainControl : public QObject {
    Q_OBJECT

  public:
    //! \brief Constructor
    //! \param options: command line options that were successfully processed
    MainControl(QSharedPointer<SettingsBase> options);

  signals:
    //! \brief Preprocessing is done.  STL's have been loaded.
    void preProcess();

    //! \brief Thread has complete. Signal for main to exit message handler.
    void finished();

  public slots:

    //! \brief Starts slicing
    void run();

    //! \brief Continue with process once security has been checked.
    void continueStartup();

  private slots:
    //! \brief Notification as to the number of parts contained in project file
    //! \param total: part total
    void partsInProject(int total);

    //! \brief Stl's have been loaded.
    void loadComplete();

    //! \brief Slicing is complete
    //! \param filepath: path to temp gcode file
    //! \param alterFile: whether or not the file is able to be altered during parsing
    void sliceComplete(QString filepath, bool alterFile);

    //! \brief Update required info for export
    //! \brief tempLocation: location of temp file
    //! \brief meta: selected meta used for tempLocation's generation
    void updateOutputInformation(QString tempLocation, GcodeMeta meta);

    //! \brief Display progress on console
    //! \brief type: current step of slicing process
    //! \brief percentage: current completion percentage
    void displayProgress(StatusUpdateStepType type, int percentage);

    //! \brief Parsing of gcode is complete.  Must still parse to allow layer time adjustments.
    void gcodeParseComplete();

  private:
    //! \brief path's to input, output, and temporary locations
    QString m_input_path, m_output_path, m_temp_location;

    //! \brief Most recently used meta
    GcodeMeta m_selected_meta;

    //! \brief SettingsBase holding all successfully processed command line options
    QSharedPointer<SettingsBase> m_options;

    //! \brief Last step of slicing process seen.  Needed to format console output.
    StatusUpdateStepType m_last_step_type;

    //! \brief Number of parts remaining to load if more than one specified.
    int m_parts_to_load;
};
} // namespace ORNL
