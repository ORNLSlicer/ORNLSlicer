#include "geometry/mesh/mesh_base.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "units/derivative_units.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
MeshBase::MeshBase() {}

MeshBase::MeshBase(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces, const QString& name,
                   const QString& path, MeshType type) {
    m_name = name;
    m_file = path;
    m_type = type;

    m_vertices_original = m_vertices = m_vertices_aligned = vertices;
    m_faces_original = m_faces = m_faces_aligned = faces;

    updateDims();
    m_original_dimensions = m_dimensions;
}

MeshBase::MeshBase(const QSharedPointer<MeshBase> mesh) {
    m_name = mesh->m_name;
    m_file = mesh->m_file;
    m_type = mesh->m_type;
    m_gen_type = mesh->m_gen_type;
    m_imported_unit = mesh->m_imported_unit;
    m_vertices = mesh->m_vertices;
    m_vertices_original = m_vertices_aligned = mesh->m_vertices_aligned;
    m_faces = mesh->m_faces;
    m_faces_original = m_faces_aligned = mesh->m_faces_aligned;
    m_transformation = mesh->m_transformation;
    m_dimensions = mesh->m_dimensions;
    m_original_dimensions = mesh->m_original_dimensions;
    m_min = mesh->m_min;
    m_max = mesh->m_max;
}

const QVector<MeshVertex> MeshBase::vertices() { return m_vertices; }

const QVector<MeshFace> MeshBase::faces() { return m_faces; }

const QVector<MeshVertex> MeshBase::originalVertices() { return m_vertices_aligned; }

const QVector<MeshFace> MeshBase::originalFaces() { return m_faces_aligned; }

void MeshBase::setTransformation(const QMatrix4x4& matrix) {
    QQuaternion rotation;
    if (isRotationalTransform(matrix, rotation)) {
        m_all_transformations.append(m_transformation);
        m_all_transformations.append(matrix);
    }

    m_transformation = matrix;

    m_vertices = m_vertices_aligned;
    m_faces = m_faces_aligned;

    //        QVector3D center = originalCentroid().toQVector3D();
    //        QMatrix4x4 part_center;
    //        part_center.translate(-center);

    // Apply transformation for all vertex.
    for (MeshVertex& vertex : m_vertices) {
        //            vertex.transform(part_center);
        vertex.transform(matrix);
    }

    convert();
    updateDims();
}

bool MeshBase::isRotationalTransform(const QMatrix4x4& matrix, QQuaternion& rotation) {
    QVector3D translation, scale;
    std::tie(translation, rotation, scale) = MathUtils::decomposeTransformMatrix(matrix);
    return (rotation.x() != 0 || rotation.y() != 0 || rotation.z() != 0);
}

void MeshBase::setTransformations(const QVector<QMatrix4x4> matrixes) {
    m_all_transformations.clear();

    for (QMatrix4x4 matrix : matrixes) {
        QQuaternion rotation;
        if (isRotationalTransform(matrix, rotation)) {
            QVector3D translationL, scaleL;
            QQuaternion rotationL;
            std::tie(translationL, rotationL, scaleL) = MathUtils::decomposeTransformMatrix(m_transformation);

            this->setTransformation(MathUtils::composeTransformMatrix(QVector3D(0, 0, 0), rotationL, scaleL));
            this->setTransformation(MathUtils::composeTransformMatrix(QVector3D(0, 0, 0), rotation, scaleL));
            this->alignAxis(
                MathUtils::composeTransformMatrix(translationL, QQuaternion(1, 0, 0, 0), QVector3D(1, 1, 1)));
        }
        else {
            this->setTransformation(matrix);
        }

        m_all_transformations.append(matrix);
    }
}

void MeshBase::alignAxis(const QMatrix4x4& matrix) {
    m_transformation = matrix;
    m_vertices_aligned = m_vertices;
    m_faces_aligned = m_faces;

    convert();
    updateDims();
    m_original_dimensions = m_dimensions;
}

