#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "step/layer/island/island_base.h"
#include "utilities/enums.h"

namespace ORNL {

//! \class MultiNozzleOptimizer
//! \brief a collection of funtions for dividing part geometry and assigning to extruders
class MultiNozzleOptimizer {
  public:
    //! \brief assigns extruder number to islands, sorting by area
    //! \param islands - list of islands that need extruder set
    //! \param nozzle_count - number of extruders that islands will be assigned to
    static void assignByArea(QVector<QSharedPointer<IslandBase>>& islands, int nozzle_count);

    //! \brief assigns extruder number to islands, sorting by location
    //! \param islands - list of islands that need extruder set
    //! \param nozzle_count - number of extruders that islands will be assigned to
    //! \param axis -  which axis to sort location by; can be X or Y
    static void assignByAxisLocation(QVector<QSharedPointer<IslandBase>>& islands, int nozzle_count, Axis axis);

  private:
    //! \brief comparison function to sort islands
    //! \param islands to be compared
    static bool islandLocationXLessThan(const QSharedPointer<IslandBase>& island_1,
                                        const QSharedPointer<IslandBase>& island_2);

    //! \brief comparison function to sort islands
    //! \param islands to be compared
    static bool islandLocationYLessThan(const QSharedPointer<IslandBase>& island_1,
                                        const QSharedPointer<IslandBase>& island_2);

    //! \brief comparison function to sort islands
    //! \param islands to be compared
    static bool islandAreaLessThan(const QSharedPointer<IslandBase>& island_1,
                                   const QSharedPointer<IslandBase>& island_2);
};
} // namespace ORNL
