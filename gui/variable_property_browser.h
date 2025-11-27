#pragma once
#include "qtpropertybrowser.h"

// Modified from QtPropertyBrowser
// Support three columns: Name, Value, Type
// Enable Name column editing
// Allow user customization of header section resize ratios

class QTreeWidgetItem;

namespace xequation
{
namespace gui
{
class VariablePropertyBrowserPrivate;

class VariablePropertyBrowser : public QtAbstractPropertyBrowser
{
    Q_OBJECT
    Q_PROPERTY(int indentation READ indentation WRITE setIndentation)
    Q_PROPERTY(bool rootIsDecorated READ rootIsDecorated WRITE setRootIsDecorated)
    Q_PROPERTY(bool alternatingRowColors READ alternatingRowColors WRITE setAlternatingRowColors)
    Q_PROPERTY(bool headerVisible READ isHeaderVisible WRITE setHeaderVisible)
    Q_PROPERTY(bool propertiesWithoutValueMarked READ propertiesWithoutValueMarked WRITE setPropertiesWithoutValueMarked
    )
  public:
    VariablePropertyBrowser(QWidget *parent = 0);
    ~VariablePropertyBrowser();

    int indentation() const;
    void setIndentation(int i);

    bool rootIsDecorated() const;
    void setRootIsDecorated(bool show);

    bool alternatingRowColors() const;
    void setAlternatingRowColors(bool enable);

    bool isHeaderVisible() const;
    void setHeaderVisible(bool visible);

    QMap<int, double> headerSectionResizeRatios() const;
    void setHeaderSectionResizeRatios(const QMap<int, double> &ratios);
    void setHeaderSectionResizeRatios(const QVector<double> &ratios);
    void setHeaderSectionResizeRatios(const QList<double> &ratios);
    void setHeaderSectionResizeRatio(int section, double ratio);

    void setExpanded(QtBrowserItem *item, bool expanded);
    bool isExpanded(QtBrowserItem *item) const;

    bool isItemVisible(QtBrowserItem *item) const;
    void setItemVisible(QtBrowserItem *item, bool visible);

    void setBackgroundColor(QtBrowserItem *item, const QColor &color);
    QColor backgroundColor(QtBrowserItem *item) const;
    QColor calculatedBackgroundColor(QtBrowserItem *item) const;

    void setPropertiesWithoutValueMarked(bool mark);
    bool propertiesWithoutValueMarked() const;

    void editItem(QtBrowserItem *item);

    void expandAll();
    void collapseAll();
    
  Q_SIGNALS:

    void collapsed(QtBrowserItem *item);
    void expanded(QtBrowserItem *item);

  protected:
    virtual void itemInserted(QtBrowserItem *item, QtBrowserItem *afterItem);
    virtual void itemRemoved(QtBrowserItem *item);
    virtual void itemChanged(QtBrowserItem *item);

    virtual void resizeEvent(QResizeEvent* event);
    virtual void showEvent(QShowEvent* event);

    virtual bool eventFilter(QObject *obj, QEvent *event);
  private:
    QScopedPointer<VariablePropertyBrowserPrivate> d_ptr;
    Q_DECLARE_PRIVATE(VariablePropertyBrowser)
    Q_DISABLE_COPY_MOVE(VariablePropertyBrowser)

    Q_PRIVATE_SLOT(d_func(), void slotCollapsed(const QModelIndex &))
    Q_PRIVATE_SLOT(d_func(), void slotExpanded(const QModelIndex &))
    Q_PRIVATE_SLOT(d_func(), void slotCurrentBrowserItemChanged(QtBrowserItem *))
    Q_PRIVATE_SLOT(d_func(), void slotCurrentTreeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *))
    Q_PRIVATE_SLOT(d_func(), void slotHeaderResized(int, int, int))
};
} // namespace gui
} // namespace xequation