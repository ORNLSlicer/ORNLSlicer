#pragma once

#include <cmath>
#include <vector>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class SettingsBase;
class BaseView;

class WriterBase;
/*!
 * \class SegmentBase
 * \brief Base class for all movements.
 */
class SegmentBase {
  public:
    //! \brief Constructor
    SegmentBase(Point start, Point end);

    //! \brief Clone
    virtual QSharedPointer<SegmentBase> clone() const = 0;

    //! \brief Destructor
    virtual ~SegmentBase() = default;

    //! \brief Start point for this segment.
    Point start() const;

    //! \brief Midpoint for this segment.
    Point midpoint() const;

    //! \brief End point for this segment.
    Point end() const;

    //! \brief Gets the layer number for this segment.
    uint layerNumber();

    //! \brief Gets the line number for this segment.
    uint lineNumber();

    //! \brief Gets the display width.
    //! \return the display width
    float displayWidth();

    //! \brief Gets the display height.
    float displayHeight();

    //! \brief Gets the display type.
    SegmentDisplayType displayType();

    //! \brief Gets the display color.
    QColor color();

    //! \brief Sets the display info for this segment corresponding to a loaded GCode file.
    //! \param display_width: Width of segment in display units.
    //! \param display_length: Length of segment in display units.
    //! \param display_height: Height of segment in display units.
    //! \param type: Type of display segment to generate.
    //! \param color: Color of the display segment to generate.
    //! \param line_num: GCode line number that this segment corresponds to.
    //! \param layer_num: Layer that this GCode line segment belongs to.
    void setDisplayInfo(float display_width, float display_length, float display_height, SegmentDisplayType type,
                        QColor color, uint line_num, uint layer_num);

    //! \brief Sets the display width of the gocde segment
    //! \param display_width the display width
    void setDisplayWidth(float display_width);

    //! \brief Sets the display height of the gcode segment
    //! \param display_height the display height
    void setDisplayHeight(float display_height);

    //! \brief Creates the vertex info for this segment. Requires info set in setGCodeInfo(). Virtual function by
    //! default does nothing. \todo This function expects segments generated using OpenGL scales. A less brittle version
    //! of this would entail scaling as needed
    //!       so that we could eventually use segments internally rather than re-generating them from GCode.
    //! \param vertices: OpenGL vertex array to append to.
    //! \param normals: OpenGL normal array to append to.
    //! \param colors: OpenGL color array to append to.
    virtual void createGraphic(std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors);

    //! \brief Set the start point of this segment.
    void setStart(Point start);

    //! \brief Sets the end point of this segment.
    void setEnd(Point end);

    //! \brief Reverse direction of the segment. Other segments can override this.
    virtual void reverse();

    //! \brief Pure virtual for gcode writer.
    virtual QString writeGCode(QSharedPointer<WriterBase> writer) = 0;

    //! \brief Get the settings base that this segment uses.
    QSharedPointer<SettingsBase> getSb() const;

    //! \brief Set the settings base that this segment uses.
    void setSb(const QSharedPointer<SettingsBase>& sb);

    //! \brief rotates the segment by the quaternion
    virtual void rotate(QQuaternion rotation);

    //! \brief shifts the segment by the values of the point
    virtual void shift(Point shift);

    //! \brief returns the minimum z-coordinate of a segment
    virtual float getMinZ() = 0;

    //! \brief returns true if the segment is printing/extruding
    bool isPrintingSegment();

    //! \brief sets the list of nozzles that should be on when this
    //!        segment prints
    //! \param list of extruders indexes
    void setNozzles(QVector<int> extruder_numbers);

    //! \brief adds a nozzle number to the list
    //! \param extruder number (indexed at 0)
    void addNozzle(int extruder_number);

    //! \brief computes the length of this segment
    //! \return the distance from start to end along this segment
    virtual Distance length();

    //! \brief Segment info metadata.
    struct SegmentInfoMeta {
        // Start point for segment info display.
        Point start;

        //! \brief End point for segment info display.
        Point end;

        //! \brief Print speed for segment info display.
        QString speed;

        //! \brief Extruder speed for segment info display.
        QString extruderSpeed;

        //! \brief Length for segment info display.
        QString length;

        //! \brief Region type for segment info display.
        QString type;

        //! \brief Default counstructor
        SegmentInfoMeta() {}

        //! \brief Check if motion is in XY plane
        bool isXYmove() { return start.x() != end.x() || start.y() != end.y(); }

        //! \brief Compute Z motion change
        float getZChange() { return end.z() - start.z(); }

        //! \brief Compute the 2d angle along X axis (couter clock wise)
        float getCCWXAngle() {
            const float y = end.y() - start.y();
            const float x = end.x() - start.x();

            float angle = 360;
            if (x != 0 || y != 0) {
                float delta = 0;
                if ((y >= 0 && x < 0) || (y < 0 && x < 0)) {
                    delta = 180;
                }
                else if (y < 0 && x >= 0) {
                    delta = 360;
                }

                angle = delta + atan(y / x) * 180 / 3.14159265358979323846;
            }

            return angle;
        }
    } m_segment_info_meta;

  protected:
    //! \brief  Start point for segment.
    Point m_start;

    //! \brief End point for segment.
    Point m_end;

    //! \brief Settings for the segment.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief Non build mods.
    PathModifiers m_non_build_modifiers;

    //! \brief Display information.
    QColor m_color;

    //! \brief The display type for this segment.
    SegmentDisplayType m_display_type;

    //! \brief The line number for this segment.
    uint m_line_num;

    //! \brief The layer number for this segment.
    uint m_layer_num;

    //! \brief The width of the segment in display units.
    float m_display_width;

    //! \brief The length of the segment in display units.
    float m_display_length;

    //! \brief The height of the segment in display units.
    float m_display_height;
};
} // namespace ORNL
