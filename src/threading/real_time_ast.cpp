#include "threading/real_time_ast.h"

#include <algorithm>
#include <limits>

#include <QApplication>
#include <data_stream.h>
#include <nlohmann/json_fwd.hpp>
#include <qabstractsocket.h>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qthread.h>
#include <qtmetamacros.h>
#include <tcp_connection.h>

#include "external_files/exporters/rpbf_exporter.h"
#include "gcode/writers/writer_base.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/step_thread.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
RealTimeAST::RealTimeAST(QString outputLocation) : AbstractSlicingThread(outputLocation) {}

void RealTimeAST::doSlice() {
    connect(this, &RealTimeAST::startPrint, this, &RealTimeAST::sendStartup);

    auto console_settings = GSM->getConsoleSettings();
    if (console_settings != nullptr && console_settings->setting<bool>(Constants::ConsoleOptionStrings::kOpenLoop))
        m_mode = RealTimeSlicingMode::kOpenLoop;

    // Handle recovery info info provided
    int layer_to_start_at = 0;
    QString recovery_file_path;
    if (console_settings != nullptr)
        recovery_file_path = console_settings->setting<QString>(Constants::ConsoleOptionStrings::kRecoveryFilePath);

    // If there is a recovery path, get its information
    if (recovery_file_path != "") {
        QFile recovery_file(recovery_file_path);
        QTextStream recovery_in(&recovery_file);
        if (recovery_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_recovery = nlohmann::json::parse(recovery_in.readAll().toStdString());

            auto layer_restart_itr = m_recovery.find("layer_number");
            if (layer_restart_itr != m_recovery.end() && !layer_restart_itr.value().is_null())
                layer_to_start_at = m_recovery["layer_number"];
        }
        recovery_file.close();
    }

    // If this is the RPBF syntax, also setup the RPBF exporter
    if (m_syntax == GcodeSyntax::kRPBF) {
        if (m_RPBF_exporter != nullptr)
            delete m_RPBF_exporter;

        int global_sector_restart = 1;
        int scan_head_restart = 0;

        auto global_sector_itr = m_recovery.find("global_sector_count");
        if (global_sector_itr != m_recovery.end() && !global_sector_itr.value().is_null())
            global_sector_restart = m_recovery["global_sector_count"];

        auto scan_head_itr = m_recovery.find("scan_head_count");
        if (scan_head_itr != m_recovery.end() && !scan_head_itr.value().is_null())
            scan_head_restart = m_recovery["scan_head_count"];

        QString RPBF_exporter_path = "";
        if (console_settings != nullptr)
            recovery_file_path = console_settings->setting<QString>(Constants::ConsoleOptionStrings::kOutputLocation);
        else
            recovery_file_path = m_temp_gcode_dir.path();

        m_RPBF_exporter =
            new RPBFExporter(recovery_file_path, layer_to_start_at, global_sector_restart, scan_head_restart);
    }

    if (CSM->parts().empty()) {
        qWarning() << "Attempted to start a slice when no data has been loaded.";
        return;
    }

    if (!m_step_threads.isEmpty() || !m_step_queue.isEmpty()) {
        qWarning() << "Background resources are still cleaning up.  Cannot slice.";
        return;
    }

    m_steps_done = (layer_to_start_at == 0) ? 0 : layer_to_start_at - 1;

    // Runs one time before anything is sliced
    this->initialSetup();

    // If we need to fast forward to resume, do that first
    skip(layer_to_start_at);

    if (layer_to_start_at != 0)
        qDebug() << "Resuming print from layer " << QString::number(layer_to_start_at);

    // If there is not already a connection and we are going to use closed loop mode
    if (m_tcp_connection == nullptr && m_mode == RealTimeSlicingMode::kClosedLoop)
        setupNetworking(); // If networking is required, this function will start the slicing process once we are
                           // connected to Sensor Control 2
    else                   // Otherwise we start slicing without networking
        processNext();
}

