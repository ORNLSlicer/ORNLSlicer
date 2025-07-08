#include "graphics/objects/printer/printer_object.h"

// Local
#include "graphics/objects/sphere/seam_object.h"
#include "managers/preferences_manager.h"

namespace ORNL {
    void PrinterObject::updateFromSettings(QSharedPointer<SettingsBase> sb)
    {
        m_sb = sb;

        this->updateMembers();
        this->updateGeometry();
        this->updateSeams();
    }

    void PrinterObject::setSeamsHidden(bool hide)
    {
        m_seams_shown = !hide;

        this->updateSeams();
    }

    float PrinterObject::getDefaultZoom()
    {
        if(m_printer_max_dims.y() > m_printer_max_dims.z())
        {
            float offset = (0.5 * m_printer_max_dims.y()) / qTan((1.0 / 2.0) * M_PI);
            float base = offset + (m_printer_max_dims.x()) - printerCenter().x();
            return base / qTan((3.0 / 18.0) * M_PI);
        }else
        {
            float offset = (0.5 * m_printer_max_dims.z()) / qCos((3.0 / 18.0) * M_PI);
            float base = offset + (m_printer_max_dims.x()) - printerCenter().x();
            return base / qTan((3.0 / 18.0) * M_PI);
        }

    }

    void PrinterObject::updateSeams()
    {
        if (!m_seams_shown)
        {
            m_seams.custom_island_opt->hide();
            m_seams.custom_path_opt->hide();
            m_seams.custom_point_opt->hide();
            m_seams.custom_point_second_opt->hide();
            return;
        }

        IslandOrderOptimization islandOrder = static_cast<IslandOrderOptimization>(m_sb->setting<int>(Constants::ProfileSettings::Optimizations::kIslandOrder));
        PathOrderOptimization pathOrder = static_cast<PathOrderOptimization>(m_sb->setting<int>(Constants::ProfileSettings::Optimizations::kPathOrder));
        PointOrderOptimization pointOrder = static_cast<PointOrderOptimization>(m_sb->setting<int>(Constants::ProfileSettings::Optimizations::kPointOrder));
        bool secondPointEnabled = m_sb->setting<bool>(Constants::ProfileSettings::Optimizations::kEnableSecondCustomLocation);

        if (islandOrder == IslandOrderOptimization::kCustomPoint)
        {
            QVector3D translation(  m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomIslandXLocation),

                                    m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomIslandYLocation),

                                    .0f);

            translation *= Constants::OpenGL::kObjectToView;

            m_seams.custom_island_opt->translateAbsolute(translation);
            m_seams.custom_island_opt->show();
        }
        else
        {
            m_seams.custom_island_opt->hide();
        }

        if (pathOrder == PathOrderOptimization::kCustomPoint)
        {
            QVector3D translation(  m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPathXLocation),

                                    m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPathYLocation),

                                   .0f);

            translation *= Constants::OpenGL::kObjectToView;

            m_seams.custom_path_opt->translateAbsolute(translation);
            m_seams.custom_path_opt->show();
        }
        else
        {
            m_seams.custom_path_opt->hide();
        }

        if(pointOrder == PointOrderOptimization::kCustomPoint)
        {
            QVector3D translation(  m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointXLocation),

                                    m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointYLocation),

                                   .0f);

            translation *= Constants::OpenGL::kObjectToView;

            m_seams.custom_point_opt->translateAbsolute(translation);
            m_seams.custom_point_opt->show();

            if (secondPointEnabled)
            {
                QVector3D secondTranslation(  m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondXLocation),

                                              m_sb->setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondYLocation),

                                              .0f);

                secondTranslation *= Constants::OpenGL::kObjectToView;

                m_seams.custom_point_second_opt->translateAbsolute(secondTranslation);
                m_seams.custom_point_second_opt->show();
            }
            else
            {
                m_seams.custom_point_second_opt->hide();
            }
        }
        else
        {
            m_seams.custom_point_opt->hide();
        }
    }

    void PrinterObject::setSettings(QSharedPointer<SettingsBase> sb)
    {
        m_sb = sb;
    }

    QSharedPointer<SettingsBase> PrinterObject::getSettings()
    {
        return m_sb;
    }

    void PrinterObject::createSeams()
    {
        m_seams.custom_island_opt       = QSharedPointer<SeamObject>::create(this->view(), PM->getVisualizationColor(VisualizationColors::kPerimeter));
        m_seams.custom_path_opt         = QSharedPointer<SeamObject>::create(this->view(), PM->getVisualizationColor(VisualizationColors::kSkin));
        m_seams.custom_point_opt         = QSharedPointer<SeamObject>::create(this->view(), PM->getVisualizationColor(VisualizationColors::kInset));
        m_seams.custom_point_second_opt  = QSharedPointer<SeamObject>::create(this->view(), PM->getVisualizationColor(VisualizationColors::kInfill));

        this->adoptChild(m_seams.custom_island_opt);
        this->adoptChild(m_seams.custom_path_opt);
        this->adoptChild(m_seams.custom_point_opt);
        this->adoptChild(m_seams.custom_point_second_opt);

        m_seams.custom_island_opt->hide();
        m_seams.custom_path_opt->hide();
        m_seams.custom_point_opt->hide();
        m_seams.custom_point_second_opt->hide();
    }

    bool PrinterObject::isTrueVolume()
    {
        return m_is_true_volume;
    }

    PrinterObject::PrinterObject(bool is_true_volume)
    {
        m_is_true_volume = is_true_volume;
    }
}
