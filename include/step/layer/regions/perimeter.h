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

    //! \brief Returns whether connected inset geometry was emitted during the latest optimization.
    bool connectedInsetGeometryConsumed() const;

   private:
    struct ConnectedInsetSegment {
        Point start;
        Point end;
        double min_x       = 0.0;
        double max_x       = 0.0;
        double min_y       = 0.0;
        double max_y       = 0.0;
        int geometry_index = -1;
    };

    struct ConnectedInsetIndexNode {
        double min_x = 0.0;
        double max_x = 0.0;
        double min_y = 0.0;
        double max_y = 0.0;
        int begin    = 0;
        int end      = 0;
        int left     = -1;
        int right    = -1;
    };

    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    void calculateModifiers(Path& path, bool supportsG3) override;

    //! \brief Creates modifiers, optionally treating forward tip wipe as an open-loop wipe.
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    //! \param open_loop_tip_wipe Whether forward tip wipe should be emitted from the open path end.
    //! \param continues_to_branch Whether the modified path immediately branches to another printing path.
    //! \param include_startup Whether startup modifiers should be generated for this path.
    void calculateModifiers(Path& path, bool supportsG3, bool open_loop_tip_wipe, bool continues_to_branch = false,
                            bool include_startup = true);

    //! \brief Applies end-of-path modifiers using inset settings.
    //! \param path Connected inset path receiving the modifiers.
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift.
    //! \param open_loop_tip_wipe Whether forward tip wipe should be emitted from the open path end.
    void calculateConnectedInsetEndModifiers(Path& path, bool supportsG3, bool open_loop_tip_wipe);

    /**
     * @brief Creates either an open or closed path with a caller-selected minimum segment length.
     * @param[in] line Polyline representing the path.
     * @param[in] min_segment_length Minimum retained segment length.
     * @param[in] closed Whether to connect the final point back to the first point.
     * @return Polyline converted to a path.
     */
    Path createPath(Polyline line, Distance min_segment_length, bool closed);

    /**
     * @brief Create a path with localized settings applied to segments based on settings regions.
     * @param[in] line Polyline representing the path.
     * @param[in] closed Whether to connect the final point back to the first point.
     * @return Path with localized settings applied.
     * @warning Handles cases of overlapping settings regions by applying the first region found.
     */
    Path createPathWithLocalizedSettings(const Polyline& line, bool closed);

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
    void applyConnectedInsetSettings(Path& path);

    //! \brief Rebuilds the spatial index used to match output points to connected inset segments.
    void rebuildConnectedInsetIndex();

    //! \brief Builds one node of the connected inset spatial index.
    int buildConnectedInsetIndexNode(int begin, int end);

    //! \brief Returns the first connected inset geometry containing a point and optionally marks every match consumed.
    int connectedInsetGeometryIndex(const Point& point, double tolerance,
                                    QVector<bool>* consumed_geometry = nullptr) const;

    //! \brief Searches one node of the connected inset spatial index.
    int connectedInsetGeometryIndex(int node_index, const Point& point, double tolerance,
                                    QVector<bool>* consumed_geometry) const;

    //! \brief Returns the inset bead width for a connected inset segment.
    //! \param start Segment start point.
    //! \param end Segment end point.
    //! \param parent_sb Settings, including localized overrides, for the segment.
    Distance connectedInsetWidthForSegment(const Point& start, const Point& end,
                                           const QSharedPointer<SettingsBase>& parent_sb) const;

    //! \brief Holds the computed geometry before it is converted into paths
    QVector<Polyline> m_computed_geometry;

    //! \brief Holds the bead width associated with each computed contour in m_computed_geometry
    QVector<Distance> m_computed_widths;

    //! \brief Holds computed inset geometry to connect after spiral perimeters.
    QVector<Polyline> m_connected_inset_geometry;

    //! \brief Holds the bead width associated with each connected inset contour.
    QVector<Distance> m_connected_inset_widths;

    //! \brief Tracks which connected inset contours were emitted during the latest optimization.
    QVector<bool> m_connected_inset_geometry_consumed;

    //! \brief Flattened connected inset segments used by the spatial index.
    QVector<ConnectedInsetSegment> m_connected_inset_segments;

    //! \brief Segment indices partitioned by the connected inset spatial index.
    QVector<int> m_connected_inset_segment_order;

    //! \brief Bounding-volume hierarchy for connected inset point queries.
    QVector<ConnectedInsetIndexNode> m_connected_inset_index;

    //! \brief Holds the layer number that we are currently on
    uint m_layer_num;
};
}  // namespace ORNL
