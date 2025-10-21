#include "widgets/gcodetextboxwidget.h"

#include "managers/preferences_manager.h"
#include "qpainter.h"
#include "qscrollbar.h"
#include "qtextdocumentfragment.h"
#include "widgets/linenumberdisplay.h"

#include <algorithm>
#include <cmath>

namespace ORNL {
GcodeTextBoxWidget::GcodeTextBoxWidget(QWidget* parent)
    : QPlainTextEdit(parent), m_highlighter(this->document()), m_previous_line(0), m_last_block_count(0),
      m_last_block_clicked_on(0) {
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_line_number_display_area = new LineNumberDisplay(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &GcodeTextBoxWidget::updateLineNumberDisplayAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &GcodeTextBoxWidget::updateLineNumberDisplayArea);

    setViewportMargins(calculateLineNumbersDisplayWidth(), 0, 0, 0);
    m_manual_cursor_move = false;
}

void GcodeTextBoxWidget::setHighlighterColors(QHash<QString, QTextCharFormat> fontColors,
                                              QSet<int> layerSkipLineNumbers) {
    m_highlighter.setColorRules(fontColors, layerSkipLineNumbers);
}

void GcodeTextBoxWidget::lineNumbersPaintEvent(QPaintEvent* event) {
    QPainter painter(m_line_number_display_area);
    painter.fillRect(event->rect(), QColor(255, 255, 255, 100));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, m_line_number_display_area->width() - 5, fontMetrics().height(), Qt::AlignCenter,
                             number);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

int GcodeTextBoxWidget::calculateLineNumbersDisplayWidth() {
    return 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * ceil(log10(std::max<int>(2, blockCount())) + 1);
}

int GcodeTextBoxWidget::getCursorBlockNumber() { return textCursor().block().blockNumber(); }

void GcodeTextBoxWidget::highlightLine(QList<int> linesToAdd, QList<int> linesToRemove, bool shouldCenter) {
    QTextCursor manipulate_cursor(textCursor());
    for (int line_num : linesToAdd) {
        int total = qAbs(line_num - manipulate_cursor.blockNumber());
        manipulate_cursor.movePosition(QTextCursor::StartOfBlock);

        if (line_num < manipulate_cursor.blockNumber())
            manipulate_cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, total);
        else
            manipulate_cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, total);

        QTextBlockFormat format;
        format.setBackground(QBrush(PreferencesManager::getInstance()->getTheme().getGcodeTextboxHighlightLineColor()));
        manipulate_cursor.setBlockFormat(format);
        setTextCursor(manipulate_cursor);
        if (shouldCenter)
            this->centerCursor();

        m_selected_blocks.insert(line_num);
    }

    for (int line_num : linesToRemove) {
        int total = qAbs(line_num - manipulate_cursor.blockNumber());
        manipulate_cursor.movePosition(QTextCursor::StartOfBlock);

        if (line_num < manipulate_cursor.blockNumber())
            manipulate_cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, total);
        else
            manipulate_cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, total);

        QTextBlockFormat format;
        format.setBackground(QBrush(PreferencesManager::getInstance()->getTheme().getGcodeTextboxBaseLineColor()));
        manipulate_cursor.setBlockFormat(format);
        setTextCursor(manipulate_cursor);
        m_selected_blocks.remove(line_num);
    }
}

bool GcodeTextBoxWidget::getCursorManualMove() {
    // if m_manual_cursor_move is true, the cursor move is by either clicking the line or arrowing up/down lines
    // if this cursor move results in a layer change, it will then not set the cursor to the beginning of the layer
    return m_manual_cursor_move;
}

void GcodeTextBoxWidget::setCursorManualMoveFalse() { m_manual_cursor_move = false; }

void GcodeTextBoxWidget::moveCursorToLine(int line_num) {
    if (!m_manual_cursor_move) {
        // move further down and move back to the line to ensure that the GCode editor will place the line on top
        int visible_lines = this->height() / this->fontMetrics().height();
        int move_ahead_lines = std::min(document()->blockCount() - line_num, visible_lines);
        QTextBlock block = document()->findBlockByLineNumber(line_num + move_ahead_lines);
        if (block.isValid()) {
            setTextCursor(QTextCursor(block));
        }
    }

    setTextCursor(QTextCursor(document()->findBlockByLineNumber(line_num - 1)));
    m_manual_cursor_move = false;
}