void RealTimeAST::cleanThread() {
    StepThread* st = qobject_cast<StepThread*>(QObject::sender());

    if (this->shouldCancel()) {
        m_step_threads.removeOne(st);
        if (m_step_threads.isEmpty())
            m_step_queue.clear();
    }
    else {
        // If the queue is empty, then start destroying unused threads.
        if (m_step_queue.empty()) {
            m_step_threads.removeOne(st);
            delete st;

            // If all threads have been destroyed, the slice is complete.
            if (m_step_threads.empty()) {
                // Post process
                this->postProcess();

                if (this->shouldCancel())
                    return;

                // Add GCODE header on first layer only
                if (m_steps_done == 0)
                    this->writeGCodeSetup();

                this->writeGCode();

                ++m_steps_done;

                if (m_cross_section_generated) // There is going to be another layer to compute
                {
                    this->sendGCode();
                    emit statusUpdate(StatusUpdateStepType::kRealTimeLayerCompleted, m_steps_done);

                    if (m_mode == RealTimeSlicingMode::kOpenLoop)
                        processNext(); // Process next layer without sensor data
                }
                else // All layers are done, so send signal
                {
                    this->writeGCodeShutdown();
                    m_state = 4; // Move state to signify that this is the last gcode to be sent
                    this->sendGCode();
                    emit statusUpdate(StatusUpdateStepType::kRealTimeLayerCompleted,
                                      -1); // Neg 1 signifies part is done
                    emit sliceComplete();  // Signal to program that we are done
                }
            }

            return;
        }

        // Move the next item in the queue to the free thread.
        QSharedPointer<Step> step = m_step_queue.dequeue();
        st->setStep(step);

        // Connect the signal, start the step, and disconnect to avoid recieving unrelated signals.
        QObject::connect(this, &RealTimeAST::stepStart, st, &StepThread::doStep);
        emit stepStart();
        QObject::disconnect(this, &RealTimeAST::stepStart, st, &StepThread::doStep);
    }
}

void RealTimeAST::writeGCodeSetup() {
    float minimum_x(std::numeric_limits<float>::max()), minimum_y(std::numeric_limits<float>::max()),
        maximum_x(std::numeric_limits<float>::min()), maximum_y(std::numeric_limits<float>::min());

    for (QSharedPointer<Part> curr_part : CSM->parts()) {
        if (curr_part->rootMesh()->type() == MeshType::kClipping) // Skip parts that were used for clipping
            continue;

        minimum_x = std::min(minimum_x, curr_part->rootMesh()->min().x());
        minimum_y = std::min(minimum_y, curr_part->rootMesh()->min().y());
        maximum_x = std::max(maximum_x, curr_part->rootMesh()->max().x());
        maximum_y = std::max(maximum_y, curr_part->rootMesh()->max().y());
    }

    m_gcode_output += m_base->writeSlicerHeader(toString(m_syntax));
    m_gcode_output += m_base->writeSettingsHeader(m_syntax);
    m_gcode_output += m_base->writeInitialSetup(Distance(minimum_x), Distance(minimum_y), Distance(maximum_x),
                                                Distance(maximum_y), -1);
}

void RealTimeAST::writeGCodeShutdown() {
    m_gcode_output += m_base->writeShutdown();
    m_gcode_output += m_base->writeSettingsFooter();
}

void RealTimeAST::sendGCode() {
    auto console_settings = GSM->getConsoleSettings();

    RealTimeSlicingOutput output_type = RealTimeSlicingOutput::kNetwork;
    if (console_settings != nullptr)
        output_type = static_cast<RealTimeSlicingOutput>(
            console_settings->setting<int>(Constants::ConsoleOptionStrings::kRealTimeCommunicationMode));

    QString command;

    switch (output_type) {
        case (RealTimeSlicingOutput::kFile): {
            // Write to file
            QTextStream stream(&m_temp_gcode_output_file);
            stream << m_gcode_output;

            if (m_syntax == GcodeSyntax::kRPBF)
                m_RPBF_exporter->saveLayer(m_gcode_output);

            break;
        }
        case (RealTimeSlicingOutput::kNetwork): {
            // Add gcode to our message
            command = m_queue_gcode_command + " " + "P\"" + m_printer_name + "\" " + "C\"" + m_gcode_output + "\"";
            break;
        }
        default:
            break;
    };

    saveRecoveryFile();

    m_gcode_output.clear();

    if (m_mode == RealTimeSlicingMode::kClosedLoop) {
        m_data_stream->send(command);
    }
    else if (m_mode == RealTimeSlicingMode::kOpenLoop) {
        m_gcode_output.clear();
    }
}

