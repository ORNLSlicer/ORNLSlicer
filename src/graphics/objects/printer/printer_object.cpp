#include "graphics/objects/printer/printer_object.h"

#include <math.h>

#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

#include <QMatrix4x4>
#include <QPointF>
#include <QVector>
#include <qcolor.h>
#include <qmath.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "graphics/objects/sphere/seam_object.h"
#include "graphics/support/part_picker.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
constexpr float kMinimumOptimizationGuideLength = 1.5f;
constexpr float kOptimizationGuidePrinterScale = 0.5f;

QSharedPointer<GraphicsObject> createOptimizationGuide(BaseView* view, QColor color) {
    const std::vector<float> vertices = {-0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};
    const std::vector<float> colors = {color.redF(), color.greenF(), color.blueF(), color.alphaF(),
                                       color.redF(), color.greenF(), color.blueF(), color.alphaF()};
    const std::vector<float> normals;

    auto guide = QSharedPointer<GraphicsObject>::create(view, vertices, normals, colors, GL_LINES);
    guide->setOnTop(true);
    guide->hide();

    return guide;
}

QVector3D optimizationGuideDirection(const QSharedPointer<SettingsBase>& sb) {
    QVector3D direction(sb->setting<float>(PS::Slicing::kSlicingVectorX),
                        sb->setting<float>(PS::Slicing::kSlicingVectorY),
                        sb->setting<float>(PS::Slicing::kSlicingVectorZ));

    if (direction.isNull()) {
        direction = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else {
        direction.normalize();
    }

    return direction;
}

QVector3D optimizationPointTranslation(const QSharedPointer<SettingsBase>& sb, const QString& x_setting,
                                       const QString& y_setting, const QString& z_setting, float bed_z) {
    QVector3D translation(sb->setting<double>(x_setting), sb->setting<double>(y_setting),
                          sb->setting<double>(z_setting));

    translation *= Constants::OpenGL::kObjectToView;
    translation.setZ(bed_z + translation.z());

    return translation;
}

void updateOptimizationGuide(const QSharedPointer<GraphicsObject>& guide, const QVector3D& center,
                             const QVector3D& direction, float length) {
    QMatrix4x4 transform;
    transform.translate(center);
    transform.rotate(QQuaternion::rotationTo(QVector3D(1.0f, 0.0f, 0.0f), direction));
    transform.scale(length);

    guide->setTransformation(transform);
    guide->show();
}

void showOptimizationAnchor(const QSharedPointer<SeamObject>& marker, const QSharedPointer<GraphicsObject>& guide,
                            const QVector3D& translation, const QVector3D& direction, float guide_length) {
    marker->translateAbsolute(translation);
    marker->show();
    updateOptimizationGuide(guide, translation, direction, guide_length);
}

void hideOptimizationAnchor(const QSharedPointer<SeamObject>& marker, const QSharedPointer<GraphicsObject>& guide) {
    marker->hide();
    guide->hide();
}
} // namespace

void PrinterObject::updateFromSettings(QSharedPointer<SettingsBase> sb) {
    m_sb = sb;

    this->updateMembers();
    this->updateGeometry();
    this->updateSeams();
}

void PrinterObject::setSeamsHidden(bool hide) {
    m_seams_shown = !hide;

    this->updateSeams();
}

PrinterObject::OptimizationPointPick PrinterObject::pickOptimizationPoint(const QMatrix4x4& projection,
                                                                          const QMatrix4x4& view, QPointF mouse_ndc_pos,
                                                                          bool ortho) {
    QVector<OptimizationPointPick> candidates;
    candidates.push_back({m_seams.custom_island_opt, PS::Optimizations::kCustomIslandXLocation,
                          PS::Optimizations::kCustomIslandYLocation});
    candidates.push_back({m_seams.custom_path_opt, PS::Optimizations::kCustomPathXLocation,
                          PS::Optimizations::kCustomPathYLocation});
    candidates.push_back({m_seams.custom_point_opt, PS::Optimizations::kCustomPointXLocation,
                          PS::Optimizations::kCustomPointYLocation});
    candidates.push_back({m_seams.custom_point_second_opt, PS::Optimizations::kCustomPointSecondXLocation,
                          PS::Optimizations::kCustomPointSecondYLocation});

    float nearest = std::numeric_limits<float>::infinity();
    OptimizationPointPick picked;
    for (const OptimizationPointPick& candidate : candidates) {
        if (!candidate.isValid() || candidate.object->hidden()) {
            continue;
        }

        const float distance =
            PartPicker::pickDistance(projection, view, mouse_ndc_pos, candidate.object->triangles(), ortho);
        if (distance < nearest) {
            nearest = distance;
            picked = candidate;
        }
    }

    return picked;
}

