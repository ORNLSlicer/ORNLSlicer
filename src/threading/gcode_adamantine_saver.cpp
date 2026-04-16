#include "threading/gcode_adamantine_saver.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QStringList>
#include <QTextStream>
#include <qcontainerfwd.h>

#include "gcode/gcode_meta.h"

namespace ORNL {
GCodeAdamantineSaver::GCodeAdamantineSaver(QString tempLocation, QString path, QString filename, QString text,
                                           GcodeMeta meta)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeAdamantineSaver::run() {
    // I want to remove all the text in this file contained in parentheses.
    QRegularExpression comments("((\\(.*?\\)))\n");
    // I want to remove double & triple returns
    QRegularExpression tripleReturns("\n\n\n");
    QRegularExpression doubleReturns("\n\n");

    QFile tempFile(m_temp_location % "temp");
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&tempFile);
        // modify m_text to remove comments and triple returns
        m_text.remove(comments)
            .remove(tripleReturns)
            .replace(doubleReturns, "\n"); //.remove(tripleReturns).remove(doubleReturns);
        // move the last two lines to the top of the file
        QStringList lines = m_text.split("\n");
        lines.removeFirst();
        lines.removeLast();
        QString lastLine = lines.last();
        lines.removeLast();
        QString secondToLastLine = lines.last();
        lines.removeLast();
        lines.prepend(lastLine);
        lines.prepend(secondToLastLine);
        // write text to out file
        for (QString line : lines) {
            out << line << "\n";
        }
        // close file
        tempFile.close();
        QFile::rename(tempFile.fileName(), m_filename);
    }
}

} // namespace ORNL
