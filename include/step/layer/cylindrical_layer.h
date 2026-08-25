#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "step/layer/layer.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * @class CylindricalLayer
 * @brief Direct path layer shared by radial and helical cylindrical slicing.
 *
 * A cylindrical layer stores precomputed cylindrical paths and writes them
 * directly, bypassing the normal polymer island and region generation stack.
 * Path-pattern-specific behavior is limited to post-process ordering.
 */
class CylindricalLayer : public Layer {
   public:
    /*!
     * @brief Constructs a cylindrical layer.
     * @param layer_nr One-based layer number used in generated gcode comments.
     * @param sb Settings for this cylindrical layer.
     * @param path_pattern Cylindrical path pattern used to order generated paths.
     */
    CylindricalLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb, CylindricalPathPattern path_pattern);

    /*!
     * @brief Adds one generated cylindrical bead path to this layer.
     * @param path Path containing an optional travel and print segments.
     */
    void addPath(const Path& path);

    /*!
     * @brief Writes direct cylindrical paths without polymer regions or islands.
     * @param writer Gcode writer used by each segment.
     * @return Gcode for all paths in this cylindrical layer.
     */
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! @brief Cylindrical paths are generated during slicing, so compute is a no-op.
    void compute() override;

    /*!
     * @brief Orders cylindrical paths and inserts travels.
     * @param currentLocation Location updated for downstream layer bookkeeping.
     */
    void calculateModifiers(Point& currentLocation) override;

    /*!
     * @brief Returns the initial location used to enter this cylindrical layer.
     * @return First path start, or origin when empty.
     */
    Point getStartLocation() const;

    /*!
     * @brief Returns minimum Z among generated paths.
     * @return Minimum path Z, or 0 when this layer has no paths.
     */
    float getMinZ() override;

    /*!
     * @brief Returns the end of the final generated path.
     * @return Last generated segment endpoint, or origin when this layer has no paths.
     */
    Point getEndLocation() override;

    /*!
     * @brief Checks whether this cylindrical layer contains any generated paths.
     * @return True if at least one path has been added.
     */
    bool hasPaths() const;

   private:
    //! @brief Precomputed travel and print paths for this cylindrical layer.
    QVector<Path> m_paths;

    //! @brief Path pattern used to select radial or helical ordering behavior.
    CylindricalPathPattern m_path_pattern;
};
}  // namespace ORNL
