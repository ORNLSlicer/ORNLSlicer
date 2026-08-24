#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "step/layer/layer.h"

namespace ORNL {
/*!
 * @class RadialLayer
 * @brief Direct path layer used by the radial cylinder slicer.
 *
 * A radial layer stores precomputed paths and writes them directly.  It does
 * not own polymer islands or regions because radial slicing v1 clips ring
 * paths against the model and skips the normal polymer path-generation stack.
 */
class RadialLayer : public Layer {
   public:
    /*!
     * @brief Constructs a radial layer.
     * @param layer_nr One-based layer number used in generated gcode comments.
     * @param sb Settings for this radial layer.
     */
    RadialLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb);

    /*!
     * @brief Adds one generated radial bead path to this layer.
     * @param path Path containing an optional travel and print line segments.
     */
    void addPath(const Path& path);

    /*!
     * @brief Writes direct radial paths without polymer regions or islands.
     * @param writer Gcode writer used by each segment.
     * @return Gcode for all paths in this radial layer.
     */
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! @brief Radial paths are generated during slicing, so compute is a no-op.
    void compute() override;

    /*!
     * @brief Orders same-circle radial arcs with configured path-order settings and inserts travels.
     * @param currentLocation Location updated for downstream layer bookkeeping.
     */
    void calculateModifiers(Point& currentLocation) override;

    /*!
     * @brief Returns the initial location used to enter this radial layer.
     * @return First travel start if one exists, otherwise the first path start, or origin when empty.
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
     * @brief Checks whether this radial layer contains any generated paths.
     * @return True if at least one path has been added.
     */
    bool hasPaths() const;

   private:
    //! @brief Precomputed travel and print paths for this radial layer.
    QVector<Path> m_paths;
};
}  // namespace ORNL
