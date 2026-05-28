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
 * @class HelicalLayer
 * @brief Direct path layer used by the helical slicer.
 *
 * A helical layer represents one cylindrical radius worth of clipped helix
 * fragments.  It stores precomputed paths and writes them directly, bypassing
 * the normal polymer island and region generation stack.
 */
class HelicalLayer : public Layer {
  public:
    /*!
     * @brief Constructs a helical layer.
     * @param layer_nr One-based layer number used in generated gcode comments.
     * @param sb Settings for this helical layer.
     */
    HelicalLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb);

    /*!
     * @brief Adds one generated helical bead path to this layer.
     * @param path Path containing an optional travel and print line segments.
     */
    void addPath(const Path& path);

    /*!
     * @brief Writes direct helical paths without polymer regions or islands.
     * @param writer Gcode writer used by each segment.
     * @return Gcode for all paths in this helical layer.
     */
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! @brief Helical paths are generated during slicing, so compute is a no-op.
    void compute() override;

    /*!
     * @brief Orders open helical fragments and inserts travels.
     * @param currentLocation Location updated for downstream layer bookkeeping.
     */
    void calculateModifiers(Point& currentLocation) override;

    /*!
     * @brief Returns the initial location used to enter this helical layer.
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
     * @brief Checks whether this helical layer contains any generated paths.
     * @return True if at least one path has been added.
     */
    bool hasPaths() const;

  private:
    //! @brief Precomputed travel and print paths for this helical layer.
    QVector<Path> m_paths;
};
} // namespace ORNL
