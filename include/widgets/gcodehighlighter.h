#pragma once

#include <QSyntaxHighlighter>
#include <qhash.h>
#include <qobject.h>
#include <qtmetamacros.h>

class QTextDocument;

namespace ORNL {
//! \class GcodeHighLighter
//! \brief This class overrides typical highlighter to highlight text in specific way
class GcodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
  public:
    //! \brief Default Constructor
    //! \param parent: parent text document to which highglighting will be applied
    GcodeHighlighter(QTextDocument* parent);

    //! \brief Sets rules for coloring
    //! \param colorHash: Hash of individual lines and associated color based on gcode comments
    void setColorRules(QHash<QString, QTextCharFormat> colorHash);

  protected:
    //! \brief Line highlight override
    void highlightBlock(const QString& text) override;

  private:
    //! \brief Hash holding gcode line as key with associated format
    QHash<QString, QTextCharFormat> m_color_hash;
};
} // namespace ORNL
