#include "widgets/main_toolbar.h"

#include <algorithm>

#include <QComboBox>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QInputDialog>
#include <QLayout>
#include <QMenu>
#include <qaction.h>
#include <qfiledevice.h>
#include <qicon.h>
#include <qmath.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qsizepolicy.h>
#include <qtabbar.h>
#include <qtmetamacros.h>
#include <qtoolbar.h>
#include <qwidget.h>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "geometry/mesh/open_mesh.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
MainToolbar::MainToolbar(QWidget* parent) : m_parent(parent), QToolBar(parent) {
    setup();
    setupSubWidgets();
}

void MainToolbar::setup() {
    // Load stylesheet
    this->setupStyle();

    // Add drop shadow
    auto* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);

    // Disable floating and movable, instead we place the toolbar in the window, but outside of a dock area
    this->setFloatable(false);
    this->setMovable(false);

    // Set size
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    resize(m_parent->size());
    this->raise();
}

void MainToolbar::setupSubWidgets() {
    // View tabs
    m_tabs = buildTabs();
    this->addWidget(m_tabs);
    this->addSeparator();

    // Load buttons
    m_add_btn = buildIconButton(":/icons/file_black.png", "Load new model from file", false);
    m_add_btn->setPopupMode(QToolButton::InstantPopup);
    m_add_btn->setObjectName("menuButton");
    m_add_btn->setMenu(buildAddMenu());
    this->addWidget(m_add_btn);

    // Shape add buttons
    m_shape_btn = buildIconButton(":/icons/shape_black.png", "Generates a new shape of a given type", false);
    m_shape_btn->setPopupMode(QToolButton::InstantPopup);
    m_shape_btn->setObjectName("menuButton");
    m_shape_btn->setMenu(buildShapeMenu());
    this->addWidget(m_shape_btn);
    this->addSeparator();

    // Slicing Plane Button
    m_slicing_planes_btn = buildIconButton(":/icons/slicing_plane.png", "Show slicing plane for each part", true);
    this->addWidget(m_slicing_planes_btn);
    connect(m_slicing_planes_btn, &QToolButton::toggled, this,
            [this](bool checked) { emit showSlicingPlanes(checked); });

    // Seam buttons
    m_seam_btn = buildIconButton(":/icons/map_markers_black.png", "Show optimization points", true);
    handleModifiedSetting("");
    this->addWidget(m_seam_btn);
    connect(m_seam_btn, &QToolButton::toggled, this, [this](bool checked) { emit showSeams(checked); });

    // Overhang Button
    m_overhang_button = buildIconButton(":/icons/support_overhang.png", "Show support overhangs", true);
    this->addWidget(m_overhang_button);
    connect(m_overhang_button, &QToolButton::toggled, this, [this](bool checked) { emit showOverhang(checked); });

    // Billboarding Button
    m_billboarding_button = buildIconButton(":/icons/name_black.png", "Show part names in view", true);
    this->addWidget(m_billboarding_button);
    connect(m_billboarding_button, &QToolButton::toggled, this, [this](bool checked) { emit showLabels(checked); });
    this->addSeparator();

    // Bead Inspection Tool / Segment Info Button
    m_segment_info_button = buildIconButton(":/icons/info.png", "Show g-code Bead / Segment Info", true);
    this->addWidget(m_segment_info_button);
    m_segment_info_button->setEnabled(false);
    connect(m_segment_info_button, &QToolButton::toggled, this,
            [this](bool checked) { emit showSegmentInfo(checked); });

    // 2D Gcode Button
    m_2d_gcode_btn = buildIconButton(":/icons/2d_black.png", "Shows g-code preview in orthographic 2D", true);
    this->addWidget(m_2d_gcode_btn);
    m_2d_gcode_btn->setEnabled(false);
    connect(m_2d_gcode_btn, &QToolButton::toggled, this, [this](bool checked) { emit setOrthoGcode(checked); });

    // Show model ghosts
    m_show_ghosts_btn = buildIconButton(":/icons/model_ghosts_black.png", "Shows model previews in g-code view", true);
    this->addWidget(m_show_ghosts_btn);
    m_show_ghosts_btn->setEnabled(false);
    connect(m_show_ghosts_btn, &QToolButton::toggled, this, [this](bool checked) { emit showGhosts(checked); });

    // Export Gcode Button
    m_export_gcode_btn = new QToolButton(this);
    m_export_gcode_btn->setIcon(QIcon(":/icons/export_black.png"));
    m_export_gcode_btn->setToolTip("Export g-code File");
    this->addWidget(m_export_gcode_btn);
    m_export_gcode_btn->setEnabled(false);
    connect(m_export_gcode_btn, &QToolButton::clicked, this, [this]() { emit exportGCode(); });
    this->addSeparator();

    // Slice Button
    m_slice_btn = new QToolButton(this);
    m_slice_btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_slice_btn->setText("SLICE");
    m_slice_btn->setToolTip("Slice loaded parts");
    m_slice_btn->setObjectName("sliceButton");
    m_slice_btn->setEnabled(false);
    m_slice_btn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    connect(m_slice_btn, &QToolButton::clicked, this, [this]() { emit slice(); });
    this->addWidget(m_slice_btn);
}

