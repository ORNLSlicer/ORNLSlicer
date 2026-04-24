#pragma once

#include <QElapsedTimer>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qmap.h>
#include <qpoint.h>
#include <qquaternion.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "graphics/base_view.h"
#include "graphics/graphics_object.h"
#include "part/part.h"

namespace ORNL {
// Forward
class PartObject;
class PrinterObject;
class PlaneObject;
class GridObject;
class SphereObject;
class PartMetaModel;
class PartMetaItem;
class RightClickMenu;

/*!
 * \brief The main part manipulation view in ORNLSlicer.
 *
 * The part view is the main view in ORNLSlicer. It is responsible for the manipulation of
 * added geometry in the build volume.
 *
 * This view makes use of the PartMetaModel class, which is intended to allow for easy
 * communication between the various sibling widgets.
 */
class PartView : public BaseView {
    Q_OBJECT
  public:
    //! \brief Constructor for the view.
    //! \param sb: Settings to use for the print volume and other affected elements.
    PartView(QSharedPointer<SettingsBase> sb);

    //! \brief Sets the model that this view should track and modify.
    //! \param m: Model to track.
    void setModel(QSharedPointer<PartMetaModel> m);

    //! \brief Returns the list of parts that are not aligned with the surface.
    QList<QSharedPointer<Part>> floatingParts();

    //! \brief Returns the list of parts that are outside the build volume.
    QList<QSharedPointer<Part>> externalParts();

  public slots:
    //! \brief Shows or hides part labels.
    void showLabels(bool show);

    //! \brief Shows or hides part slicing planes.
    void showSlicingPlanes(bool show);

    //! \brief Shows or hides overhangs.
    void showOverhang(bool show);

    //! \brief Shows or hides opt graphics.
    void showSeams(bool show);

    //! \brief Puts the part view into the alignment state. Next clicked triange will
    //!        be aligned to the passed plane.
    //! \param plane: Plane normal to align to.
    void setupAlignment(QVector3D plane);

    //! \brief Centers a part in the build volume by name.
    //! \param name: Name of part to drop.
    //! \todo This should be done using the selected parts instead.
    void centerPart(QString name);

    //! @brief Centers (x, y) the selected parts in the build volume.
    //! @details Maintains relative positions of selected parts.
    void centerSelectedParts();

    //! @brief Drops the selected parts to the printer bed.
    //! @details Maintains relative positions of selected parts.
    void dropSelectedParts();

    //! \brief Callback for when printer settings are changed.
    //! \param sb: New settings object.
    void updatePrinterSettings(QSharedPointer<SettingsBase> sb);

    //! \brief Callback for when optimization settings are changed.
    //! \param sb: New settings object.
    void updateOptimizationSettings(QSharedPointer<SettingsBase> sb);

    //! \brief Callback for when overhang settings are changed.
    //! \param sb: New settings object.
    void updateOverhangSettings(QSharedPointer<SettingsBase> sb);

    //! \brief Callback for when slicing settings are changed.
    //! \param sb: New settings object.
    void updateSlicingSettings(QSharedPointer<SettingsBase> sb);

    //! \brief Moves the camera to its default zoom and orientation.
    virtual void resetCamera() override;

  signals:
    //! \brief Notification of parts that are outside and/or not aligned. Emitted after translations.
    void positioningIssues(QList<QSharedPointer<Part>> opl, QList<QSharedPointer<Part>> fpl);

  protected:
    //! \brief Initalizes the view with the printer and the associated objects.
    void initView() override;

