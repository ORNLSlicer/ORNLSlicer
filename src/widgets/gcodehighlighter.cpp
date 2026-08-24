#include "widgets/gcodehighlighter.h"

#include <qhash.h>
#include <qobject.h>
#include <qsyntaxhighlighter.h>
#include <qtextformat.h>

namespace ORNL {
GcodeHighlighter::GcodeHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {}

void GcodeHighlighter::setColorRules(QHash<QString, QTextCharFormat> colorHash) {
    m_color_hash = colorHash;
}

void GcodeHighlighter::highlightBlock(const QString& text) {
    const auto color = m_color_hash.constFind(text);
    if (color != m_color_hash.constEnd()) { setFormat(0, text.length(), color.value()); }
}

}  // namespace ORNL