void MeshBase::resetAlignedAxis(const QMatrix4x4& matrix) {
    m_all_transformations.clear();

    m_transformation = matrix;
    m_vertices_aligned = m_vertices = m_vertices_original;
    m_faces_aligned = m_faces = m_faces_original;

    center();
    convert();
    updateDims();
    m_original_dimensions = m_dimensions;
}

const QMatrix4x4& MeshBase::transformation() const { return m_transformation; }

QVector<QMatrix4x4> MeshBase::transformations() {
    if (m_all_transformations.isEmpty() || m_all_transformations.last() != m_transformation)
        m_all_transformations.append(m_transformation);

    return m_all_transformations;
}

void MeshBase::scale(Distance3D bounds) {
    QMatrix4x4 scale_matrix;
    QVector3D scale(1, 1, 1);
    if (bounds.x < m_dimensions.x) {
        scale.setX((bounds.x() / m_dimensions.x()));
    }
    if (bounds.y < m_dimensions.y) {
        scale.setY((bounds.y() / m_dimensions.y()));
    }
    if (bounds.z < m_dimensions.z) {
        scale.setZ((bounds.z() / m_dimensions.z()));
    }
    scale_matrix.scale(scale);
    this->setTransformation(scale_matrix);
}

void MeshBase::scaleUniform(Distance3D bounds) {
    double scale_factor = std::numeric_limits<double>::max();
    if (bounds.x < m_dimensions.x) {
        scale_factor = std::min(scale_factor, (bounds.x() / m_dimensions.x()));
    }
    if (bounds.y < m_dimensions.y) {
        scale_factor = std::min(scale_factor, (bounds.y() / m_dimensions.y()));
    }
    if (bounds.z < m_dimensions.z) {
        scale_factor = std::min(scale_factor, (bounds.z() / m_dimensions.z()));
    }
    QMatrix4x4 scale_matrix;
    QVector3D scale(scale_factor, scale_factor, scale_factor);
    scale_matrix.scale(scale);
    this->setTransformation(scale_matrix);
}

Point MeshBase::centroid() {
    QVector3D center;
    for (MeshVertex mesh_vertex : m_vertices) {
        center += mesh_vertex.location;
    }
    return Point(center / float(m_vertices.size()));
}

Point MeshBase::originalCentroid() {
    QVector3D center;
    for (MeshVertex& mesh_vertex : m_vertices_aligned) {
        center += mesh_vertex.location;
    }
    return Point(center / float(m_vertices_aligned.size()));
}

Distance3D MeshBase::dimensions() { return m_dimensions; }

Distance3D MeshBase::originalDimensions() { return m_original_dimensions; }

Point MeshBase::max() { return m_max; }

Point MeshBase::min() { return m_min; }

std::pair<Point, Point> MeshBase::getAxisExtrema(QVector3D vector) {
    Point min, max;

    Plane plane = Plane(m_min, vector);
    Distance min_distance = plane.distanceToPoint(m_max); // init to max dist
    Distance max_distance = plane.distanceToPoint(m_min); // init to min dist
    for (MeshVertex vertex : m_vertices) {
        Distance distance = plane.distanceToPoint(vertex.location);
        if (distance < min_distance) {
            min_distance = distance;
            min = vertex.location;
        }
        else if (distance > max_distance) {
            max_distance = distance;
            max = vertex.location;
        }
    }

    return std::make_pair(min, max);
}

const QString& MeshBase::name() { return m_name; }

void MeshBase::setName(const QString& name) { m_name = name; }

const QString& MeshBase::path() { return m_file; }

void MeshBase::setPath(const QString& path) { m_file = path; }

Distance MeshBase::unit() { return m_imported_unit; }

void MeshBase::setUnit(const Distance unit) { m_imported_unit = unit; }

const MeshType MeshBase::type() { return m_type; }

void MeshBase::setType(const MeshType type) { m_type = type; }

bool MeshBase::isClosed() { return m_is_closed; }

const MeshGeneratorType MeshBase::genType() { return m_gen_type; }

