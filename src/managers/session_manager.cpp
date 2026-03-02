
#include "managers/session_manager.h"

#include "QCoreApplication"
#include "QStandardPaths"
#include "QUuid"
#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/mesh_loader.h"
#include "threading/session_loader.h"
#include "threading/slicers/image_slicer.h"
#include "threading/slicers/polymer_slicer.h"
#include "threading/slicers/real_time_polymer_slicer.h"
#include "threading/slicers/real_time_rpbf_slicer.h"
#include "threading/slicers/rpbf_slicer.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {
QSharedPointer<SessionManager> SessionManager::m_singleton = QSharedPointer<SessionManager>();

QSharedPointer<SessionManager> SessionManager::getInstance() {
    if (m_singleton.isNull()) {
        m_singleton.reset(new SessionManager());
    }
    return m_singleton;
}

SessionManager::SessionManager() : m_file(QString()), m_sensor_files_generated(false) {
    // Create static location for gcode output for session
    QString appPathStr = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appPath(appPathStr);
    QString httpAppLocationStr = QDir::temp().absolutePath() + "/http_config";
    QDir httpConfigPath(httpAppLocationStr);
    try {
        if (!appPath.exists())
            QDir().mkpath(appPathStr);

        if (!httpConfigPath.exists())
            QDir().mkpath(httpAppLocationStr);
    } catch (...) { qWarning() << "Check your path, cannot create directory:" + appPathStr; }

    defaultGcodeFile = appPath.filePath("gcode_output");
    loadHistory();

    // clear any temp STLs created from http import
    appPath.setNameFilters(QStringList() << "*.stl");
    appPath.setFilter(QDir::Files);
    for (QString tempStls : appPath.entryList()) {
        appPath.remove(tempStls);
    }
}

SessionManager::~SessionManager() {
    // Free the C allocated memory.
    for (model_data file : m_models) {
        free(file.model);
    }
    saveHistory();
}

void SessionManager::loadHistory() {
    QDir path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(path.filePath("app.history"));

    if (file.exists()) {
        file.open(QIODevice::ReadOnly);
        QString history = file.readAll();
        fifojson j = json::parse(history.toStdString());
        QString defaultLocation = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kPrinter,
                                             QString::fromStdString(j.value("printer_setting", "LFAM_03in")));
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kMaterial,
                                             QString::fromStdString(j.value("material_setting", "LFAM_03in")));
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kProfile,
                                             QString::fromStdString(j.value("profile_setting", "LFAM_03in")));
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kExperimental,
                                             QString::fromStdString(j.value("experimental_setting", "LFAM_03in")));

        m_most_recent_model_location = j.value("model_location", defaultLocation);
        m_most_recent_project_location = j.value("project_location", defaultLocation);
        m_most_recent_gcode_location = j.value("gcode_location", defaultLocation);
        m_most_recent_setting_folder_location = j.value("setting_folder_location", defaultLocation);
        m_most_recent_layer_bar_setting_folder_location = j.value("layer_bar_setting_folder_location", defaultLocation);
        m_most_recent_http_config = j.value("http_config", QString());

        file.close();
    }
    else {
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kPrinter, "LFAM_03in");
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kMaterial, "LFAM_03in");
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kProfile, "LFAM_03in");
        m_most_recent_setting_history.insert(Constants::Settings::SettingTab::kExperimental, "LFAM_03in");

        m_most_recent_model_location = m_most_recent_project_location = m_most_recent_gcode_location =
            m_most_recent_setting_folder_location = m_most_recent_layer_bar_setting_folder_location =
                QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        m_most_recent_http_config = QString();

        m_dirty_history = true;
    }
}

