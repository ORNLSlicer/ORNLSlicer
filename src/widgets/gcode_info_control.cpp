#include "widgets/gcode_info_control.h"

#include "managers/preferences_manager.h"

namespace ORNL {
GCodeInfoControl::GCodeInfoControl(QWidget* parent) : QWidget(parent) { setupWidget(); }

void GCodeInfoControl::setGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode) {
    m_gcode = gcode;

    m_line_no_list.clear();
    fillSegmentInfo(0);

    m_headercb_lines->clear();
}

void GCodeInfoControl::fillSegmentInfo(uint lineNo) {
    if (m_line_no_list.length() > 0 && lineNo > 0) {
        for (auto& layer : m_gcode) {
            if (layer.isEmpty() || layer.back()->lineNumber() < lineNo)
                continue;

            for (auto& seg : layer) {
                if (seg->lineNumber() == lineNo) {
                    m_infolbl_type->setText(seg->m_segment_info_meta.type);
                    m_infolbl_speed->setText(seg->m_segment_info_meta.speed);
                    m_infolbl_extruder_speed->setText(seg->m_segment_info_meta.extruderSpeed);
                    m_infolbl_length->setText(seg->m_segment_info_meta.length);
                    m_infolbl_layer_no->setText(QString::number(seg->layerNumber()));
                    m_infolbl_line_no->setText(QString::number(lineNo));

                    if (seg->m_segment_info_meta.isXYmove()) {
                        updateDirection(360 - seg->m_segment_info_meta.getCCWXAngle());
                    }
                    else {
                        float diff = seg->m_segment_info_meta.getZChange();
                        if (diff == 0)
                            m_infolbl_direction->setVisible(false);
                        else
                            updateZDirection(diff > 0 ? 180 : 0);
                    }

                    return;
                }
            }
        }
    }

    m_infolbl_type->setText("");
    m_infolbl_speed->setText("");
    m_infolbl_extruder_speed->setText("");
    m_infolbl_length->setText("");
    m_infolbl_layer_no->setText("");
    m_infolbl_line_no->setText("");

    m_infolbl_direction->setVisible(false);
}

void GCodeInfoControl::addSegmentInfo(int selectedLineNumber) {
    QString textVal = QString::number(selectedLineNumber);

    int index = m_line_no_list.indexOf(selectedLineNumber);
    if (index < 0) {
        index = 0;
        for (; index < m_line_no_list.length(); ++index) {
            if (m_line_no_list[index] > selectedLineNumber)
                break;
        }

        m_line_no_list.insert(index, selectedLineNumber);
        m_headercb_lines->insertItem(index, textVal);
    }

    m_headercb_lines->setCurrentText(textVal);
}

void GCodeInfoControl::removeSegmentInfo(int selectedLineNumber) {
    int index = m_line_no_list.indexOf(selectedLineNumber);
    if (index < 0) {
        fillSegmentInfo(0);
        return;
    }

    m_line_no_list.removeAt(index);
    m_headercb_lines->removeItem(index);
}

void GCodeInfoControl::updateDirection(double angle) {
    m_infolbl_direction->setVisible(true);
    m_infolbl_direction->setPixmap(m_infopm_direction->transformed(QTransform().rotate(angle)));
}

void GCodeInfoControl::updateZDirection(double angle) {
    m_infolbl_direction->setVisible(true);
    m_infolbl_direction->setPixmap(m_infopm_direction_z->transformed(QTransform().rotate(angle)));
}

