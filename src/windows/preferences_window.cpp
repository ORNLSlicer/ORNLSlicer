#include "windows/preferences_window.h"

#include "QFileDialog"
#include "QGridLayout"
#include "QLabel"
#include "QLineEdit"
#include "QMenu"
#include "QMenuBar"
#include "QScrollArea"
#include "QSpinBox"
#include "QStandardPaths"
#include "QTabWidget"
#include "widgets/visualization_color_picker.h"

namespace ORNL {

PreferencesWindow::PreferencesWindow(QWidget* parent)
    : QMainWindow(parent), m_preferences_manager(PreferencesManager::getInstance()) {
    setupLayout();
    setupEvents();
}

PreferencesWindow::~PreferencesWindow() {}

void PreferencesWindow::setupLayout() {
    setWindowTitle("Preferences");

    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/slicer-2_logo.png"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    QMenuBar* menubar = new QMenuBar();

    // actions must be added immediately
    QMenu* fileMenu = menubar->addMenu("File");
    fileMenu->addAction("Import Preferences", this, &PreferencesWindow::importPreferences);
    fileMenu->addAction("Export Preferences", this, &PreferencesWindow::exportPreferences);

    this->setMenuBar(menubar);

    QWidget* centralWidget = new QWidget();
    this->setCentralWidget(centralWidget);

    QVBoxLayout* vlayout = new QVBoxLayout(centralWidget);
    centralWidget->setLayout(vlayout);

    QHBoxLayout* m_theme_layout = new QHBoxLayout();

    QWidget* themeWidget = new QWidget(this);
    vlayout->addWidget(themeWidget, 0, Qt::AlignTop);
    themeWidget->setLayout(m_theme_layout);

    m_theme_combobox = new QComboBox();
    m_theme_combobox->addItems(Constants::UI::Themes::kThemes);
    m_theme_combobox->setCurrentText(m_preferences_manager->getThemeText());

    m_theme_layout->addWidget(new QLabel("Current Theme:"));
    m_theme_layout->addWidget(m_theme_combobox);

    QTabWidget* m_tab_widget = new QTabWidget(this);
    vlayout->addWidget(m_tab_widget, 1);

    QGridLayout* m_unit_tab_layout = new QGridLayout();

    QWidget* unitWidget = new QWidget(m_tab_widget);
    m_tab_widget->addTab(unitWidget, "Units");
    unitWidget->setLayout(m_unit_tab_layout);

    m_import_unit_combobox = new QComboBox();
    m_import_unit_combobox->addItems(Constants::Units::kDistanceUnits);
    m_import_unit_combobox->setCurrentText(m_preferences_manager->getImportUnit().toString());

    m_distance_unit_combobox = new QComboBox();
    m_distance_unit_combobox->addItems(Constants::Units::kDistanceUnits);
    m_distance_unit_combobox->setCurrentText(m_preferences_manager->getDistanceUnitText());

    m_velocity_unit_combobox = new QComboBox();
    m_velocity_unit_combobox->addItems(Constants::Units::kVelocityUnits);
    m_velocity_unit_combobox->setCurrentText(m_preferences_manager->getVelocityUnitText());

    m_acceleration_unit_combobox = new QComboBox();
    m_acceleration_unit_combobox->addItems(Constants::Units::kAccelerationUnits);
    m_acceleration_unit_combobox->setCurrentText(m_preferences_manager->getAccelerationUnitText());

    m_density_unit_combobox = new QComboBox();
    m_density_unit_combobox->addItems(Constants::Units::kDensityUnits);
    m_density_unit_combobox->setCurrentText(m_preferences_manager->getDensityUnitText());

    m_angle_unit_combobox = new QComboBox();
    m_angle_unit_combobox->addItems(Constants::Units::kAngleUnits);
    m_angle_unit_combobox->setCurrentText(m_preferences_manager->getAngleUnitText());

    m_time_unit_combobox = new QComboBox();
    m_time_unit_combobox->addItems(Constants::Units::kTimeUnits);
    m_time_unit_combobox->setCurrentText(m_preferences_manager->getTimeUnitText());

    m_temperature_unit_combobox = new QComboBox();
    m_temperature_unit_combobox->addItems(Constants::Units::kTemperatureUnits);
    m_temperature_unit_combobox->setCurrentText(m_preferences_manager->getTemperatureUnitText());

    m_voltage_unit_combobox = new QComboBox();
    m_voltage_unit_combobox->addItems(Constants::Units::kVoltageUnits);
    m_voltage_unit_combobox->setCurrentText(m_preferences_manager->getVoltageUnitText());

    m_mass_unit_combobox = new QComboBox();
    m_mass_unit_combobox->addItems(Constants::Units::kMassUnits);
    m_mass_unit_combobox->setCurrentText(m_preferences_manager->getMassUnitText());

    m_rotation_unit_combobox = new QComboBox();
    m_rotation_unit_combobox->addItems(Constants::Units::kRotationUnits);
    m_rotation_unit_combobox->setCurrentText(m_preferences_manager->getRotationUnitText());

    m_unit_tab_layout->addWidget(new QLabel("Imported Model Unit:"), 0, 0);
    m_unit_tab_layout->addWidget(m_import_unit_combobox, 0, 1);
    m_unit_tab_layout->addWidget(new QLabel("Distance Unit:"), 1, 0);
    m_unit_tab_layout->addWidget(m_distance_unit_combobox, 1, 1);
    m_unit_tab_layout->addWidget(new QLabel("Velocity Unit:"), 2, 0);
    m_unit_tab_layout->addWidget(m_velocity_unit_combobox, 2, 1);
    m_unit_tab_layout->addWidget(new QLabel("Acceleration Unit:"), 3, 0);
    m_unit_tab_layout->addWidget(m_acceleration_unit_combobox, 3, 1);
    m_unit_tab_layout->addWidget(new QLabel("Angle Unit:"), 4, 0);
    m_unit_tab_layout->addWidget(m_angle_unit_combobox, 4, 1);
    m_unit_tab_layout->addWidget(new QLabel("Time Unit:"), 5, 0);
    m_unit_tab_layout->addWidget(m_time_unit_combobox, 5, 1);
    m_unit_tab_layout->addWidget(new QLabel("Temperature Unit:"), 6, 0);
    m_unit_tab_layout->addWidget(m_temperature_unit_combobox, 6, 1);
    m_unit_tab_layout->addWidget(new QLabel("Voltage Unit:"), 7, 0);
    m_unit_tab_layout->addWidget(m_voltage_unit_combobox, 7, 1);
    m_unit_tab_layout->addWidget(new QLabel("Mass Unit:"), 8, 0);
    m_unit_tab_layout->addWidget(m_mass_unit_combobox, 8, 1);
    m_unit_tab_layout->addWidget(new QLabel("Rotation Unit:"), 9, 0);
    m_unit_tab_layout->addWidget(m_rotation_unit_combobox, 9, 1);
    m_unit_tab_layout->addWidget(new QLabel("Density Unit:"), 10, 0);
    m_unit_tab_layout->addWidget(m_density_unit_combobox, 10, 1);
    m_unit_tab_layout->addWidget(new QLabel("<i>Caution: Changing units may result in a small loss of accuracy.</i>"),
                                 11, 0, 1, 2);

    QWidget* notificationsWidget = new QWidget(m_tab_widget);
    m_tab_widget->addTab(notificationsWidget, "Notifications");

    QGridLayout* m_notifications_tab_layout = new QGridLayout();
    notificationsWidget->setLayout(m_notifications_tab_layout);

    void (PreferencesWindow::*funcPtr)(PreferenceChoice) = &PreferencesWindow::shiftProjectPreferenceChanged;

    QGroupBox* shiftBox =
        createContainer(m_preferences_manager->getProjectShiftPreference(),
                        QList<QString>({"Load Project - Part Shift", "Ask User", "Don't ask - shift automatically",
                                        "Don't ask - don't shift"}),
                        funcPtr);
    shiftBox->setToolTip("Controls whether parts will be shifted when loading a project");
    m_notifications_tab_layout->addWidget(shiftBox, 0, 0);
    m_boxes.push_back(shiftBox);

    funcPtr = &PreferencesWindow::alignPreferenceChanged;
    QGroupBox* alignBox =
        createContainer(m_preferences_manager->getAlignPreference(),
                        QList<QString>({"Align Part - Slice", "Ask User", "Don't ask - align automatically",
                                        "Don't ask - don't align"}),
                        funcPtr);
    alignBox->setToolTip("Controls whether parts will be aligned with print bed when slicing");
    m_notifications_tab_layout->addWidget(alignBox, 0, 1);
    m_boxes.push_back(alignBox);

    funcPtr = &PreferencesWindow::shiftFilePreferenceChanged;
    QGroupBox* fileshiftBox =
        createContainer(m_preferences_manager->getFileShiftPreference(),
                        QList<QString>({"Load Part - File Shift", "Ask User", "Don't ask - shift automatically",
                                        "Don't ask - don't shift"}),
                        funcPtr);
    fileshiftBox->setToolTip("Controls whether parts will automatically be shifted to avoid collisions on load");
    m_notifications_tab_layout->addWidget(fileshiftBox, 1, 0);
    m_boxes.push_back(fileshiftBox);

    // Camera tab
    QWidget* cameraWidget = new QWidget(m_tab_widget);
    m_tab_widget->addTab(cameraWidget, "Camera");

    QGridLayout* camera_tab_layout = new QGridLayout();
    cameraWidget->setLayout(camera_tab_layout);

    auto camera_checkbox = new QCheckBox();
    camera_tab_layout->addWidget(new QLabel("Invert Camera:"), 0, 0, Qt::AlignTop);
    camera_tab_layout->addWidget(camera_checkbox, 0, 1, Qt::AlignTop);
    camera_checkbox->setChecked(PreferencesManager::getInstance()->invertCamera());
    connect(camera_checkbox, &QCheckBox::clicked, PreferencesManager::getInstance().get(),
            &PreferencesManager::setInvertCamera);

    // Parts
    QWidget* part_tab_widget = new QWidget(m_tab_widget);
    m_tab_widget->addTab(part_tab_widget, "Parts");

    QGridLayout* parts_tab_layout = new QGridLayout();
    part_tab_widget->setLayout(parts_tab_layout);

    // Implicit transforms
    auto implicit_tranforms_checkbox = new QCheckBox();
    parts_tab_layout->addWidget(new QLabel("Use implicit transforms:"), 0, 0, Qt::AlignTop);
    parts_tab_layout->addWidget(implicit_tranforms_checkbox, 0, 1, Qt::AlignTop);
    implicit_tranforms_checkbox->setChecked(PreferencesManager::getInstance()->getUseImplicitTransforms());
    connect(implicit_tranforms_checkbox, &QCheckBox::clicked, PreferencesManager::getInstance().get(),
            &PreferencesManager::setUseImplicitTransforms);

    // Drop parts on bed
    auto auto_drop_parts_checkbox = new QCheckBox();
    parts_tab_layout->addWidget(new QLabel("Always drop parts to bed:"), 1, 0, Qt::AlignTop);
    parts_tab_layout->addWidget(auto_drop_parts_checkbox, 1, 1, Qt::AlignTop);
    auto_drop_parts_checkbox->setChecked(PreferencesManager::getInstance()->getAlwaysDropParts());
    connect(auto_drop_parts_checkbox, &QCheckBox::clicked, PreferencesManager::getInstance().get(),
            &PreferencesManager::setShouldAlwaysDrop);

    parts_tab_layout->setRowStretch(3, 1);

    // Visualization Colors tab
    QScrollArea* scrollArea = new QScrollArea(m_tab_widget);
    scrollArea->setWidgetResizable(true);
    m_tab_widget->addTab(scrollArea, "Visualization Colors");

    QWidget* colorWidget = new QWidget();
    scrollArea->setWidget(colorWidget);

    QGridLayout* color_tab_layout = new QGridLayout();
    color_tab_layout->setVerticalSpacing(0);
    colorWidget->setLayout(color_tab_layout);

    int i = 0;
    for (const auto& color : PreferencesManager::getInstance()->getVisualizationColors()) {
        color_tab_layout->addWidget(new VisualizationColorPicker(QString::fromStdString(color.first), color.second),
                                    i++, 0, 1, 2, Qt::AlignTop);
    }

    // ComWithApps tab
    QGridLayout* com_with_apps_tab_layout = new QGridLayout();
    QWidget* comWithAppsWidget = new QWidget(m_tab_widget);
    m_tab_widget->addTab(comWithAppsWidget, "Com With Apps");
    comWithAppsWidget->setLayout(com_with_apps_tab_layout);

    com_with_apps_tab_layout->addWidget(new QLabel("Katana Server "), 0, 0, Qt::AlignTop);

    com_with_apps_tab_layout->addWidget(new QLabel("Send Output:"), 1, 0, Qt::AlignTop);
    auto katana_checkbox = new QCheckBox();
    com_with_apps_tab_layout->addWidget(katana_checkbox, 1, 1, Qt::AlignTop);
    katana_checkbox->setChecked(PreferencesManager::getInstance()->getKatanaSendOutput());
    connect(katana_checkbox, &QCheckBox::clicked, PreferencesManager::getInstance().get(),
            &PreferencesManager::setKatanaSendOutput);

    com_with_apps_tab_layout->addWidget(new QLabel("TCP IP:"), 2, 0, Qt::AlignTop);
    auto tcp_ip = new QLineEdit();
    com_with_apps_tab_layout->addWidget(tcp_ip, 2, 1, Qt::AlignTop);
    tcp_ip->setText(PreferencesManager::getInstance()->getKatanaTCPIp());
    connect(tcp_ip, &QLineEdit ::textChanged, PreferencesManager::getInstance().get(),
            &PreferencesManager::setKatanaTCPIp);

    com_with_apps_tab_layout->addWidget(new QLabel("TCP Port:"), 3, 0, Qt::AlignTop);
    auto tcp_port_box = new QSpinBox();
    com_with_apps_tab_layout->addWidget(tcp_port_box, 3, 1, Qt::AlignTop);
    tcp_port_box->setMinimum(1);

    // Lag tab
    QWidget* lagWidget = new QWidget(m_tab_widget);
    m_tab_widget->addTab(lagWidget, "Lag");

    QGridLayout* lag_tab_layout = new QGridLayout();
    lagWidget->setLayout(lag_tab_layout);
    lag_tab_layout->addWidget(new QLabel("Lag between layers (ms):"), 0, 0, Qt::AlignTop);

    QSpinBox* layer_lag_box = new QSpinBox();
    lag_tab_layout->addWidget(layer_lag_box, 0, 1, Qt::AlignTop);
    layer_lag_box->setMinimum(1);
    layer_lag_box->setMaximum(5000);
    layer_lag_box->setValue(PreferencesManager::getInstance()->getLayerLag());
    connect(layer_lag_box, QOverload<int>::of(&QSpinBox::valueChanged), PreferencesManager::getInstance().get(),
            &PreferencesManager::setLayerLag);

    lag_tab_layout->addWidget(new QLabel("Lag between segments (ms):"), 0, 3, Qt::AlignTop);

    QSpinBox* segment_lag_box = new QSpinBox();
    lag_tab_layout->addWidget(segment_lag_box, 0, 4, Qt::AlignTop);
    segment_lag_box->setMinimum(1);
    segment_lag_box->setMaximum(5000);
    segment_lag_box->setValue(PreferencesManager::getInstance()->getSegmentLag());
    connect(segment_lag_box, QOverload<int>::of(&QSpinBox::valueChanged), PreferencesManager::getInstance().get(),
            &PreferencesManager::setSegmentLag);

    tcp_port_box->setMaximum(65535);
    tcp_port_box->setValue(PreferencesManager::getInstance()->getKatanaTCPPort());
    connect(tcp_port_box, QOverload<int>::of(&QSpinBox::valueChanged), PreferencesManager::getInstance().get(),
            &PreferencesManager::setKatanaTCPPort);

    com_with_apps_tab_layout->setRowStretch(5, 1);
    // End ComWithApps tab
}

QGroupBox* PreferencesWindow::createContainer(PreferenceChoice choice, QList<QString> displayStrings,
                                              void (PreferencesWindow::*func)(PreferenceChoice)) {
    QGroupBox* groupBox = new QGroupBox(displayStrings[0]);

    QRadioButton* m_ask_radio_button = new QRadioButton(displayStrings[1]);
    QRadioButton* m_do_automatically_radio_button = new QRadioButton(displayStrings[2]);
    QRadioButton* m_skip_automatically_radio_button = new QRadioButton(displayStrings[3]);

    if (choice == PreferenceChoice::kAsk)
        m_ask_radio_button->setChecked(true);
    else if (choice == PreferenceChoice::kPerformAutomatically)
        m_do_automatically_radio_button->setChecked(true);
    else if (choice == PreferenceChoice::kSkipAutomatically)
        m_skip_automatically_radio_button->setChecked(true);

    connect(m_ask_radio_button, &QRadioButton::clicked, this, [this, func] { (this->*func)(PreferenceChoice::kAsk); });
    connect(m_do_automatically_radio_button, &QRadioButton::clicked, this,
            [this, func] { (this->*func)(PreferenceChoice::kPerformAutomatically); });
    connect(m_skip_automatically_radio_button, &QRadioButton::clicked, this,
            [this, func] { (this->*func)(PreferenceChoice::kSkipAutomatically); });

    QVBoxLayout* vbox = new QVBoxLayout;
    vbox->addWidget(m_ask_radio_button);
    vbox->addWidget(m_do_automatically_radio_button);
    vbox->addWidget(m_skip_automatically_radio_button);
    vbox->addStretch(1);
    groupBox->setLayout(vbox);

    return groupBox;
}

void PreferencesWindow::setPreferenceValue(QGroupBox* box, PreferenceChoice choice) {
    QList<QRadioButton*> allRadioButtons = box->findChildren<QRadioButton*>();

    if (choice == PreferenceChoice::kAsk)
        allRadioButtons[0]->setChecked(true);
    else if (choice == PreferenceChoice::kPerformAutomatically)
        allRadioButtons[1]->setChecked(true);
    else if (choice == PreferenceChoice::kSkipAutomatically)
        allRadioButtons[2]->setChecked(true);
}

void PreferencesWindow::setupEvents() {
    connect(m_import_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setImportUnit));