void GcodeTextBoxWidget::resetHighlight() {
    m_previous_line = -1;
    m_highlighted_block = QTextBlock();
}

void GcodeTextBoxWidget::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    QRect rectangle = contentsRect();
    m_line_number_display_area->setGeometry(
        QRect(rectangle.left(), rectangle.top(), calculateLineNumbersDisplayWidth(), rectangle.height()));
}

void GcodeTextBoxWidget::updateLineNumberDisplayAreaWidth(int blockCount) {
    setViewportMargins(calculateLineNumbersDisplayWidth(), 0, 0, 0);

    if (m_last_block_count != blockCount && m_highlighted_block != QTextBlock()) {
        if (m_last_block_count < blockCount) {
            QTextCursor old_cursor(m_highlighted_block);
            QTextBlockFormat plain_format;
            plain_format.setBackground(QColor(Qt::white));
            old_cursor.setBlockFormat(plain_format);

            m_highlighted_block = textCursor().block();
        }
        else if (m_last_block_count > blockCount) {
            m_highlighted_block = textCursor().block();
            QTextCursor cursor(m_highlighted_block);
            QTextBlockFormat format;
            format.setBackground(QColor(Qt::yellow));
            cursor.setBlockFormat(format);
        }
        m_last_block_count = blockCount;
    }
}

void GcodeTextBoxWidget::updateLineNumberDisplayArea(const QRect& rect, int height) {
    if (height) {
        m_line_number_display_area->scroll(0, height);
    }
    else {
        m_line_number_display_area->update(0, rect.y(), m_line_number_display_area->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        setViewportMargins(calculateLineNumbersDisplayWidth(), 0, 0, 0);
    }
}

void GcodeTextBoxWidget::mouseReleaseEvent(QMouseEvent* event) {
    int scrollPos = verticalScrollBar()->value();

    m_manual_cursor_move = true;
    if (m_previous_line == textCursor().blockNumber()) {
        return;
    }

    QList<int> linesToAdd, linesToRemove;
    Qt::KeyboardModifiers modifier = QGuiApplication::queryKeyboardModifiers();
    if (modifier == Qt::ControlModifier) {
        if (m_selected_blocks.contains(textCursor().blockNumber()))
            linesToRemove.push_back(textCursor().blockNumber());
        else
            linesToAdd.push_back(textCursor().blockNumber());

        m_last_block_clicked_on = textCursor().blockNumber();
    }
    else if (modifier == Qt::ShiftModifier) {
        QVector<int> minMaxVec {m_last_block_clicked_on, textCursor().blockNumber()};
        std::sort(minMaxVec.begin(), minMaxVec.end());

        for (int i = minMaxVec[0]; i < minMaxVec[1]; ++i) {
            if (!m_selected_blocks.contains(i))
                linesToAdd.push_back(i);
        }
        for (int key : m_selected_blocks.values()) {
            if (key < minMaxVec[0] || key > minMaxVec[1])
                linesToRemove.push_back(key);
        }
    }
    else {
        if (!textCursor().selection().isEmpty()) {
            QTextCursor cursor_copy(textCursor());
            int start = textCursor().selectionStart();
            int end = textCursor().selectionEnd();
            cursor_copy.setPosition(start);
            int firstLine = cursor_copy.blockNumber();
            cursor_copy.setPosition(end);
            int lastLine = cursor_copy.blockNumber();

            for (int i = firstLine; i <= lastLine; ++i) {
                if (!m_selected_blocks.contains(i))
                    linesToAdd.push_back(i);
            }
        }
        else {
            linesToRemove = m_selected_blocks.values();
            linesToAdd.push_back(textCursor().blockNumber());
        }
        m_last_block_clicked_on = textCursor().blockNumber();
    }

    emit lineChange(linesToAdd, linesToRemove);

    verticalScrollBar()->setValue(scrollPos);
    setTextCursor(QTextCursor(document()->findBlockByLineNumber(m_last_block_clicked_on)));
}

void GcodeTextBoxWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        QList<int> linesToAdd, linesToRemove;
        linesToRemove = m_selected_blocks.values();

        m_manual_cursor_move = true;
        int lineId = textCursor().blockNumber();

        if (event->key() == Qt::Key_Up) {
            --lineId;
        }
        else if (event->key() == Qt::Key_Down) {
            ++lineId;
        }

        linesToAdd.push_back(lineId);
        emit lineChange(linesToAdd, linesToRemove);

        // signal lineChange will trigger a correct cursor move to highlightable lines, but not to most
        // non-highlightable lines
        setTextCursor(QTextCursor(document()->findBlockByLineNumber(lineId)));
    }
    else {
        // default event handler for other keys
        QPlainTextEdit::keyPressEvent(event);
    }
}

