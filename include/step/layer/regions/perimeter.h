#pragma once

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "step/layer/regions/region_base.h"

namespace ORNL {
class Perimeter : public RegionBase {
   public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param index: index for region order
    //! \param settings_polygons: a vector of settings polygons to apply
    //! \param uncut_geometry: original geometry before setting region cutting
    Perimeter(const QSharedPointer<SettingsBase>& sb, const int index,
              const QVector<SettingsPolygon>& settings_polygons, PolygonList uncut_geometry);

    //! \brief Writes the gcode for the perimeter.
    //! \param writer Writer type to use for gcode output
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the perimeter region.
    void compute(uint layer_num) override;

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) override;

    //! \brief Creates paths for the perimeter region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    Path createPath(Polyline line) override;

    //! \brief gets the computed geometry
    //! \return the computed geometry
    QVector<Polyline> getComputedGeometry();

    //! \brief Sets inset geometry that should be connected onto spiral perimeter output.
    //! \param geometry: computed inset closed-loop paths
    //! \param widths: bead widths associated with the inset paths
    void setConnectedInsetGeometry(const QVector<Polyline>& geometry, const QVector<Distance>& widths);

   private:
    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    void calculateModifiers(Path& path, bool supportsG3) override;

    //! \brief Creates modifiers, optionally treating forward tip wipe as an open-loop wipe.
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    //! \param open_loop_tip_wipe Whether forward tip wipe should be emitted from the open path end.
    void calculateModifiers(Path& path, bool supportsG3, bool open_loop_tip_wipe);

    /**
     * @brief Create a path with localized settings applied to segments based on settings regions.
     * @param[in] line Polyline representing the path.
     * @return Path with localized settings applied.
     * @warning Handles cases of overlapping settings regions by applying the first region found.
     */
    Path createPathWithLocalizedSettings(const Polyline& line);

    /**
     * @brief Populates the segment settings with the passed settings base.
     * @param[in,out] segment_sb: The segment settings base to populate.
     * @param[in] parent_sb: The settings base to apply.
     */
    static void populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                        const QSharedPointer<SettingsBase>& parent_sb, const Distance& bead_width,
                                        bool adapted);

    /**
     * @brief Returns the computed adaptive width for a generated contour segment.
     * @param[in] start Segment start point.
     * @param[in] end Segment end point.
     * @param[in] parent_sb Settings used for the fallback nominal width.
     */
    Distance beadWidthForSegment(const Point& start, const Point& end,
                                 const QSharedPointer<SettingsBase>& parent_sb) const;

    /**
     * @brief Returns whether the supplied width differs from the parent bead width enough to be treated as adapted.
     * @param[in] width Bead width being applied.
     * @param[in] parent_sb Settings containing the nominal bead width.
     */
    static bool isAdaptedWidth(const Distance& width, const QSharedPointer<SettingsBase>& parent_sb);

    //! \brief Applies inset segment settings after the connected perimeter-to-inset bridge.
    //! \param path Path containing perimeter paths followed by connected inset paths.
    void applyConnectedInsetSettings(Path& path) const;

    //! \brief Returns the inset bead width for a connected inset segment.
    //! \param start Segment start point.
    //! \param end Segment end point.
    Distance connectedInsetWidthForSegment(const Point& start, const Point& end) const;

    //! \brief Holds the computed geometry before it is converted into paths
    QVector<Polyline> m_computed_geometry;

    //! \brief Holds the bead width associated with each computed contour in m_computed_geometry
    QVector<Distance> m_computed_widths;

    //! \brief Holds computed inset geometry to connect after spiral perimeters.
    QVector<Polyline> m_connected_inset_geometry;

    //! \brief Holds the bead width associated with each connected inset contour.
    QVector<Distance> m_connected_inset_widths;

    //! \brief Holds the layer number that we are currently on
    uint m_layer_num;
};
}  // namespace ORNL
