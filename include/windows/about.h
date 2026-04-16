#pragma once

#include "QWidget"

namespace ORNL {
/*!
 * \class AboutWindow
 * \brief General information about ORNLSlicer
 */
class AboutWindow : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    explicit AboutWindow(QWidget* parent = 0);

    //! \brief Destructor
    ~AboutWindow();
};
} // namespace ORNL