void SessionManager::saveHistory() {
    if (m_dirty_history) {
        fifojson j;

        j["printer_setting"] = m_most_recent_setting_history[Constants::Settings::SettingTab::kPrinter];
        j["material_setting"] = m_most_recent_setting_history[Constants::Settings::SettingTab::kMaterial];
        j["profile_setting"] = m_most_recent_setting_history[Constants::Settings::SettingTab::kProfile];
        j["experimental_setting"] = m_most_recent_setting_history[Constants::Settings::SettingTab::kExperimental];
        j["model_location"] = m_most_recent_model_location;
        j["project_location"] = m_most_recent_project_location;
        j["gcode_location"] = m_most_recent_gcode_location;
        j["setting_folder_location"] = m_most_recent_setting_folder_location;
        j["layer_bar_setting_folder_location"] = m_most_recent_layer_bar_setting_folder_location;
        j["http_config"] = m_most_recent_http_config;

        // Causes segfault if QStandardPaths is referenced here?
        // QDir path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir path = m_save_dir;
        QFile file(path.filePath("app.history"));
        file.open(QIODevice::WriteOnly);
        file.write(j.dump(4).c_str());
        file.close();
        m_dirty_history = false;
    }
}

QString SessionManager::sessionFile() { return m_file; }

bool SessionManager::loadModel(QString filename, bool saveLocation, MeshType mt, bool synchRequired, QMatrix4x4 mtrx) {
    QFileInfo file_info(filename);

    if (synchRequired) {
        auto meshes = MeshLoader::LoadMeshes(filename, mt, mtrx, PreferencesManager::getInstance()->getImportUnit());
        for (auto mesh_data : meshes) {
            addPart(mesh_data.mesh, filename, mt);
            if (!m_models.contains(file_info.fileName()))
                m_models.insert(file_info.fileName(), {mesh_data.raw_data, mesh_data.size});
            else
                free(mesh_data.raw_data);
        }
    }
    else {
        MeshLoader* loader = new MeshLoader(filename, mt, mtrx, PreferencesManager::getInstance()->getImportUnit());
        connect(loader, &MeshLoader::finished, loader, &MeshLoader::deleteLater);

        connect(loader, &MeshLoader::error, this, [this](QString msg) { emit forwardStatusUpdate(msg); });

        connect(loader, &MeshLoader::newMesh, this, [this, filename, file_info, mt](MeshLoader::MeshData mesh_data) {
            addPart(mesh_data.mesh, filename, mt);
            if (!m_models.contains(file_info.fileName()))
                m_models.insert(file_info.fileName(), {mesh_data.raw_data, mesh_data.size});
            else
                free(mesh_data.raw_data);
        });
        loader->start();
    }

    if (saveLocation)
        setMostRecentModelLocation(file_info.absoluteFilePath());

    return true;
}

void SessionManager::addPart(QSharedPointer<Part> new_part, bool notify) {
    m_load_mutex.lock();

    // Try to find a name for this part.
    QString name = new_part->name();
    QString org_name = name;
    uint count = 1;

    while (m_parts.contains(name)) {
        name = org_name + "_" + QString::number(count);
        count++;
    }

    new_part->setName(name);
    m_parts.insert(name, new_part);

    if (notify)
        emit partAdded(new_part);
    m_load_mutex.unlock();
}

void SessionManager::addPart(QSharedPointer<MeshBase> new_mesh, QString filename, MeshType mt) {
    m_load_mutex.lock();
    QSharedPointer<Part> new_part = QSharedPointer<Part>::create(new_mesh, filename, mt);

    // Try to find a name for this part.
    QString file_name;
    if (filename == "") // filename is blank because the part is being loaded via project import
    {
        file_name = new_part->name();
        QString org_name = file_name;
        uint count = 1;

        while (m_parts.contains(file_name)) {
            file_name = org_name + "_" + QString::number(count);
            count++;
        }
    }
    else // filename exists because the part is being loaded via UI button
    {
        // Find everything after the last /
        QStringList full_name_parts = filename.split("/");
        file_name = full_name_parts.at(full_name_parts.size() - 1);
        QString orig_file_name = file_name;

        // Add a number to duplicated file names
        uint count = 1;
        while (m_parts.contains(file_name)) {
            // Separate at the periods, then join everything before the last period
            QStringList orig_file_name_parts = orig_file_name.split(".");
            QString name;
            for (int i = 0; i < orig_file_name_parts.size() - 1; i++) {
                name += orig_file_name_parts[i] + ".";
            }
            name.chop(1); // Without this, the file name will always include an extra period
            // Add a number to signify a new instance of an existing file name
            file_name = name + "_" + QString::number(count);
            // Re-add the file extension
            file_name += "." + orig_file_name_parts[orig_file_name_parts.size() - 1];
            count++;
        }
    }

    new_part->setName(file_name);
    m_parts.insert(file_name, new_part);

    emit partAdded(new_part);
    m_load_mutex.unlock();
}

