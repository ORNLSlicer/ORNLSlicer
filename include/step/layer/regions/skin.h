#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class Skin : public RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param index: index for region order
    //! \param settings_polygons: a vector of settings polygons to apply
    Skin(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Writes the gcode for the skin.
    //! \param writer Writer type to use for gcode output
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the skin region.
    void compute(uint layer_num, QSharedPointer<SyncManager>& sync) override;

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param innerMostClosedContour: used for subsequent path modifiers
    //! \param outerMostClosedContour: used for subsequent path modifiers
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                  QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) override;

    //! \brief Creates paths for the skin region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    Path createPath(Polyline line) override;

    //! \brief Adds geometry above the skin.
    //! \param poly_list geometry to add
    void addUpperGeometry(const PolygonList& poly_list);

    //! \brief Adds geometry below the skin.
    //! \param poly_list geometry to add
    void addLowerGeometry(const PolygonList& poly_list);

    //! \brief Sets booleans accordingly
    //! \param top bool whether or not upper geometry includes top of object (must always
    //! add skins if near top)
    //! \param bottom bool whether or not upper geometry includes bottom of object (must always
    //! add skins if near bottom)
    //! \param gradual bool whether or not gradual geometry includes top of object (must always
    //! add additional division if near top)
    void setGeometryIncludes(bool top, bool bottom, bool gradual);

    //! \brief Adds geometry above the skin for gradual pattern steps.
    //! \param poly_list geometry to add
    void addGradualGeometry(const PolygonList& poly_list);

  private:
    //! \brief Helper function to create pattern for skin and gradual areas
    //! \param pattern Selected pattern
    //! \param geometry Geometric bounds to apply pattern to
    //! \param beadWidth Currently set bead width
    //! \param lineSpacing Line spacing calculated from infill percentage
    //! \param patternAngle Selected angle for pattern rotation
    QVector<Polyline> createPatternForArea(InfillPatterns pattern, PolygonList& geometry, Distance beadWidth,
                                           Distance lineSpacing, Angle patternAngle);

    //! \brief Helper function to optimize skin and gradual areas
    //! \param poo currently loaded path optimizer
    //! \param supportsG3 whether or not G2/G3 is supported for subsequent path modifiers
    //! \param innerMostClosedContour used for subsequent path modifiers
    //! \param current_location updating to most recent location
    void optimizeHelper(PolylineOrderOptimizer poo, bool supportsG3, QVector<Path>& innerMostClosedContour,
                        Point& current_location, InfillPatterns pattern, QVector<Polyline> lines, PolygonList geometry);

    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    //! \param innerMostClosedContour used for Prestarts (currently only skins/infill)
    void calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) override;

    //! \brief Holds the computed geometry before it is converted into paths
    QVector<Polyline> m_computed_geometry;

    //! \brief Holds the computed gradual geometry before it is converted into paths
    QVector<QVector<Polyline>> m_gradual_computed_geometry;

    //! Compute corresponding skins
    void computeTopSkin(const int& top_count);
    void computeBottomSkin(const int& bottom_count);
    void computeGradualSkinSteps(const int& gradual_count);

    //! \brief The geometry above the current layer relevant for this skin.
    QVector<PolygonList> m_upper_geometry;

    //! \brief The geometry below the current layer relevant for this skin.
    QVector<PolygonList> m_lower_geometry;

    //! \brief The geometry above the current layer relevant for this skin's gradual areas.
    QVector<PolygonList> m_gradual_geometry;

    //! \brief Holds intermediate results of geometry calculations for above/below.
    PolygonList m_skin_geometry;

    //! \brief Holds intermediate results of geometry calculations for gradual.
    QVector<PolygonList> m_gradual_skin_geometry;

    //! \brief Indicates whether upper geometry includes top layer
    bool m_upper_geometry_includes_top;

    //! \brief Indicates whether lower geometry includes bottom layer
    bool m_lower_geometry_includes_bottom;

    //! \brief Indicates whether gradual geometry includes top layer
    bool m_gradual_geometry_includes_top;

    //! \brief Holds paths created by gradual areas as these paths may be different patterns
    // QVector<QVector<Path>> m_gradual_paths;
};
} // namespace ORNL
