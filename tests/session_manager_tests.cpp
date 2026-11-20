#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include <zip/zip.h>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "threading/mesh_loader.h"
#include "threading/session_loader.h"
#include "utilities/constants.h"

namespace {

bool writeZipEntry(zip_t* zip, const std::string& name, const std::string& text) {
    if (zip_entry_open(zip, name.c_str()) < 0) return false;

    int result = zip_entry_write(zip, text.c_str(), text.size());
    zip_entry_close(zip);
    return result >= 0;
}

bool writeProjectWithOldGlobal(const QString& path) {
    QByteArray path_bytes = path.toUtf8();
    zip_t* zip            = zip_open(path_bytes.constData(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (zip == nullptr) return false;

    fifojson global = fifojson::object();
    global[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion] = 2.0;
    global[ORNL::Constants::SettingFileStrings::kSettings] =
        fifojson::array({fifojson::object({{"slicer_type", 0}, {"slicing_vector_x", 0.25}})});

    fifojson session                                    = fifojson::object();
    session[ORNL::Constants::Settings::Session::kParts] = fifojson::object();

    bool success = writeZipEntry(zip, ORNL::Constants::Settings::Session::Files::kGlobal, global.dump(4)) &&
                   writeZipEntry(zip, ORNL::Constants::Settings::Session::Files::kSession, session.dump(4)) &&
                   writeZipEntry(zip, ORNL::Constants::Settings::Session::Files::kLocal, fifojson::array().dump(4));

    zip_close(zip);
    return success;
}

std::optional<fifojson> readGlobalFromProject(const QString& path) {
    QByteArray path_bytes = path.toUtf8();
    zip_t* zip            = zip_open(path_bytes.constData(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    if (zip == nullptr) return std::nullopt;

    void* buffer       = nullptr;
    size_t buffer_size = 0;
    if (zip_entry_open(zip, ORNL::Constants::Settings::Session::Files::kGlobal.c_str()) < 0 ||
        zip_entry_read(zip, &buffer, &buffer_size) < 0) {
        zip_close(zip);
        return std::nullopt;
    }

    zip_entry_close(zip);
    std::string text(static_cast<char*>(buffer), buffer_size);
    free(buffer);
    zip_close(zip);

    return fifojson::parse(text);
}

bool hierarchyPersists(const QString& project_path) {
    auto session = ORNL::CSM;
    session->clearParts();

    auto make_part = [&session](const QString& name, ORNL::MeshType type, float x) {
        QSharedPointer<ORNL::MeshBase> mesh;
        if (name == "Z_parent" || name == "B_child") {
            const QByteArray stl_source = R"(solid hierarchy
facet normal 0 0 -1
outer loop
vertex 0 0 0
vertex 0 2 0
vertex 1 0 0
endloop
endfacet
facet normal 0 -1 0
outer loop
vertex 0 0 0
vertex 1 0 0
vertex 0 0 3
endloop
endfacet
facet normal -1 0 0
outer loop
vertex 0 0 0
vertex 0 0 3
vertex 0 2 0
endloop
endfacet
facet normal 1 1 1
outer loop
vertex 1 0 0
vertex 0 2 0
vertex 0 0 3
endloop
endfacet
endsolid hierarchy
)";
            const QString filename      = name + ".stl";
            void* model_data            = std::malloc(stl_source.size());
            std::memcpy(model_data, stl_source.constData(), stl_source.size());
            session->models()[filename] = {model_data, static_cast<size_t>(stl_source.size())};
            const auto meshes = ORNL::MeshLoader::LoadMeshes(filename, type, QMatrix4x4(), ORNL::Distance(ORNL::mm),
                                                             model_data, stl_source.size());
            if (meshes.isEmpty()) return QSharedPointer<ORNL::Part>();
            mesh = meshes.first().mesh;
        }
        else { mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(1000, 2000, 3000)); }
        mesh->setName(name);
        mesh->setType(type);
        QMatrix4x4 transform;
        transform.translate(x, 2000, 3000);
        transform.rotate(35, 0, 0, 1);
        transform.scale(1.2f, 0.8f, 1.5f);
        transform.rotate(x / 100, 0, 1, 0);
        mesh->setTransformation(transform);
        auto part = QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
        session->addPart(part, false);
        return part;
    };

    auto parent     = make_part("Z_parent", ORNL::MeshType::kBuild, 1000);
    auto child      = make_part("B_child", ORNL::MeshType::kSettings, 2000);
    auto grandchild = make_part("A_grandchild", ORNL::MeshType::kClipping, 3000);
    auto sibling    = make_part("C_sibling", ORNL::MeshType::kBuild, 4000);
    auto standalone = make_part("D_standalone", ORNL::MeshType::kBuild, 5000);
    if (!ORNL::Testing::expect(parent && child && grandchild && sibling && standalone,
                               "Could not import the STL hierarchy fixtures."))
        return false;

    // A standalone rotation can already be baked into a mesh before it is grouped.
    const auto live_transform = sibling->rootMesh()->transformation();
    sibling->rootMesh()->resetAlignedAxis(QMatrix4x4());
    QMatrix4x4 baked_transform;
    baked_transform.rotate(20, 1, 0, 0);
    sibling->rootMesh()->setTransformation(baked_transform);
    sibling->rootMesh()->alignAxis(QMatrix4x4());
    baked_transform.setToIdentity();
    baked_transform.scale(1.1f, 0.9f, 1.2f);
    baked_transform.rotate(15, 0, 0, 1);
    sibling->rootMesh()->setTransformation(baked_transform);
    sibling->rootMesh()->alignAxis(QMatrix4x4());
    sibling->rootMesh()->setTransformation(live_transform);

    parent->adoptChild(child);
    parent->adoptChild(sibling);
    child->adoptChild(grandchild);
    child->setModifierEnabled(false);
    session->renamePart(parent, "Z_renamed");

    QMap<QString, QVector<ORNL::MeshVertex>> original_vertices;
    for (auto part : session->parts()) { original_vertices[part->name()] = part->rootMesh()->vertices(); }
    const fifojson original = session->partsJson();
    const auto& parts       = original[ORNL::Constants::Settings::Session::kParts];
    if (!ORNL::Testing::expect(parts["B_child"].contains("parent") && parts["B_child"]["parent"] == "Z_renamed" &&
                                   parts["A_grandchild"]["parent"] == "B_child" &&
                                   !parts["D_standalone"].contains("parent"),
                               "Saved parts should identify their parents by current name and omit root parents."))
        return false;
    if (!ORNL::Testing::expect(
            !parts["B_child"][ORNL::Constants::Settings::Session::kModifierEnabled].get<bool>() &&
                parts["A_grandchild"][ORNL::Constants::Settings::Session::kModifierEnabled].get<bool>(),
            "Project metadata must preserve disabled and enabled modifier states."))
        return false;

    ORNL::SessionLoader saver(project_path, true);
    saver.run();
    session->clearParts();
    for (const QString& filename : {QString("Z_parent.stl"), QString("B_child.stl")}) {
        std::free(session->models()[filename].model);
        session->models().remove(filename);
    }

    auto model = QSharedPointer<ORNL::PartMetaModel>::create();
    QStringList notified_parts;
    int announced_parts     = -1;
    bool total_before_parts = false;
    const auto record_total = [&](int total) {
        announced_parts    = total;
        total_before_parts = notified_parts.isEmpty();
    };
    auto total_connection = QObject::connect(session.get(), &ORNL::SessionManager::totalPartsInProject, record_total);
    auto connection       = QObject::connect(session.get(), &ORNL::SessionManager::partAdded,
                                             [&model, &notified_parts](QSharedPointer<ORNL::Part> part) {
                                           notified_parts.append(part->name());
                                           model->newItem(part);
                                             });
    ORNL::SessionLoader loader(project_path, false);
    loader.run();
    QObject::disconnect(connection);
    QObject::disconnect(total_connection);

    if (!ORNL::Testing::expect(total_before_parts && announced_parts == 2 && announced_parts == notified_parts.size(),
                               "Project loading must announce the root notification count before adding parts."))
        return false;

    if (!ORNL::Testing::expect(session->count() == 5,
                               "Saving and reloading a project should preserve all hierarchy members."))
        return false;
    for (auto part : session->parts()) {
        const auto expected = original_vertices[part->name()];
        const auto actual   = part->rootMesh()->vertices();
        if (!ORNL::Testing::expect(actual.size() == expected.size(), "Reloaded part vertex count changed."))
            return false;
        for (int i = 0; i < actual.size(); ++i) {
            if (!ORNL::Testing::expect((actual[i].location - expected[i].location).length() < 0.05f,
                                       "Saving and reloading transformed hierarchy members changed world geometry."))
                return false;
        }
    }
    if (!ORNL::Testing::expect(
            notified_parts == QStringList({"D_standalone", "Z_renamed"}) && model->items().size() == 5 &&
                !session->getPart("B_child")->modifierEnabled() &&
                session->getPart("A_grandchild")->modifierEnabled() &&
                model->lookupByPointer(session->getPart("A_grandchild"))->parent()->part() ==
                    session->getPart("B_child"),
            "Project loading must restore modifier activation and notify complete root hierarchies once."))
        return false;

    fifojson legacy = original;
    for (auto& part : legacy[ORNL::Constants::Settings::Session::kParts]) {
        part.erase("parent");
        part.erase("aligned_transform");
        part.erase("source_dim");
        part.erase(ORNL::Constants::Settings::Session::kModifierEnabled);
    }
    session->clearParts();
    notified_parts.clear();
    announced_parts    = -1;
    total_before_parts = false;
    total_connection   = QObject::connect(session.get(), &ORNL::SessionManager::totalPartsInProject, record_total);
    connection = QObject::connect(session.get(), &ORNL::SessionManager::partAdded,
                                  [&](QSharedPointer<ORNL::Part> part) { notified_parts.append(part->name()); });
    session->loadPartsJson(legacy);
    QObject::disconnect(connection);
    QObject::disconnect(total_connection);
    if (!ORNL::Testing::expect(total_before_parts && announced_parts == 5 && announced_parts == notified_parts.size(),
                               "Legacy projects must announce and notify every independent object."))
        return false;
    for (auto part : session->parts()) {
        if (!ORNL::Testing::expect(
                part->parent().isNull() && part->children().isEmpty() && part->modifierEnabled(),
                "Legacy projects should load independent objects with modifiers enabled by default."))
            return false;
    }

    session->loadPartsJson(original);
    if (!ORNL::Testing::expect(session->getPart("B_child_1")->parent() == session->getPart("Z_renamed_1") &&
                                   session->getPart("A_grandchild_1")->parent() == session->getPart("B_child_1") &&
                                   !session->getPart("B_child_1")->modifierEnabled(),
                               "Imported hierarchy references and activation must survive part name collisions."))
        return false;

    fifojson malformed                                                              = original;
    malformed[ORNL::Constants::Settings::Session::kParts]["B_child"]["parent"]      = "A_grandchild";
    malformed[ORNL::Constants::Settings::Session::kParts]["C_sibling"]["parent"]    = "C_sibling";
    malformed[ORNL::Constants::Settings::Session::kParts]["D_standalone"]["parent"] = "missing";
    malformed[ORNL::Constants::Settings::Session::kParts]["Z_renamed"]["parent"]    = 17;
    session->clearParts();
    session->loadPartsJson(malformed);
    if (!ORNL::Testing::expect(session->getPart("B_child")->parent().isNull() &&
                                   session->getPart("A_grandchild")->parent() == session->getPart("B_child") &&
                                   session->getPart("C_sibling")->parent().isNull() &&
                                   session->getPart("D_standalone")->parent().isNull() &&
                                   session->getPart("Z_renamed")->parent().isNull(),
                               "Invalid parent references should leave objects accessible without hierarchy cycles."))
        return false;

    session->clearParts();
    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create temporary directory.")) return EXIT_FAILURE;

    auto session = ORNL::CSM;
    session->clearRecentFiles();

    const QString project_one     = temp_dir.path() + "/alpha.s2p";
    const QString project_two     = temp_dir.path() + "/beta.S2P";
    const QString project_ignored = temp_dir.path() + "/notes.txt";

    session->addRecentProjectFile(project_one);
    session->addRecentProjectFile(project_two);
    session->addRecentProjectFile(project_one);
    session->addRecentProjectFile(project_ignored);

    QStringList recent_projects = session->getRecentProjectFiles();
    if (!ORNL::Testing::expect(recent_projects.size() == 2,
                               "Recent project history should deduplicate and filter by extension."))
        return EXIT_FAILURE;
    if (!ORNL::Testing::expect(recent_projects[0] == QFileInfo(project_one).absoluteFilePath(),
                               "Most recently reopened project should move to the front."))
        return EXIT_FAILURE;
    if (!ORNL::Testing::expect(recent_projects[1] == QFileInfo(project_two).absoluteFilePath(),
                               "Previous project should remain after a duplicate is moved."))
        return EXIT_FAILURE;

    for (int i = 0; i < 12; ++i) { session->addRecentModelFile(temp_dir.path() + QString("/model-%1.stl").arg(i)); }
    session->addRecentModelFile(temp_dir.path() + "/fixture.s2p");

    QStringList recent_models = session->getRecentModelFiles();
    if (!ORNL::Testing::expect(recent_models.size() == 10, "Recent model history should be capped at ten files."))
        return EXIT_FAILURE;
    if (!ORNL::Testing::expect(recent_models[0] == QFileInfo(temp_dir.path() + "/model-11.stl").absoluteFilePath(),
                               "Newest model should be first in recent model history."))
        return EXIT_FAILURE;
    if (!ORNL::Testing::expect(recent_models.back() == QFileInfo(temp_dir.path() + "/model-2.stl").absoluteFilePath(),
                               "Recent model history should drop entries past the cap."))
        return EXIT_FAILURE;

    const QString step_model = temp_dir.path() + "/bracket.step";
    session->addRecentModelFile(step_model);
    recent_models = session->getRecentModelFiles();
    if (!ORNL::Testing::expect(recent_models[0] == QFileInfo(step_model).absoluteFilePath(),
                               "STEP model should be accepted in recent model history."))
        return EXIT_FAILURE;

    session->removeRecentFile(step_model);
    if (!ORNL::Testing::expect(!session->getRecentModelFiles().contains(QFileInfo(step_model).absoluteFilePath()),
                               "Removed recent model should not remain in history."))
        return EXIT_FAILURE;

    session->clearRecentFiles();
    if (!ORNL::Testing::expect(session->getRecentProjectFiles().isEmpty() && session->getRecentModelFiles().isEmpty(),
                               "Clearing recent files should clear both project and model history."))
        return EXIT_FAILURE;

    const QString clipping_model = temp_dir.path() + "/clipping.stl";
    const QString settings_model = temp_dir.path() + "/settings.stl";
    const QString build_model    = temp_dir.path() + "/build.stl";
    session->addRecentModelFile(clipping_model, ORNL::MeshType::kClipping);
    session->addRecentModelFile(settings_model, ORNL::MeshType::kSettings);
    session->addRecentModelFile(build_model, ORNL::MeshType::kBuild);

    recent_models = session->getRecentModelFiles();
    if (!ORNL::Testing::expect(recent_models.size() == 1, "Only build models should be added to recent model history."))
        return EXIT_FAILURE;
    if (!ORNL::Testing::expect(recent_models[0] == QFileInfo(build_model).absoluteFilePath(),
                               "Build model should be retained after non-build model history entries are ignored."))
        return EXIT_FAILURE;

    QString project_path = temp_dir.path() + "/old-settings-project.s2p";
    if (!ORNL::Testing::expect(writeProjectWithOldGlobal(project_path), "Could not write project fixture."))
        return EXIT_FAILURE;

    ORNL::SessionLoader* loader = session->loadSession(false, project_path, false);
    if (!ORNL::Testing::expect(loader != nullptr,
                               "CLI-style session load rejected the old project before auto-updating settings."))
        return EXIT_FAILURE;

    QEventLoop loop;
    bool finished  = false;
    bool succeeded = false;
    QObject::connect(loader, &ORNL::SessionLoader::loadSucceeded, &loop, [&succeeded]() { succeeded = true; });
    QObject::connect(loader, &ORNL::SessionLoader::finished, &loop, [&finished, &loop]() {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, [&loop]() { loop.quit(); });
    loop.exec();

    if (!ORNL::Testing::expect(finished, "Timed out waiting for CLI-style session load to finish."))
        return EXIT_FAILURE;
    if (!ORNL::Testing::expect(succeeded, "CLI-style session load did not complete successfully.")) return EXIT_FAILURE;

    std::optional<fifojson> archived_global = readGlobalFromProject(project_path);
    if (!ORNL::Testing::expect(archived_global.has_value(), "Could not read archived global settings from project."))
        return EXIT_FAILURE;

    double archived_version =
        (*archived_global)[ORNL::Constants::SettingFileStrings::kHeader][ORNL::Constants::SettingFileStrings::kVersion];
    if (!ORNL::Testing::expect(archived_version == 2.0,
                               "CLI-style session load unexpectedly modified the project archive."))
        return EXIT_FAILURE;

    double loaded_slice_normal_x =
        ORNL::GSM->getGlobal()->setting<double>(ORNL::Constants::ProfileSettings::Slicing::kSlicePlaneNormalX);
    if (!ORNL::Testing::expect(loaded_slice_normal_x == 0.25,
                               "CLI-style session load did not use migrated global settings in memory."))
        return EXIT_FAILURE;

    if (!hierarchyPersists(temp_dir.filePath("hierarchy.s2p"))) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
