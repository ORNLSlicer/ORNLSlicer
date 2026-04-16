#pragma once

#include <QDate>
#include <QFile>
#include <QStringBuilder>
#include <QTextStream>
#include <QVector3D>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "gcode/gcode_meta.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class PathSegment;

class WriterBase {
  public:
    //! \brief Default Constructor
    WriterBase(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);

    //! \brief Writes a tag denoting that the gcode came from this slicer
    virtual QString writeSlicerHeader(const QString& syntax);

    //! \brief Writes important settings info to the header for the operator to read
    virtual QString writeSettingsHeader(GcodeSyntax syntax);

    //! \brief Writes initial setup instructions for the machine state
    virtual QString writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                      int num_layers) = 0;

    //! \brief Writes comment to announce layer change
    virtual QString writeLayerChange(uint layer_number);

    //! \brief Writes G-Code to be executed at the start of the layer
    virtual QString writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) = 0;

    //! \brief Writes G-Code to be executed each layer before each part
    virtual QString writeBeforePart(QVector3D normal = QVector3D()) = 0;

    //! \brief Writes G-Code to be exectued before each island
    virtual QString writeBeforeIsland() = 0;

    //! \brief Writes G-Code to be exectued before each scan
    virtual QString writeBeforeScan(Point min, Point max, int layer, int boundingBox, Axis axis, Angle angle) {
        return QString();
    }

    //! \brief Writes G-Code to be executed at the start of each region
    virtual QString writeBeforeRegion(RegionType type, int pathSize = 0) = 0;

    //! \brief Writes G-Code to be executed at the start of each path
    virtual QString writeBeforePath(RegionType type) = 0;

    //! \brief Writes G-Code for travel lift between paths; only used for syntaxes that can't use defined slicing plane
    virtual QString writeTravelLift(bool lift) { return QString(); }

    //! \brief Writes G-Code for traveling between paths
    virtual QString writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                QSharedPointer<SettingsBase> params) = 0;

    //! \brief Writes G-Code for line segment
    virtual QString writeLine(const Point& start_point, const Point& target_point,
                              const QSharedPointer<SettingsBase> params) = 0;

    //! \brief Writes G-Code for scan segment
    virtual QString writeScan(Point target_point, Velocity speed, bool on_off) { return QString(); }

    //! \brief Writes G-Code for arc segment
    virtual QString writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                             const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
        return QString();
    }

    //! \brief Writes G-Code to be executed after each path
    virtual QString writeAfterPath(RegionType type) = 0;

    //! \brief Writes G-Code to be executed after each region
    virtual QString writeAfterRegion(RegionType type) = 0;

    //! \brief Writes G-Code to be executed after each scan
    virtual QString writeAfterScan(Distance beadWidth, Distance laserStep, Distance laserResolution) {
        return QString();
    }

    //! \brief Writes G-Code to be executed once all scans are complete
    virtual QString writeAfterAllScans() { return QString(); }

    //! \brief Writes G-Code to be executed at the end of each island
    virtual QString writeAfterIsland() = 0;

    //! \brief Writes G-Code to be executed at the end of each part each layer
    virtual QString writeAfterPart() = 0;

    //! \brief Writes G-Code to be executed at the end of each layer
    virtual QString writeAfterLayer() = 0;

    //! \brief Writes G-Code for shutting down the machine
    virtual QString writeShutdown() = 0;

    //! \brief Writes a list of all settings with the raw value to the footer as comments
    virtual QString writeSettingsFooter();

    //! \brief Set current feedrate
    void setFeedrate(Velocity feedrate);

    //! \brief Return current feedrate
    Velocity getFeedrate() const;

    //! \brief Write purge command
    virtual QString writePurge(int RPM, int duration, int delay) { return QString(); }

    //! \brief Writes G-Code for a pause, G4
    virtual QString writeDwell(Time time) = 0;

    //! \brief Writes comment for empty step
    QString writeEmptyStep();

    //! \brief writes the string param as a comment on its own line
    QString writeCommentLine(QString comment);

  protected:
    //! \brief Writes a comment
    QString comment(const QString& text);
    //! \brief Writes a comment and ends the line
    QString commentLine(const QString& text);
    //! \brief Writes a comment with a preceding space, then ends the line
    QString commentSpaceLine(const QString& text);

    //! \brief gets a vector in the direction normal to the plane and of a length = travel lift height
    QVector3D getTravelLift();

    GcodeMeta m_meta;

    //! \brief The settings the region will use.
    QSharedPointer<SettingsBase> m_sb;
    QChar m_newline;
    QString m_empty_step_comment;
    Velocity m_feedrate;
    Distance m_current_z, m_current_w, m_last_z, m_last_w;
    Point m_start_point;
    QVector<bool> m_extruders_on;

    //! \brief maintains the min z of the last layer so that we can determine if/when to move the table
    float m_min_z;

    //! \brief preallocated common prefixes that all writers use
    QChar m_space;
    QString m_x, m_y, m_z, m_w, m_f, m_s, m_p, m_i, m_j, m_k, m_r, m_l, m_e, m_q, m_a, m_b, m_G0, m_G1, m_G2, m_G3,
        m_G4, m_G5, m_M3, m_M5;

    //! \brief whether or not layer should be spiralized
    bool m_spiral_layer;

  private:
}; // class WriterBase
} // namespace ORNL
