#include "optimizers/optimization_anchor.h"

#include <cmath>

#include <qsharedpointer.h>
#include <qstring.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
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

bool usesCustomIslandLocation(const QSharedPointer<SettingsBase>& sb) {
    const IslandOrderOptimization island_order =
        static_cast<IslandOrderOptimization>(sb->setting<int>(PS::Optimizations::kIslandOrder));
    return island_order == IslandOrderOptimization::kCustomPoint;
}

bool usesCustomPathLocation(const QSharedPointer<SettingsBase>& sb) {
    const PathOrderOptimization path_order =
        static_cast<PathOrderOptimization>(sb->setting<int>(PS::Optimizations::kPathOrder));
    return path_order == PathOrderOptimization::kCustomPoint;
}

bool usesCustomPointOrderLocation(const QSharedPointer<SettingsBase>& sb) {
    const PointOrderOptimization point_order =
        static_cast<PointOrderOptimization>(sb->setting<int>(PS::Optimizations::kPointOrder));
    return usesCustomPointLocation(point_order);
}

QVector3D seamAttractorVector(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                              bool use_seam_attractor_vector) {
    QVector3D direction;
    if (use_seam_attractor_vector) {
        direction = settingVector(sb, PS::Optimizations::kSeamAttractorVectorX,
                                  PS::Optimizations::kSeamAttractorVectorY,
                                  PS::Optimizations::kSeamAttractorVectorZ);
    }

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
                       const Point& optimization_shift, bool use_seam_attractor_vector, const QString& x_key,
                       const QString& y_key, const QString& z_key) {
    return flattenIntoOptimizationFrame(projectAlongVector(settingPoint(sb, x_key, y_key, z_key), slicing_plane,
                                                           seamAttractorVector(sb, slicing_plane,
                                                                               use_seam_attractor_vector)),
                                        slicing_plane, optimization_shift);
}
} // namespace

namespace OptimizationAnchor {
Point customIslandOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                             const Point& optimization_shift) {
    return customOrderPoint(sb, slicing_plane, optimization_shift, usesCustomIslandLocation(sb),
                            PS::Optimizations::kCustomIslandXLocation, PS::Optimizations::kCustomIslandYLocation,
                            PS::Optimizations::kCustomIslandZLocation);
}

Point customPathOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                           const Point& optimization_shift) {
    return customOrderPoint(sb, slicing_plane, optimization_shift, usesCustomPathLocation(sb),
                            PS::Optimizations::kCustomPathXLocation, PS::Optimizations::kCustomPathYLocation,
                            PS::Optimizations::kCustomPathZLocation);
}

Point customPointOrderPoint(const QSharedPointer<SettingsBase>& sb, const Plane& slicing_plane,
                            const Point& optimization_shift) {
    return customOrderPoint(sb, slicing_plane, optimization_shift, usesCustomPointOrderLocation(sb),
                            PS::Optimizations::kCustomPointXLocation, PS::Optimizations::kCustomPointYLocation,
                            PS::Optimizations::kCustomPointZLocation);
}
} // namespace OptimizationAnchor
} // namespace ORNL