void GcodeTextBoxWidget::setLayerFirstLineNumbers(QList<int>& firstLineNumbers) {
    m_layer_first_line_numbers = firstLineNumbers;
}

void GcodeTextBoxWidget::search(QString searchString, int searchCount) {
    bool repeatedSearch = searchString.size() > 0 && m_search_string.compare(searchString) == 0;

    if (repeatedSearch && searchCount > 0) {
        // move the highlight to the next occurrence for repeated search
        ++m_cursor_index;

        // wrap-around at EOF
        if (m_cursor_index < m_matched_lines.size()) {
            int lineNum = m_matched_lines[m_cursor_index];

            // move the cursor there
            //   if the next match is currently visible on the current page, the page stays
            //   else jump to the page for the next match and set that line on top
            int blockNumber = firstVisibleBlock().blockNumber();
            int visible_lines = this->height() / this->fontMetrics().height();
            if (lineNum - blockNumber > visible_lines - 1 || lineNum < blockNumber)
                m_manual_cursor_move = false;
            else
                m_manual_cursor_move = true;

            moveCursorToLine(lineNum + 1);
        }
        else {
            m_cursor_index = 0;
        }
    }
    else {
        // reset highlight cursor for a new search. GCode reload automatically resets the search
        m_cursor_index = 0;
    }

    // the following code is needed even for a repeated search to keep track of the current matching occurance,
    // i.e. subsequent enter/return highlights the next occurance
    m_search_string = searchString;
    m_matched_lines.clear();

    QTextDocument* document = this->document();
    document->undo();

    QTextCursor highlightCursor(document);
    QTextCursor cursor(document);

    cursor.beginEditBlock();

    QTextCharFormat plainFormat(highlightCursor.charFormat());
    QTextCharFormat matchedColorFormat = plainFormat;
    // set background color so that the matches can also be visible in colored lines
    matchedColorFormat.setBackground(QColor(255, 240, 0));

    QTextCharFormat focusedColorFormat = plainFormat;
    focusedColorFormat.setBackground(QColor(255, 100, 0));

    bool found = false;
    int lineNum = -1;
    int matchedId = 0;
    while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
        highlightCursor = document->find(searchString, highlightCursor);
        if (!highlightCursor.isNull()) {
            m_matched_lines.append(highlightCursor.blockNumber());
            if (lineNum < 0)
                lineNum = highlightCursor.blockNumber();
            found = true;
            highlightCursor.mergeCharFormat(matchedColorFormat);
            if (matchedId == m_cursor_index)
                highlightCursor.mergeCharFormat(focusedColorFormat);
            ++matchedId;
        }
    }

    // leap to the first match for a fresh search
    if (found && m_cursor_index == 0) {
        // if the first match is not currently visible, move the cursor there
        int blockNumber = firstVisibleBlock().blockNumber();
        int visible_lines = this->height() / this->fontMetrics().height();
        if (lineNum - blockNumber > visible_lines - 1 || lineNum < blockNumber)
            moveCursorToLine(lineNum + 1);
    }

    cursor.endEditBlock();

    // this is necessary to make the PlainTextEdit emit modification signal again for content editing
    document->setModified(false);
}
} // namespace ORNL
