#pragma once

#include "QDir"
#include "QFile"
#include "QQueue"
#include "QStandardPaths"
#include "data_stream.h"
#include "part/part.h"
#include "tcp_server.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {

//! \brief Define for easy access to this singleton.
#define CSM SessionManager::getInstance()

enum class SlicerType : uint8_t;
class SessionLoader;
class AbstractSlicingThread;

/*!
 *  \class SessionManager
 *  \brief Singleton manager class that contains data about the current session.
 *  \todo This class is in need of a refactor / a possible merge with the SettingsManager.
 */
class SessionManager : public QObject {
    Q_OBJECT
  public:
    //! \brief Destructor.
    ~SessionManager();

    //! \brief Get the singleton instance of this object.
    static QSharedPointer<SessionManager> getInstance();

    //! \brief Public struct to allow access to model data.
    //! \note The void pointer contains raw malloc'ed data. This is because both mesh loader (assimp) and zip library
    //! (zip) expect a void ptr.
    struct model_data {
        void* model;
        size_t size;
    };

    //! \brief Retuns a map of filename to model_data structures.
    inline QMap<QString, model_data>& models() { return m_models; }

    //! \brief Returns a map of part name to Part class.
    inline QMap<QString, QSharedPointer<Part>>& parts() { return m_parts; }

    //! \brief Returns a Part class associated with a name.
    inline QSharedPointer<Part> getPart(QString name) { return m_parts.value(name, nullptr); }

    //! \brief Retuns the number of parts in the session.
    inline int count() { return m_parts.size(); }

    //! \brief Retuns the file last used to save the session.
    //! \todo This function will always return the autosave path since it is saved after the current session.
    QString sessionFile();

    //! \brief returns whether or not additional sensor files were generated during slice
    bool sensorFilesGenerated();

    //! \brief Accessor to get history for all three settings tabs
    QHash<QString, QString> getMostRecentSettingHistory();

    //! \brief Accessor to set history for a specific setting tab
    void setMostRecentSettingHistory(QString key, QString name);

    //! \brief Accessor to get history for the most recent model loaded location
    QString getMostRecentModelLocation();

    //! \brief Accessor to set history for the most recent model loaded location
    void setMostRecentModelLocation(QString path);

    //! \brief Accessor to get history for the most recent project loaded location
    QString getMostRecentProjectLocation();

    //! \brief Accessor to set history for the most recent project loaded location
    void setMostRecentProjectLocation(QString path);

    //! \brief Accessor to get history for the most recent gcode export location
    QString getMostRecentGcodeLocation();

    //! \brief Accessor to set history for the most recent gcode export location
    void setMostRecentGcodeLocation(QString path);

    //! \brief Accessor to get history for the most recent settings folder location
    QString getMostRecentSettingFolderLocation();

    //! \brief Accessor to set history for the most recent settings folder location
    void setMostRecentSettingFolderLocation(QString path);

    //! \brief Accessor to get history for the most recent layer bar settings folder location
    //! \return most recent location for layer bar template files
    QString getMostRecentLayerBarSettingFolderLocation();

    //! \brief Accessor to set history for the most recent layer bar settings folder location
    //! \param path: file path set by user
    void setMostRecentLayerBarSettingFolderLocation(QString path);

    //! \brief Accessor to get history for the most recently selected http config
    QString getMostRecentHTTPConfig();

    //! \brief Accessor to set history for the most recently selected http config
    void setMostRecentHTTPConfig(QString config);

    //! \brief Sets the pointer to part to copy
    //! \param new_part: part to copy for later paste
    void addCopiedPart(QSharedPointer<Part> new_part);

    //! \brief Checks preferences to see if TCP server should be started
    void setupTCPServer();

    //! \brief Sets the default gcode dir to output location to skip copy/paste of files (useful for image slicing when
    //! there are 1000s of files)
    void setDefaultGcodeDir(QString dir);