void SessionManager::reloadPart(QSharedPointer<PartMetaItem> pm) {
    QString filename = pm->part()->sourceFilePath();

    if (filename.isNull() || filename.isEmpty() || !(QFileInfo(filename).exists() && QFileInfo(filename).isFile())) {
        emit forwardStatusUpdate("Part reload failed, file \"" + filename + "\", no such source file available");
        return;
    }

    QFileInfo file_info(filename);

    MeshLoader* loader = new MeshLoader(filename, pm->part()->getMeshType(), QMatrix4x4(),
                                        PreferencesManager::getInstance()->getImportUnit());

    connect(loader, &MeshLoader::finished, loader, &MeshLoader::deleteLater);

    connect(loader, &MeshLoader::error, this, [this](QString msg) { emit forwardStatusUpdate(msg); });

    connect(loader, &MeshLoader::newMesh, this, [this, file_info, pm](MeshLoader::MeshData mesh_data) {
        pm->part()->setRootMesh(mesh_data.mesh);

        m_models[file_info.fileName()] = {mesh_data.raw_data, mesh_data.size};

        emit partReloaded(pm);
        emit forwardStatusUpdate("Reloaded Part STL, file \"" + file_info.fileName() + "\"");
    });
    loader->start();
}

void SessionManager::replacePart(QSharedPointer<PartMetaItem> pm, QString filename) {
    if (filename.isNull() || filename.isEmpty() || !(QFileInfo(filename).exists() && QFileInfo(filename).isFile())) {
        emit forwardStatusUpdate("Part reload failed, file \"" + filename + "\", no such source file available");
        return;
    }

    QFileInfo file_info(filename);

    MeshLoader* loader = new MeshLoader(filename, pm->part()->getMeshType(), QMatrix4x4(),
                                        PreferencesManager::getInstance()->getImportUnit());

    connect(loader, &MeshLoader::finished, loader, &MeshLoader::deleteLater);

    connect(loader, &MeshLoader::error, this, [this](QString msg) { emit forwardStatusUpdate(msg); });

    connect(loader, &MeshLoader::newMesh, this, [this, file_info, pm](MeshLoader::MeshData mesh_data) {
        pm->part()->setSourceFile(file_info.fileName());

        pm->part()->setRootMesh(mesh_data.mesh);

        m_models[file_info.fileName()] = {mesh_data.raw_data, mesh_data.size};

        emit partReloaded(pm);
        emit forwardStatusUpdate("Reloaded Part STL, file \"" + file_info.fileName() + "\"");
    });
    loader->start();
}

void SessionManager::addCopiedPart(QSharedPointer<Part> new_part) {
    // Try to find a name for this part.
    QString name = new_part->name();
    QString org_name = name;
    uint count = 1;

    while (m_parts.contains(name)) {
        name = org_name + "_" + QString::number(count);
        count++;
    }

    new_part->setName(name);

    m_parts.insert(name, new_part);
}

bool SessionManager::removePart(QSharedPointer<Part> part) {
    if (!m_parts.contains(part->name()))
        return false;

    // Keep the part around for a second so we can emit a removed signal. That way, any object that wants to
    // perform some finals operations on the part can still do so.
    QSharedPointer<Part> old_part = m_parts[part->name()];
    m_parts.remove(old_part->name());

    if (m_models.contains(part->rootMesh()->name()))
        m_models.remove(part->rootMesh()->name());

    for (auto& model : part->subMeshes())
        if (m_models.contains(model->name()))
            m_models.remove(model->name());

    emit partRemoved(old_part);

    return true;
}

