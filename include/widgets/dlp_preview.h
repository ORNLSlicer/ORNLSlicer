#pragma once

#include <QSharedPointer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPushButton;

namespace ORNL {
struct DlpBuild;

//! \brief Displays completed DLP images and the lane boundaries used for export.
class DlpPreview : public QWidget {
    Q_OBJECT
   public:
    explicit DlpPreview(QWidget* parent = nullptr);

    void setBuild(QSharedPointer<const DlpBuild> build);
    void clear();

   signals:
    void exportRequested();

   private:
    void updateImage();

    QSharedPointer<const DlpBuild> m_build;
    QComboBox* m_layer_selector;
    QComboBox* m_lane_selector;
    QCheckBox* m_lane_borders;
    QPushButton* m_export;
    QLabel* m_status;
    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
};
}  // namespace ORNL
