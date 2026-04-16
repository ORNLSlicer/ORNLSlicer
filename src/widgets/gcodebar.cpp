#include "widgets/gcodebar.h"

#include <QDir>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextDocumentWriter>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qframe.h>
#include <qgridlayout.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qicon.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qoverload.h>
#include <qplaintextedit.h>
#include <qset.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qtextformat.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "utilities/enums.h"
#include "widgets/gcodetextboxwidget.h"

namespace ORNL {
GcodeBar::GcodeBar(QWidget* parent) : QWidget(parent) {
    this->setupWidget();
    m_lock_distance = 0;
}

void GcodeBar::updateGcodeText(QString text, QHash<QString, QTextCharFormat> fontColors,
                               QList<int> layerFirstLineNumbers, QSet<int> layerSkipLineNumbers) {
    // new gcode so reset any previous highlight, forward along the font colors first, then add text
    // it is necessary to forward font colors first as the highlighter overrides font colors as text is added
    // so it must know which colors to set before adding text
    m_view->resetHighlight();
    m_view->setHighlighterColors(fontColors, layerSkipLineNumbers);
    m_view->setPlainText(text);
    m_layer_first_line_numbers = layerFirstLineNumbers;
    m_view->setLayerFirstLineNumbers(m_layer_first_line_numbers);
    m_refresh_btn->setEnabled(false);

    // preserve the last search. No harm if search string is empty
    m_search_count = 0;
    search();
}

void GcodeBar::clear() {
    m_view->resetHighlight();
    m_view->setPlainText("");
    m_refresh_btn->setEnabled(false);

    updateLowerSpin(0);
    updateLowerSlider(0);
    updateUpperSpin(0);
    updateUpperSlider(0);

    m_layer_upper->setMaximum(0);
    m_layer_lower->setMaximum(0);
}

void GcodeBar::updateLowerSpin(int new_value) {
    if (m_lock_layer->isChecked()) {
        if (m_layer_lower->value() > m_layer_lower->maximum() - m_lock_distance) {
            new_value = m_layer_lower->maximum() - m_lock_distance;
            m_layer_lower->blockSignals(true);
            m_layer_lower->setValue(new_value);
            m_layer_lower->blockSignals(false);
        }
    }
    m_layer_lower_slider->blockSignals(true);
    m_layer_lower_slider->setValue(new_value);
    m_layer_lower_slider->blockSignals(false);
    forwardLowerLayerUpdate(new_value);
}

void GcodeBar::updateLowerSlider(int new_value) {
    if (m_lock_layer->isChecked()) {
        if (m_layer_lower_slider->value() > m_layer_lower_slider->maximum() - m_lock_distance) {
            new_value = m_layer_lower_slider->maximum() - m_lock_distance;
            m_layer_lower_slider->blockSignals(true);
            m_layer_lower_slider->setValue(new_value);
            m_layer_lower_slider->blockSignals(false);
        }
    }
    m_layer_lower->blockSignals(true);
    m_layer_lower->setValue(new_value);
    m_layer_lower->blockSignals(false);
    forwardLowerLayerUpdate(new_value);
}

void GcodeBar::forwardLowerLayerUpdate(int new_value) {
    if (m_lock_layer->isChecked())
        m_layer_upper->setValue(new_value + m_lock_distance);
    // Before we forward, we should check if the new upper layer value is less than the lower layer,
    // and if so, set the lower layer to the upper layer.
    else if (new_value > m_layer_upper->value()) {
        // Note that this will cause the slot GcodeBar::forwardUpperLayerUpdate to be invoked
        m_layer_upper->setValue(new_value);
    }
    emit lowerLayerUpdated(new_value);
}

void GcodeBar::updateUpperSpin(int new_value) {
    if (m_lock_layer->isChecked()) {
        if (m_layer_upper->value() < m_layer_upper->minimum() + m_lock_distance) {
            new_value = m_layer_upper->minimum() + m_lock_distance;
            m_layer_upper->blockSignals(true);
            m_layer_upper->setValue(m_layer_upper->minimum() + m_lock_distance);
            m_layer_upper->blockSignals(false);
        }
    }
    m_layer_upper_slider->blockSignals(true);
    m_layer_upper_slider->setValue(new_value);
    m_layer_upper_slider->blockSignals(false);
    forwardUpperLayerUpdate(new_value);
}

void GcodeBar::updateUpperSlider(int new_value) {
    if (m_lock_layer->isChecked()) {
        if (m_layer_upper_slider->value() < m_layer_upper_slider->minimum() + m_lock_distance) {
            new_value = m_layer_upper_slider->minimum() + m_lock_distance;
            m_layer_upper_slider->blockSignals(true);
            m_layer_upper_slider->setValue(m_layer_upper_slider->minimum() + m_lock_distance);
            m_layer_upper_slider->blockSignals(false);
        }
    }
    m_layer_upper->blockSignals(true);
    m_layer_upper->setValue(new_value);
    m_layer_upper->blockSignals(false);
    forwardUpperLayerUpdate(new_value);
}

void GcodeBar::forwardUpperLayerUpdate(int new_value) {
    if (m_lock_layer->isChecked())
        m_layer_lower->setValue(new_value - m_lock_distance);
    // Before we forward, we should check if the new upper layer value is less than the lower layer,
    // and if so, set the lower layer to the upper layer.
    else if (new_value < m_layer_lower->value()) {
        // Note that this will cause the slot GcodeBar::forwardLowerLayerUpdate to be invoked
        m_layer_lower->setValue(new_value);
    }
    emit upperLayerUpdated(new_value);
}

void GcodeBar::updateSegmentLowerSpin(int new_value) {
    m_segment_lower_slider->blockSignals(true);
    m_segment_lower_slider->setValue(new_value);
    m_segment_lower_slider->blockSignals(false);
    forwardLowerSegmentUpdate(new_value);
}

void GcodeBar::updateSegmentLowerSlider(int new_value) {
    m_segment_lower->blockSignals(true);
    m_segment_lower->setValue(new_value);
    m_segment_lower->blockSignals(false);
    forwardLowerSegmentUpdate(new_value);
}

void GcodeBar::forwardLowerSegmentUpdate(int new_value) {
    if (new_value > m_segment_upper->value()) {
        // Note that this will cause the slot GcodeBar::forwardUpperLayerUpdate to be invoked
        m_segment_upper->setValue(new_value);
    }
    emit lowerSegmentUpdated(new_value);
}

void GcodeBar::updateSegmentUpperSpin(int new_value) {
    m_segment_upper_slider->blockSignals(true);
    m_segment_upper_slider->setValue(new_value);
    m_segment_upper_slider->blockSignals(false);
    forwardUpperSegmentUpdate(new_value);
}

void GcodeBar::updateSegmentUpperSlider(int new_value) {
    m_segment_upper->blockSignals(true);
    m_segment_upper->setValue(new_value);
    m_segment_upper->blockSignals(false);
    forwardUpperSegmentUpdate(new_value);
}

void GcodeBar::forwardUpperSegmentUpdate(int new_value) {
    if (new_value < m_segment_lower->value()) {
        // Note that this will cause the slot GcodeBar::forwardUpperLayerUpdate to be invoked
        m_segment_lower->setValue(new_value);
    }
    emit upperSegmentUpdated(new_value);
}

void GcodeBar::forwardLineChange(QList<int> linesToAdd, QList<int> linesToRemove) {
    this->setLineNumber(linesToAdd, linesToRemove, false);
    emit lineChanged(linesToAdd, linesToRemove);

    if (linesToAdd.count() == 1 && linesToAdd[0] > 0) {
        int layerNumber = 0;
        for (int layerStartLine : m_layer_first_line_numbers) {
            if (layerStartLine > linesToAdd[0] + 1)
                break;
            ++layerNumber;
        }
        --layerNumber;

        if (layerNumber < m_layer_lower->value()) {
            updateLowerSpin(layerNumber);
            updateLowerSlider(layerNumber);
        }
        else if (layerNumber > m_layer_upper->value()) {
            updateUpperSpin(layerNumber);
            updateUpperSlider(layerNumber);
        }
    }
}

void GcodeBar::moveToLayer(int layerNo) {
    // if moveToLayer() is called by either clicking a visible line or an up/down arrow, do not move the line to the top
    if (!m_view->getCursorManualMove() && layerNo >= 0 && layerNo < m_layer_first_line_numbers.size()) {
        m_view->moveCursorToLine(m_layer_first_line_numbers.at(layerNo));
    }
}

void GcodeBar::setMaxLayer(int max_value) {
    m_layer_lower->setMaximum(max_value);
    m_layer_lower_slider->setMaximum(max_value);
    m_layer_upper->setMaximum(max_value);
    m_layer_upper_slider->setMaximum(max_value);
}

void GcodeBar::setMaxSegment(int max_value) {
    // If layer has no geometry, max_value will be negative one
    // Make max_value zero to show no geometry exists
    if (max_value < 0)
        max_value = 0;

    m_segment_lower->setMaximum(max_value);
    m_segment_lower_slider->setMaximum(max_value);
    m_segment_upper->setMaximum(max_value);
    m_segment_upper_slider->setMaximum(max_value);

    updateSegmentUpperSpin(max_value);
    updateSegmentUpperSlider(max_value);
    updateSegmentLowerSpin(0);
    updateSegmentLowerSlider(0);
}

void GcodeBar::setLineNumber(QList<int> linesToAdd, QList<int> linesToRemove, bool shouldCenter) {
    m_view->highlightLine(linesToAdd, linesToRemove, shouldCenter);
}

void GcodeBar::setupWidget() {
    this->setupSubWidgets();
    this->setupLayouts();
    this->setupActions();
    this->setupInsert();
    this->setupEvents();
}

void GcodeBar::setupSubWidgets() {

    // Search Separator
    m_search_separator = new QFrame(this);
    m_search_separator->setFrameShape(QFrame::HLine);
    m_search_separator->setFrameShadow(QFrame::Sunken);

    // View Separator
    m_view_separator = new QFrame(this);
    m_view_separator->setFrameShape(QFrame::HLine);
    m_view_separator->setFrameShadow(QFrame::Sunken);

    // Search View
    m_search_bar = new QLineEdit(this);
    m_search_bar->setPlaceholderText("Search through GCode...");
    m_search_bar->setToolTip("Search through GCode ...");

    // Refresh button
    m_refresh_btn = new QToolButton(this);
    m_refresh_btn->setIcon(QIcon(":/icons/file_refresh_black.png"));
    m_refresh_btn->setToolTip("Refresh");
    m_refresh_btn->setEnabled(false);

    // Main View
    m_view = new GcodeTextBoxWidget(this);

    // View Selection
    m_view_sel = new QComboBox(this);

    // Hide Travel CheckBox
    m_hide_travel = new QCheckBox(this);
    m_hide_travel->setText("Hide Travel");
    m_hide_travel->setChecked(PreferencesManager::getInstance()->getHideTravelPreference());

    // Hide Support CheckBox
    m_hide_support = new QCheckBox(this);
    m_hide_support->setText("Hide Support");
    m_hide_support->setChecked(PreferencesManager::getInstance()->getHideSupportPreference());

    // True Bead Widths CheckBox
    m_true_width = new QCheckBox(this);
    m_true_width->setText("True Bead Widths");
    m_true_width->setChecked(PreferencesManager::getInstance()->getUseTrueWidthsPreference());

    // Lower Layer Spinbox
    m_layer_lower = new QSpinBox(this);
    m_layer_lower->setMinimum(0);
    m_layer_lower->setValue(0);

    // Upper Layer Spinbox
    m_layer_upper = new QSpinBox(this);
    m_layer_upper->setMinimum(0);
    m_layer_upper->setValue(1);

    // Lock checkbox
    m_lock_layer = new QCheckBox(this);
    m_lock_layer->setText("Lock Layer Range");

    // Layer Sliders
    m_layer_lower_slider = new QSlider(Qt::Horizontal, this);
    m_layer_upper_slider = new QSlider(Qt::Horizontal, this);
    m_layer_upper_slider->setValue(1);

    // Layer Labels
    m_lower_label = new QLabel("Lower Layer:", this);
    m_upper_label = new QLabel("Upper Layer:", this);

    // Layer Play button
    m_layer_play_btn = new QToolButton(this);
    m_layer_play_btn->setIcon(QIcon(":/icons/next.png"));
    m_layer_play_btn->setToolTip("Play Layers");
    m_layer_play_btn->setEnabled(true);

    // Lower Segment Spinbox
    m_segment_lower = new QSpinBox(this);
    m_segment_lower->setMinimum(0);
    m_segment_lower->setValue(0);

    // Upper Segment Spinbox
    m_segment_upper = new QSpinBox(this);
    m_segment_upper->setMinimum(0);
    m_segment_upper->setValue(1);

    // Segment Sliders
    m_segment_lower_slider = new QSlider(Qt::Horizontal, this);
    m_segment_upper_slider = new QSlider(Qt::Horizontal, this);
    m_segment_upper_slider->setValue(1);

    // Segment Labels
    m_lower_segment_label = new QLabel("First Segment:", this);
    m_upper_segment_label = new QLabel("Last Segment:", this);

    // Segment Play button
    m_segment_play_btn = new QToolButton(this);
    m_segment_play_btn->setIcon(QIcon(":/icons/next.png"));
    m_segment_play_btn->setToolTip("Play Segments");
    m_segment_play_btn->setEnabled(true);
}

void GcodeBar::setupLayouts() {
    // Main Layout
    m_layout = new QGridLayout(this);
    m_layout->setSpacing(6);
    m_layout->setContentsMargins(11, 11, 11, 11);
}

void GcodeBar::setupActions() {}

void GcodeBar::setupInsert() {
    m_layout->addWidget(m_search_bar, 0, 0, 1, 5);
    m_layout->addWidget(m_refresh_btn, 0, 5, 1, 1);

    m_layout->addWidget(m_search_separator, 1, 0, 1, 5);

    m_layout->addWidget(m_view, 2, 0, 1, 5);

    m_layout->addWidget(m_view_sel, 3, 0, 1, 5);

    m_layout->addWidget(m_view_separator, 4, 0, 1, 5);

    m_layout->addWidget(m_hide_travel, 5, 0, 1, 1);
    m_layout->addWidget(m_hide_support, 5, 1, 1, 1);
    m_layout->addWidget(m_true_width, 5, 2, 1, 1);

    m_layout->addWidget(m_lower_label, 6, 0, 1, 1);
    m_layout->addWidget(m_layer_lower, 6, 1, 1, 1);
    m_layout->addWidget(m_layer_lower_slider, 6, 2, 1, 2);
    m_layout->addWidget(m_lock_layer, 5, 4, 2, 1);

    m_layout->addWidget(m_upper_label, 7, 0, 1, 1);
    m_layout->addWidget(m_layer_upper, 7, 1, 1, 1);
    m_layout->addWidget(m_layer_upper_slider, 7, 2, 1, 2);

    m_layout->addWidget(m_layer_play_btn, 7, 4, 1, 1);

    m_layout->addWidget(m_lower_segment_label, 8, 0, 1, 1);
    m_layout->addWidget(m_segment_lower, 8, 1, 1, 1);
    m_layout->addWidget(m_segment_lower_slider, 8, 2, 1, 2);

    m_layout->addWidget(m_upper_segment_label, 9, 0, 1, 1);
    m_layout->addWidget(m_segment_upper, 9, 1, 1, 1);
    m_layout->addWidget(m_segment_upper_slider, 9, 2, 1, 2);

    m_layout->addWidget(m_segment_play_btn, 9, 4, 1, 1);
}

void GcodeBar::setupEvents() {
    connect(m_layer_lower, QOverload<int>::of(&QSpinBox::valueChanged), this, &GcodeBar::updateLowerSpin);
    connect(m_layer_lower_slider, &QSlider::valueChanged, this, &GcodeBar::updateLowerSlider);
    connect(m_layer_upper, QOverload<int>::of(&QSpinBox::valueChanged), this, &GcodeBar::updateUpperSpin);
    connect(m_layer_upper_slider, &QSlider::valueChanged, this, &GcodeBar::updateUpperSlider);

    connect(m_segment_lower, QOverload<int>::of(&QSpinBox::valueChanged), this, &GcodeBar::updateSegmentLowerSpin);
    connect(m_segment_lower_slider, &QSlider::valueChanged, this, &GcodeBar::updateSegmentLowerSlider);
    connect(m_segment_upper, QOverload<int>::of(&QSpinBox::valueChanged), this, &GcodeBar::updateSegmentUpperSpin);
    connect(m_segment_upper_slider, &QSlider::valueChanged, this, &GcodeBar::updateSegmentUpperSlider);

    connect(this, &GcodeBar::lowerLayerUpdated, this, &GcodeBar::moveToLayer);
    connect(m_search_bar, &QLineEdit::returnPressed, this, &GcodeBar::search);
    connect(m_refresh_btn, &QPushButton::clicked, this, &GcodeBar::updateGCodeView);
    connect(m_layer_play_btn, &QPushButton::clicked, this, &GcodeBar::updatePlayButton);
    connect(m_segment_play_btn, &QPushButton::clicked, this, &GcodeBar::updateSegmentPlayButton);
    connect(m_view, &GcodeTextBoxWidget::lineChange, this, &GcodeBar::forwardLineChange);
    connect(m_view, &QPlainTextEdit::modificationChanged, this, &GcodeBar::updateRefreshButton);

    connect(m_hide_travel, &QCheckBox::clicked, [this] {
        emit forwardVisibilityChange(SegmentDisplayType::kTravel, m_hide_travel->isChecked());
        PreferencesManager::getInstance()->setHideTravelPreference(m_hide_travel->isChecked());
    });

    connect(m_hide_support, &QCheckBox::clicked, [this] {
        emit forwardVisibilityChange(SegmentDisplayType::kSupport, m_hide_support->isChecked());
        PreferencesManager::getInstance()->setHideSupportPreference(m_hide_support->isChecked());
    });

    connect(m_true_width, &QCheckBox::clicked, [this] {
        emit forwardSegmentWidthChange(m_true_width->isChecked());
        PreferencesManager::getInstance()->setUseTrueWidthsPreference(m_true_width->isChecked());
    });

    connect(m_lock_layer, &QCheckBox::clicked, [this] {
        if (m_lock_layer->isChecked())
            m_lock_distance = m_layer_upper->value() - m_layer_lower->value();
    });

    // Timeout occurs when the timer is up, and the next layer or segment is drawn
    m_layer_timer = new QTimer();
    connect(m_layer_timer, &QTimer::timeout, this, &GcodeBar::drawNextLayer);
    m_segment_timer = new QTimer();
    connect(m_segment_timer, &QTimer::timeout, this, &GcodeBar::drawNextSegment);
}

void GcodeBar::updateGCodeView() {
    QDir appPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QString tempFileName = appPath.filePath("gcode_output_temp");

    QTextDocumentWriter tempWriter;
    tempWriter.setFileName(tempFileName);
    tempWriter.setFormat("plaintext");
    tempWriter.write(m_view->document());

    // load the file. "false" is to prevent adjusting GCode based on minimum layer time
    emit refreshGCode(tempFileName, false);
}

void GcodeBar::search() {
    m_search_only_change = true;
    QString searchString = m_search_bar->text().trimmed();
    // perform search even if search string is empty - otherwise highlights from the previous search stay
    m_view->search(searchString, m_search_count);
    m_search_only_change = false;
    ++m_search_count;

    emit removeHighlight();
}

void GcodeBar::updateRefreshButton(bool change) {

    // Don't enable the refresh button if the change is for formatting resulted from a search
    // QPlainTextEdit::modificationChanged is for all types of modifications. No signal available for content change
    // only
    if (!m_search_only_change)
        m_refresh_btn->setEnabled(change);
}

void GcodeBar::updatePlayButton() {
    if (m_layer_timer->isActive()) {
        // Pause drawing layers and reset image
        m_layer_timer->stop();
        m_layer_play_btn->setIcon(QIcon(":/icons/next.png"));
        m_layer_play_btn->setToolTip("Start playing layers");
    }
    else {
        // Delay between drawing layers in milliseconds
        int draw_time = PreferencesManager::getInstance()->getLayerLag();
        m_layer_timer->start(draw_time);

        m_layer_play_btn->setIcon(QIcon(":/icons/stop.png"));
        m_layer_play_btn->setToolTip("Pause");

        if (m_layer_upper_slider->value() >= m_layer_upper_slider->maximum()) {
            updateUpperSlider(0);
            updateUpperSpin(0);
        }
    }
}

void GcodeBar::updateSegmentPlayButton() {
    if (m_segment_timer->isActive()) {
        // Pause drawing layers and reset image
        m_segment_timer->stop();
        m_segment_play_btn->setIcon(QIcon(":/icons/next.png"));
        m_segment_play_btn->setToolTip("Start playing segments");
    }
    else {
        if (m_hide_travel->isChecked())
            m_hide_travel->click();
        if (m_hide_support->isChecked())
            m_hide_support->click();

        // Delay between drawing segments in milliseconds
        int draw_time = PreferencesManager::getInstance()->getSegmentLag();
        m_segment_timer->start(draw_time);

        m_segment_play_btn->setIcon(QIcon(":/icons/stop.png"));
        m_segment_play_btn->setToolTip("Pause");

        if (m_segment_upper_slider->value() >= m_segment_upper_slider->maximum()) {
            updateSegmentUpperSlider(0);
            updateSegmentUpperSpin(0);
        }
    }
}

void GcodeBar::drawNextLayer() {
    // Increase the layer slider until all layers are shown
    if (m_layer_upper_slider->value() < m_layer_upper_slider->maximum()) {
        updateUpperSlider(m_layer_upper_slider->value() + 1);
        updateUpperSpin(m_layer_upper_slider->value() + 1);
    }
    else {
        m_layer_timer->stop();
        m_layer_play_btn->setIcon(QIcon(":/icons/next.png"));
        m_layer_play_btn->setToolTip("Start playing layers");
    }
}

void GcodeBar::drawNextSegment() {
    // Increase the segment slider until all segments in window are shown
    if (m_segment_upper_slider->value() < m_segment_upper_slider->maximum()) {
        updateSegmentUpperSlider(m_segment_upper_slider->value() + 1);
        updateSegmentUpperSpin(m_segment_upper_slider->value() + 1);
    }
    else {
        m_segment_timer->stop();
        m_segment_play_btn->setIcon(QIcon(":/icons/next.png"));
        m_segment_play_btn->setToolTip("Start playing segments");
    }
}
} // namespace ORNL
