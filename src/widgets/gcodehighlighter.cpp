#include "widgets/gcodehighlighter.h"

#include <qhash.h>
#include <qobject.h>
#include <qsyntaxhighlighter.h>
#include <qtextformat.h>

namespace ORNL {
GcodeHighlighter::GcodeHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {}

void GcodeHighlighter::setColorRules(QHash<QString, QTextCharFormat> colorHash) { m_color_hash = colorHash; }

void GcodeHighlighter::highlightBlock(const QString& text) { setFormat(0, text.length(), m_color_hash[text]); }

} // namespace ORNL
