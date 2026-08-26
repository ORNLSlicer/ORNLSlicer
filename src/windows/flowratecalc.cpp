#include "windows/flowratecalc.h"

#include <math.h>

#include <QApplication>
#include <QSignalBlocker>

#include <qcombobox.h>
#include <qcontainerfwd.h>
#include <qfont.h>
#include <qframe.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qoverload.h>
#include <qsize.h>
#include <qstringliteral.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
void convertDistanceEntry(QLineEdit* entry, Distance old_unit, Distance new_unit) {
    if (entry->text().isEmpty()) { return; }

    bool text_ok          = false;
    const double raw_text = entry->text().toDouble(&text_ok);
    if (!text_ok) { return; }

    Distance value;
    value.from(raw_text, old_unit);

    const QSignalBlocker blocker(entry);
    entry->setText(QString::number(value.to(new_unit), 'g', 6));
}
}  // namespace

// TODO:
//  1. Provide unit choices for mass flow rate
FlowrateCalcWindow::FlowrateCalcWindow(QWidget* parent) : QWidget() {
    m_parent               = parent;
    m_density_metric       = g / (cm * cm * cm);
    Density IS_unit_chosen = lbm / (inch * inch * inch);
    m_density_metric_to_is = m_density_metric() / IS_unit_chosen();  // around 0.036127292

    // setFixedSize(420,260);
    setWindowTitle(QApplication::applicationDisplayName() + ": Flowrate Calculator");
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/ornlslicer_logo.png"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    m_layout            = new QGridLayout();
    QLabel* lblFlowrate = new QLabel("Flowrate Measurement");
    QFont titleFont("Arial", 10, QFont::Bold);
    lblFlowrate->setFont(titleFont);
    m_layout->addWidget(lblFlowrate, 0, 0, 1, 2);

    QLabel* lblPrintParams = new QLabel("Printing Parameters");
    lblPrintParams->setFont(titleFont);
    m_layout->addWidget(lblPrintParams, 0, 3, 1, 2);

    QLabel* lblRPM = new QLabel("Speed (RPM):");
    m_speed_rpm    = new QLineEdit();
    m_layout->addWidget(lblRPM, 1, 0);
    m_layout->addWidget(m_speed_rpm, 1, 1);

    QLabel* lblHour = new QLabel("Lbs/Hour:");
    m_lbs_hour      = new QLineEdit();
    m_layout->addWidget(lblHour, 2, 0);
    m_layout->addWidget(m_lbs_hour, 2, 1);

    // divider between two sections
    m_layout->setColumnMinimumWidth(2, 50);

    m_bead_width_label = new QLabel();
    m_bead_width       = new QLineEdit();
    m_layout->addWidget(m_bead_width_label, 1, 3);
    m_layout->addWidget(m_bead_width, 1, 4);

    m_layer_height_label = new QLabel();
    m_layer_height       = new QLineEdit();
    m_layout->addWidget(m_layer_height_label, 2, 3);
    m_layout->addWidget(m_layer_height, 2, 4);

    QLabel* lbl_printRate = new QLabel("Desired Print Rate (lbs/hr):");
    m_print_rate          = new QLineEdit();
    m_layout->addWidget(lbl_printRate, 3, 3);
    m_layout->addWidget(m_print_rate, 3, 4);

    QLabel* lbl_materialType = new QLabel("Material Type:");
    QStringList commands     = {"20% CF-ABS", "ABS",         "PPS", "50% CF-PPS", "PPSU", "25% CF-PPSU",
                                "PESU",       "25% CF-PESU", "PLA", "Concrete",   "Other"};
    m_material_type          = new QComboBox();
    m_material_type->addItems(commands);
    m_layout->addWidget(lbl_materialType, 4, 3);
    m_layout->addWidget(m_material_type, 4, 4);

    QLabel* lbl_density = new QLabel("Density (g/cm³):");
    m_density           = new QLineEdit();
    m_density->setReadOnly(true);
    m_density->setText(QString::number(toDensityValue(PrintMaterial::kABS20CF)() / m_density_metric(), 'f', 5));
    m_density->setEnabled(false);
    m_layout->addWidget(lbl_density, 5, 3);
    m_layout->addWidget(m_density, 5, 4);

    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_layout->addWidget(line, 6, 0, 1, 5);

    m_gantry_speed_label = new QLabel();
    m_gantry_speed       = new QLineEdit();
    m_gantry_speed->setReadOnly(true);
    m_layout->addWidget(m_gantry_speed_label, 7, 3);
    m_layout->addWidget(m_gantry_speed, 7, 4);

    QLabel* lbl_sprindleSpeed = new QLabel("Spindle Speed (RPM):");
    m_spindle_speed           = new QLineEdit();
    m_spindle_speed->setReadOnly(true);
    m_layout->addWidget(lbl_sprindleSpeed, 8, 3);
    m_layout->addWidget(m_spindle_speed, 8, 4);

    m_status_label = new QLabel("Please fill out all input fields");
    m_status_label->setWordWrap(true);
    m_status_label->setStyleSheet("QLabel { color : red; }");
    m_layout->addWidget(m_status_label, 7, 0, 2, 2);

    this->setLayout(m_layout);

    this->setupEvents();
    this->updateUnitLabels();
    m_saved_other_density = "";
    this->setFixedSize(this->sizeHint());
}

