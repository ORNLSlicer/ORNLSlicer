#pragma once

#include "step/layer/regions/region_base.h"

namespace ORNL {
class Infill : public RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param index: index for region order
    //! \param settings_polygons: a vector of settings polygons to apply
    Infill(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Writes the gcode for the perimeter.
    //! \param writer Writer type to use for gcode output
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the perimeter region.
    void compute(uint layer_num, QSharedPointer<SyncManager>& sync) override;

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param innerMostClosedContour: used for subsequent path modifiers
    //! \param outerMostClosedContour: used for subsequent path modifiers
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                  QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) override;

    //! \brief Creates paths for the infill region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    Path createPath(Polyline line) override;

    void setLayerCount(uint layer_count);

  private:
    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    //! \param innerMostClosedContour used for Prestarts (currently only skins/infill)
    void calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) override;

    //! \brief fills a set of geometry with infill according to settings
    //! \param geometry: what to fill
    //! \param sb: the settings to apply
    void fillGeometry(PolygonList geometry, const QSharedPointer<SettingsBase>& sb);

    //! \brief determines if the infill settings of two settingbase are the same
    //! \param a: the first set of settings
    //! \param b: the second set of settings
    //! \return if they are the same
    bool settingsSame(QSharedPointer<SettingsBase> a, QSharedPointer<SettingsBase> b);

    //! \brief Holds the computed geometry before it is converted into paths
    QVector<QVector<Polyline>> m_computed_geometry;

    //! \brief Holds a copy of the geometry for later optimization
    PolygonList m_geometry_copy;

    //! \brief Contains the layer that we are currently on
    uint m_layer_num;

    //! \brief Contains the total number of layers
    uint m_layer_count;
};
} // namespace ORNL
