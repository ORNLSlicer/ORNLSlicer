#include "graphics/objects/grid_object.h"

#include <GL/gl.h>

#include <vector>

#include <qcolor.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"
#include "graphics/support/shape_factory.h"

namespace ORNL {

GridObject::GridObject(BaseView* view, float length, float width, float x_grid, float y_grid, QColor color) {
    m_length = length;
    m_width = width;
    m_x_grid = x_grid;
    m_y_grid = y_grid;
    m_color = color;

    std::vector<float> vertices;
    std::vector<float> colors;

    ShapeFactory::createGridPlane(m_length, m_width, m_x_grid, m_y_grid, m_color, vertices, colors);

    std::vector<float> tmp_norm;
    this->populateGL(view, vertices, tmp_norm, colors, GL_LINES);

    // Get collision box - Only need vertices.
    ShapeFactory::createRectangle(length, width, 0.1, QMatrix4x4(), m_color, m_collision_vertices, colors, tmp_norm);
}

void GridObject::updateDimensions(float length, float width, float x_grid, float y_grid) {
    m_length = length;
    m_width = width;

    m_x_grid = (x_grid > 0) ? x_grid : m_x_grid;
    m_y_grid = (y_grid > 0) ? y_grid : m_y_grid;

    std::vector<float> vertices;
    std::vector<float> colors;

    ShapeFactory::createGridPlane(m_length, m_width, m_x_grid, m_y_grid, m_color, vertices, colors);

    this->replaceVertices(vertices);
    this->replaceColors(colors);

    std::vector<float> tmp_norm;
    m_collision_vertices.clear();
    ShapeFactory::createRectangle(m_length, m_width, 0.1, QMatrix4x4(), m_color, m_collision_vertices, colors,
                                  tmp_norm);
}

std::vector<Triangle> GridObject::triangles() {
    // 3 vertices specify a triangle, but each vertex refers to 3 floats
    std::vector<Triangle> triangles;
    Triangle current_triangle;

    for (int i = 0; i < m_collision_vertices.size(); i += 9) {
        current_triangle.a =
            this->transformation() *
            QVector3D(m_collision_vertices[i + 0], m_collision_vertices[i + 1], m_collision_vertices[i + 2]);

        current_triangle.b =
            this->transformation() *
            QVector3D(m_collision_vertices[i + 3], m_collision_vertices[i + 4], m_collision_vertices[i + 5]);

        current_triangle.c =
            this->transformation() *
            QVector3D(m_collision_vertices[i + 6], m_collision_vertices[i + 7], m_collision_vertices[i + 8]);

        triangles.push_back(current_triangle);
    }

    return triangles;
}

} // namespace ORNL