    //! \brief Handles the following: Alignment selection, translation selection, deselection
    void handleLeftClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Object selection
    void handleLeftDoubleClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Object translation
    void handleLeftMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Translation finalization
    void handleLeftRelease(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Rotation setup, Right click menu setup
    void handleRightClick(QPointF mouse_ndc_pos, QPointF global_pos) override;

    //! \brief Handles the following: Object rotation
    void handleRightMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Rotation finalization, right click display
    void handleRightRelease(QPointF mouse_ndc_pos, QPointF global_pos) override;

    //! \brief Handles the following: Cursor updating, Object hover highlighting
    void handleMouseMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Upward object vertical translation
    void handleWheelForward(QPointF mouse_ndc_pos, float delta) override;

    //! \brief Handles the following: Downward object vertical translation
    void handleWheelBackward(QPointF mouse_ndc_pos, float delta) override;

    //! \brief Handles the following: Overhang disable
    void handleMidClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Overhang enable
    void handleMidRelease(QPointF mouse_ndc_pos) override;

    //! \brief centers a graphics part in the printer volume
    //! \param gop the graphics part object
    void centerPart(QSharedPointer<PartObject> gop);

    //! \brief drops a graphics part to the printer floor
    //! \param gop the graphics part object
    void dropPart(QSharedPointer<PartObject> gop);

    //! \brief shifts a graphics part to not intersect with other parts
    //! \param gop the graphics part object
    void shiftPart(QSharedPointer<PartObject> gop);

  private slots:
    //! \brief Recieves updates from model about selections.
    //! \param pm: The item that was just updated.
    void modelSelectionUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Recieves updates from model about new objects.
    //! \param pm: The item that was just updated.
    void modelAdditionUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Reload objects.
    //! \param pm: The item that was just reloaded.
    void modelReloadUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Recieves updates from model about removed objects.
    //! \param pm: The item that was just updated.
    void modelRemovalUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Recieves updates from model about changes to parent/children relationships.
    //! \param pm: The item that was just updated.
    void modelParentingUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Recieves updates from model about changes to part translation/rotation/scale.
    //! \param pm: The item that was just updated.
    void modelTranformUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Recieves updates from model about changes to part mesh type / transparency.
    //! \param pm: The item that was just updated.
    void modelVisualUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief When a transformation is applied, this function is often called afterwards
    //!        to broadcast potential issues with the objects.
    void postTransformCheck();

  private:
    //! \brief Current view state.
    struct {
        //! \brief World space start of a translation event.
        QVector3D translate_start;

        //! \brief World space start for each part relative to the translate_start variable.
        QMap<QSharedPointer<PartObject>, QVector3D> part_trans_start;

        //! \brief If the view is translating or not.
        bool translating = false;

        //! \brief Mouse coordinates of the rotation start.
        QPointF rotate_start;

        //! \brief Quaternion rotations for each part when the rotation started.
        QMap<QSharedPointer<PartObject>, QQuaternion> part_rot_start;

        //! \brief If the view is rotating or not.
        bool rotating = false;

        //! \brief The currently highlighted part.
        QSharedPointer<PartObject> highlighted_part;

        //! \brief The plane that will be used to align a part when the mode.
        QVector3D align_plane_norm;

        //! \brief If the view is aligning or not.
        bool aligning = false;

        //! \brief If overhangs are shown.
        bool overhangs_shown = false;

        //! \brief If opt points are shown.
        bool seams_shown = false;

        //! \brief If slicing planes are shown.
        bool planes_shown = false;

        //! \brief If name plates are shown.
        bool names_shown = false;

        //! \brief If the view is currently blocking the model updates or not.
        bool blocking = false;

        //! \brief Timer used to determine if a rotation or a right click menu should be shown.
        QElapsedTimer right_click_timer;
    } m_state;

    //! \brief Aligns a part using the selected face.
    //! \param gop: The part to align.
    //! \param tri: The face to align with.
    //! \param plane_norm: The plane normal to align to.
    void alignPart(QSharedPointer<PartObject> gop, Triangle tri, QVector3D plane_norm);

    //! \brief Picks a part using the mouse cursor.
    //! \param mouse_ndc_pos: Cursor normalized location.
    //! \param object_set: Set of objects to search through.
    QSharedPointer<PartObject> pickPart(const QPointF& mouse_ndc_pos, QSet<QSharedPointer<PartObject>> object_set);

    //! \brief Finds an object based on the name.
    //! \param name: Name to find.
    //! \todo This should be removed when center part uses selection.
    QSharedPointer<PartObject> findObject(QString name);

    //! \brief Blocks the model from modifying the view. Useful when making model changes in the view to prevent
    //! feedback.
    void blockModel();

    //! \brief Allows the model to modify the view. Useful when making model changes in the view to prevent feedback.
    void permitModel();

    //! \brief The model that is tracked.
    QSharedPointer<PartMetaModel> m_model;

    //! \brief The loaded objects.
    QSet<QSharedPointer<PartObject>> m_part_objects;

    //! \brief The selected set of objects.
    QSet<QSharedPointer<PartObject>> m_selected_objects;

    //! \brief The right click menu.
    RightClickMenu* m_menu;

    //! \brief The printer in the volume. It is the root object for all loaded objects, allowing easy translation.
    QSharedPointer<PrinterObject> m_printer;

    //! \brief The plane that shows up underneath the objects when dragging.
    QSharedPointer<GridObject> m_low_plane;

    //! \brief Current settings for visualization.
    QSharedPointer<SettingsBase> m_sb;
};
} // namespace ORNL
