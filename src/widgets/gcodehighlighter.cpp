#include "widgets/gcodehighlighter.h"

#include <qhash.h>
#include <qobject.h>
#include <qset.h>
#include <qsyntaxhighlighter.h>
#include <qtextformat.h>

namespace ORNL {
GcodeHighlighter::GcodeHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {}

void GcodeHighlighter::setColorRules(QHash<QString, QTextCharFormat> colorHash, QSet<int> layerSkipLineNumbers) {
    m_color_hash = colorHash;
    m_layer_skip_numbers = layerSkipLineNumbers;
}

void GcodeHighlighter::highlightBlock(const QString& text) {
    if (!m_layer_skip_numbers.contains(this->currentBlock().blockNumber()))
        setFormat(0, text.length(), m_color_hash[text]);
}

} // namespace ORNL
