#include "widgets/visualization_color_picker.h"

#include <qcolor.h>
#include <qcolordialog.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"

namespace ORNL {

VisualizationColorPicker::VisualizationColorPicker(QString name, QColor color, QWidget* parent) : QWidget {parent} {
    this->name = name;
    this->color = color;

    QGridLayout* color_tab_layout = new QGridLayout(this);
    color_tab_layout->setVerticalSpacing(0);

    colorTypeLbl = new QLabel(name);
    sampleTextLbl = new QLabel("(This is how it looks!!!)");
    colorResetBtn = new QPushButton();
    colorSelectorBtn = new QPushButton();

    colorResetBtn->setMaximumWidth(80);
    colorSelectorBtn->setMaximumWidth(80);

    colorResetBtn->setToolTip("Reset color with app default");
    colorSelectorBtn->setToolTip("Select and set color");

    colorResetBtn->setStyleSheet("qproperty-icon: url(:/icons/rotate.png)");
    colorSelectorBtn->setStyleSheet("qproperty-icon: url(:/icons/palette_black.png)");

    connect(colorSelectorBtn, &QPushButton::clicked, this, [this]() { selectSetColor(); });

    connect(colorResetBtn, &QPushButton::clicked, this, [this]() {
        this->color = PreferencesManager::getInstance()->revertVisualizationColor(this->name);
        this->updateDisplay();
    });

    color_tab_layout->addWidget(colorTypeLbl, 0, 0);
    color_tab_layout->addWidget(colorSelectorBtn, 0, 1);
    color_tab_layout->addWidget(colorResetBtn, 0, 2);
    color_tab_layout->addWidget(sampleTextLbl, 0, 3);

    updateDisplay();
}

void VisualizationColorPicker::mousePressEvent(QMouseEvent*) { selectSetColor(); }

void ORNL::VisualizationColorPicker::selectSetColor() {
    QColorDialog dlg(this->color);
    dlg.setWindowTitle("Select Visualization Color (" + name + ")");
    if (dlg.exec()) {
        auto newColor = dlg.selectedColor();
        if (newColor.isValid() && newColor != this->color) {
            PreferencesManager::getInstance()->setVisualizationColor(this->name, newColor);
            this->color = newColor;

            this->updateDisplay();
        }
    }
}

void VisualizationColorPicker::updateDisplay() {
    colorTypeLbl->setStyleSheet("color:" + color.name());
    sampleTextLbl->setStyleSheet("color:" + color.name());

    colorResetBtn->setEnabled(!PreferencesManager::getInstance()->isDefaultVisualizationColor(name));
}
} // namespace ORNL
