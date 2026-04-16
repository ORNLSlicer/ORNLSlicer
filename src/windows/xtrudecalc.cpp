#include "windows/xtrudecalc.h"

#include "QApplication"
#include "managers/preferences_manager.h"
#include "utilities/enums.h"

namespace ORNL {

XtrudeCalcWindow::XtrudeCalcWindow(QWidget* parent) : QWidget() {
    m_parent = parent;

    // Combines units based on user's set preferences
    m_dens_pref = PreferencesManager::getInstance()->getMassUnit() /
                  (pow<3>(PreferencesManager::getInstance()->getDistanceUnit()));
    m_time_pref = PreferencesManager::getInstance()->getTimeUnit();
    m_dist_pref = PreferencesManager::getInstance()->getDistanceUnit();
    m_mass_pref = PreferencesManager::getInstance()->getMassUnit();
    m_velo_pref = PreferencesManager::getInstance()->getVelocityUnit();

    m_time_text = "(" + PreferencesManager::getInstance()->getTimeUnitText() + "):";
    m_dist_text = "(" + PreferencesManager::getInstance()->getDistanceUnitText() + "):";
    m_mass_text = "(" + PreferencesManager::getInstance()->getMassUnitText() + "):";
    m_density_text = "(" + PreferencesManager::getInstance()->getMassUnitText() + "/" +
                     PreferencesManager::getInstance()->getDistanceUnitText() + "³):";
    m_spindle_text = "(rev/" + PreferencesManager::getInstance()->getTimeUnitText() + "):";
    m_feed_text = "(" + PreferencesManager::getInstance()->getVelocityUnitText() + "):";
    m_fpr_text = "(" + PreferencesManager::getInstance()->getDistanceUnitText() + "/rev):";

    // setFixedSize(420,260);
    setWindowTitle(QApplication::applicationDisplayName() + ": Xtrude Calculator");
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/slicer-2_logo.ico"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    // Printing Parameter Section
    m_layout = new QGridLayout();
    QLabel* lbl_PrintParams = new QLabel("Printing Parameters");
    QFont titleFont("Arial", 10, QFont::Bold);
    lbl_PrintParams->setFont(titleFont);
    m_layout->addWidget(lbl_PrintParams, 0, 0, 1, 2);

    QLabel* lbl_materialType = new QLabel("Material Type:");
    QStringList commands = {"20% CF-ABS", "ABS",         "PPS", "50% CF-PPS", "PPSU", "25% CF-PPSU",
                            "PESU",       "25% CF-PESU", "PLA", "Concrete",   "Other"};
    m_material_type = new QComboBox();
    m_material_type->addItems(commands);
    m_layout->addWidget(lbl_materialType, 1, 0);
    m_layout->addWidget(m_material_type, 1, 1);

    m_density_label = new QLabel("Density " + m_density_text);
    m_density = new QLineEdit();
    m_density->setReadOnly(true);
    m_density->setText(QString::number(toDensityValue(PrintMaterial::kABS20CF)() / m_dens_pref(), 'g', 5));
    m_density->setEnabled(false);
    m_layout->addWidget(m_density_label, 2, 0);
    m_layout->addWidget(m_density, 2, 1);

    m_spindle_speed_label = new QLabel("Test Spindle Speed " + m_spindle_text);
    m_spindle_speed = new QLineEdit();
    m_layout->addWidget(m_spindle_speed_label, 3, 0);
    m_layout->addWidget(m_spindle_speed, 3, 1);

    m_2mtm_label = new QLabel("2 Minute Test Mass " + m_mass_text);
    m_2mtm = new QLineEdit();
    m_layout->addWidget(m_2mtm_label, 4, 0);
    m_layout->addWidget(m_2mtm, 4, 1);

    m_min_layer_time_label = new QLabel("Minimum Layer Time " + m_time_text);
    m_min_layer_time = new QLineEdit();
    m_layout->addWidget(m_min_layer_time_label, 5, 0);
    m_layout->addWidget(m_min_layer_time, 5, 1);

    m_layer_height_label = new QLabel("Layer Height " + m_dist_text);
    m_layer_height = new QLineEdit();
    m_layout->addWidget(m_layer_height_label, 6, 0);
    m_layout->addWidget(m_layer_height, 6, 1);

    m_bead_width_label = new QLabel("Bead Width " + m_dist_text);
    m_bead_width = new QLineEdit();
    m_layout->addWidget(m_bead_width_label, 7, 0);
    m_layout->addWidget(m_bead_width, 7, 1);

    m_fpr_label = new QLabel("FPR " + m_fpr_text);
    m_fpr = new QLineEdit();
    m_layout->addWidget(m_fpr_label, 9, 0);
    m_layout->addWidget(m_fpr, 9, 1);
    m_fpr->setEnabled(false);

    // Option 1 Section
    QLabel* lbl_Option1 = new QLabel("Option 1: Minimum Layer Time Driven");
    lbl_Option1->setFont(titleFont);
    m_layout->addWidget(lbl_Option1, 0, 3, 1, 2);

    m_toolpath_length_label = new QLabel("Toolpath Length " + m_dist_text);
    m_toolpath_length = new QLineEdit();
    m_layout->addWidget(m_toolpath_length_label, 1, 3);
    m_layout->addWidget(m_toolpath_length, 1, 4);

    m_o1_feed_rate_label = new QLabel("Feed Rate " + m_feed_text);
    m_o1_feed_rate = new QLineEdit();
    m_o1_feed_rate->setReadOnly(true);
    m_layout->addWidget(m_o1_feed_rate_label, 2, 3);
    m_layout->addWidget(m_o1_feed_rate, 2, 4);
    m_o1_feed_rate->setEnabled(false);

    m_o1_spindle_speed_label = new QLabel("Spindle Speed " + m_spindle_text);
    m_o1_spindle_speed = new QLineEdit();
    ;
    m_o1_spindle_speed->setReadOnly(true);
    m_layout->addWidget(m_o1_spindle_speed_label, 3, 3);
    m_layout->addWidget(m_o1_spindle_speed, 3, 4);
    m_o1_spindle_speed->setEnabled(false);

    // Option 2 Section
    QLabel* lbl_Option2 = new QLabel("Option 2: Screw Speed Driven");
    lbl_Option2->setFont(titleFont);
    m_layout->addWidget(lbl_Option2, 4, 3, 1, 2);

    m_d_spindle_speed_label = new QLabel("Desired Spindle Speed " + m_spindle_text);
    m_d_spindle_speed = new QLineEdit();
    m_layout->addWidget(m_d_spindle_speed_label, 5, 3);
    m_layout->addWidget(m_d_spindle_speed, 5, 4);

    m_o2_feed_rate_label = new QLabel("Feed Rate " + m_feed_text);
    m_o2_feed_rate = new QLineEdit();
    m_o2_feed_rate->setReadOnly(true);
    m_layout->addWidget(m_o2_feed_rate_label, 6, 3);
    m_layout->addWidget(m_o2_feed_rate, 6, 4);
    m_o2_feed_rate->setEnabled(false);

    // Option 3 Section
    QLabel* lbl_Option3 = new QLabel("Option 3: Feed Rate Driven");
    lbl_Option3->setFont(titleFont);
    m_layout->addWidget(lbl_Option3, 7, 3, 1, 2);

    m_d_feed_rate_label = new QLabel("Desired Feed Rate " + m_feed_text);
    m_d_feed_rate = new QLineEdit();
    m_layout->addWidget(m_d_feed_rate_label, 8, 3);
    m_layout->addWidget(m_d_feed_rate, 8, 4);

    m_o3_spindle_speed_label = new QLabel("Spindle Speed " + m_spindle_text);
    m_o3_spindle_speed = new QLineEdit();
    m_o3_spindle_speed->setReadOnly(true);
    m_layout->addWidget(m_o3_spindle_speed_label, 9, 3);
    m_layout->addWidget(m_o3_spindle_speed, 9, 4);
    m_o3_spindle_speed->setEnabled(false);

    QLabel* lbl_Option4 = new QLabel("(You can also use the FPR (i.e. feed per tooth with 1 tooth or simply feed per "
                                     "rev) as your feed in Option 3)");
    lbl_Option4->setWordWrap(true);
    lbl_Option4->setStyleSheet("QLabel { color : blue; }");
    m_layout->addWidget(lbl_Option4, 10, 3, 2, 2);

    // Status Message
    m_status_label = new QLabel("Please fill out all Printing Parameters before filling out any options");
    m_status_label->setWordWrap(true);
    m_status_label->setStyleSheet("QLabel { color : red; }");
    m_layout->addWidget(m_status_label, 10, 0, 2, 2);

    // General Usage Instructions
    QLabel* genDirections = new QLabel("General Directions");
    genDirections->setFont(titleFont);
    m_layout->addWidget(genDirections, 13, 3);

    m_directions =
        new QLabel("Run a 2 minute test with a set spindle speed onto a scale. Input the results for the spindle "
                   "speed, mass, density, and minimum layer time "
                   "under Printing Parameters along with a desired bead width and layer height. Once all the data is "
                   "entered, fill out an option to recieve a spindle speed and/or "
                   "feed rate based on calculations from your entered data. Note that all units are based off user "
                   "preferences and will change if preferences are modified whether "
                   "the Xtrude calculator is opened or closed, and any entered data will become less precise.");
    m_directions->setWordWrap(true);
    m_directions->setAlignment(Qt::AlignTop);
    m_layout->addWidget(m_directions, 14, 3, 5, 2);

    // Layout Embellishments
    QLabel* hybrid_icon = new QLabel();
    QPixmap pixmapIcon(":/icons/hybrid_logo.png");
    hybrid_icon->setPixmap(pixmapIcon.scaled(259, 123));
    m_layout->addWidget(hybrid_icon, 13, 0, 6, 2);

    QFrame* line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    m_layout->addWidget(line1, 8, 0, 1, 2);

    QFrame* line2 = new QFrame();
    line2->setFrameShape(QFrame::VLine);
    line2->setFrameShadow(QFrame::Sunken);
    m_layout->addWidget(line2, 0, 2, 12, 1);

    QFrame* line3 = new QFrame();
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    m_layout->addWidget(line3, 12, 0, 1, 6);

    this->setLayout(m_layout);
    this->setupEvents();
    this->setFixedSize(this->sizeHint());
}

XtrudeCalcWindow::~XtrudeCalcWindow() {}

void XtrudeCalcWindow::setupEvents() {
    connect(m_spindle_speed, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_toolpath_length, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_d_feed_rate, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_d_spindle_speed, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_2mtm, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_min_layer_time, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_bead_width, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_layer_height, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);

    connect(m_material_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &XtrudeCalcWindow::enableDensity);
    connect(m_material_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &XtrudeCalcWindow::checkInputAndCalculate);
    connect(m_density, &QLineEdit::textChanged, this, &XtrudeCalcWindow::checkInputAndCalculate);

    connect(PreferencesManager::getInstance().get(), &PreferencesManager::anyUnitChanged, this,
            &XtrudeCalcWindow::checkUnitPref);
    connect(PreferencesManager::getInstance().get(), &PreferencesManager::anyUnitChanged, this,
            &XtrudeCalcWindow::checkInputAndCalculate);
}

void XtrudeCalcWindow::checkUnitPref() {
    double check;
    bool textOkay;

    // Uses the old preference for units
    m_dens_conv = m_dens_pref() / (PreferencesManager::getInstance()->getMassUnit() /
                                   (pow<3>(PreferencesManager::getInstance()->getDistanceUnit())))();
    m_time_conv = m_time_pref() / PreferencesManager::getInstance()->getTimeUnit()();
    m_dist_conv = m_dist_pref() / PreferencesManager::getInstance()->getDistanceUnit()();
    m_mass_conv = m_mass_pref() / PreferencesManager::getInstance()->getMassUnit()();
    m_velo_conv = m_velo_pref() / PreferencesManager::getInstance()->getVelocityUnit()();

    check = m_spindle_speed->text().toDouble(&textOkay);
    if (textOkay && !m_spindle_speed->text().isEmpty()) {
        m_spindle_speed->setText(QString::number(check / m_time_conv, 'g', 5));
    }

    check = m_2mtm->text().toDouble(&textOkay);
    if (textOkay && !m_2mtm->text().isEmpty()) {
        m_2mtm->setText(QString::number(check * m_mass_conv, 'g', 5));
    }

    check = m_layer_height->text().toDouble(&textOkay);
    if (textOkay && !m_layer_height->text().isEmpty()) {
        m_layer_height->setText(QString::number(check * m_dist_conv, 'g', 5));
    }

    check = m_bead_width->text().toDouble(&textOkay);
    if (textOkay && !m_bead_width->text().isEmpty()) {
        m_bead_width->setText(QString::number(check * m_dist_conv, 'g', 5));
    }

    check = m_min_layer_time->text().toDouble(&textOkay);
    if (textOkay && !m_min_layer_time->text().isEmpty()) {
        m_min_layer_time->setText(QString::number(check * m_time_conv, 'g', 5));
    }

    check = m_toolpath_length->text().toDouble(&textOkay);
    if (textOkay && !m_min_layer_time->text().isEmpty()) {
        m_toolpath_length->setText(QString::number(check * m_dist_conv, 'g', 5));
    }

    check = m_d_spindle_speed->text().toDouble(&textOkay);
    if (textOkay && !m_d_spindle_speed->text().isEmpty()) {
        m_d_spindle_speed->setText(QString::number(check / m_time_conv, 'g', 5));
    }

    check = m_d_feed_rate->text().toDouble(&textOkay);
    if (textOkay && !m_d_feed_rate->text().isEmpty()) {
        m_d_feed_rate->setText(QString::number(check * m_velo_conv, 'g', 5));
    }

    check = m_density->text().toDouble(&textOkay);
    if (static_cast<PrintMaterial>(m_material_type->currentIndex()) == PrintMaterial::kOther && textOkay &&
        !m_density->text().isEmpty()) {
        m_density->setText(QString::number(check * m_dens_conv, 'g', 5));
    }

    // Fetches the new preference for units
    m_dens_pref = PreferencesManager::getInstance()->getMassUnit() /
                  (pow<3>(PreferencesManager::getInstance()->getDistanceUnit()));
    m_time_pref = PreferencesManager::getInstance()->getTimeUnit();
    m_dist_pref = PreferencesManager::getInstance()->getDistanceUnit();
    m_mass_pref = PreferencesManager::getInstance()->getMassUnit();
    m_velo_pref = PreferencesManager::getInstance()->getVelocityUnit();

    // Rebuilds strings using new unit preferences
    m_time_text = "(" + PreferencesManager::getInstance()->getTimeUnitText() + "):";
    m_dist_text = "(" + PreferencesManager::getInstance()->getDistanceUnitText() + "):";
    m_mass_text = "(" + PreferencesManager::getInstance()->getMassUnitText() + "):";
    m_density_text = "(" + PreferencesManager::getInstance()->getMassUnitText() + "/" +
                     PreferencesManager::getInstance()->getDistanceUnitText() + "³):";
    m_spindle_text = "(rev/" + PreferencesManager::getInstance()->getTimeUnitText() + "):";
    m_feed_text = "(" + PreferencesManager::getInstance()->getVelocityUnitText() + "):";
    m_fpr_text = "(" + PreferencesManager::getInstance()->getDistanceUnitText() + "/rev):";

    // Rebuilds labels using strings
    m_density_label->setText("Density " + m_density_text);
    m_spindle_speed_label->setText("Test Spindle Speed " + m_spindle_text);
    m_2mtm_label->setText("2 Minute Test Mass " + m_mass_text);
    m_min_layer_time_label->setText("Minimum Layer Time " + m_time_text);
    m_layer_height_label->setText("Layer Height " + m_dist_text);
    m_bead_width_label->setText("Bead Width " + m_dist_text);
    m_fpr_label->setText("FPR " + m_fpr_text);
    m_toolpath_length_label->setText("Toolpath Length " + m_dist_text);
    m_o1_feed_rate_label->setText("Feed Rate " + m_feed_text);
    m_o1_spindle_speed_label->setText("Spindle Speed " + m_spindle_text);
    m_d_spindle_speed_label->setText("Desired Spindle Speed " + m_spindle_text);
    m_o2_feed_rate_label->setText("Feed Rate " + m_feed_text);
    m_d_feed_rate_label->setText("Desired Feed Rate " + m_feed_text);
    m_o3_spindle_speed_label->setText("Spindle Speed " + m_spindle_text);

    // Changes the preset density to the new unit
    if (static_cast<PrintMaterial>(m_material_type->currentIndex()) != PrintMaterial::kOther) {
        m_density->setText(QString::number(
            toDensityValue(static_cast<PrintMaterial>(m_material_type->currentIndex()))() / m_dens_pref()));
    }
}

void XtrudeCalcWindow::enableDensity(int index) {
    PrintMaterial material = static_cast<PrintMaterial>(index);
    if (material == PrintMaterial::kOther) {
        m_density->clear();
        m_density->setEnabled(true);
        m_density->setReadOnly(false);
    }
    else {
        m_density->setText(QString::number(toDensityValue(material)() / m_dens_pref()));
        m_density->setEnabled(false);
        m_density->setReadOnly(true);
    }
}

void XtrudeCalcWindow::checkInputAndCalculate() {
    m_status_label->setText("Please fill out all Printing Parameters before filling out any options");

    // Wipe out all existing data to avoid confusion
    m_o1_feed_rate->clear();
    m_o1_spindle_speed->clear();
    m_o2_feed_rate->clear();
    m_o3_spindle_speed->clear();
    m_fpr->clear();

    if (m_density->text().isEmpty() || m_spindle_speed->text().isEmpty() || m_2mtm->text().isEmpty() ||
        m_bead_width->text().isEmpty() || m_layer_height->text().isEmpty() || m_min_layer_time->text().isEmpty()) {
        return;
    }

    // check each user input field
    bool textOkay;
    bool toolpathok = true;
    bool feedok = true;
    bool spindok = true;

    double v_beadWidth;
    double v_layerHeight;
    double v_density;
    double v_spindleSpeed;
    double v_toolpathLength;
    double v_dFeedRate;
    double v_dSpindleSpeed;
    double v_2mtm;
    double v_minLayerTime;
    double o1feedRate;
    double o1spindleSpeed;
    double o2feedRate;
    double o3spindleSpeed;

    v_density = m_density->text().toDouble(&textOkay);
    if (!textOkay && !m_density->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid density");
        return;
    }

    v_spindleSpeed = m_spindle_speed->text().toDouble(&textOkay);
    if (!textOkay && !m_spindle_speed->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid test spindle speed");
        return;
    }

    v_2mtm = m_2mtm->text().toDouble(&textOkay);
    if (!textOkay && !m_2mtm->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid 2 minute test mass");
        return;
    }

    v_layerHeight = m_layer_height->text().toDouble(&textOkay);
    if (!textOkay && !m_layer_height->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid layer height");
        return;
    }

    v_beadWidth = m_bead_width->text().toDouble(&textOkay);
    if (!textOkay && !m_bead_width->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid bead width");
        return;
    }

    v_minLayerTime = m_min_layer_time->text().toDouble(&textOkay);
    if (!textOkay && !m_min_layer_time->text().isEmpty()) {
        m_status_label->setText("Please check that you've specified a valid minimum layer time");
        return;
    }

    // FPR Calculations
    double timeMod = PreferencesManager::getInstance()->getTimeUnit()() / minute();
    double disttimevelo =
        PreferencesManager::getInstance()->getDistanceUnit()() / PreferencesManager::getInstance()->getTimeUnit()();
    double veloMod = PreferencesManager::getInstance()->getVelocityUnit()() / disttimevelo;
    double massCalc = v_2mtm * timeMod / 2;
    double extrusionCoefficient = massCalc / v_spindleSpeed;
    double FPR = extrusionCoefficient / (v_density * v_layerHeight * v_beadWidth);
    m_fpr->setText(QString::number(FPR, 'g', 5));

    // Option 1 check
    v_toolpathLength = m_toolpath_length->text().toDouble(&textOkay);
    if (!textOkay && !m_toolpath_length->text().isEmpty()) {
        toolpathok = false;
    }

    else if (!m_toolpath_length->text().isEmpty()) {
        o1feedRate = v_toolpathLength / (v_minLayerTime);
        o1spindleSpeed = o1feedRate / FPR;
        m_o1_feed_rate->setText(QString::number(o1feedRate / veloMod, 'g', 5));
        m_o1_spindle_speed->setText(QString::number(o1spindleSpeed, 'g', 5));
    }

    // Option 2 check
    v_dSpindleSpeed = m_d_spindle_speed->text().toDouble(&textOkay);
    if (!textOkay && !m_d_spindle_speed->text().isEmpty()) {
        spindok = false;
    }

    else if (!m_d_spindle_speed->text().isEmpty()) {
        o2feedRate = v_dSpindleSpeed * FPR;
        m_o2_feed_rate->setText(QString::number(o2feedRate / veloMod, 'g', 5));
    }

    // Option 3 check
    v_dFeedRate = m_d_feed_rate->text().toDouble(&textOkay);
    if (!textOkay && !m_d_feed_rate->text().isEmpty()) {
        feedok = false;
    }

    else if (!m_d_feed_rate->text().isEmpty()) {
        o3spindleSpeed = v_dFeedRate / FPR;
        m_o3_spindle_speed->setText(QString::number(o3spindleSpeed * veloMod, 'g', 5));
    }

    if (!toolpathok) {
        m_status_label->setText("Please check that you've specified a valid toolpath length");
        return;
    }

    if (!spindok) {
        m_status_label->setText("Please check that you've specified a valid desired spindle speed");
        return;
    }

    if (!feedok) {
        m_status_label->setText("Please check that you've specified a valid desired feed rate");
        return;
    }

    m_status_label->clear();
}

void XtrudeCalcWindow::closeEvent(QCloseEvent* event) {
    m_status_label->setText("Please fill out all Printing Parameters before filling out any options");
    m_spindle_speed->clear();
    m_min_layer_time->clear();
    m_toolpath_length->clear();
    m_d_feed_rate->clear();
    m_d_spindle_speed->clear();
    m_o1_feed_rate->clear();
    m_o1_spindle_speed->clear();
    m_o2_feed_rate->clear();
    m_o3_spindle_speed->clear();
    m_2mtm->clear();
    m_fpr->clear();
    m_bead_width->clear();
    m_layer_height->clear();
    m_material_type->setCurrentIndex(0);

    // set the focus back to the main window
    if (m_parent->isMinimized()) {
        m_parent->showNormal();
    }
    // setFocus() doesn't deliver, but activateWindow() serves the purpose
    m_parent->activateWindow();
}
} // namespace ORNL