  public slots:
    //! \brief loads a model into the session
    //! \param filename the path to the file
    //! \param saveLocation the location to save to
    //! \param mt the mesh type
    //! \param syncRequired if the file should be loaded synchronously (defaults to async)
    //! \param mtrx the default transform to apply
    //! \return if the mesh was loaded
    bool loadModel(QString filename, bool saveLocation, MeshType mt = MeshType::kBuild, bool syncRequired = false,
                   QMatrix4x4 mtrx = QMatrix4x4());

    //! \brief Adds a part to the session.
    //! \note While a part can be externally generated and added, this slot is primarily used to add a part after a mesh
    //! loader thread has completed.
    void addPart(QSharedPointer<Part> new_part, bool notify = true);

    //! \brief Adds a part to the session.
    //! \note While a part can be externally generated and added, this slot is primarily used to add a part after a mesh
    //! loader thread has completed. \param filename the path to the file \param Mesh type mode, defaults to build
    void addPart(QSharedPointer<MeshBase> new_mesh, QString filename = "", MeshType mt = MeshType::kBuild);

    //! \brief Reloads a part.
    void reloadPart(QSharedPointer<PartMetaItem> pm);

    void replacePart(QSharedPointer<PartMetaItem> pm, QString filename);

    //! \brief Remove a part from the session by pointer.
    bool removePart(QSharedPointer<Part> part);

    //! \brief Remove a part from the session by name.
    bool removePart(QString name);

    //! \brief Removes all parts from the session.
    void clearParts();

    //! \brief Generates a json object from the current session.
    //! \todo The session saving/loading mechanism needs some cleanup.
    fifojson partsJson() const;

    //! \brief Populates the current session from a passed json object.
    //! \note As it current stands, the session loading occurs in a separate thread but still calls this function to
    //! populate the session.
    //!       This likely requires some change to be more consistent.
    //! \todo The session saving/loading mechanism needs some cleanup.
    bool loadPartsJson(fifojson j);

    //! \brief Check if in build mode
    bool isBuildMode();

    //! \brief Signals the internal slicing thread to begin computation.
    //! \note If the internal slicing thread is unset, this function assumes that it should be a polymer slice.
    bool doSlice();

    //! \brief Signals the internal slicing thread has complete computation.
    bool sliceComplete();

    //! \brief Returns the time elapsed for the current slice.
    qint64 getSliceTimeElapsed();

    //! \brief Changes which slicer is used for computation.
    //! \todo Currently, there is only one slicer (the PolymerSlicer). When more methods are added, more Slicer types
    //! should be added
    //!       Both here and in the SlicerType enum. A dialog still needs to be written to run this function.
    bool changeSlicer(SlicerType type);

    //! \brief Creates a new session loader to save the current session.
    SessionLoader* saveSession(QString path, bool shouldTrack = true);

    //! \brief Creates a new session loader to load another session.
    //! \param shouldDelete Whether or not to delete current parts/settings before
    //! loading the session
    //! \param path The path to the project to load
    SessionLoader* loadSession(bool shouldDelete, QString path = QString());

    //! \brief Slot to receive slicing updates from.  Info to be forwarded to slice dialog
    //! \param type The current section of the step being completed
    //! \param completedPercentage The percentage complete of the current step process
    void forwardDialogUpdate(StatusUpdateStepType type, int completedPercentage);

    //! \brief Set slicing thread cancel flag
    void cancelSlice();

    //! \brief Set pointer to part to potentially copy
    //! \param part: part to copy
    void setCopiedPart(QSharedPointer<Part> part);

    //! \brief Paste previously copied part
    void pastePart();

    //! \brief Start/Restart the server based on dialog information
    //! \param port: port to start server on
    void setServerInformation(int port);

    //! \brief Send information to connected clients on tcp server
    //! \param type: currently finished processing stage
    //! \param data: data from stage to send (currently just gcode
    void sendMessage(StatusUpdateStepType type, QString data);

