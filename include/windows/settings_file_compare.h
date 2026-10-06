#pragma once

#include <QDialog>

#include "configs/settings_file_comparator.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace ORNL {

/*! \brief Window for selecting and comparing two ORNLSlicer settings files. */
class SettingsFileCompareDialog : public QDialog {
    Q_OBJECT

   public:
    explicit SettingsFileCompareDialog(QWidget* parent = nullptr);

   private slots:
    void browseFirstFile();
    void browseSecondFile();
    void compareSelectedFiles();
    void refreshCompareState();

   private:
    void browseForFile(QLineEdit* target, const QString& title);
    void displayResult(const SettingsFileComparator::Result& result);
    void clearResult();

    QString displayName(const QString& key) const;
    QString fileLabel(const QString& path) const;

    QLineEdit* m_first_file_edit;
    QLineEdit* m_second_file_edit;
    QPushButton* m_compare_button;
    QLabel* m_summary_label;
    QTabWidget* m_result_tabs;
    QTableWidget* m_changed_table;
    QTableWidget* m_unique_table;
};

}  // namespace ORNL
