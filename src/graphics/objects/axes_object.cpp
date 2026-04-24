#include "graphics/objects/axes_object.h"

#include <math.h>

#include <vector>

#include <QtMath>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"
#include "graphics/support/shape_factory.h"
#include "utilities/constants.h"

namespace ORNL {
AxesObject::AxesObject(ORNL::BaseView* view, float axis_length) {
    m_starting_length = axis_length;

    // From old base view, could do with some updating.
    QVector<QColor> axesColorList {Constants::Colors::kRed, Constants::Colors::kGreen, Constants::Colors::kBlue};
    double height = axis_length / 4;
    double radius = height / 12;

    QVector3D minPoint;

    QVector<QMatrix4x4> axesCylinderTransforms;
    QMatrix4x4 xAxis;
    xAxis.translate(QVector3D(minPoint.x() - radius, minPoint.y() - radius, minPoint.z()));
    xAxis.rotate(90, QVector3D(0, 1, 0));

    QMatrix4x4 yAxis;
    yAxis.translate(QVector3D(minPoint.x() - radius, minPoint.y() - radius, minPoint.z()));
    yAxis.rotate(-90, QVector3D(1, 0, 0));

    QMatrix4x4 zAxis;
    zAxis.translate(
        QVector3D(minPoint.x() - radius * qSin(M_PI_4), minPoint.x() - radius * qCos(M_PI_4), minPoint.z() - radius));

    axesCylinderTransforms.push_back(xAxis);
    axesCylinderTransforms.push_back(yAxis);
    axesCylinderTransforms.push_back(zAxis);

    QVector<QMatrix4x4> axesConeTransforms;
    QMatrix4x4 xAxis2;
    xAxis2.translate(QVector3D(minPoint.x() - radius + height, minPoint.y() - radius, minPoint.z()));
    xAxis2.rotate(90, QVector3D(0, 1, 0));

    QMatrix4x4 yAxis2;
    yAxis2.translate(QVector3D(minPoint.x() - radius, minPoint.y() - radius + height, minPoint.z()));
    yAxis2.rotate(-90, QVector3D(1, 0, 0));

    QMatrix4x4 zAxis2;
    zAxis2.translate(QVector3D(minPoint.x() - radius * qSin(M_PI_4), minPoint.y() - radius * qCos(M_PI_4),
                               minPoint.z() - radius + height));

    axesConeTransforms.push_back(xAxis2);
    axesConeTransforms.push_back(yAxis2);
    axesConeTransforms.push_back(zAxis2);

    std::vector<float> axesVertices;
    std::vector<float> axesColors;
    std::vector<float> axesNormals;
    for (int i = 0; i < 3; ++i) {
        ShapeFactory::createCylinder(radius, height, axesCylinderTransforms[i], axesColorList[i], axesVertices,
                                     axesColors, axesNormals);
        ShapeFactory::createCone(radius * 1.5, height / 3.0, axesConeTransforms[i], axesColorList[i], axesVertices,
                                 axesColors, axesNormals);
    }

    QMatrix4x4 joint_tfm;
    joint_tfm.translate(-QVector3D(radius, radius, 0) * 3 / 4);
    ShapeFactory::createSphere(radius * 2, 30, 30, joint_tfm, Constants::Colors::kBlack, axesVertices, axesColors,
                               axesNormals);

    this->populateGL(view, axesVertices, axesNormals, axesColors);
}

void AxesObject::updateDimensions(float axis_length) {
    this->scaleAbsolute(
        QVector3D(axis_length / m_starting_length, axis_length / m_starting_length, axis_length / m_starting_length));
}
} // namespace ORNL
