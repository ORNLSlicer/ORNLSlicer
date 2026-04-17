#pragma once

#include <QObject>
#include <QPlainTextEdit>
#include <qevent.h>
#include <qhash.h>
#include <qlist.h>
#include <qset.h>
#include <qtextformat.h>
#include <qtextobject.h>
#include <qtmetamacros.h>

#include "gcodehighlighter.h"

class QPaintEvent;
class QResizeEvent;
class QWidget;

namespace ORNL {
class LineNumberDisplay;

//! \class GcodeTextBoxWidget
//! \brief This class implements the Gcode textbox seen on the gcodetab
//! within
//!        the GUI.
class GcodeTextBoxWidget : public QPlainTextEdit {
    Q_OBJECT

  public:
    //! \brief Constructor for the Gcode Textbox.
    //! \param parent Parent class for the textbox.
    GcodeTextBoxWidget(QWidget* parent = nullptr);

    //! \brief This function handles the painting of the line numbers to be
    //! seen
    //!        on the side of the textbox display.
    //! \param event The paint event that lays out the rectangle for the
    //! line numbers.
    void lineNumbersPaintEvent(QPaintEvent* event);

    //! \brief This functiounm, calculates the width of the line numbers
    //! display box based
    //!        on the number of lines within the text document.
    int calculateLineNumbersDisplayWidth();

    //! \brief This function is a convenience funciton that returns the
    //! block number in which
    //!        the cursor is currently highlighted on.
    //! \note The block numbers are 0 indexed, so line number 1 corresponds
    //! to block number 0. \note This function takes into account that
    //! QPlainTextEdit has each block as its
    //!       own line.
    //! \return Block number that cursor is currently highlighted on.
    int getCursorBlockNumber();

    //! \brief This function handles highlighting a specific line.
    //! \param linesToAdd: lines to highlight
    //! \param linesToRemove: lines to unhighlight
    //! \param shouldCenter: whether or not to center text widget on added lines
    void highlightLine(QList<int> linesToAdd, QList<int> linesToRemove, bool shouldCenter);

    //! \brief This function resets the highlighted info after new gcode is parsed
    void resetHighlight();

    //! \brief This function forwards the font colors for all gcode text as
    //! determined during the file parse
    //! \param fontColors: keys are the lines of gcode text while values are the font color
    //! for that line (used by gcode highlighter)
    //! \param layerSkipLineNumbers: line numbers to skip highlighting if visualization reduction setting is enabled
    void setHighlighterColors(QHash<QString, QTextCharFormat> fontColors, QSet<int> layerSkipLineNumbers);

    //! \brief update layer beginning line numbers for all layers
    //! \param firstLineNumbers a vector containing first line number for all layers
    void setLayerFirstLineNumbers(QList<int>& firstLineNumbers);

    //! \brief move the cursor to the specific line and put it on top if necessary
    //! \param line_number: the line number where the cursor is moved to
    void moveCursorToLine(int line_number);

    //! \brief get the boolean that indicates if the cursor move is manual or automatic
    //! \return a boolean indicateing if the cursor move is manual or automatic
    bool getCursorManualMove();

    //! \brief set the boolean of manual cursor move back to false
    void setCursorManualMoveFalse();

    //! \brief function to search GCode
    //! \param searchString: the search string
    //! \param searchCount: the number of Enter/Return hits for the same search string
    void search(QString searchString, int searchCount);

  signals:
    //! \brief Signal to indicate a text line was highlighted/unhighlighted
    //! \param linesToAdd: Segments to highlight
    //! \param linesToRemove: Segments to unhighlight
    void lineChange(QList<int> linesToAdd, QList<int> linesToRemove);

  protected:
    //! \brief Overrides for resizing, mouse, and keys
    void resizeEvent(QResizeEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    //! \brief This function overrides for the up for and down arrows
    virtual void keyPressEvent(QKeyEvent* event) override;

  private slots:
    //! \brief Update display area for text
    void updateLineNumberDisplayAreaWidth(int blockCount);
    void updateLineNumberDisplayArea(const QRect& rect, int height);

  private:
    //! \brief Widget and highlight information
    QWidget* m_line_number_display_area;
    GcodeHighlighter m_highlighter;
    QTextBlock m_highlighted_block;
    int m_previous_line;

    //! \brief store "BEGINNING LAYER" line numbers for all layers
    QList<int> m_layer_first_line_numbers;

    //! \brief indicates if the cursor move is manual such as click or up/down arrow, or auto by layer spinning box
    bool m_manual_cursor_move;

    //! \brief saved search string
    QString m_search_string;

    //! \brief list of matched lines for the last search
    QList<int> m_matched_lines;

    //! \brief cursor index in list of all matched lines
    int m_cursor_index;

    //! \brief Blocks (lines) with highlights
    QSet<int> m_selected_blocks;

    //! \brief Blocks as of last resize
    int m_last_block_count;

    //! \brief Last block clicked on
    int m_last_block_clicked_on;
};
} // namespace ORNL
