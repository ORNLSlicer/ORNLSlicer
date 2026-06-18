#pragma once

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "step/layer/regions/region_base.h"

namespace ORNL {
class Inset : public RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param index: index for region order
    //! \param settings_polygons: a vector of settings polygons to apply
    Inset(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Writes the gcode for the inset.
    //! \param writer Writer type to use for gcode output
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the inset region.
    void compute(uint layer_num) override;

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) override;

    //! \brief Creates paths for the inset region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    Path createPath(Polyline line) override;

    //! \brief gets the computed geometry
    //! \return the computed geometry
    QVector<Polyline> getComputedGeometry();

  private:
    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    void calculateModifiers(Path& path, bool supportsG3) override;

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
                                        const QSharedPointer<SettingsBase>& parent_sb);

    //! \brief Holds the computed geometry before it is converted into paths
    QVector<Polyline> m_computed_geometry;
};
} // namespace ORNL