bool SessionManager::removePart(QString name) {
    if (!m_parts.contains(name))
        return false;

    // Keep the part around for a second so we can emit a removed signal. That way, any object that wants to
    // perform some finals operations on the part can still do so.
    QSharedPointer<Part> old_part = m_parts[name];
    m_parts.remove(name);

    emit partRemoved(old_part);

    return true;
}

void SessionManager::clearParts() {
    m_parts.clear();

    emit partsCleared();
}

fifojson SessionManager::partsJson() const {
    fifojson session_json = fifojson::object();
    for (QSharedPointer<Part> curr_part : m_parts) {
        fifojson part_json = fifojson::object();

        part_json[Constants::Settings::Session::kFile] = curr_part->rootMesh()->path().split("/").back();
        part_json[Constants::Settings::Session::kMeshType] = static_cast<int>(curr_part->rootMesh()->type());
        part_json[Constants::Settings::Session::kGenType] = static_cast<int>(curr_part->rootMesh()->genType());
        part_json[Constants::Settings::Session::kOrgDims]["x"] = curr_part->rootMesh()->originalDimensions().x;
        part_json[Constants::Settings::Session::kOrgDims]["y"] = curr_part->rootMesh()->originalDimensions().y;
        part_json[Constants::Settings::Session::kOrgDims]["z"] = curr_part->rootMesh()->originalDimensions().z;
        part_json[Constants::Settings::Session::kTransforms] = curr_part->rootMesh()->transformations();

        session_json[Constants::Settings::Session::kParts][curr_part->name().toStdString()] = part_json;
    }

    return session_json;
}

bool SessionManager::loadPartsJson(fifojson j) {
    int totalParts = 0;
    for (auto it : j[Constants::Settings::Session::kParts].items()) {
        ++totalParts;
    }

    emit totalPartsInProject(totalParts);

    for (auto it : j[Constants::Settings::Session::kParts].items()) {
        // Get mesh information
        QString name = QString::fromStdString(it.key());
        MeshType mesh_type = it.value()[Constants::Settings::Session::kMeshType];
        MeshGeneratorType gen_type = it.value()[Constants::Settings::Session::kGenType];
        Distance3D org_dims(it.value()[Constants::Settings::Session::kOrgDims]["x"],
                            it.value()[Constants::Settings::Session::kOrgDims]["y"],
                            it.value()[Constants::Settings::Session::kOrgDims]["z"]);
        auto mtrxesArray = it.value()[Constants::Settings::Session::kTransforms];

        int transformCount = 1;
        QVector<QMatrix4x4> mtrxes;
        if (mtrxesArray.size() == 0) {
            QMatrix4x4 mtrx = it.value()[Constants::Settings::Session::kTransform];
            mtrxes.append(mtrx);
        }
        else {
            transformCount = (int)mtrxesArray.size();
            for (auto i = 0; i < transformCount; i++) {
                QMatrix4x4 mx = mtrxesArray[i];
                mtrxes.append(mx);
            }
        }

        switch (gen_type) {
            case kNone: // Not generated, so load from file
            {
                QString filename = QString::fromStdString(it.value()[Constants::Settings::Session::kFile]);

                if (m_models.contains(filename)) // Allready have this model data
                {
                    auto data = m_models.value(filename);
                    auto meshes =
                        MeshLoader::LoadMeshes(filename, mesh_type, mtrxes[0], Distance(mm), data.model, data.size);
                    for (auto mesh_data : meshes) {
                        mesh_data.mesh->setTransformations(mtrxes);

                        mesh_data.mesh->setName(name);
                        addPart(mesh_data.mesh);
                    }
                }
                break;
            }
            case kRectangularBox: {
                auto mesh =
                    QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(org_dims.x, org_dims.y, org_dims.z));
                mesh->setTransformations(mtrxes);
                mesh->setType(mesh_type);
                mesh->setName(name);
                CSM->addPart(mesh);
                break;
            }
            case kTriangularPyramid: {
                auto mesh = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateTriaglePyramidMesh(org_dims.y));
                mesh->setTransformations(mtrxes);
                mesh->setType(mesh_type);
                mesh->setName(name);
                CSM->addPart(mesh);
                break;
            }
            case kCylinder: {
                auto mesh = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(org_dims.y, org_dims.z));
                mesh->setTransformations(mtrxes);
                mesh->setType(mesh_type);
                mesh->setName(name);
                CSM->addPart(mesh);
                break;
            }
            case kCone: {
                auto mesh = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateConeMesh(org_dims.y, org_dims.z));
                mesh->setTransformations(mtrxes);
                mesh->setType(mesh_type);
                mesh->setName(name);
                CSM->addPart(mesh);
                break;
            }
            case kOpenTopBox:
            case kDefaultSettingRegion: {
                auto mesh = QSharedPointer<OpenMesh>::create(
                    MeshFactory::CreateOpenTopBoxMesh(org_dims.x, org_dims.y, org_dims.z));
                mesh->setTransformations(mtrxes);
                mesh->setType(mesh_type);
                mesh->setName(name);
                CSM->addPart(mesh);
                break;
            }
        };
    }

    return true;
}

