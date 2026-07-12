#include "graphics/objects/printer/printer_object.h"

#include <math.h>

#include <limits>
#include <tuple>

#include <QMatrix4x4>
#include <QPointF>
#include <QVector>
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
        m_seams.custom_island_opt->hide();
        m_seams.custom_path_opt->hide();
        m_seams.custom_point_opt->hide();
        m_seams.custom_point_second_opt->hide();
        return;
    }

    IslandOrderOptimization islandOrder =
        static_cast<IslandOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kIslandOrder));
    PathOrderOptimization pathOrder =
        static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder));
    PointOrderOptimization pointOrder =
        static_cast<PointOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPointOrder));
    bool secondPointEnabled = m_sb->setting<bool>(PS::Optimizations::kEnableSecondCustomLocation);
    float bed_z = this->printerCenter().z() * Constants::OpenGL::kViewToObject;

    if (islandOrder == IslandOrderOptimization::kCustomPoint) {
        QVector3D translation(m_sb->setting<double>(PS::Optimizations::kCustomIslandXLocation),

                              m_sb->setting<double>(PS::Optimizations::kCustomIslandYLocation),

                              bed_z);

        translation *= Constants::OpenGL::kObjectToView;

        m_seams.custom_island_opt->translateAbsolute(translation);
        m_seams.custom_island_opt->show();
    }
    else {
        m_seams.custom_island_opt->hide();
    }

    if (pathOrder == PathOrderOptimization::kCustomPoint) {
        QVector3D translation(m_sb->setting<double>(PS::Optimizations::kCustomPathXLocation),

                              m_sb->setting<double>(PS::Optimizations::kCustomPathYLocation),

                              bed_z);

        translation *= Constants::OpenGL::kObjectToView;

        m_seams.custom_path_opt->translateAbsolute(translation);
        m_seams.custom_path_opt->show();
    }
    else {
        m_seams.custom_path_opt->hide();
    }

    if (usesCustomPointLocation(pointOrder)) {
        QVector3D translation(m_sb->setting<double>(PS::Optimizations::kCustomPointXLocation),

                              m_sb->setting<double>(PS::Optimizations::kCustomPointYLocation),

                              bed_z);

        translation *= Constants::OpenGL::kObjectToView;

        m_seams.custom_point_opt->translateAbsolute(translation);
        m_seams.custom_point_opt->show();

        if (secondPointEnabled) {
            QVector3D secondTranslation(m_sb->setting<double>(PS::Optimizations::kCustomPointSecondXLocation),

                                        m_sb->setting<double>(PS::Optimizations::kCustomPointSecondYLocation),

                                        bed_z);

            secondTranslation *= Constants::OpenGL::kObjectToView;

            m_seams.custom_point_second_opt->translateAbsolute(secondTranslation);
            m_seams.custom_point_second_opt->show();
        }
        else {
            m_seams.custom_point_second_opt->hide();
        }
    }
    else {
        m_seams.custom_point_opt->hide();
        m_seams.custom_point_second_opt->hide();
    }
}

void PrinterObject::setSettings(QSharedPointer<SettingsBase> sb) { m_sb = sb; }

QSharedPointer<SettingsBase> PrinterObject::getSettings() { return m_sb; }

void PrinterObject::createSeams() {

    m_seams.custom_island_opt = QSharedPointer<SeamObject>::create(
        this->view(), PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kPerimeter));
    m_seams.custom_path_opt = QSharedPointer<SeamObject>::create(
        this->view(), PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSkin));
    m_seams.custom_point_opt = QSharedPointer<SeamObject>::create(
        this->view(), PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInset));
    m_seams.custom_point_second_opt = QSharedPointer<SeamObject>::create(
        this->view(), PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInfill));

    this->adoptChild(m_seams.custom_island_opt);
    this->adoptChild(m_seams.custom_path_opt);
    this->adoptChild(m_seams.custom_point_opt);
    this->adoptChild(m_seams.custom_point_second_opt);

    m_seams.custom_island_opt->hide();
    m_seams.custom_path_opt->hide();
    m_seams.custom_point_opt->hide();
    m_seams.custom_point_second_opt->hide();
}

bool PrinterObject::isTrueVolume() { return m_is_true_volume; }

PrinterObject::PrinterObject(bool is_true_volume) { m_is_true_volume = is_true_volume; }
} // namespace ORNL
