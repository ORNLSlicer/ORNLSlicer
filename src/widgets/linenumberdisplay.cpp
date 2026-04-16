#include "widgets/linenumberdisplay.h"

#include <QSize>

#include "widgets/gcodetextboxwidget.h"

namespace ORNL {
LineNumberDisplay::LineNumberDisplay(GcodeTextBoxWidget* textbox) : QWidget(textbox), textBox(textbox) {}

QSize LineNumberDisplay::sizeHint() const { return QSize(textBox->calculateLineNumbersDisplayWidth(), 0); }

void LineNumberDisplay::paintEvent(QPaintEvent* event) { textBox->lineNumbersPaintEvent(event); }

} // namespace ORNL