bool SessionManager::isBuildMode() {
    for (auto part : m_parts) {
        if (part->getMeshType() == MeshType::kBuild)
            return true;
    }

    return false;
}

bool SessionManager::doSlice() {
    // check current syntax for file suffix that needs to be output
    tempGcodeFile =
        defaultGcodeFile +
        GcodeMetaList::SyntaxToMetaHash[(int)GSM->getGlobal()->setting<GcodeSyntax>(PRS::MachineSetup::kSyntax)]
            .m_file_suffix;

    SlicerType type = static_cast<SlicerType>(GSM->getGlobal()->setting<int>(ES::PrinterConfig::kSlicerType));
    m_sensor_files_generated = GSM->getGlobal()->setting<bool>(PS::LaserScanner::kLaserScanner);

    if (m_ast.isNull())
        this->changeSlicer(type);
    else {
        // See if it has changed
        if (m_slicer_type != type)
            this->changeSlicer(type);
        else
            m_ast->setGcodeOutput(tempGcodeFile);
    }

    if (m_active_connections.size() > 0)
        m_ast->setCommunicate(true);
    else
        m_ast->setCommunicate(false);

    // Request new information about the parts to be sliced.
    emit requestTransformationUpdate();

    emit startSlice();

    return true;
}

bool SessionManager::sliceComplete() {

    emit forwardSliceComplete(tempGcodeFile, true);
    return true;
}

void SessionManager::forwardDialogUpdate(StatusUpdateStepType type, int completedPercentage) {
    emit updateDialog(type, completedPercentage);
}

void SessionManager::cancelSlice() { m_ast->setCancel(); }

void SessionManager::setCopiedPart(QSharedPointer<Part> part) { m_copied_part = part; }

void SessionManager::pastePart() {
    QSharedPointer<Part> new_copy = QSharedPointer<Part>(new Part(*m_copied_part));
    // reset transforms so that part matches it's original as loaded (graphical manipulations still apply)
    new_copy->setTransformation(QMatrix4x4());

    QString name = new_copy->name();
    QString org_name = name = name.left(name.lastIndexOf('_'));
    uint count = 1;

    // Try to find a name for this part.
    while (m_parts.contains(name)) {
        name = org_name + "_" + QString::number(count);
        count++;
    }
    new_copy->rootMesh()->setName(name);

    m_load_mutex.lock();
    m_parts.insert(name, new_copy);
    emit partAdded(new_copy);
    m_load_mutex.unlock();
}