bool PrinterObject::bedIntersection(const QMatrix4x4& projection, const QMatrix4x4& view, QPointF mouse_ndc_pos,
                                    QVector3D& intersection, bool ortho) {
    QVector3D start;
    QVector3D direction;
    std::tie(start, direction) = PartPicker::getDirectionAndStart(projection, mouse_ndc_pos, view, ortho);

    if (qFuzzyIsNull(direction.z())) {
        return false;
    }

    const float distance = (this->printerCenter().z() - start.z()) / direction.z();
    if (distance < 0.0f) {
        return false;
    }

    intersection = start + (distance * direction);
    return true;
}

float PrinterObject::getDefaultZoom() {
    if (m_printer_max_dims.y() > m_printer_max_dims.z()) {
        float offset = (0.5 * m_printer_max_dims.y()) / qTan((1.0 / 2.0) * M_PI);
        float base = offset + (m_printer_max_dims.x()) - printerCenter().x();
        return base / qTan((3.0 / 18.0) * M_PI);
    }
    else {
        float offset = (0.5 * m_printer_max_dims.z()) / qCos((3.0 / 18.0) * M_PI);
        float base = offset + (m_printer_max_dims.x()) - printerCenter().x();
        return base / qTan((3.0 / 18.0) * M_PI);
    }
}

void PrinterObject::updateSeams() {
    if (!m_seams_shown) {
        hideOptimizationAnchor(m_seams.custom_island_opt, m_seams.custom_island_guide);
        hideOptimizationAnchor(m_seams.custom_path_opt, m_seams.custom_path_guide);
        hideOptimizationAnchor(m_seams.custom_point_opt, m_seams.custom_point_guide);
        hideOptimizationAnchor(m_seams.custom_point_second_opt, m_seams.custom_point_second_guide);
        return;
    }

    IslandOrderOptimization islandOrder =
        static_cast<IslandOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kIslandOrder));
    PathOrderOptimization pathOrder =
        static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder));
    PointOrderOptimization pointOrder =
        static_cast<PointOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPointOrder));
    bool secondPointEnabled = m_sb->setting<bool>(PS::Optimizations::kEnableSecondCustomLocation);
    const float bed_z = this->printerCenter().z();
    const QVector3D guide_direction = optimizationGuideDirection(m_sb);
    const float max_printer_dimension =
        std::max(std::max(m_printer_max_dims.x(), m_printer_max_dims.y()), m_printer_max_dims.z());
    const float guide_length =
        std::max(kMinimumOptimizationGuideLength, max_printer_dimension * kOptimizationGuidePrinterScale);

    if (islandOrder == IslandOrderOptimization::kCustomPoint) {
        const QVector3D translation =
            optimizationPointTranslation(m_sb, PS::Optimizations::kCustomIslandXLocation,
                                         PS::Optimizations::kCustomIslandYLocation,
                                         PS::Optimizations::kCustomIslandZLocation, bed_z);

        showOptimizationAnchor(m_seams.custom_island_opt, m_seams.custom_island_guide, translation, guide_direction,
                               guide_length);
    }
    else {
        hideOptimizationAnchor(m_seams.custom_island_opt, m_seams.custom_island_guide);
    }

    if (pathOrder == PathOrderOptimization::kCustomPoint) {
        const QVector3D translation =
            optimizationPointTranslation(m_sb, PS::Optimizations::kCustomPathXLocation,
                                         PS::Optimizations::kCustomPathYLocation,
                                         PS::Optimizations::kCustomPathZLocation, bed_z);

        showOptimizationAnchor(m_seams.custom_path_opt, m_seams.custom_path_guide, translation, guide_direction,
                               guide_length);
    }
    else {
        hideOptimizationAnchor(m_seams.custom_path_opt, m_seams.custom_path_guide);
    }

    if (usesCustomPointLocation(pointOrder)) {
        const QVector3D translation =
            optimizationPointTranslation(m_sb, PS::Optimizations::kCustomPointXLocation,
                                         PS::Optimizations::kCustomPointYLocation,
                                         PS::Optimizations::kCustomPointZLocation, bed_z);

        showOptimizationAnchor(m_seams.custom_point_opt, m_seams.custom_point_guide, translation, guide_direction,
                               guide_length);

        if (secondPointEnabled) {
            const QVector3D secondTranslation =
                optimizationPointTranslation(m_sb, PS::Optimizations::kCustomPointSecondXLocation,
                                             PS::Optimizations::kCustomPointSecondYLocation,
                                             PS::Optimizations::kCustomPointSecondZLocation, bed_z);

            showOptimizationAnchor(m_seams.custom_point_second_opt, m_seams.custom_point_second_guide,
                                   secondTranslation, guide_direction, guide_length);
        }
        else {
            hideOptimizationAnchor(m_seams.custom_point_second_opt, m_seams.custom_point_second_guide);
        }
    }
    else {
        hideOptimizationAnchor(m_seams.custom_point_opt, m_seams.custom_point_guide);
        hideOptimizationAnchor(m_seams.custom_point_second_opt, m_seams.custom_point_second_guide);
    }
}

