#include "geometry/mesh/mesh_factory.h"

#include <math.h>

#include <cmath>

#include <qcontainerfwd.h>
#include <qmath.h>
#include <qvectornd.h>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/mesh/open_mesh.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
MeshFactory::MeshFactory() {}

ClosedMesh MeshFactory::CreateBoxMesh(const Distance& length, const Distance& width, const Distance& height) {
    QVector<MeshFace> faces;
    QVector<MeshVertex> vertices;

    // Build a rectangular prism mesh
    vertices.push_back(MeshVertex(QVector3D(0, 0, 0)));
    vertices.push_back(MeshVertex(QVector3D(length(), 0, 0)));
    vertices.push_back(MeshVertex(QVector3D(0, width(), 0)));
    vertices.push_back(MeshVertex(QVector3D(length(), width(), 0)));
    vertices.push_back(MeshVertex(QVector3D(0, 0, height())));
    vertices.push_back(MeshVertex(QVector3D(length(), 0, height())));
    vertices.push_back(MeshVertex(QVector3D(0, width(), height())));
    vertices.push_back(MeshVertex(QVector3D(length(), width(), height())));

    // Walls
    faces.push_back(MeshFace(new int[3] {1, 4, 0}, new int[3] {1, 11, 2}, QVector3D(0, -1, 0)));
    faces.push_back(MeshFace(new int[3] {1, 5, 4}, new int[3] {0, 8, 7}, QVector3D(0, -1, 0)));

    faces.push_back(MeshFace(new int[3] {0, 4, 6}, new int[3] {3, 0, 8}, QVector3D(-1, 0, 0)));
    faces.push_back(MeshFace(new int[3] {0, 6, 2}, new int[3] {2, 11, 4}, QVector3D(-1, 0, 0)));

    faces.push_back(MeshFace(new int[3] {2, 6, 7}, new int[3] {5, 3, 9}, QVector3D(0, 1, 0)));
    faces.push_back(MeshFace(new int[3] {2, 7, 3}, new int[3] {4, 6, 10}, QVector3D(0, 1, 0)));

    faces.push_back(MeshFace(new int[3] {3, 7, 5}, new int[3] {7, 5, 9}, QVector3D(1, 0, 0)));
    faces.push_back(MeshFace(new int[3] {3, 5, 1}, new int[3] {6, 1, 10}, QVector3D(1, 0, 0)));

    // Top / Bottom
    faces.push_back(MeshFace(new int[3] {6, 4, 5}, new int[3] {9, 1, 2}, QVector3D(0, 0, 1)));
    faces.push_back(MeshFace(new int[3] {6, 5, 7}, new int[3] {8, 6, 4}, QVector3D(0, 0, 1)));

    faces.push_back(MeshFace(new int[3] {1, 2, 3}, new int[3] {11, 7, 5}, QVector3D(0, 0, -1)));
    faces.push_back(MeshFace(new int[3] {0, 2, 1}, new int[3] {10, 0, 3}, QVector3D(0, 0, -1)));

    ClosedMesh new_mesh(vertices, faces);
    new_mesh.setGenType(MeshGeneratorType::kRectangularBox);
    return new_mesh;
}