void SessionManager::setupTCPServer() {
    m_tcp_server = new TCPServer();

    m_step_connectivity = {
        PreferencesManager::getInstance()->getStepConnectivity(StatusUpdateStepType::kPreProcess),
        PreferencesManager::getInstance()->getStepConnectivity(StatusUpdateStepType::kCompute),
        PreferencesManager::getInstance()->getStepConnectivity(StatusUpdateStepType::kPostProcess),
        PreferencesManager::getInstance()->getStepConnectivity(StatusUpdateStepType::kGcodeGeneraton),
        PreferencesManager::getInstance()->getStepConnectivity(StatusUpdateStepType::kGcodeParsing)};

    if (PreferencesManager::getInstance()->getTcpServerAutoStart()) {
        setServerInformation(PreferencesManager::getInstance()->getTCPServerPort());
    }
}
void SessionManager::setServerInformation(int port) {
    m_tcp_server->close();
    connect(m_tcp_server, &TCPServer::newClient, this, [this](ORNL::TCPConnection* connection) {
        QSharedPointer<DataStream> data_stream = QSharedPointer<DataStream>::create(connection);
        // auto data_stream = new DataStream(connection);
        connect(data_stream.get(), &DataStream::newData, this, [this, data_stream, connection]() {
            fifojson message = json::parse(data_stream->getNextMessage().toStdString());
            QString id = QString::fromStdString(message["header"]["request-id"]);
            if (m_active_connections.contains(id)) {
                int command = message["header"]["command"];
                switch (command) {
                    case 1:
                        if (message["data"]["response"] != 200) {
                            m_active_connections.remove(id);
                            connection->close();
                        }
                        break;

                    case 5:
                        if (message["data"]["response"] == 200) {
                            m_ast->setNetworkData(StatusUpdateStepType::kGcodeGeneraton,
                                                  QString::fromStdString(message["data"]["gcode"]));
                        }
                        break;
                }
            }
        });

        QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QDateTime current = QDateTime::currentDateTime();
        QString dt = current.toString();

        m_active_connections.insert(id, connection_data {data_stream, current});
        fifojson handshake;
        handshake["header"]["request-id"] = id.toStdString();
        handshake["header"]["date"] = dt.toStdString();
        handshake["header"]["command"] = 1;
        data_stream->send(QString::fromStdString(handshake.dump(4)));
    });

    m_tcp_server->startAsync(port);
    emit forwardStatusUpdate("TCP Server started/restarted on port: " + QString::number(port));
}

void SessionManager::sendMessage(StatusUpdateStepType type, QString data) {
    if (m_active_connections.size() > 0 && m_step_connectivity[(int)type]) {
        for (QString key : m_active_connections.keys()) {
            fifojson j;
            j["header"]["request-id"] = key.toStdString();
            j["header"]["date"] = m_active_connections[key].current_date_time.toString().toStdString();
            j["header"]["command"] = (int)type + 2;
            j["data"]["gcode"] = data.toStdString();
            m_active_connections[key].data_stream->send(QString::fromStdString(j.dump(4)));
        }
    }
}

void SessionManager::setServerStepConnectivity(StatusUpdateStepType type, bool state) {
    m_step_connectivity[(int)type] = state;
}

qint64 SessionManager::getSliceTimeElapsed() {
    if (m_ast.isNull()) {
        return 0;
    }

    return m_ast->getTimeElapsed();
}

bool SessionManager::changeSlicer(SlicerType type) {
    // Disconnect the signals from the AST.
    QObject::disconnect(this, &SessionManager::startSlice, nullptr, nullptr);

    if (GSM->getConsoleSettings() != nullptr) {
        bool use_real_time = GSM->getConsoleSettings()->setting<bool>(Constants::ConsoleOptionStrings::kRealTimeMode);
        if (use_real_time && type == SlicerType::kPolymerSlice) {
            type = SlicerType::kRealTimePolymer;
        }
        else if (use_real_time && type == SlicerType::kRPBFSlice) {
            type = SlicerType::kRealTimeRPBF;
        }
    }

    // Reset the AST with a new slicer.
    switch (type) {
        case SlicerType::kPolymerSlice:
            m_ast.reset(new PolymerSlicer(tempGcodeFile));
            break;
        case SlicerType::kMetalSlice:
            //                m_ast.reset(new ...);
            break;
        case SlicerType::kRPBFSlice:
            m_ast.reset(new RPBFSlicer(tempGcodeFile));
            break;
        case SlicerType::kRealTimePolymer:
            m_ast.reset(new RealTimePolymerSlicer(tempGcodeFile));
            break;
        case SlicerType::kRealTimeRPBF:
            m_ast.reset(new RealTimeRPBFSlicer(tempGcodeFile));
            break;
        case SlicerType::kImageSlice:
            m_ast.reset(new ImageSlicer(tempGcodeFile));
            break;
    }

    m_slicer_type = type;

    // Reset part steps
    for (QSharedPointer<Part> part : m_parts) {
        part->clearSteps();
    }

    // Reconnect the signal to the AST.
    QObject::connect(this, &SessionManager::startSlice, m_ast.get(), &AbstractSlicingThread::doSlice);
    connect(m_ast.get(), &AbstractSlicingThread::statusUpdate, this, &SessionManager::forwardDialogUpdate);
    connect(m_ast.get(), &AbstractSlicingThread::sliceComplete, this, &SessionManager::sliceComplete);
    connect(m_ast.get(), &AbstractSlicingThread::sendMessage, this, &SessionManager::sendMessage);
    return true;
}

