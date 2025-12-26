#include "threading/session_loader.h"

#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"

namespace ORNL {
SessionLoader::SessionLoader(QString filename, bool save) : m_filename(filename), m_save(save) {
    // NOP
}

void SessionLoader::run() {
    if (m_save)
        this->saveSession();
    else
        this->loadSession();
}

void SessionLoader::saveSession() {
    struct zip_t* zip = zip_open(m_filename.toUtf8(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (zip == nullptr)
        return;

    QMap<QString, QSharedPointer<Part>> meshes;
    auto parts = CSM->parts();
    for (auto& part : parts) {
        meshes.insert(part->rootMesh()->name(), part);

        for (auto& submesh : part->subMeshes())
            meshes.insert(submesh->name(), part);
    }

    // Save models
    for (auto it = CSM->models().begin(); it != CSM->models().end(); it++) {
        QString file_name = it.key();
        QFileInfo file_info(file_name);

        // Hack to make sure mesh can be found if loaded from project or stl.
        // This is bad.
        QString basename = file_info.baseName();
        if (meshes.contains(basename) || meshes.contains(file_name)) // If this mesh was in the list of active meshes
        {
            // To and back from QString in a single line. What an adventure.
            std::string filename = it.key().split("/").back().toStdString();

            zip_entry_open(zip, (Constants::Settings::Session::Files::kModel + "/" + filename).c_str());
            int success = zip_entry_write(zip, it.value().model, it.value().size);
            zip_entry_close(zip);

            if (success < 0) {
                zip_close(zip);
                return;
            }
        }
    }

    struct session_file {
        std::string file;
        fifojson json;
    };
    QVector<session_file> jsons;

    // Add part transforms
    jsons.append({Constants::Settings::Session::Files::kSession,
                  CSM->partsJson()}); // TODO: this need updated to reflect parent/ child relationships

    // Add global settings
    jsons.append({Constants::Settings::Session::Files::kGlobal, GSM->globalJson()});

    // Add part settings and ranges
    json part_jsons;
    for (auto& part : CSM->parts()) {
        json json;
        json[Constants::Settings::Session::LocalFile::kName] = part->name();
        json[Constants::Settings::Session::LocalFile::kSettings] = part->getSb()->json();
        json[Constants::Settings::Session::LocalFile::kRanges] = part->SettingsRangesToJson();
        part_jsons.push_back(json);
    }
    jsons.append({Constants::Settings::Session::Files::kLocal, part_jsons});

    // Write jsons to files
    for (session_file curr_json : jsons) {
        zip_entry_open(zip, curr_json.file.c_str());

        std::string dump = curr_json.json.dump(4);
        zip_entry_write(zip, dump.c_str(), dump.length());

        zip_entry_close(zip);
    }

    zip_entry_open(zip, "Version.txt");
    std::string version = "This is an ORNL Slicer project file.\nVersion: " + std::string(BOOST_PP_STRINGIZE(SLICER2_VERSION)) + "\n";
    zip_entry_write(zip, version.c_str(), version.length());
    zip_entry_close(zip);

    zip_close(zip);
}

fifojson SessionLoader::getSettingsFromZip() {
    struct zip_t* zip = zip_open(m_filename.toUtf8(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    fifojson result = fifojson::parse(loadStringFromZip(zip, Constants::Settings::Session::Files::kGlobal));
    zip_close(zip);
    return result;
}

void SessionLoader::updateSettingsJson(fifojson j) { m_new_json = j; }

void SessionLoader::loadSession() {

    if (!m_new_json.empty()) {
        struct zip_t* zip = zip_open(m_filename.toUtf8(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'd');
        if (zip == nullptr)
            return;

        std::string entry = Constants::Settings::Session::Files::kGlobal;
        char* entries[] = {&entry[0]};
        zip_entries_delete(zip, entries, 1);

        std::string dump = m_new_json.dump(4);
        zip_entry_open(zip, Constants::Settings::Session::Files::kGlobal.c_str());
        zip_entry_write(zip, dump.c_str(), dump.length());
        zip_entry_close(zip);

        zip_close(zip);
    }

    struct zip_t* zip = zip_open(m_filename.toUtf8(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');

    if (zip == nullptr)
        return;

    // Load every model in the project file.
    int entries = zip_entries_total(zip);
    for (int i = 0; i < entries; ++i) {
        zip_entry_openbyindex(zip, i);
        QString name = zip_entry_name(zip);

        // There is no way to iterate over just a sub dir using this library. Just compare the file name and see if it's
        // in our model dir. The number of files is small enough that this shouldn't be a problem.
        if (!name.startsWith("model/")) {
            zip_entry_close(zip);
            continue;
        }

        QString int_name = name.split("/").back(); //"#SESSION#/" + name.split("/").back();

        // Load a mesh.
        void* data = nullptr;
        size_t fsize;
        zip_entry_read(zip, &data, &fsize);

        CSM->models()[int_name] = {data, fsize};

        zip_entry_close(zip);
    }

    // Load global settings
    GSM->loadGlobalJson(fifojson::parse(loadStringFromZip(zip, Constants::Settings::Session::Files::kGlobal)));

    // Load transforms
    CSM->loadPartsJson(fifojson::parse(loadStringFromZip(zip, Constants::Settings::Session::Files::kSession)));

    // Load part settings and ranges
    auto parts_and_ranges = json::parse(loadStringFromZip(zip, Constants::Settings::Session::Files::kLocal));
    for (auto& part_json : parts_and_ranges) {
        std::string name = part_json[Constants::Settings::Session::LocalFile::kName];
        QSharedPointer<Part> part = CSM->getPart(QString::fromStdString(name));
        part->getSb()->json(part_json[Constants::Settings::Session::LocalFile::kSettings]);

        auto ranges = part_json[Constants::Settings::Session::LocalFile::kRanges];
        part->loadRangesFromJson(ranges);
    }

    zip_close(zip);
}

std::string SessionLoader::loadStringFromZip(struct zip_t* zip, const std::string& key) {
    void* buf = nullptr;
    size_t bufsize;

    zip_entry_open(zip, key.c_str());
    zip_entry_read(zip, &buf, &bufsize);
    zip_entry_close(zip);
    if (bufsize <= 0) {
        zip_close(zip);
        return "{}";
    }

    std::string str = std::string((char*)buf, bufsize);
    free(buf);

    return str;
}
} // namespace ORNL
