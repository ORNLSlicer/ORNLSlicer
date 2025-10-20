#pragma once

#include <QColor>
#include <QWidget>

namespace ORNL {
/*!
 * @class Theme
 * @brief Holds the path to the theme styles and the colors used for drawn objects
 */
class Theme {
  public:
    //! @brief Constructor
    //! @param themeNum Number corresponding to the desired theme (should be taken from ThemeName enumerator)
    Theme(int themeNum);

    //! @brief Sets the values of the current theme according to theme choice
    //! @param themeNum Number corresponding to the desired theme (should be taken from ThemeName enumerator)
    void chooseTheme(int themeNum);

    //! @brief Sets the path of the current theme
    //! @param path The path to the current theme's folder
    void setPath(QString path);

    //! @brief Sets the dot color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotColor(int r, int g, int b, int a = 255);

    //! @brief Sets the dot color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setDotColor(QColor color);

    //! @brief Sets the selected dot color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotSelectedColor(int r, int g, int b, int a = 255);

    //! @brief Sets the selected dot color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setDotSelectedColor(QColor color);

    //! @brief Sets the dot hover color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotHoverColor(int r, int g, int b, int a = 255);

    //! @brief Sets the dot hover color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setDotHoverColor(QColor color);

    //! @brief Sets the dot priority color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotPrioColor(int r, int g, int b, int a = 255);

    //! @brief Sets the dot priority color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setDotPrioColor(QColor color);

    //! @brief Sets the color of the line connecting paired dots in the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotPairedColor(int r, int g, int b, int a = 255);

    //! @brief Sets the color of the line connecting paired dots using a QColor
    //! @param color QColor specifying the desired color
    void setDotPairedColor(QColor color);

    //! @brief Sets the grouped dot color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotGroupedColor(int r, int g, int b, int a = 255);

    //! @brief Sets the grouped dot color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setDotGroupedColor(QColor color);

    //! @brief Sets the dot label color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setDotLabelColor(int r, int g, int b, int a = 255);

    //! @brief Sets the dot label color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setDotLabelColor(QColor color);

    //! @brief Sets the view background color of the current theme
    //! @param r Red value of the desired color (0.00 - 1.00; extended RGB)
    //! @param g Green value of the desired color (0.00 - 1.00; extended RGB)
    //! @param b Blue value of the desired color (0.00 - 1.00; extended RGB)
    //! @param a Alpha value of the desired color (0.00 - 1.00). Default is 1.0
    void setBgColor(double r, double g, double b, double a = 1.0);

    //! @brief Sets the layerbar major line color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setLayerbarMajorColor(int r, int g, int b, int a = 255);

    //! @brief Sets the layerbar major line color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setLayerbarMajorColor(QColor color);

    //! @brief Sets the layerbar minor line color of the current theme
    //! @param r Red value of the desired color (0-255)
    //! @param g Green value of the desired color (0-255)
    //! @param b Blue value of the desired color (0-255)
    //! @param a Alpha value of the desired color (0-255). Default is 255
    void setLayerbarMinorColor(int r, int g, int b, int a = 255);

    //! @brief Sets the layerbar minor line color of the current theme using a QColor
    //! @param color QColor specifying the desired color
    void setLayerbarMinorColor(QColor color);

    //! @brief Sets the G-code text editor line colors for the current theme
    //! @param base Array of [r, g, b] values for the base line color (each is 0-255)
    //! @param highlight Array of [r, g, b] values for the highlight line color (each is 0-255)
    void setGcodeTextboxLineColors(QList<int> base, QList<int> highlight);

    //! @brief Sets the G-code text editor line colors for the current theme using QColors
    //! @param base_color QColor specifying the base line color
    //! @param highlight_color QColor specifying the highlight line color
    void setGcodeTextboxLineColors(QColor base_color, QColor highlight_color);

    //! @brief Return the dot colors
    //! @return Vector of QColor containing the dot colors used by the theme
    QVector<QColor> getDotColors();

    //! @brief Get the dot priority color
    //! @return QColor representing the dot priority color
    QColor getDotPrioColor();

    //! @brief Get the paired dot line color
    //! @return QColor representing the color of the line connecting paired dots
    QColor getDotPairedColor();

    //! @brief Get the layerbar major line color
    //! @return QColor representing the major layerbar line color
    QColor getLayerbarMajorColor();

    //! @brief Get the layerbar minor line color
    //! @return QColor representing the minor layerbar line color
    QColor getLayerbarMinorColor();

    //! @brief Get the background color
    //! @return Vector of double containing the background color components [r, g, b, a]
    QVector<double> getBgColor();

    //! @brief Get the base line color used in the G-code text editor
    //! @return QColor representing the base line color
    QColor getGcodeTextboxBaseLineColor();

    //! @brief Get the highlight line color used in the G-code text editor
    //! @return QColor representing the highlight line color
    QColor getGcodeTextboxHighlightLineColor();

    //! @brief Returns the folder path of the current theme
    //! @return QString holding the folder path for the current theme
    QString getFolderPath();

  private:
    //! Folder path
    QString m_folder_path;

    //! Colors
    QVector<double> m_bgColor;
    QVector<QColor> m_dotColors;
    QColor m_dotColor_paired;
    QColor m_dotColor_prio;
    QColor m_lineColor_major;
    QColor m_lineColor_minor;
    QColor m_gcodeLineColor_base;
    QColor m_gcodeLineColor_highlight;
};
} // namespace ORNL
