#pragma once

#include <qlist.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "graphics/graphics_object.h"
#include "graphics/objects/printer/printer_object.h"

namespace ORNL {
// Forward
class AxesObject;
class CylinderPlaneObject;

/*!
 * \brief Printer that uses cylindrical coordinates.
 */
class CylindricalPrinterObject : public PrinterObject {
  public:
    //! \brief Constructor
    //! \param view: View to render to.
    //! \param sb: Settings to use.
    //! \param is_true_volume: if this printer is to drawn as a true volume.
    CylindricalPrinterObject(BaseView* view, QSharedPointer<SettingsBase> sb, bool is_true_volume);

    //! \brief the center of the printer volume
    //! \return the center as a QVector3D
    QVector3D printerCenter();

    //! \brief List of parts that are external to the build volume.
    QList<QSharedPointer<PartObject>> externalParts();

  protected:
    //! \brief Hook for updating member variables.
    void updateMembers();
    //! \brief Hook for updating printer geometry.
    void updateGeometry();

  private:
    //! \brief Dims
    float m_radius;
    float m_height;
    float m_x_grid;
    float m_y_grid;

    //! \brief Object spawn location
    QVector3D m_floor_center;

    //! \brief Axes object in corner.
    QSharedPointer<AxesObject> m_axes;
    //! \brief Reflective floor surface.
    QSharedPointer<CylinderPlaneObject> m_floor_plane;
};
} // namespace ORNL