OpenMesh MeshFactory::CreateOpenTopBoxMesh(const Distance& length, const Distance& width, const Distance& height) {
    QVector<MeshFace> faces;
    QVector<MeshVertex> vertices;

    // Build a rectangular prism mesh
    vertices.push_back(MeshVertex(QVector3D(0, 0, 0)));
    vertices.push_back(MeshVertex(QVector3D(length(), 0, 0)));
    vertices.push_back(MeshVertex(QVector3D(0, width(), 0)));
    vertices.push_back(MeshVertex(QVector3D(length(), width(), 0)));
    vertices.push_back(MeshVertex(QVector3D(0, 0, height())));
    vertices.push_back(MeshVertex(QVector3D(length(), 0, height())));
    vertices.push_back(MeshVertex(QVector3D(0, width(), height())));
    vertices.push_back(MeshVertex(QVector3D(length(), width(), height())));

    faces.push_back(MeshFace(new int[3] {1, 4, 0}, new int[3] {0, 0, 0}, QVector3D(0, -1, 0)));
    faces.push_back(MeshFace(new int[3] {1, 5, 4}, new int[3] {0, 0, 0}, QVector3D(0, -1, 0)));

    faces.push_back(MeshFace(new int[3] {0, 4, 6}, new int[3] {0, 0, 0}, QVector3D(-1, 0, 0)));
    faces.push_back(MeshFace(new int[3] {0, 6, 2}, new int[3] {0, 0, 0}, QVector3D(-1, 0, 0)));

    faces.push_back(MeshFace(new int[3] {2, 6, 7}, new int[3] {0, 0, 0}, QVector3D(0, 1, 0)));
    faces.push_back(MeshFace(new int[3] {2, 7, 3}, new int[3] {0, 0, 0}, QVector3D(0, 1, 0)));

    faces.push_back(MeshFace(new int[3] {3, 7, 5}, new int[3] {0, 0, 0}, QVector3D(1, 0, 0)));
    faces.push_back(MeshFace(new int[3] {3, 5, 1}, new int[3] {0, 0, 0}, QVector3D(1, 0, 0)));

    faces.push_back(MeshFace(new int[3] {0, 4, 1}, new int[3] {0, 0, 0}, QVector3D(0, 1, 0), true));
    faces.push_back(MeshFace(new int[3] {4, 5, 1}, new int[3] {0, 0, 0}, QVector3D(0, 1, 0), true));

    faces.push_back(MeshFace(new int[3] {6, 4, 0}, new int[3] {0, 0, 0}, QVector3D(1, 0, 0), true));
    faces.push_back(MeshFace(new int[3] {2, 6, 0}, new int[3] {0, 0, 0}, QVector3D(1, 0, 0), true));

    faces.push_back(MeshFace(new int[3] {7, 6, 2}, new int[3] {0, 0, 0}, QVector3D(0, -1, 0), true));
    faces.push_back(MeshFace(new int[3] {3, 7, 2}, new int[3] {0, 0, 0}, QVector3D(0, -1, 0), true));

    faces.push_back(MeshFace(new int[3] {5, 7, 3}, new int[3] {0, 0, 0}, QVector3D(-1, 0, 0), true));
    faces.push_back(MeshFace(new int[3] {1, 5, 3}, new int[3] {0, 0, 0}, QVector3D(-1, 0, 0), true));

    OpenMesh new_mesh(vertices, faces);
    new_mesh.setGenType(MeshGeneratorType::kOpenTopBox);
    return new_mesh;
}

ClosedMesh MeshFactory::CreateTriaglePyramidMesh(const Distance& length) {
    QVector<MeshFace> faces;
    QVector<MeshVertex> vertices;

    vertices.push_back(MeshVertex(QVector3D(0, 0, 0)));
    vertices.push_back(MeshVertex(QVector3D(0, length(), 0)));
    vertices.push_back(MeshVertex(QVector3D(qSqrt(qPow(length(), 2) - qPow(length() / 2.0, 2)), length() / 2.0, 0)));
    double x = (length() / 2.0) * 0.57735; // tan(30 deg)
    double y = length() / 2.0;
    double h = qSqrt(qPow(x, 2) + qPow(y, 2));
    double height = qSqrt(qPow(length(), 2) - qPow(h, 2));
    vertices.push_back(MeshVertex(QVector3D(x, y, height)));

    faces.push_back(MeshFace(new int[3] {0, 1, 2}, new int[3] {1, 2, 3},
                             QVector3D::crossProduct(vertices[0].location - vertices[2].location,
                                                     vertices[1].location - vertices[2].location)
                                 .normalized()));
    faces.push_back(MeshFace(new int[3] {0, 3, 1}, new int[3] {0, 2, 3},
                             QVector3D::crossProduct(vertices[0].location - vertices[3].location,
                                                     vertices[1].location - vertices[3].location)
                                 .normalized()));
    faces.push_back(MeshFace(new int[3] {1, 3, 2}, new int[3] {0, 1, 3},
                             QVector3D::crossProduct(vertices[1].location - vertices[3].location,
                                                     vertices[2].location - vertices[3].location)
                                 .normalized()));
    faces.push_back(MeshFace(new int[3] {0, 2, 3}, new int[3] {0, 1, 2},
                             QVector3D::crossProduct(vertices[2].location - vertices[3].location,
                                                     vertices[1].location - vertices[3].location)
                                 .normalized()));

    ClosedMesh new_mesh(vertices, faces);
    new_mesh.setGenType(MeshGeneratorType::kTriangularPyramid);
    return new_mesh;
}