QTabBar* MainToolbar::buildTabs() {
    auto* tabs = new QTabBar(this);
    tabs->setMinimumWidth(220);
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // tabs->setExpanding(true);

    tabs->addTab("Part View");
    tabs->addTab("G-Code View");

    connect(tabs, &QTabBar::currentChanged, this, [this](int index) {
        enableCorrectOptions();
        emit viewChanged(index);
        this->raise();
    });

    return tabs;
}

QToolButton* MainToolbar::buildIconButton(const QString& icon_loc, const QString& tooltip, bool toggle) {
    auto* button = new QToolButton(this);
    button->setIcon(QIcon(icon_loc));
    button->setToolTip(tooltip);
    button->setCheckable(toggle);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    return button;
}

QMenu* MainToolbar::buildAddMenu() {
    auto* add_menu = new QMenu(this);
    auto* build_part_action = new QAction("Load Build Model", this);
    build_part_action->setIcon(QIcon(":/icons/print_head.png"));
    connect(build_part_action, &QAction::triggered, this, [this]() { emit loadModel(MeshType::kBuild); });
    add_menu->addAction(build_part_action);

    // Add an option for adding a clipping mesh
    auto* clipping_part_action = new QAction("Load Clipping Model", this);
    clipping_part_action->setIcon(QIcon(":/icons/clip.png"));
    connect(clipping_part_action, &QAction::triggered, this, [this]() { emit loadModel(MeshType::kClipping); });
    add_menu->addAction(clipping_part_action);

    // Add an option for adding a settings mesh
    auto* settings_part_action = new QAction("Load Settings Model", this);
    settings_part_action->setIcon(QIcon(":/icons/gear.png"));
    connect(settings_part_action, &QAction::triggered, this, [this]() { emit loadModel(MeshType::kSettings); });
    add_menu->addAction(settings_part_action);

    return add_menu;
}

