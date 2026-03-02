#pragma once

#include "QQueue"
#include "QThread"
#include "data_stream.h"
#include "external_files/exporters/rpbf_exporter.h"
#include "gcode/gcode_parser.h"
#include "tcp_connection.h"
#include "threading/abs_slicing_thread.h"
#include "threading/step_thread.h"
#include "utilities/enums.h"

namespace ORNL {
class Step;
class Part;

/*!
 * \class RealTimeAST
 * \brief Defines the abstract implementation of the real slicing thread.
 */
class RealTimeAST : public AbstractSlicingThread {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param output_location the location to output gcode
    RealTimeAST(QString output_location);

  public slots:
    //! \brief Main function that starts slice.
    void doSlice() override;

  signals:
    //! \brief Signal to the active steps to begin computation.
    void stepStart();

    void startPrint();

  protected slots:
    //! \brief Upon completion of thread running step object, this slot will clean up the thread.
    //!        If more objects are on the queue, then this will run the thread
    void cleanThread() override;

    //! \brief sends current gcode to remote and saves to file
    virtual void sendGCode();

    //! \brief sends the startup command to the printer telling it to prepare
    virtual void sendStartup();

    //! \brief sends the shutdown command to the printer telling it to shutdown after printing
    virtual void sendShutdown();

  protected:
    //! \brief writes setup info to global variable
    virtual void writeGCodeSetup();

    //! \brief writes shutdown info to global variable
    virtual void writeGCodeShutdown();

    //! \brief skips processing up to a layer number
    //! \param layer_num layer number to skip to
    virtual void skip(int layer_num) = 0;

    //! \brief saves a recovery file with current slicing info to file
    virtual void saveRecoveryFile();

    //! \brief handles responses from Sensor Control 2 and controls the next state of the thread;
    void handleRemoteMessage();

    //! \brief Queue of steps to be processed.
    QQueue<QSharedPointer<Step>> m_step_queue;

    //! \brief a status flag that indicates if more cross sections are left for processing
    bool m_cross_section_generated = true;

    //! \brief number of global steps that have been computed
    int m_steps_done = 0;

    //! \brief a two-way connection to a remote
    DataStream* m_data_stream = nullptr;

    //! \brief holds gcode as it is written for a layer
    QString m_gcode_output;

  private:
    //! \brief runs single time when the slicing process is started before any layers are computed
    virtual void initialSetup();

    //! \brief starts TCP server thread and setups signals to handle new connections
    void setupNetworking();

    //! \brief computes the next layer and add it to the part
    //! \param data optional data to process on
    //! \note data is sent from remote between calls
    void processNext(nlohmann::json data = nlohmann::json());

    //! \brief Step threads to run.
    QVector<StepThread*> m_step_threads;

    //! \brief Start size of processing queue.  Used to avoid evaluting .size repeatedly
    int m_queue_start_size;

    //! \brief a flag to track if the slicing process needs to be continued based on if there are more layers to slice
    bool m_should_continue = false;

    //! \brief a flag to signal that this thread is waiting for new data before it continues
    bool m_waiting_for_data = true;

    //! \brief the TCP connection that connects to Sensor Control 2
    TCPConnection* m_tcp_connection = nullptr;

    //! \brief optional exporter used to save RPBF files
    RPBFExporter* m_RPBF_exporter = nullptr;

    //! \brief the mode of real time slicing
    RealTimeSlicingMode m_mode = RealTimeSlicingMode::kClosedLoop;

    //! \brief Recovery info
    nlohmann::json m_recovery;

    QString m_printer_name;

    const quint32 MAX_NUMBER_OF_CONNECTION_RETIES = 10;

    int m_connection_attempts = 0;

    //! \brief the real time state of the thread
    //!        The states are:
    //!         0: Sent startup request, waiting for response before continuing
    //!         1: Sent gcode, waiting for response before continuing
    //!         2: Sent sync request, waiting for response (Sensor control to sync with printer step i.e. layer done)
    //!         before continuing 3: Sent sensor request, waiting for response before continuing
    //!         4. Send footer-gcode, waiting for response before continuing
    //!         5. Sent shutdown request, waiting for response before continuing
    //! \note the commands 1 -> 2 -> 3 are repeated for every layer
    int m_state = 0;

    // Sensor control 2 commands
    const QString m_start_command = "M1051";
    const QString m_pause_command = "M1052";
    const QString m_end_command = "M1053";
    const QString m_queue_gcode_command = "M1054";
    const QString m_sync_commmand = "M1055";
    const QString m_report_sensor_readings_commmand = "M1056";
};
} // namespace ORNL
