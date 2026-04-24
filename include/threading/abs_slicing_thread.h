#pragma once

#include <QElapsedTimer>
#include <QQueue>
#include <QThread>
#include <nlohmann/json_fwd.hpp>
#include <qdir.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "gcode/writers/writer_base.h"
#include "utilities/enums.h"

namespace ORNL {
class Step;
class Part;

/*!
 * \class AbstractSlicingThread
 * \brief Defines the abstract implementation of the slicing thread.
 * \note For more information about the abstract slicing architecture, see the documentation.
 */
class AbstractSlicingThread : public QObject {
    Q_OBJECT
  public:
    //! \brief Constructor
    AbstractSlicingThread(QString outputLocation, bool skipGcode = false);

    //! \brief Destructor
    virtual ~AbstractSlicingThread();

    //! \brief Sets bounds for min/max layers to slice
    //! \param min: Starting layer
    //! \param max: Ending layer
    void setBounds(int min, int max);

    //! \brief Sets gcode output location for subsequent slice
    //! \param output: File location
    void setGcodeOutput(QString output);

    //! \brief Sets cancel flag
    void setCancel();

    //! \brief Sets whether or not to communicate via tcp server
    //! \param communicate: whether or not to transmit
    void setCommunicate(bool communicate);

    //! \brief Sets network return data (response from client) for processing step
    //! \param stage: processing step to set info for (currently only after gcode generation)
    //! \param data: data to set (currently gcode)
    void setNetworkData(StatusUpdateStepType stage, QString data);

    //! \brief Get the time it took to slice
    qint64 getTimeElapsed();

  public slots:
    //! \brief Main function that starts slice.
    virtual void doSlice() = 0;

    //! \brief Forwards status update information using statusUpdate signal
    //! \param type: The current section of the step being completed
    //! \param completedPercentage: The percentage complete of the current step process
    void forwardStatus(StatusUpdateStepType type, int completedPercentage);

  signals:
    //! \brief Signal to the active steps to begin computation.
    void stepStart();

    //! \brief Signal to the session manager/main window with status update information
    //! \param type: The current section of the step being completed
    //! \param completedPercentage: The percentage complete of the current step process
    void statusUpdate(StatusUpdateStepType type, int completedPercentage);

    //! \brief Signal to session manager that slicing is complete
    void sliceComplete();

    //! \brief Send output from processing stage for tcp transfer
    //! \param type: type of processing step (currently only after gcode generation)
    //! \param msg: message to send (currently only gcode)
    void sendMessage(StatusUpdateStepType type, QString msg);

  protected:
    //! \brief Pure virtual for preprocess step. This is always run first.
    //! \param opt_data optional sensor data
    virtual void preProcess(nlohmann::json opt_data = nlohmann::json()) = 0;

    //! \brief Pure virtual for postprocess step. This is always run just before gcode generation.
    //! \param opt_data optional sensor data
    virtual void postProcess(nlohmann::json opt_data = nlohmann::json()) = 0;

    //! \brief Function that writes setup block of gcode.  For now, all formats have
    //! the same block style.
    //! \param file: File pointer to write gcode to
    //! \param base: WriterBase to do the appropriate gcode writing
    void writeGCodeSetup();

    //! \brief Function that writes the gcode. Implementation can be overridden.
    //! \param file: File pointer to write gcode to
    //! \param base: WriterBase to do the appropriate gcode writing
    virtual void writeGCode() = 0;

    //! \brief Function that writes shutdown block of gcode.  For now, all formats have
    //! the same block style.
    //! \param file: File pointer to write gcode to
    //! \param base: WriterBase to do the appropriate gcode writing
    void writeGCodeShutdown();

    //! \brief Returns max steps among all sliced parts
    int getMaxSteps();

    //! \brief Sets max steps among all sliced parts.  Calculated as part of the child
    //! slicer class
    //! \param steps: Max steps
    void setMaxSteps(int steps);

    //! \brief Used for child classes to access min/max layer to slice
    //! \return Min or max layer
    int getMinBound();
    int getMaxBound();

    //! \brief Accessor for child slicers to see if cancel flag has been set
    bool shouldCancel();

    //! \brief whether or not to communicate results after processing step on tcp server
    //! \return whether or not to communicate
    bool shouldCommunicate();

    //! \brief the output gcode file
    QFile m_temp_gcode_output_file;

    //! \brief the location of the temp gcode file
    QDir m_temp_gcode_dir;

    //! \brief the writer in use by this slicer
    QSharedPointer<WriterBase> m_base;

    //! \brief the syntax in use by this slicer
    GcodeSyntax m_syntax;

    //! \brief Range of steps to compute
    int m_min, m_max;

    //! \brief Max Step Count
    int m_max_steps;

    //! \brief Whether to skip gcode file generation for specific syntaxes
    bool m_skip_gcode;

    //! \brief Timer for tracking time to slice
    QElapsedTimer m_timer;
    qint64 m_elapsed_time = 0;

  protected slots:
    //! \brief Upon completion of thread running step object, this slot will clean up the thread.
    //!        If more objects are on the queue, then this will run the thread
    virtual void cleanThread() = 0;

  private:
    //! \brief Internal thread.
    QThread m_internal_thread;

    //! \brief Cancel flag set by session manager if user clicks on cancel button
    bool m_should_cancel;

    //! \brief whether or not to send processing step output to manager for transmission on tcp server
    bool m_should_communicate;
};
} // namespace ORNL
