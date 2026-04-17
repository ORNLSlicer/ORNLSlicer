#pragma once

#include <vector>

#include <qcolor.h>
#include <qmatrix4x4.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"
#include "utilities/constants.h"

namespace ORNL {
/*!
 * \brief A arrow drawn using GL_LINES. Mostly used to draw parenting relationships between parts.
 */
class ArrowObject : public GraphicsObject {
  public:
    //! \brief Constructor
    //! \param view: View to render to.
    //! \param begin: Beginning point.
    //! \param end: End point.
    //! \param color Color of arrow.
    ArrowObject(BaseView* view, QVector3D begin, QVector3D end, QColor color = Constants::Colors::kBlack);

    //! \brief Constructor
    //! \param view: View to render to.
    //! \param begin: Object to track as tail.
    //! \param end: Object to track as head.
    //! \param color Color of arrow.
    ArrowObject(BaseView* view, QSharedPointer<GraphicsObject> begin, QSharedPointer<GraphicsObject> end,
                QColor color = Constants::Colors::kBlack);

    //! \brief Sets the beginning of the arrow.
    void setBegin(QVector3D begin);
    //! \brief Sets the end of the arrow.
    void setEnd(QVector3D end);

    //! \brief If tracking, update the arrow to the object center locations.
    void updateEndpoints();

  private:
    //! \brief Initalizes arrow.
    void initArrow(BaseView* view);
    //! \brief Finds a transform for the arrow between the beginning and end.
    QMatrix4x4 findTransform(QVector3D begin, QVector3D end);

    //! \brief Objects that are tracked.
    QSharedPointer<GraphicsObject> m_head_tracking;
    QSharedPointer<GraphicsObject> m_tail_tracking;

    //! \brief Beginning and end points.
    QVector3D m_begin;
    QVector3D m_end;

    //! \param Vertices for the line.
    std::vector<float> m_tail_vertices;
    //! \param Vertices for the arrow head.
    std::vector<float> m_head_vertices;

    //! \brief Color
    QColor m_color;
};
} // namespace ORNL