    connect(m_distance_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setDistanceUnit));

    connect(m_velocity_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setVelocityUnit));

    connect(m_acceleration_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setAccelerationUnit));

    connect(m_density_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setDensityUnit));

    connect(m_angle_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setAngleUnit));

    connect(m_time_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setTimeUnit));

    connect(m_temperature_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setTemperatureUnit));

    connect(m_voltage_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setVoltageUnit));

    connect(m_mass_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setMassUnit));

    connect(m_rotation_unit_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setRotationUnit));

    connect(m_theme_combobox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            m_preferences_manager.data(), QOverload<QString>::of(&PreferencesManager::setTheme));

    connect(m_preferences_manager.get(), &PreferencesManager::importUnitChanged, this,
            &PreferencesWindow::updateImportUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::distanceUnitChanged, this,
            &PreferencesWindow::updateDistanceUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::velocityUnitChanged, this,
            &PreferencesWindow::updateVelocityUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::accelerationUnitChanged, this,
            &PreferencesWindow::updateAccelerationUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::densityUnitChanged, this,
            &PreferencesWindow::updateDensityUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::angleUnitChanged, this,
            &PreferencesWindow::updateAngleUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::timeUnitChanged, this,
            &PreferencesWindow::updateTimeUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::temperatureUnitChanged, this,
            &PreferencesWindow::updateTemperatureUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::voltageUnitChanged, this,
            &PreferencesWindow::updateVoltageUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::massUnitChanged, this,
            &PreferencesWindow::updateMassUnitVisual);
    connect(m_preferences_manager.get(), &PreferencesManager::themeChanged, this,
            &PreferencesWindow::updateThemeVisual);
}