SessionLoader* SessionManager::saveSession(QString path, bool shouldTrack) {
    // Request an update.
    emit requestTransformationUpdate();

    SessionLoader* loader = new SessionLoader(path, true);
    connect(loader, &SessionLoader::finished, loader, &SessionLoader::deleteLater);

    loader->start();
    m_file = path;

    if (shouldTrack)
        setMostRecentProjectLocation(QFileInfo(path).absolutePath());

    return loader;
}

void SessionManager::loadSession(bool shouldDelete, QString path) {
    // Clear out old data if necessary.
    if (shouldDelete) {
        for (model_data file : m_models)
            free(file.model);

        m_parts.clear();
        emit partsCleared();

        m_models.clear();
    }

    GSM->clearGlobal();

    // m_should_shift = shouldShift;

    SessionLoader* loader = new SessionLoader(path, false);
    QString filename =
        QString::fromStdString(Constants::Settings::Session::Files::kGlobal) + " in project file: " + path + " ";
    fifojson settings = loader->getSettingsFromZip();
    int result = GSM->checkVersion(filename, settings, true);
    if (result == 1)
        loader->updateSettingsJson(settings);

    if (result >= 0) {
        connect(loader, &SessionLoader::finished, loader, &SessionLoader::deleteLater);
        loader->start();
    }

    m_file = path;
}

QHash<QString, QString> SessionManager::getMostRecentSettingHistory() { return m_most_recent_setting_history; }

void SessionManager::setMostRecentSettingHistory(QString key, QString name) {
    m_most_recent_setting_history[key] = name;
    m_dirty_history = true;
}

QString SessionManager::getMostRecentModelLocation() { return m_most_recent_model_location; }

void SessionManager::setMostRecentModelLocation(QString path) {
    m_most_recent_model_location = path;
    m_dirty_history = true;
}

QString SessionManager::getMostRecentProjectLocation() { return m_most_recent_project_location; }

void SessionManager::setMostRecentProjectLocation(QString path) {
    m_most_recent_project_location = path;
    m_dirty_history = true;
}

QString SessionManager::getMostRecentGcodeLocation() { return m_most_recent_gcode_location; }

void SessionManager::setMostRecentGcodeLocation(QString path) {
    m_most_recent_gcode_location = path;
    m_dirty_history = true;
}

QString SessionManager::getMostRecentSettingFolderLocation() { return m_most_recent_setting_folder_location; }

void SessionManager::setMostRecentSettingFolderLocation(QString path) {
    m_most_recent_setting_folder_location = path;
    m_dirty_history = true;
}

QString SessionManager::getMostRecentLayerBarSettingFolderLocation() {
    return m_most_recent_layer_bar_setting_folder_location;
}

void SessionManager::setMostRecentLayerBarSettingFolderLocation(QString path) {
    m_most_recent_layer_bar_setting_folder_location = path;
    m_dirty_history = true;
}
bool SessionManager::sensorFilesGenerated() { return m_sensor_files_generated; }

QString SessionManager::getMostRecentHTTPConfig() { return m_most_recent_http_config; }

void SessionManager::setMostRecentHTTPConfig(QString config) { m_most_recent_http_config = config; }

void SessionManager::setDefaultGcodeDir(QString dir) { defaultGcodeFile = dir + "\\gcode_output"; }
} // namespace ORNL
