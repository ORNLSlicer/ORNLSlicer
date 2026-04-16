#pragma once

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 *  \class TravelSegment
 *  \brief Segment for travel movements.
 */
class TravelSegment : public SegmentBase {
  public:
    //! \brief Constructor
    //! \param start point
    //! \param end point
    //! \param liftType type of lift to apply to travel (only raise, only lower, both, none)
    //! Default lift type is both
    TravelSegment(Point start, Point end, TravelLiftType liftType = TravelLiftType::kBoth);

    //! \brief Set the travel lift type
    void setLiftType(TravelLiftType newLiftType);

    //! \brief Clone
    QSharedPointer<SegmentBase> clone() const override;

    //! \brief Write gcode for travel segment.
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief returns minimum z-coordinate of the travel
    float getMinZ() override;

  private:
    //! private type to track what type of lift to apply to this particular travel
    TravelLiftType m_lift_type;
};
} // namespace ORNL
