#pragma once

#include <optional>

#include <QObject>
#include <qcontainerfwd.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class PathOrderOptimizer;
/*!
 * \class RegionBase
 * \brief Base class for all region types.
 * \note For more information about the abstract slicing architecture, see the documentation.
 */
class RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param index: index for region order
    //! \param settings_polygons: a vector of settings polygons to apply
    RegionBase(const QSharedPointer<SettingsBase>& sb, const int index,
               const QVector<SettingsPolygon>& settings_polygons, PolygonList uncut_geometry = PolygonList(),
               RegionType region_type = RegionType::kUnknown);

    //! \brief Constructor
    //! \param sb: the settings
    //! \param settings_polygons: a vector of settings polygons to apply
    RegionBase(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons,
               RegionType region_type = RegionType::kUnknown);

    //! \brief Destructor
    virtual ~RegionBase() = default;

    //! \brief Writes the GCode for this region.
    //! \param writer: writer for gcode syntax
    virtual QString writeGCode(QSharedPointer<WriterBase> writer) = 0;

    //! \brief Performs the computation for this region.
    //! \param layer_num: current layer number
    virtual void compute(uint layer_num) = 0;

    //! \brief Performs the optimization for this region.
    //! \param layerNumber: current layer
    //! \param current_location: current location
    //! \param shouldNextPathBeCCW: CW or CCW state of last contour when using additional DOF
    virtual void optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) = 0;

    //! \brief Get the paths generated from this region.
    //! \return Reference to region paths
    QVector<Path>& getPaths();

    //! \brief Returns the first material-depositing start point in the region, if one exists.
    std::optional<Point> getFirstPrintingStartPoint();

    //! \brief Sets the prior layer's start point for consecutive point ordering.
    void setPreviousLayerStartPoint(const std::optional<Point>& point);

    //! \brief Gets the prior layer's start point for consecutive point ordering.
    std::optional<Point> getPreviousLayerStartPoint() const;

    //! \brief Reverse path ordering in this region.
    void reversePaths();

    //! \brief Get the geometry for the region.
    //! \return Copy of geometry
    PolygonList getGeometry() const;

    //! \brief Set the geometry;
    //! \param geometry: geometry to set
    void setGeometry(const PolygonList& geometry);

    //! \brief Get the settings that the region will use.
    //! \return Pointer to settings base
    QSharedPointer<SettingsBase> getSb() const;

    //! \brief Set the settings that the region will use.
    //! \param sb: Pointer to settings base to set
    void setSb(const QSharedPointer<SettingsBase>& sb);

    //! \brief Sets the slicing frame used while optimizing flattened layer geometry.
    void setOptimizationFrame(const Plane& slicing_plane, const Point& optimization_shift);

    //! \brief transforms the region by rotating by then quaternion, then shifting
    void transform(QQuaternion rotation, Point shift);

    //! \brief returns the minimun z-value of a region
    //! \return minimum z value
    float getMinZ();

    //! \brief return index that represents region order
    //! \return region order index
    int getIndex();

    //! \brief Returns the concrete region type.
    RegionType getRegionType() const;

    //! \brief Records the layer number used for the last optimization pass.
    void setOptimizedLayerNumber(int layer_number);

    //! \brief Returns the layer number used for the last optimization pass.
    int getOptimizedLayerNumber() const;

    //! \brief returns the material number of a region
    //! \return material number
    int getMaterialNumber();

    //! \brief set the material number of a region
    //! \param material_number: material number to set
    void setMaterialNumber(int material_number);

    //! \brief Update material numbers for the segments in each region
    //! \param transition_distance: distance needed for material transition
    //! \param next_material_number: material number to set for transition segments
    void calculateMultiMaterialTransition(Distance& transition_distance, int next_material_number);

    //! \brief Fits eligible planar line segments to G2/G3 arcs.
    void fitCircularArcs(const QSharedPointer<SettingsBase>& global_sb);

    //! \brief Sets whether last region was spiralized or not (path optimizer needs this info)
    //! \param spiral: whether or not last region was spiralized
    void setLastSpiral(bool spiral);

  protected:
    //! \brief Generates paths for the region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    virtual Path createPath(Polyline line) = 0;

    //! \brief Adds a path to the region.
    //! \param path: path to append
    void appendPath(const Path& path);

    //! \brief Returns the path-order custom anchor in the current optimization frame.
    Point customPathOrderPoint() const;

    //! \brief Returns the point-order custom anchor in the current optimization frame.
    Point customPointOrderPoint() const;

    //! \brief adds the modifiers for each region
    //! \param path: path to add modifiers to
    //! \param supportsG3: whether or not the system supports G3 command
    virtual void calculateModifiers(Path& path, bool supportsG3) = 0;

    //! \brief The geometery this region will work on.
    PolygonList m_geometry;

    //! \brief The resultant paths of the computation.
    QVector<Path> m_paths;

    //! \brief The settings the region will use.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief The settings polygon this region may use
    QVector<SettingsPolygon> m_settings_polygons;

    //! \brief The material used for the region.
    int m_material_number;

    //! \brief Index for order
    int m_index;

    //! \brief Concrete type for matching regions across layers.
    RegionType m_region_type;

    //! \brief Layer number used for the last optimization pass.
    int m_optimized_layer_number = -1;

    //! \brief Previous layer's physical start point for consecutive point ordering.
    std::optional<Point> m_previous_layer_start_point;

    //! \brief Whether last region was spiralized
    bool m_was_last_region_spiral;

    //! \brief Uncut geometry to modify pathing
    PolygonList m_uncut_geometry;

    //! \brief Slicing plane and shift for flattened optimization-space custom anchors.
    Plane m_optimization_slicing_plane;
    Point m_optimization_shift;
};
} // namespace ORNL
