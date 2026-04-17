#include "widgets/cmd_widget.h"

#include <qgridlayout.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qtextedit.h>
#include <qwidget.h>

namespace ORNL {

CmdWidget::CmdWidget(QWidget* parent) : QWidget(parent) { this->setupWidget(); }

void CmdWidget::operator<<(QString str) { this->append(str); }

void CmdWidget::print(QString str) { this->append(str); }

void CmdWidget::runCmd(QString cmd) {
    if (cmd.isEmpty())
        return;
    this->append(cmd);
}

void CmdWidget::append(QString str) {
    // Use invokeMethod to make output thread safe.
    QMetaObject::invokeMethod(m_output, "append", Qt::QueuedConnection, Q_ARG(QString, str));
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

/*void CmdWidget::fetchFromCmdEdit() {
    QString cmd = m_input->text();
    m_input->clear();
    this->runCmd(cmd);
}*/

void CmdWidget::setupWidget() {
    this->setupSubWidgets();
    this->setupLayout();
    this->setupInsert();
    this->setupEvents();
}

void CmdWidget::setupSubWidgets() {
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);

    // m_input = new QLineEdit(this);
    // m_input->setPlaceholderText("Input Command...");

    // m_accept = new QToolButton(this);
    // m_accept->setIcon(QIcon(":/icons/forward_flat.png"));
    // m_accept->setToolTip("Submit");
}

void CmdWidget::setupLayout() { m_layout = new QGridLayout(this); }

void CmdWidget::setupInsert() {
    m_layout->addWidget(m_output, 0, 0, 1, 2);
    // m_layout->addWidget(m_input, 1, 0, 1, 1);
    // m_layout->addWidget(m_accept, 1, 1, 1, 1);
}

void CmdWidget::setupEvents() {
    // connect(m_input, &QLineEdit::returnPressed, this, &CmdWidget::fetchFromCmdEdit);
    // connect(m_accept, &QToolButton::pressed, this, &CmdWidget::fetchFromCmdEdit);
}

} // namespace ORNL
