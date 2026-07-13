#pragma once

#include <QPair>
#include <QVector>
#include <qsharedpointer.h>

#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * @class RadialWriter
 * @brief Basic X/Y/Z/A/C writer for radial cylinder slicing.
 *
 * The writer keeps X/Y/Z in model coordinates, emits the configured fixed table
 * tilt on A, and computes C from each point's angle around the radial slicing center.
 * C-axis values are unwrapped between moves to avoid large rotary discontinuities.
 */
class RadialWriter : public WriterBase {
  public:
    /*!
     * @brief Constructs a radial 3+2 axis writer.
     * @param meta Gcode syntax metadata for output units and comments.
     * @param sb Global settings used while writing.
     */
    RadialWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);

    /*!
     * @brief Supplies the effective part-local helical clipping methods for the settings header.
     * @param methods Part name and clipping method pairs for parts that produced helical paths.
     */
    void setHelicalClippingMethods(const QVector<QPair<QString, HelicalClippingMethod>>& methods);

    /*!
     * @brief Writes radial-specific process-agnostic slicing settings.
     * @param syntax Gcode syntax being written. Currently informational only.
     * @return Header comments for radial slicing geometry and rotary settings.
     */
    QString writeSettingsHeader(GcodeSyntax syntax) override;

    /*!
     * @brief Writes startup, optional bounding box, and radial layer count.
     * @param minimum_x Build minimum X.
     * @param minimum_y Build minimum Y.
     * @param maximum_x Build maximum X.
     * @param maximum_y Build maximum Y.
     * @param num_layers Number of radial layers to report in the header.
     * @return Initial gcode block.
     */
    QString writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                              int num_layers) override;

    /*!
     * @brief Resets per-layer writer state before a radial layer.
     * @param min_z Minimum layer Z.  Currently informational only.
     * @param sb Layer settings.  Currently informational only.
     * @return Empty string because radial layers do not require layer prologue commands.
     */
    QString writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) override;

    //! @brief Radial slicing does not emit a separate part prologue.
    QString writeBeforePart(QVector3D normal) override;

    //! @brief Radial slicing does not emit island prologue commands.
    QString writeBeforeIsland() override;

    //! @brief Radial slicing does not emit region prologue commands.
    QString writeBeforeRegion(RegionType type, int pathSize) override;

    //! @brief Radial slicing does not emit path prologue commands.
    QString writeBeforePath(RegionType type) override;

    /*!
     * @brief Writes a travel move with X/Y/Z/A/C coordinates.
     * @param start_location Start point for the travel.
     * @param target_location End point for the travel.
     * @param lType Travel lift mode used to expand the travel into lift, traverse, and lower moves.
     * @param params Segment settings containing radial center metadata.
     * @return G0 travel command.
     */
    QString writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                        QSharedPointer<SettingsBase> params) override;

    /*!
     * @brief Writes a printing move with X/Y/Z/A/C coordinates.
     * @param start_point Start point for the line.
     * @param target_point End point for the line.
     * @param params Segment settings containing speed and radial center metadata.
     * @return G1 print command.
     */
    QString writeLine(const Point& start_point, const Point& target_point,
                      const QSharedPointer<SettingsBase> params) override;

    //! @brief Radial slicing does not emit path epilogue commands.
    QString writeAfterPath(RegionType type) override;

    //! @brief Radial slicing does not emit region epilogue commands.
    QString writeAfterRegion(RegionType type) override;

    //! @brief Radial slicing does not emit island epilogue commands.
    QString writeAfterIsland() override;

    //! @brief Radial slicing does not emit a separate part epilogue.
    QString writeAfterPart() override;

    /*!
     * @brief Writes optional user-configured layer change code.
     * @return Layer change gcode or an empty string.
     */
    QString writeAfterLayer() override;

    /*!
     * @brief Writes shutdown code and motor disable.
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
    /*!
     * @brief Formats X/Y/Z/A/C coordinates for a point.
     * @param destination Point being written.
     * @param params Segment settings containing radial center metadata.
     * @return Coordinate parameter string.
     */
    QString writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params);

    /*!
     * @brief Computes and unwraps the C-axis angle for a point around the radial center.
     * @param destination Point whose angular position is being written.
     * @param params Segment settings containing radial center metadata.
     * @return C-axis value in degrees.
     */
    double cAxisForPoint(const Point& destination, const QSharedPointer<SettingsBase>& params);

    //! @brief Parameter prefix for the C rotary axis.
    QString m_c;

    //! @brief Tracks whether any travel move has been emitted.
    bool m_first_travel = true;

    //! @brief Forces feedrate output at the start of each radial layer.
    bool m_layer_start = true;

    //! @brief Indicates whether m_last_c_degrees contains a valid previous C value.
    bool m_have_last_c = false;

    //! @brief Last emitted C-axis value, used for rotary unwrapping.
    double m_last_c_degrees = 0.0;

    //! @brief Effective part-local clipping methods reported in helical G-code headers.
    QVector<QPair<QString, HelicalClippingMethod>> m_helical_clipping_methods;
};
} // namespace ORNL