QMenu* MainToolbar::buildShapeMenu() {
    auto* shape_menu = new QMenu(this);

    auto* settings_box_action = new QAction("Create Box Settings Region", this);
    connect(settings_box_action, &QAction::triggered, this, [this]() {
        double printer_x = qFabs(GSM->getGlobal()->setting<double>(PRS::Dimensions::kXMax) -
                                 GSM->getGlobal()->setting<double>(PRS::Dimensions::kXMin));

        double printer_y = qFabs(GSM->getGlobal()->setting<double>(PRS::Dimensions::kYMax) -
                                 GSM->getGlobal()->setting<double>(PRS::Dimensions::kYMin));

        // Default the cube to being 30% of the smalles printer dimension (X and Y)
        double cube_size = std::min(printer_x, printer_y) * 0.3;

        auto printer_z_min = GSM->getGlobal()->setting<double>(PRS::Dimensions::kZMin);
        auto printer_z_max = GSM->getGlobal()->setting<double>(PRS::Dimensions::kZMax);

        if (GSM->getGlobal()->setting<bool>(PRS::Dimensions::kEnableW)) {
            printer_z_max += qFabs((GSM->getGlobal()->setting<double>(PRS::Dimensions::kWMax) -
                                    GSM->getGlobal()->setting<double>(PRS::Dimensions::kWMin)));
        }

        double printer_height = qFabs(printer_z_max - printer_z_min);

        auto new_mesh =
            QSharedPointer<OpenMesh>::create(MeshFactory::CreateOpenTopBoxMesh(cube_size, cube_size, printer_height));
        QString name = promptForName();
        if (name != "") {
            new_mesh->setName(name);
            new_mesh->setType(MeshType::kSettings);
            new_mesh->setGenType(MeshGeneratorType::kDefaultSettingRegion);
            auto new_part = QSharedPointer<Part>::create(new_mesh);
            CSM->addPart(new_mesh);
        }
    });
    shape_menu->addAction(settings_box_action);

    auto* rect_prism_action = new QAction("Create Rectangular Prism", this);
    connect(rect_prism_action, &QAction::triggered, this, [this, PM = PreferencesManager::getInstance()]() {
        bool len_ok, width_ok, height_ok;

        double length = promptForSize("Enter length", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), len_ok);
        if (!len_ok) {
            return;
        }
        double width = promptForSize("Enter width", PreferencesManager::getInstance()->getDistanceUnitText(),
                                     PreferencesManager::getInstance()->getDistanceUnit()(), width_ok);
        if (!width_ok) {
            return;
        }
        double height = promptForSize("Enter height", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), height_ok);
        if (!height_ok) {
            return;
        }

        auto new_mesh = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(length, width, height));
        QString name = promptForName();
        if (name != "") {
            new_mesh->setName(name);
            auto new_part = QSharedPointer<Part>::create(new_mesh);
            CSM->addPart(new_mesh);
        }
    });
    shape_menu->addAction(rect_prism_action);

    auto* open_rect_prism_action = new QAction("Create Open Top Rectangular Prism", this);
    connect(open_rect_prism_action, &QAction::triggered, this, [this, PM = PreferencesManager::getInstance()]() {
        bool len_ok, width_ok, height_ok;

        double length = promptForSize("Enter length", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), len_ok);
        if (!len_ok) {
            return;
        }
        double width = promptForSize("Enter width", PreferencesManager::getInstance()->getDistanceUnitText(),
                                     PreferencesManager::getInstance()->getDistanceUnit()(), width_ok);
        if (!width_ok) {
            return;
        }
        double height = promptForSize("Enter height", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), height_ok);
        if (!height_ok) {
            return;
        }

        auto new_mesh = QSharedPointer<OpenMesh>::create(MeshFactory::CreateOpenTopBoxMesh(length, width, height));
        QString name = promptForName();
        if (name != "") {
            new_mesh->setName(name);
            auto new_part = QSharedPointer<Part>::create(new_mesh);
            CSM->addPart(new_mesh);
        }
    });
    shape_menu->addAction(open_rect_prism_action);

    auto* triangle_pryamid_action = new QAction("Create Triangular Pyramid", this);
    connect(triangle_pryamid_action, &QAction::triggered, this, [this, PM = PreferencesManager::getInstance()]() {
        bool ok;
        double length = promptForSize("Enter height", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), ok);

        if (ok) {
            auto new_mesh = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateTriaglePyramidMesh(length));
            QString name = promptForName();
            if (name != "") {
                new_mesh->setName(name);
                auto new_part = QSharedPointer<Part>::create(new_mesh);
                CSM->addPart(new_mesh);
            }
        }
    });
    shape_menu->addAction(triangle_pryamid_action);

    auto* cylinder_action = new QAction("Create Cylinder", this);
    connect(cylinder_action, &QAction::triggered, this, [this, PM = PreferencesManager::getInstance()]() {
        bool ok;
        double radius = promptForSize("Enter radius", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), ok);
        double height = promptForSize("Enter height", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), ok);
        int resolution = int(promptForSize("Enter resolution", "segments", 1.0, ok));

        if (ok) {
            auto new_mesh =
                QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(radius, height, resolution));
            QString name = promptForName();
            if (name != "") {
                new_mesh->setName(name);
                auto new_part = QSharedPointer<Part>::create(new_mesh);
                CSM->addPart(new_mesh);
            }
        }
    });
    shape_menu->addAction(cylinder_action);

    auto* cone_action = new QAction("Create Cone", this);
    connect(cone_action, &QAction::triggered, this, [this, PM = PreferencesManager::getInstance()]() {
        bool ok;
        double radius = promptForSize("Enter radius", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), ok);
        double height = promptForSize("Enter height", PreferencesManager::getInstance()->getDistanceUnitText(),
                                      PreferencesManager::getInstance()->getDistanceUnit()(), ok);
        int resolution = int(promptForSize("Enter resolution", "segments", 1.0, ok));

        if (ok) {
            auto new_mesh = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateConeMesh(radius, height, resolution));
            QString name = promptForName();
            if (name != "") {
                new_mesh->setName(name);
                auto new_part = QSharedPointer<Part>::create(new_mesh);
                CSM->addPart(new_mesh);
            }
        }
    });
    shape_menu->addAction(cone_action);

    return shape_menu;
}