void MeshBase::setGenType(MeshGeneratorType type) { m_gen_type = type; }

std::pair<float*, uint> MeshBase::glVertexArray() {
    float* vertices_array = new float[9 * m_faces_aligned.size()];
    QVector<MeshVertex> vert = m_vertices_aligned;

    int i = 0;
    for (MeshFace mesh_face : m_faces_aligned) {
        vertices_array[i] = vert.at(mesh_face.vertex_index[0]).location.x();
        vertices_array[i + 1] = vert.at(mesh_face.vertex_index[0]).location.y();
        vertices_array[i + 2] = vert.at(mesh_face.vertex_index[0]).location.z();

        vertices_array[i + 3] = vert.at(mesh_face.vertex_index[1]).location.x();
        vertices_array[i + 4] = vert.at(mesh_face.vertex_index[1]).location.y();
        vertices_array[i + 5] = vert.at(mesh_face.vertex_index[1]).location.z();

        vertices_array[i + 6] = vert.at(mesh_face.vertex_index[2]).location.x();
        vertices_array[i + 7] = vert.at(mesh_face.vertex_index[2]).location.y();
        vertices_array[i + 8] = vert.at(mesh_face.vertex_index[2]).location.z();
        i += 9;
    }

    return std::make_pair(vertices_array, 9 * m_faces_aligned.size() * sizeof(float));
}

std::pair<float*, uint> MeshBase::glNormalArray() {
    float* normals_array = new float[9 * m_faces_aligned.size()];

    int i = 0;
    for (MeshFace mesh_face : m_faces_aligned) {
        normals_array[i] = mesh_face.normal.x();
        normals_array[i + 1] = mesh_face.normal.y();
        normals_array[i + 2] = mesh_face.normal.z();

        normals_array[i + 3] = mesh_face.normal.x();
        normals_array[i + 4] = mesh_face.normal.y();
        normals_array[i + 5] = mesh_face.normal.z();

        normals_array[i + 6] = mesh_face.normal.x();
        normals_array[i + 7] = mesh_face.normal.y();
        normals_array[i + 8] = mesh_face.normal.z();
        i += 9;
    }

    return std::make_pair(normals_array, 9 * m_faces_aligned.size() * sizeof(float));
}

void MeshBase::updateDims() {
    // Min and max.
    m_min.x(Constants::Limits::Maximums::kMaxFloat);
    m_min.y(Constants::Limits::Maximums::kMaxFloat);
    m_min.z(Constants::Limits::Maximums::kMaxFloat);
    m_max.x(Constants::Limits::Minimums::kMinFloat);
    m_max.y(Constants::Limits::Minimums::kMinFloat);
    m_max.z(Constants::Limits::Minimums::kMinFloat);

    // Update min/max.
    for (const MeshVertex& mesh : m_vertices) {
        QVector3D v = mesh.location;

        m_min.x(std::min(v.x(), m_min.x()));
        m_min.y(std::min(v.y(), m_min.y()));
        m_min.z(std::min(v.z(), m_min.z()));
        m_max.x(std::max(v.x(), m_max.x()));
        m_max.y(std::max(v.y(), m_max.y()));
        m_max.z(std::max(v.z(), m_max.z()));
    }

    // Update dimensions.
    m_dimensions = (m_max - m_min).toDistance3D();
}

int MeshBase::GetFaceIdxWithPoints(int idx0, int idx1, int notFaceIdx, QVector<MeshVertex>& vertices) {
    for (unsigned int i = 0; i < vertices[idx0].connected_faces.size(); i++) {
        int f0 = vertices[idx0].connected_faces[i];
        if (f0 == notFaceIdx)
            continue;
        for (unsigned int j = 0; j < vertices[idx1].connected_faces.size(); j++) {
            int f1 = vertices[idx1].connected_faces[j];
            if (f1 == notFaceIdx)
                continue;
            if (f0 == f1)
                return f0;
        }
    }
    return -1;
}

} // namespace ORNL
