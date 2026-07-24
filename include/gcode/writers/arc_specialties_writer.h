#pragma once

#include <QPair>
#include <QVector>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * @class ArcSpecialtiesWriter
 * @brief Arc Specialties writer using X/Y/Z and XR/YR/ZR/AP/CP motion fields.
 *
 * Arc Specialties output keeps generated path coordinates as user-frame endpoints relative to the active work offset,
 * then applies the configured G-Code coordinate frame rotation before output. Feed moves use tool-frame rotations
 * XR=180, YR=0, and ZR=-135; rapid travel moves use ZR=-90. AP comes from the existing Axis A setting. Planar paths use
 * Axis C as a fixed CP positioner value, while cylindrical paths compute CP from each transformed endpoint's angle
 * around the transformed radial slicing center plus Axis C. Helical paths report CP as the positive angular sweep from
 * the transformed helical start angle plus Axis C. When Supports G2/G3 is enabled, print arcs are emitted as G02/G03
 * with I/J center parameters; cylindrical radial and helical arcs are divided according to Arcs per Revolution.
 */
class ArcSpecialtiesWriter : public WriterBase {
  public:
    /*!
     * @brief Constructs an Arc Specialties writer.
     * @param meta Gcode syntax metadata for output units and comments.
     * @param sb Global settings used while writing.
     */
    ArcSpecialtiesWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);

    /*!
     * @brief Supplies the effective part-local helical boundary policies for the settings header.
     * @param methods Part name and boundary policy pairs for parts that produced helical paths.
     */
    void setHelicalPathBoundaryPolicy(const QVector<QPair<QString, HelicalPathBoundaryPolicy>>& methods);

    /*!
     * @brief Supplies the effective part-local helical handedness values for the settings header.
     * @param handedness Part name and handedness pairs for parts that produced helical paths.
     */
    void setHelicalPathHandedness(const QVector<QPair<QString, HelicalPathHandedness>>& handedness);

    /*!
     * @brief Writes Arc Specialties setup notes for the selected slicing workflow.
     * @param syntax Gcode syntax being written. Currently informational only.
     * @return Header comments for Arc Specialties motion assumptions.
     */
    QString writeSettingsHeader(GcodeSyntax syntax) override;

    /*!
     * @brief Writes startup comments, optional user start code, and layer count.
     * @param minimum_x Build minimum X.
     * @param minimum_y Build minimum Y.
     * @param maximum_x Build maximum X.
     * @param maximum_y Build maximum Y.
     * @param num_layers Number of layers to report in the header.
     * @return Initial gcode block.
     */
    QString writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                              int num_layers) override;

    /*!
     * @brief Resets per-layer writer state before a layer.
     * @param min_z Minimum layer Z. Currently informational only.
     * @param sb Layer settings. Currently informational only.
     * @return Empty string because Arc Specialties layers do not require layer prologue commands.
     */
    QString writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) override;

    //! @brief Arc Specialties slicing does not emit a separate part prologue.
    QString writeBeforePart(QVector3D normal) override;

    //! @brief Arc Specialties slicing does not emit island prologue commands.
    QString writeBeforeIsland() override;

    //! @brief Arc Specialties slicing does not emit region prologue commands.
    QString writeBeforeRegion(RegionType type, int pathSize) override;

    //! @brief Tracks the active path type without emitting path prologue commands.
    QString writeBeforePath(RegionType type) override;

    /*!
     * @brief Writes a travel move with Arc Specialties coordinates and orientation fields.
     * @param start_location Start point for the travel.
     * @param target_location End point for the travel.
     * @param lType Travel lift mode used to expand the travel into lift, traverse, and lower moves.
     * @param params Segment settings containing speed and, for cylindrical paths, radial center metadata.
     * @return G00 travel command block.
     */
    QString writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                        QSharedPointer<SettingsBase> params) override;

    /*!
     * @brief Writes a printing move with Arc Specialties coordinates and orientation fields.
     * @param start_point Start point for the line. Currently informational only.
     * @param target_point End point for the line.
     * @param params Segment settings containing speed and, for cylindrical paths, radial center metadata.
     * @return G01 print command.
     */
    QString writeLine(const Point& start_point, const Point& target_point,
                      const QSharedPointer<SettingsBase> params) override;

    /*!
     * @brief Writes a printing arc with Arc Specialties coordinates, orientation fields, and I/J center offsets.
     * @param start_point Start point for the arc.
     * @param end_point End point for the arc.
     * @param center_point Arc center point.
     * @param angle Arc sweep angle. Currently informational only.
     * @param ccw True for counter-clockwise G03 output, false for clockwise G02 output.
     * @param params Segment settings containing speed and, for cylindrical paths, radial center metadata.
     * @return G02/G03 print command, or a G01 move when arc output is disabled.
     */
    QString writeArc(const Point& start_point, const Point& end_point, const Point& center_point, const Angle& angle,
                     const bool& ccw, const QSharedPointer<SettingsBase> params) override;

    //! @brief Writes configured path epilogue commands for compatible region types.
    QString writeAfterPath(RegionType type) override;

    //! @brief Arc Specialties slicing does not emit region epilogue commands.
    QString writeAfterRegion(RegionType type) override;

    //! @brief Arc Specialties slicing does not emit island epilogue commands.
    QString writeAfterIsland() override;

    //! @brief Arc Specialties slicing does not emit a separate part epilogue.
    QString writeAfterPart() override;

    /*!
     * @brief Writes optional user-configured layer change code.
     * @return Layer change gcode or an empty string.
     */
    QString writeAfterLayer() override;

    /*!
     * @brief Writes optional user-configured shutdown code.
     * @return Final gcode block.
     */
    QString writeShutdown() override;

    /*!
     * @brief Writes a dwell command when the requested duration is positive.
     * @param time Dwell duration.
     * @return Dwell command or an empty string.
     */
    QString writeDwell(Time time) override;

  private:
    //! \brief Writes G-Code to enable extrusion
    QString writeExtruderOn();
    //! \brief Writes G-Code to disable extrusion
    QString writeExtruderOff();

    /*!
     * @brief Writes a single Arc Specialties motion command.
     * @param command G-code command string.
     * @param destination Endpoint being written.
     * @param speed Feedrate to emit.
     * @param params Segment settings containing speed and, for cylindrical paths, radial center metadata.
     * @param comment Motion comment.
     * @return Complete motion line.
     */
    QString writeMotion(const QString& command, const Point& destination, Velocity speed,
                        const QSharedPointer<SettingsBase>& params, const QString& comment);

    /*!
     * @brief Formats X/Y/Z/XR/YR/ZR/AP/CP coordinate fields for a point.
     * @param destination Point being written.
     * @param params Segment settings containing, for cylindrical paths, radial center metadata.
     * @param tool_frame_zr ZR orientation value to emit.
     * @return Coordinate parameter string.
     */
    QString writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                             double tool_frame_zr);

    /*!
     * @brief Formats I/J arc center parameters for the selected center interpretation mode.
     * @param start_point Arc start point.
     * @param center_point Arc center point.
     * @return Arc center parameter string.
     */
    QString writeArcCenterParameters(const Point& start_point, const Point& center_point);

    /*!
     * @brief Returns whether G02/G03 I/J values should be written as absolute coordinates.
     * @return True for absolute center mode, false for relative center mode.
     */
    bool usesAbsoluteArcCenters() const;

    /*!
     * @brief Computes the CP value for the active slicing mode, normalized to [0, 360).
     * @param destination Point whose angular position is being written.
     * @param params Segment settings containing, for cylindrical paths, radial center metadata.
     * @return CP value in degrees.
     */
    double cpAxisForPoint(const Point& destination, const QSharedPointer<SettingsBase>& params);

    /*!
     * @brief Returns the transformed helical start angle used as the CP sweep reference.
     * @param params Segment settings containing helical start-angle metadata.
     * @return Start angle in degrees.
     */
    double helicalStartAngle(const QSharedPointer<SettingsBase>& params) const;

    /*!
     * @brief Returns whether the active cylindrical path pattern is helical.
     * @return True when the writer is emitting cylindrical helical paths.
     */
    bool isHelicalPathPattern() const;

    /*!
     * @brief Returns the print-move comment for the active slicing mode and path type.
     * @return Region, radial, or helical comment text.
     */
    QString printMoveComment() const;

    /*!
     * @brief Returns whether the writer is emitting cylindrical radial or helical paths.
     * @return True when Slicing Mode is Cylindrical.
     */
    bool isCylindricalSlicingMode() const;

    //! @brief Tracks whether any travel move has been emitted.
    bool m_first_travel = true;

    //! @brief Forces feedrate output at the start of each layer.
    bool m_layer_start = true;

    //! @brief Tracks whether G161 absolute-center mode was enabled during setup.
    bool m_absolute_arc_center_mode_enabled = false;

    //! @brief Active region type used for planar print-move comments.
    RegionType m_region_type = RegionType::kUnknown;

    //! @brief Tracks current bead number.
    int m_current_bead = 0;

    //! @brief Tracks layer number.
    int m_current_layer = 0;

    //! @brief Effective part-local boundary policies reported in helical G-code headers.
    QVector<QPair<QString, HelicalPathBoundaryPolicy>> m_helical_path_boundary_policy;

    //! @brief Effective part-local handedness values reported in helical G-code headers.
    QVector<QPair<QString, HelicalPathHandedness>> m_helical_path_handedness;
};
} // namespace ORNL