void PreferencesWindow::closeEvent(QCloseEvent* event) {
    // if window closed and changes made, save them
    if (m_preferences_manager->isDirty())
        m_preferences_manager->exportPreferences();

    // m_window_manager->removePreferencesWindow();
    event->accept();
}

void PreferencesWindow::importPreferences() {
    QString filepath = QFileDialog::getOpenFileName(this, "Load .preferences file",
                                                    QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                    "*.preferences");
    if (!filepath.isNull()) {
        m_preferences_manager->importPreferences(filepath);
        m_distance_unit_combobox->setCurrentText(m_preferences_manager->getDistanceUnitText());
        m_velocity_unit_combobox->setCurrentText(m_preferences_manager->getVelocityUnitText());
        m_acceleration_unit_combobox->setCurrentText(m_preferences_manager->getAccelerationUnitText());
        m_density_unit_combobox->setCurrentText(m_preferences_manager->getDensityUnitText());
        m_angle_unit_combobox->setCurrentText(m_preferences_manager->getAngleUnitText());
        m_time_unit_combobox->setCurrentText(m_preferences_manager->getTimeUnitText());
        m_temperature_unit_combobox->setCurrentText(m_preferences_manager->getTemperatureUnitText());
        m_mass_unit_combobox->setCurrentText(m_preferences_manager->getMassUnitText());
        m_voltage_unit_combobox->setCurrentText(m_preferences_manager->getVoltageUnitText());
        m_rotation_unit_combobox->setCurrentText(m_preferences_manager->getRotationUnitText());

        setPreferenceValue(m_boxes[0], m_preferences_manager->getProjectShiftPreference());
        setPreferenceValue(m_boxes[1], m_preferences_manager->getAlignPreference());
        setPreferenceValue(m_boxes[2], m_preferences_manager->getFileShiftPreference());
    }
}