void MainToolbar::enableCorrectOptions() {
    if (m_tabs->currentIndex()) {
        m_add_btn->setEnabled(false);
        m_shape_btn->setEnabled(false);
        m_slicing_planes_btn->setEnabled(false);
        m_overhang_button->setEnabled(false);
        m_billboarding_button->setEnabled(false);
        m_seam_btn->setEnabled(false);
        m_segment_info_button->setEnabled(true);
        m_2d_gcode_btn->setEnabled(true);
        m_show_ghosts_btn->setEnabled(true);
        m_export_gcode_btn->setEnabled(true);
    }
    else {
        m_add_btn->setEnabled(true);
        m_shape_btn->setEnabled(true);
        m_slicing_planes_btn->setEnabled(true);
        m_overhang_button->setEnabled(true);
        m_billboarding_button->setEnabled(true);
        m_segment_info_button->setEnabled(false);
        m_2d_gcode_btn->setEnabled(false);
        m_show_ghosts_btn->setEnabled(false);
        m_export_gcode_btn->setEnabled(false);
        handleModifiedSetting(""); // Checks and sets seam button
    }
}

QString MainToolbar::promptForName() {
    bool ok;
    QString name;

    QString label = "Enter a Name:";

    while (name.isEmpty()) {
        name = QInputDialog::getText(this, tr("New Mesh Name"), label, QLineEdit::Normal, "", &ok);

        if (!ok)
            break;

        if (CSM->getPart(name) != nullptr) {
            label = "Name already in use. Please enter another:";
            name = "";
        }
    }

    return name;
}

double MainToolbar::promptForSize(const QString& label_text, const QString& unit_text, const double unit_conversion,
                                  bool& ok) {
    QString str_value;
    QString label = label_text + "(" + unit_text + "):";
    double value = 0.0;
    ok = true;

    while (str_value.isEmpty()) {
        str_value = QInputDialog::getText(this, tr("Input"), label, QLineEdit::Normal, "", &ok);

        if (!ok)
            break;

        bool conv_ok = false;
        value = str_value.toDouble(&conv_ok);

        if (!conv_ok) {
            label = "Error with number. " + label_text + "(" + unit_text + "):";
            str_value = "";
        }
    }

    return value * unit_conversion;
}

void MainToolbar::setView(int index) {
    m_tabs->setCurrentIndex(index);
    this->raise();
}

void MainToolbar::setupStyle() {
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "main_toolbar.qss"));
    style->open(QIODevice::ReadOnly);
    this->setStyleSheet(style->readAll());
    style->close();
}

void MainToolbar::resize(QSize new_size) {
    const int max_width = Constants::UI::MainToolbar::kMaxWidth - Constants::UI::MainToolbar::kEndOffset;

    int new_widget_size =
        new_size.width() - Constants::UI::MainToolbar::kStartOffset - Constants::UI::MainToolbar::kEndOffset;
    if (new_widget_size > max_width || new_widget_size < 0) {
        this->setMinimumWidth(max_width);
        this->setMaximumWidth(max_width);
        new_widget_size = max_width;
    }
    else {
        this->setMinimumWidth(new_widget_size);
        this->setMaximumWidth(new_widget_size);
    }

    // Center widget
    int center_pos = new_size.width() / 2;
    int widget_pos = center_pos - (new_widget_size / 2);
    this->move(widget_pos, Constants::UI::MainToolbar::kVerticalOffset);

    this->raise();
}

void MainToolbar::setSliceAbility(bool status) { m_slice_btn->setEnabled(status); }

void MainToolbar::setExportAbility(bool status) { m_export_gcode_btn->setEnabled(status); }

void MainToolbar::handleModifiedSetting(const QString& setting_key) {
    IslandOrderOptimization islandOrder =
        static_cast<IslandOrderOptimization>(GSM->getGlobal()->setting<int>(PS::Optimizations::kIslandOrder));
    PathOrderOptimization pathOrder =
        static_cast<PathOrderOptimization>(GSM->getGlobal()->setting<int>(PS::Optimizations::kPathOrder));
    PointOrderOptimization pointOrder =
        static_cast<PointOrderOptimization>(GSM->getGlobal()->setting<int>(PS::Optimizations::kPointOrder));
    bool secondPointEnabled = GSM->getGlobal()->setting<bool>(PS::Optimizations::kEnableSecondCustomLocation);

    // Disable button.
    if (islandOrder != IslandOrderOptimization::kCustomPoint && pathOrder != PathOrderOptimization::kCustomPoint &&
        pointOrder != PointOrderOptimization::kCustomPoint && !secondPointEnabled) {
        m_seam_btn->setDisabled(true);
        m_seam_btn->setToolTip("Custom optimization points are not set");
        m_seam_btn->setChecked(false);
        emit showSeams(false);
    }
    else {
        m_seam_btn->setDisabled(false);
        m_seam_btn->setToolTip("Show optimization points");
    }
}
} // namespace ORNL
