#pragma once

#include <QObject>
#include <QWidget>

namespace ORNL {
class GcodeTextBoxWidget;

class LineNumberDisplay : public QWidget {
  public:
    LineNumberDisplay(GcodeTextBoxWidget* textbox);

    QSize sizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    GcodeTextBoxWidget* textBox;
};
} // namespace ORNL
