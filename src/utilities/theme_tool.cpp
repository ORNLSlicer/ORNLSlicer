#include "utilities/theme_tool.h"

namespace ORNL {
Theme::Theme(int themeNum) {
    m_dotColors << Qt::darkGreen << Qt::darkYellow << QColor(255, 172, 28) << Qt::darkBlue << Qt::white;
    m_bgColor << 1.0 << 1.0 << 1.0 << 1.0;
    this->chooseTheme(themeNum);
}

void Theme::chooseTheme(int themeNum) {
    // Dark mode
    if (themeNum == 1) {
        this->setPath(":/styles/qdarkstyle/");
        this->setBgColor(0.169, 0.196, 0.203);
        this->setDotColor(132, 196, 70);
        this->setDotHoverColor(246, 255, 150);
        this->setDotSelectedColor(222, 118, 45);
        this->setDotGroupedColor(42, 223, 232);
        this->setDotPairedColor(42, 223, 232);
        this->setDotLabelColor(0, 0, 0);
        this->setLayerbarMajorColor(255, 255, 255);
        this->setLayerbarMinorColor(77, 85, 87);
        this->setGcodeTextboxLineColors(QList<int> {43, 50, 52}, QList<int> {68, 86, 23});
    }

    // Default mode
    else {
        this->setPath(":/styles/qbasestyle/");
        this->setBgColor(1, 1, 1);
        this->setDotColor(Qt::darkGreen);
        this->setDotHoverColor(Qt::darkYellow);
        this->setDotSelectedColor(QColor(255, 172, 28));
        this->setDotGroupedColor(Qt::darkBlue);
        this->setDotPairedColor(Qt::darkBlue);
        this->setDotLabelColor(Qt::white);
        this->setLayerbarMajorColor(Qt::black);
        this->setLayerbarMinorColor(Qt::darkGray);
        this->setGcodeTextboxLineColors(Qt::white, Qt::yellow);
    }
}

// -----------------------------------------------------------------------------
// Folder Path
// -----------------------------------------------------------------------------

void Theme::setPath(QString path) { m_folder_path = path; }

QString Theme::getFolderPath() { return m_folder_path; }

// -----------------------------------------------------------------------------
// Dot Colors
// -----------------------------------------------------------------------------
// Base color
void Theme::setDotColor(int r, int g, int b, int a) { m_dotColors[0] = QColor(r, g, b, a); }
void Theme::setDotColor(QColor color) { m_dotColors[0] = color; }

// Hover color
void Theme::setDotHoverColor(int r, int g, int b, int a) { m_dotColors[1] = QColor(r, g, b, a); }
void Theme::setDotHoverColor(QColor color) { m_dotColors[1] = color; }

// Selected color
void Theme::setDotSelectedColor(int r, int g, int b, int a) { m_dotColors[2] = QColor(r, g, b, a); }
void Theme::setDotSelectedColor(QColor color) { m_dotColors[2] = color; }

// Grouped color
void Theme::setDotGroupedColor(int r, int g, int b, int a) { m_dotColors[3] = QColor(r, g, b, a); }
void Theme::setDotGroupedColor(QColor color) { m_dotColors[3] = color; }

// Label color
void Theme::setDotLabelColor(int r, int g, int b, int a) { m_dotColors[4] = QColor(r, g, b, a); }
void Theme::setDotLabelColor(QColor color) { m_dotColors[4] = color; }

QVector<QColor> Theme::getDotColors() { return m_dotColors; }

// -----------------------------------------------------------------------------
// Paired Dot Color (line drawn between dots on layer bar)
// -----------------------------------------------------------------------------
void Theme::setDotPairedColor(int r, int g, int b, int a) { m_dotColor_paired = QColor(r, g, b, a); }
void Theme::setDotPairedColor(QColor color) { m_dotColor_paired = color; }

QColor Theme::getDotPairedColor() { return m_dotColor_paired; }

// -----------------------------------------------------------------------------
// Layerbar Major Line Color
// -----------------------------------------------------------------------------
void Theme::setLayerbarMajorColor(int r, int g, int b, int a) { m_lineColor_major = QColor(r, g, b, a); }
void Theme::setLayerbarMajorColor(QColor color) { m_lineColor_major = color; }

QColor Theme::getLayerbarMajorColor() { return m_lineColor_major; }

// -----------------------------------------------------------------------------
// Layerbar Minor Line Color
// -----------------------------------------------------------------------------
void Theme::setLayerbarMinorColor(int r, int g, int b, int a) { m_lineColor_minor = QColor(r, g, b, a); }
void Theme::setLayerbarMinorColor(QColor color) { m_lineColor_minor = color; }

QColor Theme::getLayerbarMinorColor() { return m_lineColor_minor; }

// -----------------------------------------------------------------------------
// View Background Color
// -----------------------------------------------------------------------------
void Theme::setBgColor(double r, double g, double b, double a) {
    m_bgColor[0] = r;
    m_bgColor[1] = g;
    m_bgColor[2] = b;
    m_bgColor[3] = a;
}

QVector<double> Theme::getBgColor() { return m_bgColor; }

// -----------------------------------------------------------------------------
// G-code Text Editor Colors
// -----------------------------------------------------------------------------
void Theme::setGcodeTextboxLineColors(QList<int> base, QList<int> highlight) {
    m_gcodeLineColor_base = QColor(base[0], base[1], base[2]);
    m_gcodeLineColor_highlight = QColor(highlight[0], highlight[1], highlight[2]);
}
void Theme::setGcodeTextboxLineColors(QColor base_color, QColor highlight_color) {
    m_gcodeLineColor_base = base_color;
    m_gcodeLineColor_highlight = highlight_color;
}

QColor Theme::getGcodeTextboxBaseLineColor() { return m_gcodeLineColor_base; }

QColor Theme::getGcodeTextboxHighlightLineColor() { return m_gcodeLineColor_highlight; }

} // namespace ORNL
