#pragma once

#include <qlist.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "graphics/graphics_object.h"

namespace ORNL {
// Forward
class PartObject;
class SeamObject;

/*!
 * \brief The base class for printer graphics.
 *
 * The PrinterObject class takes care of propagating settings and updates down the line to
 * the derived classes.
 */
class PrinterObject : public GraphicsObject {
  public:
    //! \brief Update this printer using new settings.
    //! \param sb: Settings to use.
    void updateFromSettings(QSharedPointer<SettingsBase> sb);

    //! \brief the center of the printer volume
    //! \return the center as a QVector3D
    virtual QVector3D printerCenter() = 0;

    //! \brief List of parts that are external to the build volume.
    virtual QList<QSharedPointer<PartObject>> externalParts() = 0;

    //! \brief Shows or hides seams.
    void setSeamsHidden(bool hide);

    //! \brief gets the default zoom level for the printer
    //! \return the default zoom in OpenGL space
    float getDefaultZoom();

  protected:
    //! \brief Empty constructor (for derived classes).
    PrinterObject(bool is_true_volume);

    //! \brief Hook for updating member variables in derived classes.
    virtual void updateMembers() = 0;
    //! \brief Hook for updating printer geometry in derived classes.
    virtual void updateGeometry() = 0;
    //! \brief Updates seam locations based on settings.
    void updateSeams();

    //! \brief Sets the settings base that this printer should use from derived classes.
    void setSettings(QSharedPointer<SettingsBase> sb);
    //! \brief Gets the settings base for this printer.
    QSharedPointer<SettingsBase> getSettings();

    //! \brief Creates the seam graphics.
    void createSeams();

    //! \brief If this printer is a "true" volume. That is, drawn at the exact coordinates
    //! \return if printer is true volume
    bool isTrueVolume();

    //! \brief the max dim of the printer
    QVector3D m_printer_max_dims;

  private:
    //! \brief If seams are shown.
    bool m_seams_shown = false;

    //! \brief If this printer is a "true" volume. That is, drawn at the exact coordinates
    bool m_is_true_volume = false;

    //! \brief Settings.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief All seam graphics.
    struct {
        QSharedPointer<SeamObject> custom_island_opt;
        QSharedPointer<SeamObject> custom_path_opt;
        QSharedPointer<SeamObject> custom_point_opt;
        QSharedPointer<SeamObject> custom_point_second_opt;
    } m_seams;
};
} // namespace ORNL
