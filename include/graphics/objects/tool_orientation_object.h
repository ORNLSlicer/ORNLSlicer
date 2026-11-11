#pragma once

#include "graphics/graphics_object.h"
#include "graphics/support/tool_orientation_preview.h"

namespace ORNL {
//! \brief Batched XYZ line triads, separate from selectable G-code geometry.
class ToolOrientationObject : public GraphicsObject {
   public:
    ToolOrientationObject(BaseView* view, const QVector<ToolOrientationPose>& poses, float axis_length);

    //! \brief Omits the ordinary triad represented by the enlarged active pose without rebuilding buffers.
    void excludeLine(uint line);

   protected:
    void draw() override;

   private:
    QList<uint> m_lines;
    int m_excluded_index = -1;
};
}  // namespace ORNL
