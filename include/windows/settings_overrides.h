#pragma once

#include <QDialog>
#include <QUuid>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;

namespace ORNL {
//! \brief Read-only overview of part and layer overrides in the current project.
class SettingsOverridesDialog : public QDialog {
    Q_OBJECT
   public:
    explicit SettingsOverridesDialog(QWidget* parent = nullptr);

    //! \brief Export the filtered table in its current order and displayed units.
    bool exportCsv(const QString& path, QString* error = nullptr) const;

   signals:
    //! \brief Reveal a setting in its owning part or layer range; negative bounds mean part settings.
    void showSettingRequested(QUuid part_id, int low, int high, QString key);

   public slots:
    //! \brief Rebuild the overview from the current project settings.
    void refresh();

    //! \brief Coalesce project changes into a refresh while the overview is visible.
    void scheduleRefresh();

    //! \brief Defer scans while a project is being loaded on another thread.
    void setRefreshSuspended(bool suspended);

    //! \brief Copy selected visible rows, including headers, as tab-separated text.
    void copySelectedRows();

   protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    void filterRows();
    void updateNavigation();
    void showSelectedSetting();
    void chooseCsvFile();
    QString tableText(QChar delimiter, bool selected_only) const;

    QTableWidget* m_table;
    QLineEdit* m_filter;
    QComboBox* m_part_filter;
    QComboBox* m_scope_filter;
    QLabel* m_summary;
    QPushButton* m_refresh_button;
    QPushButton* m_show_setting_button;
    QPushButton* m_copy_button;
    QPushButton* m_export_button;
    QTimer* m_refresh_timer;
    bool m_refresh_suspended = false;
    bool m_columns_sized     = false;
};
}  // namespace ORNL
