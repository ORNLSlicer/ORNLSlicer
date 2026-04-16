#pragma once

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"

namespace ORNL {
/*!
 *  \class ScanSegment
 *  \brief Segment for scan movements.
 */
class ScanSegment : public SegmentBase {
  public:
    //! \brief Constructor
    ScanSegment(Point start, Point end);

    //! \brief Clone
    QSharedPointer<SegmentBase> clone() const override;

    //! \brief Write gcode for scan segment.
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Set segment variable as to whether or not to activate scanner
    //! \param on bool whether or not to set data collection on/off
    void setDataCollection(bool on);

    //! \brief must be implemented from segment_base, don't acutally need it
    float getMinZ() override;

  private:
    //! \brief boolean to track whether or not to trigger laser scanner
    bool m_on_off;
};
} // namespace ORNL
