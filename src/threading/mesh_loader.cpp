#include "threading/mesh_loader.h"

#include <QLinkedList>
#include <QStack>
#include <QTemporaryFile>
#include <QtDebug>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <tuple>
#include <utility>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/boost/graph/helpers.h>
#include <CGAL/exceptions.h>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <qcontainerfwd.h>
#include <qdir.h>
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
namespace {
constexpr double kStepLinearDeflectionMm       = 0.1;
constexpr double kStepAngularDeflectionRadians = 0.1;

bool isStepFile(const QString& suffix) {
    return suffix.compare("step", Qt::CaseInsensitive) == 0 || suffix.compare("stp", Qt::CaseInsensitive) == 0;
}

void applyInitialTransform(const QSharedPointer<MeshBase>& mesh, QMatrix4x4 transform, Distance unit) {
    auto center = mesh->originalCentroid();
    mesh->center();

    if (transform.isIdentity()) {
        Distance conv(unit);
        conv = conv.to(mm);
        transform.scale(QVector3D(conv(), conv(), conv()));
        mesh->setUnit(unit);

        if (PreferencesManager::getInstance()->getUseImplicitTransforms()) transform.translate(center.toQVector3D());
    }

    mesh->setTransformation(transform);
}

MeshTypes::SurfaceMesh buildSurfaceMeshFromStep(const QString& file_path) {
    MeshTypes::SurfaceMesh surface_mesh;

    Interface_Static::SetCVal("xstep.cascade.unit", "MM");

    STEPControl_Reader reader;
    const QByteArray file_path_data = file_path.toUtf8();
    if (reader.ReadFile(file_path_data.constData()) != IFSelect_RetDone) return surface_mesh;

    if (reader.TransferRoots() == 0) return surface_mesh;

    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) return surface_mesh;

    BRepMesh_IncrementalMesh mesher(shape, kStepLinearDeflectionMm, false, kStepAngularDeflectionRadians, true);
    mesher.Perform();
    if (!mesher.IsDone()) return surface_mesh;

    using VertexIndex = MeshTypes::SurfaceMesh::Vertex_index;
    std::map<std::tuple<long long, long long, long long>, VertexIndex> vertex_lookup;

    auto add_vertex = [&surface_mesh, &vertex_lookup](const gp_Pnt& point) {
        const long long x = std::llround(point.X() * 1000.0);
        const long long y = std::llround(point.Y() * 1000.0);
        const long long z = std::llround(point.Z() * 1000.0);
        const auto key    = std::make_tuple(x, y, z);

        auto existing = vertex_lookup.find(key);
        if (existing != vertex_lookup.end()) return existing->second;

        const VertexIndex index = surface_mesh.add_vertex(MeshTypes::Point_3(x, y, z));
        vertex_lookup[key]      = index;
        return index;
    };

    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        TopoDS_Face face = TopoDS::Face(explorer.Current());

        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) continue;

        const gp_Trsf& transform = location.Transformation();
        for (int triangle_index = 1; triangle_index <= triangulation->NbTriangles(); ++triangle_index) {
            Standard_Integer n1;
            Standard_Integer n2;
            Standard_Integer n3;
            triangulation->Triangle(triangle_index).Get(n1, n2, n3);

            std::array<Standard_Integer, 3> node_ids = {n1, n2, n3};
            if (face.Orientation() == TopAbs_REVERSED) std::swap(node_ids[1], node_ids[2]);

            std::array<VertexIndex, 3> indices;
            for (int i = 0; i < 3; ++i) {
                gp_Pnt point = triangulation->Node(node_ids[i]);
                point.Transform(transform);
                indices[i] = add_vertex(point);
            }

            if (indices[0] == indices[1] || indices[1] == indices[2] || indices[2] == indices[0]) continue;

            auto face_index = surface_mesh.add_face(indices[0], indices[1], indices[2]);
            if (face_index == MeshTypes::SurfaceMesh::null_face())
                surface_mesh.add_face(indices[0], indices[2], indices[1]);
        }
    }

    return surface_mesh;
}