void GCodeInfoControl::setupWidget() {
    QLabel* lbl2DAxis = new QLabel;

    m_infolbl_type = new QLabel;
    m_infolbl_speed = new QLabel;
    m_infolbl_extruder_speed = new QLabel;
    m_infolbl_length = new QLabel;
    m_infolbl_layer_no = new QLabel;
    m_infolbl_line_no = new QLabel;
    m_infolbl_direction = new QLabel;
    m_infopm_direction = new QPixmap(":/icons/right_flat.png");
    m_infopm_direction_z = new QPixmap(":/icons/down_black.png");
    lbl2DAxis->setPixmap(QPixmap(":/icons/2d_axis.png"));

    m_info_display = new QFrame;
    m_info_display_indicator = new QLabel;
    m_info_display_indicator->setPixmap(QPixmap(":/icons/down_black.png"));

    lbl2DAxis->setToolTip("Print Orientation, 2D (X Y)\nOnly Top Projection View (With Out Any Rotation) Considered");
    m_infolbl_direction->setToolTip("Print Direction\nOnly Top Projection View (With Out Any Rotation) Considered");

    m_info_grid = new QGridLayout(m_info_display);
    m_info_grid->setRowMinimumHeight(0, 75);
    m_info_grid->setRowMinimumHeight(1, 15);
    m_info_grid->setColumnStretch(1, 1);
    m_info_grid->setVerticalSpacing(0);

    setupHeaderWidget();

    m_info_grid->addWidget(lbl2DAxis, 0, 0);
    m_info_grid->addWidget(m_infolbl_direction, 0, 1);
    m_info_grid->addWidget(new QLabel("Type"), 2, 0);
    m_info_grid->addWidget(m_infolbl_type, 2, 1);
    m_info_grid->addWidget(new QLabel("Print Speed"), 3, 0);
    m_info_grid->addWidget(m_infolbl_speed, 3, 1);
    m_info_grid->addWidget(new QLabel("Extruder Speed"), 4, 0);
    m_info_grid->addWidget(m_infolbl_extruder_speed, 4, 1);
    m_info_grid->addWidget(new QLabel("Length"), 5, 0);
    m_info_grid->addWidget(m_infolbl_length, 5, 1);
    m_info_grid->addWidget(new QLabel("Layer #"), 6, 0);
    m_info_grid->addWidget(m_infolbl_layer_no, 6, 1);
    m_info_grid->addWidget(new QLabel("G-Code Line #"), 7, 0);
    m_info_grid->addWidget(m_infolbl_line_no, 7, 1);

    fillSegmentInfo(0);
}

void GCodeInfoControl::setupHeaderWidget() {
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);
    this->setVisible(false);
    this->resize(QSize(520, 350));

    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "gcode_info_control.qss"));
    style->open(QIODevice::ReadOnly);
    auto gcodeInfoControlStyle = style->readAll();
    style->close();

    QClickableFrame* infoHeader = new QClickableFrame;
    infoHeader->setFixedHeight(55);
    infoHeader->setFrameStyle(QFrame::Panel | QFrame::Raised);
    infoHeader->setStyleSheet(gcodeInfoControlStyle);
    connect(infoHeader, &QClickableFrame::mouseLeftButtonClicked, this, [this]() {
        if (m_info_display->isVisible()) {
            m_info_display_indicator->setPixmap(QPixmap(":/icons/up_black.png"));
            m_info_display->hide();
        }
        else {
            m_info_display_indicator->setPixmap(QPixmap(":/icons/down_black.png"));
            m_info_display->show();
        }

        this->update();
    });

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_info_display, 0, Qt::AlignmentFlag::AlignBottom);
    layout->addWidget(infoHeader, 0, Qt::AlignmentFlag::AlignBottom);

    QHBoxLayout* hlayout = new QHBoxLayout(infoHeader);
    hlayout->setContentsMargins(0, 0, 0, 0);

    QLabel* picture = new QLabel;
    picture->setPixmap((new QIcon(":/icons/ornlslicer_logo.png"))->pixmap(QSize(28, 28), QIcon::Normal, QIcon::On));

    QFrame* line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setFixedSize(1, 32);
    line->setFrameShadow(QFrame::Sunken);

    m_headercb_lines = new QComboBox;
    m_headercb_lines->setFixedWidth(120);
    connect(m_headercb_lines, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int selIndex) {
        fillSegmentInfo(m_line_no_list.length() > 0 && selIndex >= 0 ? m_line_no_list[selIndex] : 0);
    });

    hlayout->addWidget(picture);
    hlayout->addWidget(new QLabel("Bead / Segment Info"));
    hlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
    hlayout->addWidget(m_info_display_indicator);
    hlayout->addWidget(line);
    hlayout->addWidget(m_headercb_lines);
}
} // namespace ORNL
