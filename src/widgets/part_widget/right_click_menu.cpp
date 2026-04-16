#include "widgets/part_widget/right_click_menu.h"

#include <QFileDialog>
#include <QLabel>
#include <QWidgetAction>
#include <qaction.h>
#include <qicon.h>
#include <qlist.h>
#include <qmenu.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qslider.h>
#include <qwidget.h>

#include "managers/session_manager.h"
#include "utilities/enums.h"
#include "widgets/part_widget/model/part_meta_item.h"

namespace ORNL {
RightClickMenu::RightClickMenu(QWidget* parent) : QMenu(("Context menu"), parent) {
    this->setupActions();
    this->setupEvents();
}

void RightClickMenu::setupActions() {
    m_switch_to_build_action = new QAction("Switch to Build", this);
    m_switch_to_clipper_action = new QAction("Switch to Clipper", this);
    m_switch_to_setting_action = new QAction("Switch to Setting", this);
    m_reset_transformation_action = new QAction("Reset Transformation", this);
    m_replace_part_action = new QAction("Replace Part STL", this);
    m_reload_part_action = new QAction("Reload Part(s) STL", this);
    m_delete_part_action = new QAction("Delete Part(s)", this);
    m_lock_part_action = new QAction("Toggle Part Lock(s)", this);

    m_switch_to_clipper_action->setIcon(QIcon(":/icons/clip.png"));
    m_switch_to_build_action->setIcon(QIcon(":/icons/print_head.png"));
    m_switch_to_setting_action->setIcon(QIcon(":/icons/settings_black.png"));
    m_reset_transformation_action->setIcon(QIcon(":/icons/restore.png"));
    m_replace_part_action->setIcon(QIcon(":/icons/folder_black.png"));
    m_reload_part_action->setIcon(QIcon(":/icons/file_refresh_black.png"));
    m_delete_part_action->setIcon(QIcon(":/icons/delete_black.png"));
    m_lock_part_action->setIcon(QIcon(":/icons/lock.png"));

    this->addAction(m_switch_to_build_action);
    this->addAction(m_switch_to_clipper_action);
    this->addAction(m_switch_to_setting_action);
    this->addSeparator();
    this->addAction(m_lock_part_action);
    this->addAction(m_reset_transformation_action);
    this->addAction(m_replace_part_action);
    this->addAction(m_reload_part_action);
    this->addAction(m_delete_part_action);
    this->addSeparator();

    m_transparency_menu = new QMenu("Transparency", this);
    m_transparency_menu->setIcon(QIcon(":/icons/transparency_black.png"));
    m_transparency_menu->setStyleSheet("QMenu{padding: 5px;}");
    this->addMenu(m_transparency_menu);

    QWidgetAction* m_widget_action = new QWidgetAction(this);
    m_transparency_slider = new QSlider(Qt::Orientation::Horizontal);
    m_transparency_slider->setMinimum(0);
    m_transparency_slider->setMaximum(225);
    m_transparency_slider->setValue(0);
    m_transparency_slider->setMinimumSize(m_transparency_menu->size());
    m_transparency_slider->setTracking(true);
    m_widget_action->setDefaultWidget(m_transparency_slider);
    m_transparency_menu->addAction(m_widget_action);

    m_wireframe_action = new QAction("Show wireframe", this);
    m_wireframe_action->setIcon(QIcon(":/icons/vector_triangle_black.png"));
    m_wireframe_action->setCheckable(true);
    m_wireframe_action->setChecked(false);
    this->addAction(m_wireframe_action);

    m_solidwireframe_action = new QAction("Show solid wireframe", this);
    m_solidwireframe_action->setIcon(QIcon(":/icons/vector_triangle_black_solid.png"));
    m_solidwireframe_action->setCheckable(true);
    m_solidwireframe_action->setChecked(false);
    this->addAction(m_solidwireframe_action);
}

void RightClickMenu::setupEvents() {
    connect(m_switch_to_clipper_action, &QAction::triggered, this, [this]() {
        m_switch_to_build_action->setDisabled(false);
        m_switch_to_setting_action->setDisabled(false);
        m_switch_to_clipper_action->setDisabled(true);

        for (auto item : m_selected_items) {
            item->setMeshType(MeshType::kClipping);
        }
    });

    connect(m_switch_to_build_action, &QAction::triggered, this, [this]() {
        m_switch_to_build_action->setDisabled(true);
        m_switch_to_setting_action->setDisabled(false);
        m_switch_to_clipper_action->setDisabled(false);

        for (auto item : m_selected_items) {
            item->setMeshType(MeshType::kBuild);
        }
    });

    connect(m_switch_to_setting_action, &QAction::triggered, this, [this]() {
        m_switch_to_build_action->setDisabled(false);
        m_switch_to_setting_action->setDisabled(true);
        m_switch_to_clipper_action->setDisabled(false);

        for (auto item : m_selected_items) {
            item->setMeshType(MeshType::kSettings);
        }
    });

    connect(m_reset_transformation_action, &QAction::triggered, this, [this]() {
        for (auto item : m_selected_items)
            item->resetTransformation();
    });

    connect(m_replace_part_action, &QAction::triggered, this, [this]() {
        QString filepath = QFileDialog::getOpenFileName(nullptr, QObject::tr("Open STL clipping file"),
                                                        CSM->getMostRecentModelLocation(),
                                                        QObject::tr("Model File (*.stl *.3mf *.obj *.amf)"));

        if (filepath.isNull()) {
            return;
        }

        m_selected_items.first()->replaceInModel(filepath);
    });

    connect(m_reload_part_action, &QAction::triggered, this, [this]() {
        for (auto item : m_selected_items) {
            item->reloadInModel();
        }
    });

    connect(m_delete_part_action, &QAction::triggered, this, [this]() {
        for (auto item : m_selected_items) {
            item->removeFromModel();
        }

        m_selected_items.clear();
    });

    connect(m_transparency_slider, &QSlider::valueChanged, this, [this](int value) {
        for (auto item : m_selected_items) {
            item->setTransparency(255 - value);
        }
    });

    connect(m_wireframe_action, &QAction::triggered, this, [this]() {
        for (auto item : m_selected_items) {
            // Solid wireframe and wireframe cannot both be active, uncheck the other
            item->setSolidWireframe(false);
            m_solidwireframe_action->setChecked(false);
            item->setWireframe(m_wireframe_action->isChecked());
        }
    });
    connect(m_lock_part_action, &QAction::triggered, this, [this]() {
        for (auto item : m_selected_items) {
            item->graphicsPart()->setLocked(!item->graphicsPart()->locked());
        }
    });
    connect(m_solidwireframe_action, &QAction::triggered, this, [this]() {
        for (auto item : m_selected_items) {
            // Solid wireframe and wireframe cannot both be active, uncheck the other
            item->setWireframe(false);
            m_wireframe_action->setChecked(false);
            item->setSolidWireframe(m_solidwireframe_action->isChecked());
        }
    });
}

void RightClickMenu::show(const QPointF& pos, QList<QSharedPointer<PartMetaItem>> items) {
    m_selected_items = items;
    this->disableActions();

    // Must be updated each time a right click occurs in case you switch from one object to another
    // suppress signals since nothing has actually changed
    if (!items.empty()) {
        m_transparency_slider->blockSignals(true);
        m_transparency_slider->setValue(255 - items.at(0)->transparency());
        m_transparency_slider->blockSignals(false);
    }

    this->exec(pos.toPoint());
}

void RightClickMenu::disableActions() {
    if (!m_selected_items.empty()) {
        bool enable_all = false;
        MeshType all_type = m_selected_items.at(0)->meshType();
        for (auto item : m_selected_items) {
            if (all_type != item->meshType()) {
                enable_all = true;
                break;
            }
        }

        if (enable_all) {
            m_switch_to_build_action->setDisabled(false);
            m_switch_to_clipper_action->setDisabled(false);
            m_switch_to_setting_action->setDisabled(false);
        }
        else {
            switch (all_type) {
                case (kSupport):
                case (kBuild):
                    m_switch_to_build_action->setDisabled(true);
                    m_switch_to_clipper_action->setDisabled(false);
                    m_switch_to_setting_action->setDisabled(false);
                    break;
                case (kClipping):
                    m_switch_to_build_action->setDisabled(false);
                    m_switch_to_clipper_action->setDisabled(true);
                    m_switch_to_setting_action->setDisabled(false);
                    break;
                case (kSettings):
                    m_switch_to_build_action->setDisabled(false);
                    m_switch_to_clipper_action->setDisabled(false);
                    m_switch_to_setting_action->setDisabled(true);
                    break;
            }
        }

        m_reset_transformation_action->setDisabled(false);
        m_reload_part_action->setDisabled(false);
        m_delete_part_action->setDisabled(false);
        m_transparency_menu->setDisabled(false);
        m_wireframe_action->setDisabled(false);
        m_solidwireframe_action->setDisabled(false);

        if (m_selected_items.size() == 1) {
            m_replace_part_action->setDisabled(false);
        }
        else {
            m_replace_part_action->setDisabled(true);
        }
    }
    else {
        // Disable all part options
        m_switch_to_clipper_action->setDisabled(true);
        m_switch_to_build_action->setDisabled(true);
        m_switch_to_setting_action->setDisabled(true);
        m_reset_transformation_action->setDisabled(true);
        m_replace_part_action->setDisabled(true);
        m_reload_part_action->setDisabled(true);
        m_delete_part_action->setDisabled(true);
        m_transparency_menu->setDisabled(true);
        m_wireframe_action->setDisabled(true);
        m_solidwireframe_action->setDisabled(true);
    }
}
} // namespace ORNL
