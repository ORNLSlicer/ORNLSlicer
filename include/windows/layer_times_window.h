#pragma once

#include <QGridLayout>
#include <QLineEdit>
#include <QObject>
#include <QTextEdit>
#include <QWidget>
#include <qlist.h>
#include <qtmetamacros.h>

#include "units/unit.h"

namespace ORNL {
/*!
 * \class LayerTimesWindow
 * \brief Window that displays layer times
 */
class LayerTimesWindow : public QWidget {
    Q_OBJECT

  public:
    //! \brief Standard widget constructor
    //! \param parent Pointer to parent window
    LayerTimesWindow(QWidget* parent);

  public slots:

    //! \brief Slot to receive updated time information from gcode parse
    //! \param layerTimes list of times for each layer
    //! \param minlayerTime user-specified minimum layer time for cooling purpose
    void updateTimeInformation(QList<QList<Time>> layer_times, QList<double> layer_FR_modifiers,
                               bool adjusted_layer_time);

    //! \brief Clear Layer times text
    void clear();

  private:
    //! \brief Setup widget internal events
    void setupEvents();
    //! \brief Update text after user changes threshold or new time information is received
    void updateText();

    //! \brief Layout for widget
    QGridLayout* m_layout;

    //! \brief Inputs for layout
    QLineEdit* m_InputPartName;
    QTextEdit* m_layer_times_edit;
    QLineEdit* m_min_layer_time_edit;

    //! \brief Local copy of layer times received from gcode parse
    QList<QList<Time>> m_layer_times;

    //! \brief Local copy of layer calculated feedrate modifiers for each layer
    QList<double> m_layer_FR_modifiers;

    //! \brief bool value indicating if layer time was adjusted
    bool m_adjusted_layer_time;

    //! \brief total time as calculated from layer
    Time m_total_time;

    //! \brief total adjusted time, accounting for forced minimum layer times
    Time m_total_adjusted_time;

    //! \brief min and max layer time and associated index as calculated from layers
    Time m_min, m_max;
    int m_min_index, m_max_index;
};
} // namespace ORNL
