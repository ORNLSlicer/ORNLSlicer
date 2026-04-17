#include "threading/mesh_loader.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include <QLinkedList>
#include <QStack>
#include <QtDebug>
#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <qcontainerfwd.h>
#include <qfileinfo.h>
#include <qmap.h>
#include <qmatrix4x4.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/open_mesh.h"
#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
MeshLoader::MeshLoader(QString file_path, MeshType mt, QMatrix4x4 transform, Distance unit)
    : m_file_path(file_path), m_mesh_type(mt), m_transform(transform), m_unit(unit) {}

void MeshLoader::run() {
    auto meshes = LoadMeshes(m_file_path, m_mesh_type, m_transform, m_unit);

    if (meshes.isEmpty())
        emit error("Error importing mesh: " + QFileInfo(m_file_path).fileName());

    for (auto mesh_data : meshes)
        emit newMesh(mesh_data);
}

QVector<MeshLoader::MeshData> MeshLoader::LoadMeshes(QString file_path, MeshType mt, QMatrix4x4 transform,
                                                     Distance unit, void* raw_data, size_t file_size) {
    QVector<MeshData> loaded_meshes;

    QFileInfo file_info(file_path);

    std::pair<void*, size_t> file_data;

    if (raw_data == nullptr || file_size == 0) // Data not provided, so load from file
    {
        if (!file_info.exists())
            return loaded_meshes;

        file_data = LoadRawData(file_info.absoluteFilePath());
    }
    else
        file_data = std::make_pair(raw_data, file_size);

    const aiScene* scene = nullptr;

    Assimp::Importer importer;

    auto model_type = file_info.suffix();

    if (model_type == "stl" || model_type == "STL") {
        scene =
            importer.ReadFileFromMemory(file_data.first, file_data.second,
                                        aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType,
                                        "stl"); // Tell assimp we are using STL.
    }
    else if (model_type == "3mf" || model_type == "3MF") {
        scene =
            importer.ReadFileFromMemory(file_data.first, file_data.second,
                                        aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType,
                                        "3mf"); // Tell assimp we are using 3mf.
    }
    else if (model_type == "obj" || model_type == "OBJ") {
        scene = importer.ReadFileFromMemory(file_data.first, file_data.second,
                                            aiProcess_DropNormals | aiProcess_JoinIdenticalVertices |
                                                aiProcess_Triangulate | aiProcess_SortByPType,
                                            "obj"); // Tell assimp we are using obj.
    }
    else if (model_type == "amf" || model_type == "AMF") {
        scene = importer.ReadFileFromMemory(file_data.first, file_data.second,
                                            aiProcess_DropNormals | aiProcess_JoinIdenticalVertices |
                                                aiProcess_Triangulate | aiProcess_SortByPType,
                                            "amf"); // Tell assimp we are using obj.
    }
    else {
        scene = importer.ReadFileFromMemory(file_data.first, file_data.second,
                                            aiProcess_DropNormals | aiProcess_JoinIdenticalVertices |
                                                aiProcess_SortByPType);
    }

    if (scene == nullptr)
        return loaded_meshes;

    if (scene->HasMeshes()) {
        int num_models_added = 0;
        for (int i = 0, end = scene->mNumMeshes; i < end; ++i) {
            auto mesh = scene->mMeshes[i];
            if (mesh->mNumFaces > 0 && mesh->mNumVertices > 0) {
                QString name = file_info.baseName();

                if (scene->mNumMeshes > 1)
                    name += "_" + QString::number(num_models_added);

                QSharedPointer<MeshBase> new_mesh;

                // Try to build a closed mesh first
                MeshTypes::Polyhedron polyhedron;
                MeshBuilderAssimp<MeshTypes::HalfedgeDescriptor> builder(mesh);
                polyhedron.delegate(builder);

                if (GSM->getGlobal()->setting<bool>(PS::SpecialModes::kEnableFixModel))
                    ClosedMesh::CleanPolyhedron(polyhedron);

                if (builder.wasError() || !polyhedron.is_closed()) {
                    MeshTypes::SurfaceMesh sm = BuildSurfaceMesh(mesh);
                    new_mesh = QSharedPointer<OpenMesh>::create(sm, name, file_info.fileName());
                }
                else {
                    new_mesh = QSharedPointer<ClosedMesh>::create(polyhedron, name, file_info.fileName());
                }
                new_mesh->setType(mt);

                // Center the mesh about itself
                auto center = new_mesh->originalCentroid();
                new_mesh->center();

                if (transform.isIdentity()) // If the transform was not provided
                {
                    // Scale to the default unit
                    Distance conv(unit);
                    conv = conv.to(mm);
                    transform.scale(QVector3D(conv(), conv(), conv()));
                    new_mesh->setUnit(unit);

                    if (PreferencesManager::getInstance()->getUseImplicitTransforms()) {
                        transform.translate(center.toQVector3D());
                    }
                }

                // Apply transform
                new_mesh->setTransformation(transform);

                loaded_meshes.push_back({new_mesh, file_data.first, file_data.second});

                ++num_models_added;
            }
        }
    }

    return loaded_meshes;
}

std::pair<void*, size_t> MeshLoader::LoadRawData(QString file_path) {

    // Load raw data
    // Some C here to get a void pointer of the model.
    FILE* fptr = fopen(file_path.toUtf8(), "rb");

    fseek(fptr, 0L, SEEK_END);
    size_t fsize = ftell(fptr);
    fseek(fptr, 0L, SEEK_SET);

    void* data = malloc(fsize);
    if (data == nullptr)
        return std::make_pair(nullptr, 0);

    int readres = fread(data, 1, fsize, fptr);

    if (readres != fsize)
        return std::make_pair(nullptr, 0);

    fclose(fptr);

    return std::make_pair(data, fsize);
}

MeshTypes::SurfaceMesh MeshLoader::BuildSurfaceMesh(aiMesh* mesh) {
    MeshTypes::SurfaceMesh sm;
    typedef MeshTypes::SurfaceMesh::Vertex_index VertexIndex;
    QMap<uint, VertexIndex> points;

    for (uint i = 0, end = mesh->mNumVertices; i < end; ++i)
        points[i] = sm.add_vertex(
            MeshTypes::Point_3(mesh->mVertices[i].x * 1000, mesh->mVertices[i].y * 1000, mesh->mVertices[i].z * 1000));

    for (uint i = 0, end = mesh->mNumFaces; i < end; ++i) {
        auto& face = mesh->mFaces[i];
        auto face_desc = sm.add_face(points[face.mIndices[0]], points[face.mIndices[1]], points[face.mIndices[2]]);
    }
    return sm;
}
} // namespace ORNL
