#pragma once

#include <QTreeView>
#include <qabstractitemmodel.h>

#include "value_tree_model.h"

namespace xequation
{
namespace gui
{
class ValueTreeView : public QTreeView
{
    Q_OBJECT
  public:
    explicit ValueTreeView(QWidget *parent = nullptr);
    ~ValueTreeView() override;
    void SetValueModel(ValueTreeModel* model);
    ValueTreeModel *value_model() const { return value_model_; }
    void SetHeaderSectionResizeRatio(int idx, double ratio);
  protected:
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

    // make it private
    void setModel(QAbstractItemModel *model) override;

    void OnHeaderResized(int logical_index, int old_size, int new_size);
    void OnVerticalScrollbarValueChanged(int value);
    void FetchMoreIfNeeded();
  private:
    void SetupUI();
    void SetupConnections();
    void ResizeHeaderSections();
  private:
    ValueTreeModel* value_model_;
    QMap<int, double> header_section_resize_ratios_;
    bool header_interactive_dragging_ = false;
};
}
}