QSharedPointer<MeshBase> buildMeshFromStepSurfaceMesh(MeshTypes::SurfaceMesh& surface_mesh, const QFileInfo& file_info,
                                                      MeshType mesh_type) {
    if (surface_mesh.number_of_faces() == 0 || surface_mesh.number_of_vertices() == 0) return nullptr;

    QSharedPointer<MeshBase> mesh;
    if (CGAL::is_closed(surface_mesh)) {
        MeshTypes::Polyhedron polyhedron;
        CGAL::copy_face_graph(surface_mesh, polyhedron);

        if (GSM->getGlobal()->setting<bool>(PS::SpecialModes::kEnableFixModel)) {
            MeshTypes::Polyhedron repaired_polyhedron = polyhedron;
            try {
                ClosedMesh::RepairResult repair_result = ClosedMesh::CleanPolyhedronWithStatus(repaired_polyhedron);
                if (repair_result == ClosedMesh::RepairResult::kSuccess) { polyhedron = repaired_polyhedron; }
                else {
                    qWarning() << "Model repair did not complete for" << file_info.fileName() << "-"
                               << ClosedMesh::RepairResultDescription(repair_result) << "Importing unrepaired mesh.";
                }
            } catch (const CGAL::Failure_exception& error) {
                qWarning() << "CGAL model repair failed for" << file_info.fileName()
                           << "- importing unrepaired mesh:" << error.what();
            } catch (const std::exception& error) {
                qWarning() << "Model repair failed for" << file_info.fileName()
                           << "- importing unrepaired mesh:" << error.what();
            }
        }

        if (polyhedron.is_closed())
            mesh = QSharedPointer<ClosedMesh>::create(polyhedron, file_info.baseName(), file_info.fileName());
    }

    if (mesh.isNull())
        mesh = QSharedPointer<OpenMesh>::create(surface_mesh, file_info.baseName(), file_info.fileName());

    mesh->setType(mesh_type);
    return mesh;
}

QVector<MeshLoader::MeshData> loadStepMeshes(const QFileInfo& file_info, MeshType mesh_type, QMatrix4x4 transform,
                                             Distance unit, std::pair<void*, size_t> file_data, bool read_from_memory) {
    QVector<MeshLoader::MeshData> loaded_meshes;

    QString step_path = file_info.absoluteFilePath();
    QTemporaryFile temporary_step_file;
    if (read_from_memory) {
        temporary_step_file.setFileTemplate(
            QDir::temp().filePath("ornlslicer_step_XXXXXX." + file_info.suffix().toLower()));
        if (!temporary_step_file.open()) return loaded_meshes;

        if (temporary_step_file.write(static_cast<const char*>(file_data.first), file_data.second) !=
            static_cast<qint64>(file_data.second))
            return loaded_meshes;

        temporary_step_file.close();
        step_path = temporary_step_file.fileName();
    }

    MeshTypes::SurfaceMesh surface_mesh = buildSurfaceMeshFromStep(step_path);
    QSharedPointer<MeshBase> mesh       = buildMeshFromStepSurfaceMesh(surface_mesh, file_info, mesh_type);
    if (mesh.isNull()) return loaded_meshes;

    applyInitialTransform(mesh, transform, unit);
    loaded_meshes.push_back({mesh, file_data.first, file_data.second});

    return loaded_meshes;
}
}  // namespace

MeshLoader::MeshLoader(QString file_path, MeshType mt, QMatrix4x4 transform, Distance unit)
    : m_file_path(file_path), m_mesh_type(mt), m_transform(transform), m_unit(unit) {}

void MeshLoader::run() {
    auto meshes = LoadMeshes(m_file_path, m_mesh_type, m_transform, m_unit);

    if (meshes.isEmpty()) emit error("Error importing mesh: " + QFileInfo(m_file_path).fileName());

    for (auto mesh_data : meshes) emit newMesh(mesh_data);
}

