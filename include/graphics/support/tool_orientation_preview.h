#pragma once

#include <QList>
#include <QQuaternion>
#include <QSharedPointer>
#include <QVector3D>
#include <QVector>
#include <optional>

#include "geometry/segment_base.h"

namespace ORNL {
//! \brief A tool frame at a G-code command endpoint, in preview coordinates.
struct ToolOrientationPose {
    uint line;
    QVector3D position;
    QQuaternion orientation;
};

//! \brief Cached endpoint sampling and selection shared by the orientation overlay and its tests.
class ToolOrientationPreview {
   public:
    //! \brief Resets file-specific state and sizes the axes from the complete oriented path.
    void reset(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode);

    //! \brief Rebuilds endpoint samples for an inclusive layer/segment range and hidden-type mask.
    void refresh(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode, uint low_layer, uint high_layer,
                 uint low_segment, uint high_segment, SegmentDisplayType hidden_types);

    //! \brief Tracks selection recency using the editor's zero-based line numbers.
    void updateSelection(const QList<int>& added, const QList<int>& removed);
    void clearSelection();
    QList<int> selectedLines() const;

    float axisLength() const;
    const QVector<ToolOrientationPose>& sampledPoses() const;
    std::optional<ToolOrientationPose> activePose() const;

   private:
    float m_axis_length = 0.0f;
    QVector<QSharedPointer<SegmentBase>> m_visible_segments;
    QVector<ToolOrientationPose> m_samples;
    QList<uint> m_selected_lines;
};
}  // namespace ORNL
