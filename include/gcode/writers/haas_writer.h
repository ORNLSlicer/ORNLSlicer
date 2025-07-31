#ifndef HAAS_WRITER_H
#define HAAS_WRITER_H

//! \file haas_writer.h

#include "gcode/writers/writer_base.h"
#include "managers/settings/settings_manager.h"
#include "gcode/gcode_meta.h"

namespace ORNL
{
    /*!
     * \class HaasWriter
     * \brief The gcode writer for the Haas syntax
     */
    class HaasWriter : public WriterBase
    {
    public:

        //! \brief Constructor
        HaasWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);

        //! \brief Writes initial setup instructions for the machine state
        QString writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y, int num_layers) override;

         //! \brief Writes G-Code to be executed at the start of the layer
        QString writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) override;

         //! \brief Writes G-Code to be executed each layer before each part
        QString writeBeforePart(QVector3D normal) override;

        //! \brief Writes G-Code to be exectued before each island
        QString writeBeforeIsland() override;

        //! \brief Writes G-Code to be executed at the start of each region
        QString writeBeforeRegion(RegionType type, int pathSize) override;

        //! \brief Writes G-Code to be executed at the start of each path
        QString writeBeforePath(RegionType type) override;

        //! \brief Writes G-Code for traveling between paths
        QString writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                            QSharedPointer<SettingsBase> params) override;

        //! \brief Writes G-Code for line
        QString writeLine(const Point& start_point,
                          const Point& target_point,
                          const QSharedPointer<SettingsBase> params) override;

        //! \brief Writes G-Code for arc
        QString writeArc(const Point &start_point,
                         const Point &end_point,
                         const Point &center_point,
                         const Angle &angle,
                         const bool &ccw,
                         const QSharedPointer<SettingsBase> params) override;


        //! \brief Writes G-Code to be executed after each path
        QString writeAfterPath(RegionType type) override;

        //! \brief Writes G-Code to be executed after each region
        QString writeAfterRegion(RegionType type) override;

        //! \brief Writes G-Code to be executed at the end of each island
        QString writeAfterIsland() override;

        //! \brief Writes G-Code to be executed at the end of each part each layer
        QString writeAfterPart() override;

        //! \brief Writes G-Code to be executed at the end of each layer
        QString writeAfterLayer() override;

        //! \brief Writes G-Code for shutting down the machine
        QString writeShutdown() override;

        //! \brief Write purge command
        QString writePurge(int RPM, int duration, int delay) override;

        //! \brief Writes G-Code for a pause, G4
        QString writeDwell(Time time) override;

    private:

        //! \brief Writes G-Code to enable the extruder
        QString writeExtruderOn(RegionType type, int rpm);
        //! \brief Writes G-Code to disable the extruder
        QString writeExtruderOff();

        //! \brief Writes gcode coordinates WXYZ for a move or travel to the destination point
        QString writeCoordinates(Point destination);

        AngularVelocity m_current_rpm;

        //! \brief true if first travel, false for subsequent travels
        bool m_first_travel;
        //! \brief true if first printing segment, false for subsquent paths - needed for Spiralize
        bool m_first_print;
        //! \brief true is first print motion of the layer
        bool m_layer_start;

        //! \brief preallocated prefixes commonly used in this syntax
        int m_material_number;
        int m_tool_number;

    };  // class HaasWriter
}  // namespace ORNL
#endif  // HAAS_WRITER_H
