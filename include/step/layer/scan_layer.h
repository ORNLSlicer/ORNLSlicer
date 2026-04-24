#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/polygon_list.h"
#include "step/layer/regions/region_base.h"
#include "step/step.h"

namespace ORNL {
class IslandBase;
/*!
 *  \class ScanLayer
 *  \brief Step that generates pathing for a laser or thermal scan.
 */
class ScanLayer : public Step {
  public:
    //! \brief Constructor
    ScanLayer(int layer, const QSharedPointer<SettingsBase>& sb);

    //! \brief Writes the code for the layer.
    //! \param writer WriterBase for outputting gcode
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the layer.
    void compute() override;

    //! \brief Connect next path via travels to build islands in Layer class
    //! \param start Point to defines current location
    //! \param index Int that serves to lookup key
    void connectPaths(Point& start, int& start_index, QVector<QSharedPointer<RegionBase>>& previousRegions);

    //! \brief Calculates modifiers (none currently apply to scan pathing)
    //! \param currentLocation Point that describes current location
    void calculateModifiers(Point& currentLocation) override;

    //! \brief gets the last location in this layer
    //! \return the last location of (0, 0, 0) if there are no paths
    Point getEndLocation() override;

    //! \brief remove rotation and shift compensation during cross sectioning using
    //!        m_plane_normal and m_shift amount; should only be called once
    //!        when dealing with clean objects
    void unorient();

    //! \brief compensates for rotation and shift during cross sectioning using
    //!        m_plane_normal and m_shift amount; should only be called once
    void reorient();

    //! \brief returns the minimum z of a layer
    float getMinZ() override;

    //! \brief Sets scan as first
    void setFirst();

  private:
    //! \brief Layer number
    int m_layer_num;

    //! \brief First travel connection.  Potentially requires a unique case.
    bool m_first_connect;
};
} // namespace ORNL