FlowrateCalcWindow::~FlowrateCalcWindow() {}

void FlowrateCalcWindow::setupEvents() {
    connect(m_speed_rpm, &QLineEdit::textChanged, this, &FlowrateCalcWindow::checkInputAndCalculate);
    connect(m_lbs_hour, &QLineEdit::textChanged, this, &FlowrateCalcWindow::checkInputAndCalculate);
    connect(m_bead_width, &QLineEdit::textChanged, this, &FlowrateCalcWindow::checkInputAndCalculate);
    connect(m_layer_height, &QLineEdit::textChanged, this, &FlowrateCalcWindow::checkInputAndCalculate);
    connect(m_print_rate, &QLineEdit::textChanged, this, &FlowrateCalcWindow::checkInputAndCalculate);

    connect(m_material_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FlowrateCalcWindow::enableDensity);
    connect(m_material_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FlowrateCalcWindow::checkInputAndCalculate);
    connect(m_density, &QLineEdit::textChanged, this, &FlowrateCalcWindow::saveOtherDensity);
    connect(m_density, &QLineEdit::textChanged, this, &FlowrateCalcWindow::checkInputAndCalculate);

    connect(PreferencesManager::getInstance().get(), &PreferencesManager::distanceUnitChanged, this,
            &FlowrateCalcWindow::distanceUnitsChanged);
    connect(PreferencesManager::getInstance().get(), &PreferencesManager::velocityUnitChanged, this,
            &FlowrateCalcWindow::velocityUnitsChanged);
}

void FlowrateCalcWindow::updateUnitLabels() {
    QSharedPointer<PreferencesManager> preferences = PreferencesManager::getInstance();

    m_bead_width_label->setText("Bead Width (" + preferences->getDistanceUnitText() + "):");
    m_layer_height_label->setText("Layer Height (" + preferences->getDistanceUnitText() + "):");
    m_gantry_speed_label->setText("Gantry Speed (" + preferences->getVelocityUnitText() + "):");

    setFixedSize(sizeHint());
}

void FlowrateCalcWindow::distanceUnitsChanged(Distance new_value, Distance old_value) {
    if (new_value == old_value) { return; }

    convertDistanceEntry(m_bead_width, old_value, new_value);
    convertDistanceEntry(m_layer_height, old_value, new_value);
    updateUnitLabels();
    checkInputAndCalculate();
}

void FlowrateCalcWindow::velocityUnitsChanged(Velocity new_value, Velocity old_value) {
    if (new_value == old_value) { return; }

    updateUnitLabels();
    checkInputAndCalculate();
}

void FlowrateCalcWindow::enableDensity(int index) {
    PrintMaterial material = static_cast<PrintMaterial>(index);
    if (material == PrintMaterial::kOther) {
        m_density->setText(m_saved_other_density);
        m_density->setEnabled(true);
        m_density->setReadOnly(false);
    }
    else {
        m_density->setText(QString::number(toDensityValue(material)() / m_density_metric()));
        m_density->setEnabled(false);
        m_density->setReadOnly(true);
    }
}

