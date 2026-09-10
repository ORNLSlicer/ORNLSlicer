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
 * then applies the configured G-Code coordinate frame rotation before output. AP comes from the existing Axis A
 * setting. Planar paths use Axis C as a fixed CP positioner value, while cylindrical paths compute CP from each
 * transformed endpoint's angle around the transformed radial slicing center plus Axis C. Helical paths report CP as the
 * positive angular sweep from transformed helical top dead center plus the configured start angle offset and Axis C.
 * The initial TRAFO-off world approach uses ZR=-90; work-object motion uses XR=180, YR=0, and ZR=-135. When Supports
 * G2/G3 is enabled, print arcs are emitted as G02/G03 with I/J center parameters; cylindrical radial and helical arcs
 * are divided according to Arcs per Revolution.
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
     * @brief Supplies the effective part-local helical Z clip rounding values for the settings header.
     *
     * @param rounding Part name and rounding pairs for parts that produced helical paths.
     */
    void setHelicalPathZClipRounding(const QVector<QPair<QString, HelicalPathZClipRounding>>& rounding);

    /*!
     * @brief Supplies the effective part-local helical handedness values for the settings header.
     * @param
     * handedness Part name and handedness pairs for parts that produced helical paths.
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

    //! @brief Writes or defers the layer marker so Arc Specialties startup positioning stays outside the layer block.
    QString writeLayerChange(uint layer_number) override;

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
    //! @brief Arc Specialties tool-frame rotation fields.
    struct ToolFrameRotation {
        double xr;
        double yr;
        double zr;
    };

    //! \brief Writes G-Code to enable the welder
    QString writeWelderOn();
    /*!
     * @brief Writes G-Code to disable the welder.
     * @param mode Arc Specialties G83 mode: 0 stops welding, 1 retracts, 2 clips wire and homes, 3 moves linearly to
     * the retract position, and 4 moves linearly back to the previous programmed position.
     * @return Welder shutdown block.
     */
    QString writeWelderOff(int mode = 0);

    /*!
     * @brief Returns the quote-stripped G80 weld schedule file configured for output.
     * @return Sanitized schedule file path, or an empty string when unset.
     */
    QString g80WeldScheduleFile() const;

    /*!
     * @brief Returns whether print motion feedrates should reference the G80 schedule speed variable.
     * @return True when a sanitized G80 weld schedule file is configured.
     */
    bool usesG80ScheduleSpeedVariable() const;

    /*!
     * @brief Formats the Arc Specialties motion feedrate field.
     * @param speed Numeric feedrate used when schedule speed output is inactive.
     * @return Either a numeric F field or the G80 schedule speed variable field.
     */
    QString writeMotionFeedrate(Velocity speed) const;

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
     * @brief Writes a single Arc Specialties motion command while computing CP from a reference point.
     * @param command G-code command string.
     * @param destination Endpoint being written.
     * @param speed Feedrate to emit.
     * @param params Segment settings containing speed and, for cylindrical paths, radial center metadata.
     * @param comment Motion comment.
     * @param cp_reference Point used to compute the CP orientation field.
     * @return Complete motion line.
     */
    QString writeMotion(const QString& command, const Point& destination, Velocity speed,
                        const QSharedPointer<SettingsBase>& params, const QString& comment, const Point& cp_reference);

    /*!
     * @brief Starts per-layer Beckhoff block numbering for executable bead-body lines.
     */
    void startLayerBlockNumbering();

    /*!
     * @brief Initializes the per-layer Beckhoff block number counter without activating numbering.
     */
    void initializeLayerBlockNumbering();

    /*!
     * @brief Stops per-layer block numbering for layer transition and shutdown lines.
     */
    void stopLayerBlockNumbering();

    /*!
     * @brief Returns whether Arc Specialties block numbers should be emitted.
     * @return True when the setting is enabled.
     */
    bool shouldEmitBlockNumbers() const;

    /*!
     * @brief Adds the next per-layer block number to each executable line in a block.
     * @param block G-Code block to format.
     * @param force True to number schedule/setup lines before bead-body numbering is active.
     * @return Block with Beckhoff-style N numbers when enabled and active or forced.
     */
    QString writeNumberedBlock(const QString& block, bool force = false);

    /*!
     * @brief Writes the Arc Specialties kinematics and TRAFO setup block once.
     * @return Startup kinematics block, or an empty string after it has already been emitted.
     */
    QString writeStartupKinematics();

    /*!
     * @brief Writes any layer marker deferred until after startup kinematics are enabled.
     * @return Deferred layer marker, or an empty string when none is pending.
     */
    QString writePendingLayerChange();

    /*!
     * @brief Writes layer-local output held until the first ordered helical print motion chooses OptStopMode.
     * @return Pending pre-print layer output, or an empty string when none is pending.
     */
    QString writePendingLayerPrefix();

    /*!
     * @brief Writes any helical region schedule deferred until the first ordered print motion.
     * @return Pending G80 schedule block, or an empty string when none is pending.
     */
    QString writePendingRegionSchedule();

    /*!
     * @brief Formats X/Y/Z/XR/YR/ZR/AP/CP coordinate fields for a point.
     * @param destination Point being written.
     * @param params Segment settings containing, for cylindrical paths, radial center metadata.
     * @param tool_frame_rotation Tool-frame orientation value to emit.
     * @return Coordinate parameter string.
     */
    QString writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                             const ToolFrameRotation& tool_frame_rotation);

    /*!
     * @brief Formats X/Y/Z/XR/YR/ZR/AP/CP coordinate fields with CP computed from a reference point.
     * @param destination Point being written.
     * @param params Segment settings containing, for cylindrical paths, radial center metadata.
     * @param tool_frame_rotation Tool-frame orientation value to emit.
     * @param cp_reference Point used to compute the CP orientation field.
     * @return Coordinate parameter string.
     */
    QString writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                             const ToolFrameRotation& tool_frame_rotation, const Point& cp_reference);

    /*!
     * @brief Returns the tool frame for a motion comment and segment settings.
     * @param comment Motion comment being emitted.
     * @param params Segment settings used to identify the helical region.
     * @return Tool-frame rotation for the motion.
     */
    ToolFrameRotation toolFrameRotationForMotion(const QString& comment,
                                                 const QSharedPointer<SettingsBase>& params) const;

    /*!
     * @brief Returns the first post-kinematics travel point above the first-layer lower point.
     * @param travel_destination Current first travel destination before the first-layer lower adjustment.
     * @param travel_lower_destination First-layer point written by the TRAVEL LOWER command.
     * @param travel_lift_height Configured travel lift height used as the emitted Z buffer.
     * @return First TRAVEL destination with emitted Y aligned to the lower point and emitted Z buffered above it.
     */
    Point firstTravelPointAboveTravelLowerDestination(const Point& travel_destination,
                                                      const Point& travel_lower_destination,
                                                      Distance travel_lift_height) const;

    /*!
     * @brief Returns the safe world-space startup approach point for the first travel.
     * @param travel_destination Computed first travel destination in the active part/work frame.
     * @param params Segment settings containing, for cylindrical paths, radial center metadata.
     * @return First TRAFO-off world approach point, with Z set above the maximum part Z or configured cylinder height.
     */
    Point safeStartupWorldApproachPoint(const Point& travel_destination,
                                        const QSharedPointer<SettingsBase>& params) const;

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
     * @brief Returns the transformed offset-adjusted helical start angle used as the CP sweep reference.
     * @param params Segment settings containing helical start-angle offset metadata.
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
     * @param params Segment settings used to report parser-facing region metadata.
     * @return Region, radial, or helical comment text.
     */
    QString printMoveComment(const QSharedPointer<SettingsBase>& params) const;

    /*!
     * @brief Returns whether the writer is emitting cylindrical radial or helical paths.
     * @return True when Slicing Mode is Cylindrical.
     */
    bool isCylindricalSlicingMode() const;

    /*!
     * @brief Returns whether the supplied settings describe cylindrical helical output.
     * @param params Settings to query before falling back to writer settings.
     * @return True when Slicing Mode is Cylindrical and Cylindrical Path Pattern is Helical.
     */
    bool isHelicalPathPattern(const QSharedPointer<SettingsBase>& params) const;

    /*!
     * @brief Returns whether layer-local output should be held until the helical OptStopMode is known.
     * @param params Settings used to identify helical output.
     * @return True when the layer marker is pending and the first helical print motion has not been seen.
     */
    bool shouldBufferHelicalLayerPrefix(const QSharedPointer<SettingsBase>& params) const;

    /*!
     * @brief Formats the Arc Specialties optional stop mode selected by actual helical CP rotation direction.
     * @param start_point Motion start point after path ordering.
     * @param end_point Motion end point after path ordering.
     * @param params Segment settings used to compute CP.
     * @return OptStopMode assignment for the first helical print move in a layer, or an empty string otherwise.
     */
    QString writeHelicalOptStopMode(const Point& start_point, const Point& end_point,
                                    const QSharedPointer<SettingsBase>& params);

    /*!
     * @brief Writes the pending layer marker, helical OptStopMode, and held pre-print layer output.
     * @param start_point Motion start point after path ordering.
     * @param end_point Motion end point after path ordering.
     * @param params Segment settings used to compute CP.
     * @return Pending layer-start output for the first print move.
     */
    QString writeHelicalLayerStart(const Point& start_point, const Point& end_point,
                                   const QSharedPointer<SettingsBase>& params);

    //! @brief Tracks whether any travel move has been emitted.
    bool m_first_travel = true;

    //! @brief Forces feedrate output at the start of each layer.
    bool m_layer_start = true;

    //! @brief Tracks whether G161 absolute-center mode was enabled during setup.
    bool m_absolute_arc_center_mode_enabled = false;

    //! @brief Tracks whether startup kinematics setup has been emitted.
    bool m_startup_kinematics_written = false;

    //! @brief Layer marker held until the initial world approach and kinematics block are complete.
    QString m_pending_layer_change;

    //! @brief Layer-local output held until the first helical print move determines OptStopMode.
    QString m_pending_layer_prefix;

    //! @brief Active region type used for planar print-move comments.
    RegionType m_region_type = RegionType::kUnknown;

    //! @brief Tracks current bead number.
    int m_current_bead = 0;

    //! @brief Tracks layer number.
    int m_current_layer = 0;

    //! @brief Tracks the next Beckhoff block number for the active layer.
    int m_next_block_number = 0;

    //! @brief Tracks whether per-layer block numbering has been initialized for this layer.
    bool m_layer_block_numbering_started = false;

    //! @brief Tracks whether executable lines should currently receive block numbers.
    bool m_layer_block_numbering_active = false;

    //! @brief Tracks whether the helical rotation-direction mode has been emitted for the active layer.
    bool m_helical_opt_stop_mode_written = false;

    //! @brief Holds helical G80 schedule output until ordered print motion determines C rotation direction.
    QString m_pending_region_schedule;

    //! @brief Effective part-local Z clip rounding values reported in helical G-code headers.
    QVector<QPair<QString, HelicalPathZClipRounding>> m_helical_path_z_clip_rounding;

    //! @brief Effective part-local handedness values reported in helical G-code headers.
    QVector<QPair<QString, HelicalPathHandedness>> m_helical_path_handedness;
};
}  // namespace ORNL