void PreferencesWindow::updateThemeVisual() {
    emit updateTheme();
    m_theme_combobox->blockSignals(true);
    m_theme_combobox->setCurrentText(m_preferences_manager->getThemeText());
    m_theme_combobox->blockSignals(false);
}

void PreferencesWindow::updateImportUnitVisual(Distance oldUnit, Distance newUnit) {
    m_import_unit_combobox->blockSignals(true);
    m_import_unit_combobox->setCurrentText(m_preferences_manager->getImportUnit().toString());
    m_import_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateDistanceUnitVisual(Distance oldUnit, Distance newUnit) {
    m_distance_unit_combobox->blockSignals(true);
    m_distance_unit_combobox->setCurrentText(m_preferences_manager->getDistanceUnitText());
    m_distance_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateVelocityUnitVisual(Velocity oldUnit, Velocity newUnit) {
    m_velocity_unit_combobox->blockSignals(true);
    m_velocity_unit_combobox->setCurrentText(m_preferences_manager->getVelocityUnitText());
    m_velocity_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateAccelerationUnitVisual(Acceleration oldUnit, Acceleration newUnit) {
    m_acceleration_unit_combobox->blockSignals(true);
    m_acceleration_unit_combobox->setCurrentText(m_preferences_manager->getAccelerationUnitText());
    m_acceleration_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateDensityUnitVisual(Density oldUnit, Density newUnit) {
    m_density_unit_combobox->blockSignals(true);
    m_density_unit_combobox->setCurrentText(m_preferences_manager->getDensityUnitText());
    m_density_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateAngleUnitVisual(Angle oldUnit, Angle newUnit) {
    m_angle_unit_combobox->blockSignals(true);
    m_angle_unit_combobox->setCurrentText(m_preferences_manager->getAngleUnitText());
    m_angle_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateTimeUnitVisual(Time oldUnit, Time newUnit) {
    m_time_unit_combobox->blockSignals(true);
    m_time_unit_combobox->setCurrentText(m_preferences_manager->getTimeUnitText());
    m_time_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateTemperatureUnitVisual(Temperature oldUnit, Temperature newUnit) {
    m_temperature_unit_combobox->blockSignals(true);
    m_temperature_unit_combobox->setCurrentText(m_preferences_manager->getTemperatureUnitText());
    m_temperature_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateVoltageUnitVisual(Voltage oldUnit, Voltage newUnit) {
    m_voltage_unit_combobox->blockSignals(true);
    m_voltage_unit_combobox->setCurrentText(m_preferences_manager->getVoltageUnitText());
    m_voltage_unit_combobox->blockSignals(false);
}

void PreferencesWindow::updateMassUnitVisual(Mass oldUnit, Mass newUnit) {
    m_mass_unit_combobox->blockSignals(true);
    m_mass_unit_combobox->setCurrentText(m_preferences_manager->getMassUnitText());
    m_mass_unit_combobox->blockSignals(false);
}

void PreferencesWindow::exportPreferences() {
    QString filepath = QFileDialog::getSaveFileName(this, "Save .preferences file",
                                                    QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                    "*.preferences");
    if (!filepath.isNull()) {
        m_preferences_manager->exportPreferences(filepath);
    }
}

void PreferencesWindow::shiftProjectPreferenceChanged(PreferenceChoice shift) {
    m_preferences_manager->setProjectShiftPreference(shift);
}

void PreferencesWindow::shiftFilePreferenceChanged(PreferenceChoice shift) {
    m_preferences_manager->setFileShiftPreference(shift);
}

void PreferencesWindow::alignPreferenceChanged(PreferenceChoice align) {
    m_preferences_manager->setAlignPreference(align);
}
} // namespace ORNL
