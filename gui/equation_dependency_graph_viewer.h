#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWidget>
#include <QGraphicsSvgItem>

#include "core/equation_group.h"

namespace xequation
{
namespace gui
{
class EquationDependencyGraphViewer : public QWidget
{
    Q_OBJECT
  public:
    explicit EquationDependencyGraphViewer(QWidget *parent = nullptr);
    ~EquationDependencyGraphViewer() override;
    void OnEquationGroupAdded(const EquationGroup *group);
    void OnEquationGroupUpdated(const EquationGroup *group, bitmask::bitmask<EquationGroupUpdateFlag> update_flags);
    void OnEquationGroupRemoving(const EquationGroup *group);
    void OnDependencyGraphImageGenerated(const QString &image_path);
    qreal GetMaxScale() const;
    qreal GetMinScale() const;

  signals:
    void DependencyGraphImageRequested();

  protected:
    void SetupUI();
    void SetupConnections();

    void wheelEvent(QWheelEvent *event) override;

    void ZoomIn();
    void ZoomOut();
    void ResetZoom();
    void FitToView();
    void SaveImage();
    void resizeEvent(QResizeEvent *event) override;

  private:
    QGraphicsScene *scene_;
    QGraphicsView *view_;
    QGraphicsSvgItem *svg_item_;
    QString current_image_path_;
};
} // namespace gui
} // namespace xequation