void FlowrateCalcWindow::saveOtherDensity() {
    // if enabled, Other must be selected
    if (m_material_type->isEnabled()) { m_saved_other_density = m_density->text(); }
}

void FlowrateCalcWindow::checkInputAndCalculate() {
    m_status_label->setText("Please fill out all input fields");

    if (m_density->text().isEmpty() || m_speed_rpm->text().isEmpty() || m_lbs_hour->text().isEmpty() ||
        m_bead_width->text().isEmpty() || m_layer_height->text().isEmpty() || m_print_rate->text().isEmpty()) {
        return;
    }

    // wipe out all existing data to avoid confusion
    m_gantry_speed->clear();
    m_spindle_speed->clear();

    // check each user input field
    bool textOkay;

    double v_speedRPM;
    double v_lbsHour;
    double v_beadWidthInput;
    double v_layerHeightInput;
    double v_printRate;
    double v_density;

    v_density = m_density->text().toDouble(&textOkay);
    if (!textOkay && !m_density->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid density");
        return;
    }

    v_speedRPM = m_speed_rpm->text().toDouble(&textOkay);
    if (!textOkay && !m_speed_rpm->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid RPM");
        return;
    }

    v_lbsHour = m_lbs_hour->text().toDouble(&textOkay);
    if (!textOkay && !m_lbs_hour->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid Pounds Per Hour");
        return;
    }

    v_beadWidthInput = m_bead_width->text().toDouble(&textOkay);
    if (!textOkay && !m_bead_width->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid bead width");
        return;
    }
    v_layerHeightInput = m_layer_height->text().toDouble(&textOkay);
    if (!textOkay && !m_layer_height->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid layer height");
        return;
    }

    v_printRate = m_print_rate->text().toDouble(&textOkay);
    if (!textOkay && !m_print_rate->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid print rate");
        return;
    }

    m_status_label->clear();

    Distance bead_width;
    bead_width.from(v_beadWidthInput, PreferencesManager::getInstance()->getDistanceUnit());
    Distance layer_height;
    layer_height.from(v_layerHeightInput, PreferencesManager::getInstance()->getDistanceUnit());
    v_density = v_density * m_density_metric_to_is;

    // calculations
    double ratio             = v_printRate / v_lbsHour;
    double volumeSec         = (v_printRate / 3600) / v_density;
    double beadWidthInches   = bead_width.to(in);
    double layerHeightInches = layer_height.to(in);
    double area              = layerHeightInches * (beadWidthInches - layerHeightInches) +
                               M_PI * (layerHeightInches / 2) * (layerHeightInches / 2);
    Velocity gantrySpeed     = (volumeSec / area) * in / s;
    double spindleSpeed      = ratio * v_speedRPM;

    // write to textboxes
    m_gantry_speed->setText(
        QString::number(gantrySpeed.to(PreferencesManager::getInstance()->getVelocityUnit()), 'f', 4));
    m_spindle_speed->setText(QString::number(spindleSpeed, 'f', 0));

    // Warning message in the same window - thus no need for an additional click
    m_status_label->clear();
    if (spindleSpeed > 400) { m_status_label->setText("Warning: Spindle Speed Exceeds 400 RPM"); }
}

void FlowrateCalcWindow::closeEvent(QCloseEvent* event) {
    m_status_label->setText("Please fill out all input fields");
    m_gantry_speed->clear();
    m_spindle_speed->clear();
    m_material_type->setCurrentIndex(0);

    m_speed_rpm->clear();
    m_lbs_hour->clear();

    m_bead_width->clear();
    m_layer_height->clear();

    m_print_rate->clear();

    // set the focus back to the main window
    if (m_parent->isMinimized()) { m_parent->showNormal(); }
    // setFocus() doesn't deliver, but activateWindow() serves the purpose
    m_parent->activateWindow();
}
}  // namespace ORNL
