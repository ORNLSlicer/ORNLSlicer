#pragma once

#include <QComboBox>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QVector3D>
#include <QWidget>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "geometry/segment_base.h"

namespace ORNL {
/*!
 * \class QClickableFrame
 * \brief is a clickable frame that handles mouse press event
 */
class QClickableFrame : public QFrame {
    Q_OBJECT

  public:
    //! \brief Constructor
    //! \param parent: the widget this sits on
    explicit QClickableFrame(QFrame* parent = nullptr) : QFrame(parent) {}

  signals:
    //! \brief Signal that the mouse left button was clicked.
    void mouseLeftButtonClicked();

  private:
    //! \brief Mouse press event
    void mousePressEvent(QMouseEvent* event) {
        if (event->buttons() == Qt::LeftButton)
            emit mouseLeftButtonClicked();
    }
};

/*!
 * \class GCodeInfoControl
 * \brief is a widget that lists segment / bead info
 */
class GCodeInfoControl : public QWidget {
    Q_OBJECT

  public:
    //! \brief Constructor
    //! \param parent: the widget this sits on
    explicit GCodeInfoControl(QWidget* parent = nullptr);

    //! \brief list of segments.
    void setGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode);

    //! \brief Add segment to info tracking list.
    void addSegmentInfo(int selectedLineNumber);

    //! \brief Remove segment from info tracking list.
    void removeSegmentInfo(int selectedLineNumber);

    //! \brief Updates the camera view matrix used to display segment travel direction.
    void setViewMatrix(const QMatrix4x4& viewMatrix);

  private:
    //! \brief Display xy direction
    void updateDirection(double angle);

    //! \brief Display z direction
    void updateZDirection(double angle);

    //! \brief Display direction for the current camera view.
    void updateDirectionForSegment(const QSharedPointer<SegmentBase>& segment);

    //! \brief Converts a 3D travel vector to a screen-space angle.
    double viewAngleForDirection(const QVector3D& direction, double fallbackAngle) const;

    //! \brief Initilizes the widget.
    void setupWidget();

    //! \brief Constructs the widgets within the setting header frame and the layout that holds the subwidgets.
    //! Subwidgets include the icon, the label text, the expand/collapse arrow.
    void setupHeaderWidget();

    //! \brief Load and fill segment info of last line in list
    //! \param lineNo: select the line number
    void fillSegmentInfo(uint lineNo);

    //! \brief Finds the display segment for a G-Code line number.
    QSharedPointer<SegmentBase> segmentForLine(uint lineNo);

    //! \brief Updates the centerline distance readout for the selected segment pair.
    void updateCenterDistanceInfo(uint lineNo);

    //! \brief Formats a distance for display using the preferred distance unit.
    QString formatDistance(double microns) const;

    //! \brief list of segments
    QVector<QVector<QSharedPointer<SegmentBase>>> m_gcode;

    //! \brief Segment currently shown in the info panel.
    QSharedPointer<SegmentBase> m_current_segment;

    //! \brief Current camera view matrix used to orient the direction arrow.
    QMatrix4x4 m_view_matrix;

    //! \brief Int list of gcode line numbers that are currently selected
    QList<int> m_line_no_list;

    //! \brief controls inside this Widget
    QFrame* m_info_display;
    QLabel* m_info_display_indicator;
    QGridLayout* m_info_grid;
    QLabel* m_infolbl_type;
    QLabel* m_infolbl_speed;
    QLabel* m_infolbl_extruder_speed;
    QLabel* m_infolbl_length;
    QLabel* m_infolbl_center_distance;
    QLabel* m_infolbl_layer_no;
    QLabel* m_infolbl_line_no;
    QLabel* m_infolbl_direction;
    QPixmap* m_infopm_direction;
    QPixmap* m_infopm_direction_z;
    QComboBox* m_headercb_lines;
};
} // namespace ORNL