void RealTimeAST::sendStartup() {
    m_state = 0;
    QString command = m_start_command + " " + "P\"" + m_printer_name + "\"";
    m_data_stream->send(command);
}

void RealTimeAST::sendShutdown() {
    m_state = 5;
    QString command = m_end_command + " " + "P\"" + m_printer_name + "\"";
    m_data_stream->send(command);
}

void RealTimeAST::saveRecoveryFile() {
    // Save recovery file to same dir as slicing
    QString recovery_path = m_temp_gcode_dir.path() % "/" % "recovery.json";
    QFile recovery(recovery_path);

    nlohmann::json recovery_info;
    recovery_info["layer_number"] = m_steps_done;

    // If RPBF slicing then also save extra info
    if (m_syntax == GcodeSyntax::kRPBF) {
        recovery_info["global_sector_count"] = m_RPBF_exporter->getGlobalSectorCount();
        recovery_info["scan_head_count"] = m_RPBF_exporter->getScanHeadCount();
    }

    if (recovery.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream recovery_out(&recovery);
        recovery_out << recovery_info.dump().c_str();
    }
    recovery.close();
}

void RealTimeAST::handleRemoteMessage() {
    QString response_msg = m_data_stream->getNextMessage();
    switch (m_state) {
        case 0: // Sent startup, waiting for response
        {
            if (response_msg == "ok") // This start was good
            {
                m_state = 1;
                processNext(); // Start slicing process
            }
            break;
        }
        case 1: // Sent gcode, waiting for response
        {
            if (response_msg == "ok") // This gcode was received
            {
                QString command = m_sync_commmand + " " + "P\"" + m_printer_name + "\"";
                m_data_stream->send(command);
                m_state = 2;
            }
            break;
        }
        case 2: // Sent sync, waiting for response
        {
            if (response_msg == "ok") {
                QString command = m_report_sensor_readings_commmand + " " + "P\"" + m_printer_name + "\"";
                m_data_stream->send(command);
                m_state = 3;
            }
            break;
        }
        case 3: // Sent sensor request, waiting for response
        {
            try {
                nlohmann::json data = json::parse(response_msg.toStdString());
                m_state = 1;       // Loop back to state 1 to slice more layers
                processNext(data); // Do next slice with feedback data
            } catch (nlohmann::json::exception e) { qDebug() << "json parse error: " << e.what(); }

            break;
        }
        case 4: // Send footer-gcode, waiting for response before continuing
        {
            if (response_msg == "ok") // This gcode was received
            {
                m_state = 5; // Dead state for when we are not waitig on anything
                sendShutdown();
            }
            break;
        }
        case 5: // Sent gcode, waiting for response
        {
            if (response_msg == "ok") // This gcode was received
            {
                m_state = -1; // Dead state for when we are not waitig on anything
            }
            break;
        }
    }
}

void RealTimeAST::initialSetup() {}