ClosedMesh MeshFactory::CreateCylinderMesh(const Distance& radius, const Distance& height, const int resolution) {
    QVector<MeshFace> faces;
    QVector<MeshVertex> vertices;

    // Bottom circle vertices
    for (int i = 0; i < resolution; ++i) {
        double theta = 2.0f * M_PI * float(i) / double(resolution);

        double x = radius() * cosf(theta);
        double y = radius() * sinf(theta);

        vertices.push_back(MeshVertex(QVector3D(x + radius(), y + radius(), 0)));
    }

    // Top circle vertices
    for (int i = 0; i < resolution; ++i) {
        double theta = 2.0f * M_PI * float(i) / double(resolution);

        double x = radius() * cosf(theta);
        double y = radius() * sinf(theta);

        vertices.push_back(MeshVertex(QVector3D(x + radius(), y + radius(), height())));
    }

    int bottom_center_index = vertices.size();
    vertices.push_back(QVector3D(radius(), radius(), 0)); // Bottom center
    int top_center_index = vertices.size();
    vertices.push_back(QVector3D(radius(), radius(), height())); // Top center

    // Side faces
    for (int i = 0; i < resolution; ++i) {
        // Add first triangle
        int* v1 = new int[3] {i, (i + 1) % resolution, ((i + 1) % resolution) + resolution};
        auto normal1 = QVector3D::crossProduct(vertices[v1[0]].location - vertices[v1[2]].location,
                                               vertices[v1[1]].location - vertices[v1[2]].location)
                           .normalized();
        faces.push_back(MeshFace(v1, new int[3], normal1));

        // Add second triangle
        int* v2 = new int[3] {((i + 1) % resolution) + resolution, i + resolution, i};
        auto normal2 = QVector3D::crossProduct(vertices[v2[0]].location - vertices[v2[2]].location,
                                               vertices[v2[1]].location - vertices[v2[2]].location)
                           .normalized();
        faces.push_back(MeshFace(v2, new int[3], normal2));
    }

    // Bottom circle faces
    for (int i = 0; i < resolution; ++i) {
        int* v = new int[3] {bottom_center_index, (i + 1) % resolution, i};

        auto normal = QVector3D::crossProduct(vertices[v[0]].location - vertices[v[2]].location,
                                              vertices[v[1]].location - vertices[v[2]].location)
                          .normalized();

        faces.push_back(MeshFace(v, new int[3], normal));
    }

    // Bottom circle faces
    for (int i = resolution; i < (resolution * 2); ++i) {
        int next = i + 1;
        if (next == resolution * 2)
            next = resolution;
        int* v = new int[3] {i, next, top_center_index};

        auto normal = QVector3D::crossProduct(vertices[v[0]].location - vertices[v[2]].location,
                                              vertices[v[1]].location - vertices[v[2]].location)
                          .normalized();

        faces.push_back(MeshFace(v, new int[3], normal));
    }

    ClosedMesh m(vertices, faces);
    m.setGenType(MeshGeneratorType::kCylinder);
    return m;
}

ClosedMesh MeshFactory::CreateConeMesh(const Distance& radius, const Distance& height, const int resolution) {
    QVector<MeshFace> faces;
    QVector<MeshVertex> vertices;

    // Bottom circle vertices
    for (int i = 0; i < resolution; ++i) {
        double theta = 2.0f * M_PI * float(i) / double(resolution);

        double x = radius() * cosf(theta);
        double y = radius() * sinf(theta);

        vertices.push_back(MeshVertex(QVector3D(x + radius(), y + radius(), 0)));
    }

    int bottom_center_index = vertices.size();
    vertices.push_back(QVector3D(radius(), radius(), 0)); // Bottom center

    // Bottom circle faces
    for (int i = 0; i < resolution; ++i) {
        int* v = new int[3] {bottom_center_index, (i + 1) % resolution, i};

        auto normal = QVector3D::crossProduct(vertices[v[0]].location - vertices[v[2]].location,
                                              vertices[v[1]].location - vertices[v[2]].location)
                          .normalized();

        faces.push_back(MeshFace(v, new int[3], normal));
    }

    int tip_index = vertices.size();
    vertices.push_back(QVector3D(radius(), radius(), height())); // Tip

    // Side faces
    for (int i = 0; i < resolution; ++i) {
        int* v = new int[3] {i, (i + 1) % resolution, tip_index};

        auto normal = QVector3D::crossProduct(vertices[v[0]].location - vertices[v[2]].location,
                                              vertices[v[1]].location - vertices[v[2]].location)
                          .normalized();

        faces.push_back(MeshFace(v, new int[3], normal));
    }

    ClosedMesh m(vertices, faces);
    m.setGenType(MeshGeneratorType::kCone);
    return m;
}

} // namespace ORNL