void PrinterObject::setSettings(QSharedPointer<SettingsBase> sb) { m_sb = sb; }

QSharedPointer<SettingsBase> PrinterObject::getSettings() { return m_sb; }

void PrinterObject::createSeams() {
    const QColor island_color =
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kPerimeter);
    const QColor path_color = PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSkin);
    const QColor point_color = PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInset);
    const QColor second_point_color =
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInfill);

    m_seams.custom_island_opt = QSharedPointer<SeamObject>::create(this->view(), island_color);
    m_seams.custom_path_opt = QSharedPointer<SeamObject>::create(this->view(), path_color);
    m_seams.custom_point_opt = QSharedPointer<SeamObject>::create(this->view(), point_color);
    m_seams.custom_point_second_opt = QSharedPointer<SeamObject>::create(this->view(), second_point_color);

    m_seams.custom_island_guide = createOptimizationGuide(this->view(), island_color);
    m_seams.custom_path_guide = createOptimizationGuide(this->view(), path_color);
    m_seams.custom_point_guide = createOptimizationGuide(this->view(), point_color);
    m_seams.custom_point_second_guide = createOptimizationGuide(this->view(), second_point_color);

    this->adoptChild(m_seams.custom_island_opt);
    this->adoptChild(m_seams.custom_path_opt);
    this->adoptChild(m_seams.custom_point_opt);
    this->adoptChild(m_seams.custom_point_second_opt);
    this->adoptChild(m_seams.custom_island_guide);
    this->adoptChild(m_seams.custom_path_guide);
    this->adoptChild(m_seams.custom_point_guide);
    this->adoptChild(m_seams.custom_point_second_guide);

    hideOptimizationAnchor(m_seams.custom_island_opt, m_seams.custom_island_guide);
    hideOptimizationAnchor(m_seams.custom_path_opt, m_seams.custom_path_guide);
    hideOptimizationAnchor(m_seams.custom_point_opt, m_seams.custom_point_guide);
    hideOptimizationAnchor(m_seams.custom_point_second_opt, m_seams.custom_point_second_guide);
}

bool PrinterObject::isTrueVolume() { return m_is_true_volume; }

PrinterObject::PrinterObject(bool is_true_volume) { m_is_true_volume = is_true_volume; }
} // namespace ORNL