void RealTimeAST::setupNetworking() {
    // Fetch ip and port settings from console settings
    auto console_settings = GSM->getConsoleSettings();
    QString ip;
    int port = 0;
    if (console_settings != nullptr) {
        port = console_settings->setting<int>(Constants::ConsoleOptionStrings::kRealTimeNetworkPort);
        ip = console_settings->setting<QString>(Constants::ConsoleOptionStrings::kRealTimeNetworkIP);
        m_printer_name = console_settings->setting<QString>(Constants::ConsoleOptionStrings::kRealTimePrinter);
    }
    else {
        // Since no console settings exsist, use defaults
        port = 12345;
        ip = "localhost";
        m_printer_name = "Default";
    }

    m_tcp_connection = new TCPConnection();

    // When connected to Sensor Control 2, build a data stream to send text and start slicing
    connect(m_tcp_connection, &TCPConnection::connected, this, [this]() {
        // Build new data stream
        m_data_stream = new DataStream(m_tcp_connection);
        connect(m_data_stream, &DataStream::newData, this, &RealTimeAST::handleRemoteMessage);

        m_connection_attempts = 0; // Since we connected, reset connection attempt counter

        // Start the slicing process
        emit startPrint();
    });

    // If the connection request timesout, try to reconnect up to MAX_NUMBER_OF_CONNECTION_RETIES times
    // If it can not re-establish, save progess to recovery file
    connect(m_tcp_connection, &TCPConnection::timeout, this, [this, ip, port]() {
        // Failed to connect to printer
        qWarning() << "Failed to connect to printer at " + ip + ":" + QString::number(port) +
                          ". Trying again in 5 seconds...";

        QThread::sleep(5);

        if (m_connection_attempts > MAX_NUMBER_OF_CONNECTION_RETIES) {
            qWarning() << "Max connection attempts reached, saving progress and shutting down";
            saveRecoveryFile();
            emit sliceComplete(); // Signal to program that we are done
        }
        else {
            qWarning() << "Trying connection attempt " + QString::number(m_connection_attempts) + " of " +
                              QString::number(MAX_NUMBER_OF_CONNECTION_RETIES);

            m_connection_attempts++;
            m_tcp_connection->setupNewAsync(ip, port);
        }
    });

    // If the connection closes for any reason, try to reconnect up to MAX_NUMBER_OF_CONNECTION_RETIES times
    // If it can not re-establish, save progess to recovery file
    connect(m_tcp_connection, &TCPConnection::disconnected, this, [this, ip, port]() {
        // Failed to connect to printer
        qWarning() << "Lost connection to printer, try to reconnect in 5 seconds";

        QThread::sleep(5);

        if (m_connection_attempts > MAX_NUMBER_OF_CONNECTION_RETIES) {
            qWarning() << "Max connection attempts reached, saving progress and shutting down";
            saveRecoveryFile();
            emit sliceComplete(); // Signal to program that we are done
        }
        else {
            qWarning() << "Trying reconnect attempt " + QString::number(m_connection_attempts) + " of " +
                              QString::number(MAX_NUMBER_OF_CONNECTION_RETIES);

            m_connection_attempts++;
            m_tcp_connection->setupNewAsync(ip, port);
        }
    });

    // Try to connect
    m_connection_attempts++;
    m_tcp_connection->setupNewAsync(ip, port);
}

void RealTimeAST::processNext(nlohmann::json data) {
    this->preProcess(data);

    int total_steps = 0;
    for (QSharedPointer<Part> part : CSM->parts()) {
        total_steps += part->countStepPairs();
    }

    // Instantiate the ideal thread amount.
    for (int i = 0, end = (total_steps > QThread::idealThreadCount() ? QThread::idealThreadCount() : total_steps);
         i < end; ++i) {
        StepThread* st = new StepThread();
        m_step_threads.push_back(st);

        QObject::connect(this, &RealTimeAST::stepStart, st, &StepThread::doStep);
        QObject::connect(st, &StepThread::completed, this, &RealTimeAST::cleanThread);
    }

    // For every selected step in every part, add the step to the queue.
    for (QSharedPointer<Part> part : CSM->parts()) {
        if (part->rootMesh()->type() == MeshType::kClipping) // Skip parts that were used for clipping
            continue;

        QList<QSharedPointer<Step>> allSteps = part->steps();
        for (QSharedPointer<Step> step : allSteps) {
            step->setSync(part->getSync());

            if (step->isDirty() && !m_step_queue.contains(step)) {
                m_step_queue.append(step);
            }
        }
    }

    m_queue_start_size = m_step_queue.size();

    // For every thread available, give it a step to compute.
    for (StepThread* st : m_step_threads) {
        if (m_step_queue.empty())
            break;

        QSharedPointer<Step> step = m_step_queue.dequeue();
        st->setStep(step);
    }

    if (this->shouldCancel()) {
        m_step_threads.clear();
        m_step_queue.clear();
        return;
    }

    emit stepStart();

    // After starting steps, ensure that all threads are disconnected. Otherwise, the compute function will be recalled
    // in cleanThread().
    QObject::disconnect(this, &RealTimeAST::stepStart, nullptr, nullptr);
}
} // namespace ORNL
