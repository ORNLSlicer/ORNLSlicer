#pragma once

#include <qquaternion.h>
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
class GKNWriter : public WriterBase {
  public:
    //! \brief Constructor
    GKNWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);

    QString writeSlicerHeader(const QString& syntax) override;

    QString writeSettingsHeader(GcodeSyntax syntax) override;

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

    //! \brief Writes G-Code for scan
    QString writeScan(Point target_point, Velocity speed, bool on_off) override;

    //! \brief Writes G-Code to be executed after each path
    QString writeAfterPath(RegionType type) override;

    //! \brief Writes G-Code to be executed after each region
    QString writeAfterRegion(RegionType type) override;

    //! \brief Writes G-Code to be executed after each scan
    QString writeAfterScan(Distance beadWidth, Distance laserStep, Distance laserResolution) override;

    //! \brief Writes G-Code to be executed once all scans are complete
    QString writeAfterAllScans() override;

    //! \brief Writes G-Code to be executed at the end of each island
    QString writeAfterIsland() override;

    //! \brief Writes G-Code to be executed at the end of each part each layer
    QString writeAfterPart() override;

    //! \brief Writes G-Code to be executed at the end of each layer
    QString writeAfterLayer() override;

    //! \brief Writes G-Code for shutting down the machine
    QString writeShutdown() override;

    //! \brief Writes G-Code for a pause, G4
    QString writeDwell(Time time) override;

  private:
    //! \brief Writes G-Code to enable the extruder
    QString writeExtruderOn(RegionType type);
    //! \brief Writes G-Code to disable the extruder
    QString writeExtruderOff();
    //! \brief Writes update code for laser power and wire feeder
    QString writeUpdateLaserAndWire();

    //! \brief Writes gcode coordinates XYZ for a move or travel to the destination point
    QString writeCoordinates(Point destination, Velocity speed, RegionType type, bool isTravel = false);

    AngularVelocity m_current_rpm;

    //! \brief true if first travel, false for subsequent travels
    bool m_first_travel;
    bool m_first_layer_travel;

    //! \brief true if wire has been cut, false if it needs cut
    bool m_wire_cut;
    //! \brief true if tip has been wiped since extruder was last on
    bool m_wipe_tip;

    //! \brief Preallocated prefixes
    QString m_laser_prefix, m_variable_delim, m_v, m_zero;

    //! \brief orientation variables - m_A = Z, m_B = Y, m_C = X, m_E1 - tilt, m_E2 - rotation,
    //! m_actual_angle - complement of E2 since positive rotation is actually the opposite direction
    Angle m_A, m_B, m_C, m_E1, m_E2, m_actual_angle;

    //! \brief composite quaternions for arm and table
    QQuaternion m_arm_orientation, m_table_orientation;

    //! \brief convenience bools for whether or not E1 and E2 are enabled
    bool m_can_tilt, m_can_rotate;

    //! \brief bool to track which direction we are rotating if E2 is enabled
    bool m_ccw;

    //! \brief Preallocated axis for calculation when E1 is enabled
    QVector3D m_z_axis;

    //! \brief Suppress next travel lift if after M7
    bool m_suppress_next_travel_lift;

    //! \brief Override next travel lift if after scan
    bool m_override_travel_lift;

}; // class GKNWriter
} // namespace ORNL