    //! \brief Sets which processing stages will forward information to tcp server (currently just gcode)
    //! \param type: stage to set
    //! \param state: whether or not to transmit
    void setServerStepConnectivity(StatusUpdateStepType type, bool state);

  signals:

    //! \brief Signal to the slice dialog with status update information
    //! \param type The current section of the step being completed
    //! \param completedPercentage The percentage complete of the current step process
    void updateDialog(StatusUpdateStepType type, int completedPercentage);

    //! \brief Signal that a part has been added.
    void partAdded(QSharedPointer<Part> part);

    //! \brief Signal that a part has been reloaded.
    void partReloaded(QSharedPointer<PartMetaItem> pm);

    //! \brief Signal that a part has been removed.
    void partRemoved(QSharedPointer<Part> part);

    //! \brief Signal that requests all part's transformation information be updated.
    void requestTransformationUpdate();

    //! \brief Signal that emits a loaded transformation to GUI
    void transformationLoaded(QString part_name, QMatrix4x4 transform);

    //! \brief Signal to tell GUI to clear all the parts
    void partsCleared();

    //! \brief Signal to start slicing thread.
    void startSlice();

    //! \brief Signal that slicing has complete.  Forward to main window to start visualization and adjust layer times.
    void forwardSliceComplete(QString filepath, bool alterFile);

    //! \brief Signal that slicing thread has started writing the GCode file.
    void forwardStatusUpdate(QString status);

    //! \brief Signal for total number of parts expected to load from project
    void totalPartsInProject(int total);

  private:
    //! \brief Constructor
    SessionManager();

    //! \brief Load session history for various dialogs
    void loadHistory();

    //! \brief Save session history for various dialogs
    void saveHistory();

    //! \brief Singleton pointer.
    static QSharedPointer<SessionManager> m_singleton;

    //! \brief Parts that are loaded in the session.
    QMap<QString, QSharedPointer<Part>> m_parts;

    //! \brief Model data.
    //! \note This maps filename to malloc'ed data. This is because both mesh loader (assimp) and zip library (zip)
    //! expect a void ptr.
    QMap<QString, model_data> m_models;

    //! \brief Current session file.
    QString m_file;

    //! \brief Currently active slicer.
    QSharedPointer<AbstractSlicingThread> m_ast;

    //! \brief Location to write default gcode file to.
    QString defaultGcodeFile;
    QString tempGcodeFile;

    //! \brief Variables for most recent location for various dialogs and setting choices
    QHash<QString, QString> m_most_recent_setting_history;
    QString m_most_recent_model_location;
    QString m_most_recent_project_location;
    QString m_most_recent_gcode_location;
    QString m_most_recent_setting_folder_location;
    QString m_most_recent_layer_bar_setting_folder_location;
    QString m_most_recent_http_config;

    //! \brief boolean to track when history needs written to file on close
    bool m_dirty_history;

    //! \brief bool to track whether additional files need to be considered for export
    bool m_sensor_files_generated;

    //! \brief default slicer is polymer
    SlicerType m_slicer_type = SlicerType::kPolymerSlice;

    //! \brief Mutex to serialize final step of loading parts.  Map of parts
    //! must be accessed sequentially.
    QMutex m_load_mutex;

    //! \brief Save location.
    QDir m_save_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    //! \brief pointer to part to potentially paste
    QSharedPointer<Part> m_copied_part;

    //! \brief current server
    TCPServer* m_tcp_server;

    //! \brief struct to hold server connection information
    struct connection_data {
        QSharedPointer<DataStream> data_stream;
        QDateTime current_date_time;
    };

    //! \brief current tcp server connections
    QHash<QString, connection_data> m_active_connections;

    //! \brief whether or not to transmit information between each major processing step (currently just gcode)
    QVector<bool> m_step_connectivity;
};
} // namespace ORNL
