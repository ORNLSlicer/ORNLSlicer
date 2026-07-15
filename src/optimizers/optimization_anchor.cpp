#include "optimizers/optimization_anchor.h"

#include <cmath>

#include <qsharedpointer.h>
#include <qstring.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
constexpr float kProjectionDenominatorEpsilon = 1.0e-6f;

Point settingPoint(const QSharedPointer<SettingsBase>& sb, const QString& x_key, const QString& y_key,
                   const QString& z_key) {
    return Point(sb->setting<double>(x_key), sb->setting<double>(y_key), sb->setting<double>(z_key));
}

QVector3D settingVector(const QSharedPointer<SettingsBase>& sb, const QString& x_key, const QString& y_key,
                        const QString& z_key) {
    return QVector3D(sb->setting<float>(x_key), sb->setting<float>(y_key), sb->setting<float>(z_key));
}

QVector3D seamAttractorVector(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane) {
    QVector3D direction = settingVector(sb, PS::Optimizations::kSeamAttractorVectorX,
                                        PS::Optimizations::kSeamAttractorVectorY,
                                        PS::Optimizations::kSeamAttractorVectorZ);

    if (direction.isNull()) {
        direction = slicing_plane.normal();
    }

    if (direction.isNull()) {
        direction = QVector3D(0.0f, 0.0f, 1.0f);
    }

    direction.normalize();
    return direction;
}

Point projectAlongVector(const Point& anchor, const Plane& slicing_plane, QVector3D direction) {
    QVector3D normal = slicing_plane.normal();
    if (normal.isNull()) {
        return anchor;
    }

    normal.normalize();
    if (direction.isNull()) {
        direction = normal;
    }
    else {
        direction.normalize();
    }

    float denominator = QVector3D::dotProduct(direction, normal);
    if (std::fabs(denominator) <= kProjectionDenominatorEpsilon) {
        direction = normal;
        denominator = 1.0f;
    }

    const float distance_to_plane =
        QVector3D::dotProduct((slicing_plane.point() - anchor).toQVector3D(), normal) / denominator;
    return anchor + Point::fromQVector3D(direction * distance_to_plane);
}

Point flattenIntoOptimizationFrame(const Point& point, const Plane& slicing_plane, const Point& optimization_shift) {
    const QVector3D normal = slicing_plane.normal();
    if (normal.isNull()) {
        return point;
    }

    const QQuaternion rotation = MathUtils::CreateQuaternion(normal, QVector3D(0, 0, 1));
    const QVector3D shifted = (point - optimization_shift).toQVector3D();
    return Point::fromQVector3D(rotation.rotatedVector(shifted)) + optimization_shift;
}

Point customOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                       const Point& optimization_shift, const QString& x_key, const QString& y_key,
                       const QString& z_key) {
    return flattenIntoOptimizationFrame(projectAlongVector(settingPoint(sb, x_key, y_key, z_key), slicing_plane,
                                                           seamAttractorVector(sb, slicing_plane)),
                                        slicing_plane, optimization_shift);
}
} // namespace

namespace OptimizationAnchor {
Point customIslandOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                             const Point& optimization_shift) {
    return customOrderPoint(sb, slicing_plane, optimization_shift, PS::Optimizations::kCustomIslandXLocation,
                            PS::Optimizations::kCustomIslandYLocation, PS::Optimizations::kCustomIslandZLocation);
}

Point customPathOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                           const Point& optimization_shift) {
    return customOrderPoint(sb, slicing_plane, optimization_shift, PS::Optimizations::kCustomPathXLocation,
                            PS::Optimizations::kCustomPathYLocation, PS::Optimizations::kCustomPathZLocation);
}

Point customPointOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                            const Point& optimization_shift) {
    return customOrderPoint(sb, slicing_plane, optimization_shift, PS::Optimizations::kCustomPointXLocation,
                            PS::Optimizations::kCustomPointYLocation, PS::Optimizations::kCustomPointZLocation);
}
} // namespace OptimizationAnchor
} // namespace ORNL
