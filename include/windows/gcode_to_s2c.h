#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <optional>

#include <qcheckbox.h>
#include <qdialogbuttonbox.h>
#include <qgridlayout.h>
#include <qpushbutton.h>
#include <qtmetamacros.h>

#include "utilities/qt_json_conversion.h"

namespace ORNL {

class GcodeToS2CDialog : public QDialog {
    Q_OBJECT
   public:
    explicit GcodeToS2CDialog(QWidget* parent = nullptr);

    QString outputFilePath() const;

   private slots:
    void browseGcodeFile();
    void browseOutputFile();
    void refreshAcceptState();
    void accept() override;

   private:
    void setupUi();
    void setDefaultOutputPath();
    std::optional<fifojson> promptForMissingValue(const QString& key, const fifojson& master_entry);

    QGridLayout* m_layout;
    QLineEdit* m_gcode_edit;
    QLineEdit* m_output_edit;
    QPushButton* m_gcode_browse;
    QPushButton* m_output_browse;
    QCheckBox* m_use_defaults;
    QLabel* m_status_label;
    QDialogButtonBox* m_buttons;
};

}  // namespace ORNL
