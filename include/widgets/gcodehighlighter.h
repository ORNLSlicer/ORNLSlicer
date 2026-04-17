#pragma once

#include <QSyntaxHighlighter>
#include <qhash.h>
#include <qobject.h>
#include <qset.h>
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
    //! \param layerSkipLineNumbers: lines to skip coloring even if a match is found in the hash
    //! due to visualization reduction settings being enabled
    void setColorRules(QHash<QString, QTextCharFormat> colorHash, QSet<int> layerSkipLineNumbers);

  protected:
    //! \brief Line highlight override
    void highlightBlock(const QString& text) override;

  private:
    //! \brief Hash holding gcode line as key with associated format
    QHash<QString, QTextCharFormat> m_color_hash;

    //! \brief Set of lines to skip even if found in hash
    QSet<int> m_layer_skip_numbers;
};
} // namespace ORNL
