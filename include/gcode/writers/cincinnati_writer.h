#pragma once

#include <qsharedpointer.h>
#include <qvectornd.h>

#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class CincinnatiWriter
 * \brief The gcode writer for the Cincinnati Inc. syntax
 */
class CincinnatiWriter : public WriterBase {
  public:
    //! \brief Constructor
    CincinnatiWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);

    //! \brief Writes initial setup instructions for the machine state
    QString writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                              int num_layers) override;

    //! \brief Writes G-Code to be executed at the start of the layer
    QString writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) override;

    //! \brief Writes G-Code to be executed each layer before each part
    QString writeBeforePart(QVector3D normal) override;

    //! \brief Writes G-Code to be exectued before each island
    QString writeBeforeIsland() override;

    //! \brief Writes G-Code to be executed before each scan
    QString writeBeforeScan(Point min, Point max, int layer, int boundingBox, Axis axis, Angle angle) override;

    //! \brief Writes G-Code to be executed at the start of each region
    QString writeBeforeRegion(RegionType type, int pathSize) override;

    //! \brief Writes G-Code to be executed at the start of each path
    QString writeBeforePath(RegionType type) override;

    //! \brief Writes G-Code for traveling between paths
    QString writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                        QSharedPointer<SettingsBase> params) override;

    //! \brief Writes G-Code for line
    QString writeLine(const Point& start_point, const Point& target_point,
                      const QSharedPointer<SettingsBase> params) override;

    //! \brief Writes G-Code for arc
    QString writeArc(const Point& start_point, const Point& end_point, const Point& center_point, const Angle& angle,
                     const bool& ccw, const QSharedPointer<SettingsBase> params) override;

    //! \brief Writes G-Code for scan
    QString writeScan(Point target_point, Velocity speed, bool on_off) override;

    //! \brief Writes G-Code to be executed after each path
    QString writeAfterPath(RegionType type) override;

    //! \brief Writes G-Code to be executed after each region
    QString writeAfterRegion(RegionType type) override;

    //! \brief Writes G-Code to be executed after each scan
    QString writeAfterScan(Distance beadWidth, Distance laserStep, Distance laserResolution) override;

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
    //! \brief Writes G-Code to enable the tamper
    QString writeTamperOn();
    //! \brief Writes G-Code to disable the tamper
    QString writeTamperOff();
    //! \brief Writes G-Code to enable the extruder
    QString writeExtruderOn(RegionType type, int rpm, int extruder_number);
    //! \brief Writes G-Code to disable the extruder
    QString writeExtruderOff(int extruder_number);
    //! \brief Writes G-Code to update the acceleration value
    QString writeAcceleration(Acceleration acc);

    //! \brief Writes gcode coordinates WXYZ for a move or travel to the destination point
    QString writeCoordinates(Point destination);

    //! \brief returns the correct ZW value for a point
    //! \param destination the target point
    //! \return a gcode string that contains the correct move
    QString getZWValue(const Point& destination);

    //! \brief State variables
    AngularVelocity m_current_rpm;
    int m_current_recipe;
    RegionType m_region_type;

    //! \brief true if first travel, false for subsequent travels
    bool m_first_travel;
    //! \brief true if current motion is a lift move
    bool m_is_lift;
    //! \brief true if travel move that isn't classified as lift or lower
    bool m_is_travel;
    //! \brief true if the travel contains a Z coordinate
    bool m_z_travel;
    //! \brief true if the travel contains a W coordinate
    bool m_w_travel;
    //! \brief true if first printing segment, false for subsquent paths - needed for Spiralize
    bool m_first_print;
    //! \brief true is first print motion of the layer
    bool m_layer_start;

    //! \brief preallocated prefixs commonly used in this syntax
    QString m_laser_prefix;
    QChar m_laser_delimiter;
    QString m_M10, m_M11, m_M64, m_M65;
    int m_material_number;

    //! \brief wire feed state (on/off)
    bool m_wire_feed;

    //! \brief wire cutoff point
    Point m_wire_cutoff;
    bool m_need_wirecut;

}; // class CincinnatiWriter
} // namespace ORNL