QVector<MeshLoader::MeshData> MeshLoader::LoadMeshes(QString file_path, MeshType mt, QMatrix4x4 transform,
                                                     Distance unit, void* raw_data, size_t file_size) {
    QVector<MeshData> loaded_meshes;

    QFileInfo file_info(file_path);

    std::pair<void*, size_t> file_data;
    const bool read_from_memory = raw_data != nullptr && file_size != 0;

    if (raw_data == nullptr || file_size == 0)  // Data not provided, so load from file
    {
        if (!file_info.exists()) return loaded_meshes;

        file_data = LoadRawData(file_info.absoluteFilePath());
    }
    else
        file_data = std::make_pair(raw_data, file_size);

    if (file_data.first == nullptr || file_data.second == 0) {
        if (!read_from_memory && file_data.first != nullptr) free(file_data.first);
        return loaded_meshes;
    }

    const aiScene* scene = nullptr;

    Assimp::Importer importer;

    auto model_type = file_info.suffix();

    if (isStepFile(model_type)) {
        auto step_meshes = loadStepMeshes(file_info, mt, transform, unit, file_data, read_from_memory);
        if (step_meshes.isEmpty() && !read_from_memory) free(file_data.first);
        return step_meshes;
    }

    if (model_type == "stl" || model_type == "STL") {
        scene =
            importer.ReadFileFromMemory(file_data.first, file_data.second,
                                        aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType,
                                        "stl");  // Tell assimp we are using STL.
    }
    else if (model_type == "3mf" || model_type == "3MF") {
        scene =
            importer.ReadFileFromMemory(file_data.first, file_data.second,
                                        aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType,
                                        "3mf");  // Tell assimp we are using 3mf.
    }
    else if (model_type == "obj" || model_type == "OBJ") {
        scene = importer.ReadFileFromMemory(
            file_data.first, file_data.second,
            aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_SortByPType,
            "obj");  // Tell assimp we are using obj.
    }
    else if (model_type == "amf" || model_type == "AMF") {
        scene = importer.ReadFileFromMemory(
            file_data.first, file_data.second,
            aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_SortByPType,
            "amf");  // Tell assimp we are using obj.
    }
    else {
        scene = importer.ReadFileFromMemory(
            file_data.first, file_data.second,
            aiProcess_DropNormals | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
    }

    if (scene == nullptr) {
        if (!read_from_memory) free(file_data.first);
        return loaded_meshes;
    }

    if (scene->HasMeshes()) {
        int num_models_added = 0;
        for (int i = 0, end = scene->mNumMeshes; i < end; ++i) {
            auto mesh = scene->mMeshes[i];
            if (mesh->mNumFaces > 0 && mesh->mNumVertices > 0) {
                QString name = file_info.baseName();

                if (scene->mNumMeshes > 1) name += "_" + QString::number(num_models_added);

                QSharedPointer<MeshBase> new_mesh;

                // Try to build a closed mesh first
                MeshTypes::Polyhedron polyhedron;
                MeshBuilderAssimp<MeshTypes::HalfedgeDescriptor> builder(mesh);
                polyhedron.delegate(builder);

                if (!builder.wasError() && GSM->getGlobal()->setting<bool>(PS::SpecialModes::kEnableFixModel)) {
                    MeshTypes::Polyhedron repaired_polyhedron = polyhedron;
                    try {
                        ClosedMesh::RepairResult repair_result =
                            ClosedMesh::CleanPolyhedronWithStatus(repaired_polyhedron);
                        if (repair_result == ClosedMesh::RepairResult::kSuccess) { polyhedron = repaired_polyhedron; }
                        else {
                            qWarning() << "Model repair did not complete for" << file_info.fileName() << "-"
                                       << ClosedMesh::RepairResultDescription(repair_result)
                                       << "Importing unrepaired mesh.";
                        }
                    } catch (const CGAL::Failure_exception& error) {
                        qWarning() << "CGAL model repair failed for" << file_info.fileName()
                                   << "- importing unrepaired mesh:" << error.what();
                    } catch (const std::exception& error) {
                        qWarning() << "Model repair failed for" << file_info.fileName()
                                   << "- importing unrepaired mesh:" << error.what();
                    }
                }

                if (builder.wasError() || !polyhedron.is_closed()) {
                    MeshTypes::SurfaceMesh sm = BuildSurfaceMesh(mesh);
                    new_mesh                  = QSharedPointer<OpenMesh>::create(sm, name, file_info.fileName());
                }
                else { new_mesh = QSharedPointer<ClosedMesh>::create(polyhedron, name, file_info.fileName()); }
                new_mesh->setType(mt);

                // Center the mesh about itself
                auto center = new_mesh->originalCentroid();
                new_mesh->center();

                if (transform.isIdentity())  // If the transform was not provided
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

    if (loaded_meshes.isEmpty() && !read_from_memory) free(file_data.first);

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
    if (data == nullptr) return std::make_pair(nullptr, 0);

    int readres = fread(data, 1, fsize, fptr);

    if (readres != fsize) return std::make_pair(nullptr, 0);

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
        auto& face     = mesh->mFaces[i];
        auto face_desc = sm.add_face(points[face.mIndices[0]], points[face.mIndices[1]], points[face.mIndices[2]]);
    }
    return sm;
}
}  // namespace ORNL
