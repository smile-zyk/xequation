#include "variable_property_browser.h"
#include "variable_property_manager.h"
#include <QDebug>
#include <QtCore/QSet>
#include <QtGui/QFocusEvent>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QItemDelegate>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTreeWidget>

namespace xequation
{
namespace gui
{
class VariablePropertyBrowserView;
class VariablePropertyBrowserPrivate
{
    VariablePropertyBrowser *q_ptr;
    Q_DECLARE_PUBLIC(VariablePropertyBrowser)

  public:
    VariablePropertyBrowserPrivate();
    void init(QWidget *parent);

    void propertyInserted(QtBrowserItem *index, QtBrowserItem *afterIndex);
    void propertyRemoved(QtBrowserItem *index);
    void propertyChanged(QtBrowserItem *index);
    QWidget *createEditor(VariableProperty *property, QWidget *parent) const
    {
        return q_ptr->createEditor(property, parent);
    }
    VariableProperty *indexToProperty(const QModelIndex &index) const;
    QTreeWidgetItem *indexToItem(const QModelIndex &index) const;
    QtBrowserItem *indexToBrowserItem(const QModelIndex &index) const;
    bool lastColumn(int column) const;
    void disableItem(QTreeWidgetItem *item) const;
    void enableItem(QTreeWidgetItem *item) const;
    bool hasValue(QTreeWidgetItem *item) const;

    void slotCollapsed(const QModelIndex &index);
    void slotExpanded(const QModelIndex &index);

    QColor calculatedBackgroundColor(QtBrowserItem *item) const;

    void resizeHeaderSections();

    void slotHeaderResized(int logicalIndex, int oldSize, int newSize);

    VariablePropertyBrowserView *treeWidget() const
    {
        return m_treeWidget;
    }
    bool markPropertiesWithoutValue() const
    {
        return m_markPropertiesWithoutValue;
    }

    QtBrowserItem *currentItem() const;
    void setCurrentItem(QtBrowserItem *browserItem, bool block);
    void editItem(QtBrowserItem *browserItem);

    void slotCurrentBrowserItemChanged(QtBrowserItem *item);
    void slotCurrentTreeItemChanged(QTreeWidgetItem *newItem, QTreeWidgetItem *);

    QTreeWidgetItem *editedItem() const;

  private:
    void updateItem(QTreeWidgetItem *item);

    QMap<QtBrowserItem *, QTreeWidgetItem *> m_indexToItem;
    QMap<QTreeWidgetItem *, QtBrowserItem *> m_itemToIndex;

    QMap<int, double> m_sectionResizeRatios;

    QMap<QtBrowserItem *, QColor> m_indexToBackgroundColor;

    VariablePropertyBrowserView *m_treeWidget;

    bool m_headerVisible;
    bool m_headerInteractiveDragging = false;
    class VariablePropertyBrowserDelegate *m_delegate;
    bool m_markPropertiesWithoutValue;
    bool m_browserChangedBlocked;
    QIcon m_expandIcon;
};

// ------------ VariablePropertyBrowserView
class VariablePropertyBrowserView : public QTreeWidget
{
    Q_OBJECT
  public:
    VariablePropertyBrowserView(QWidget *parent = 0);
    void setEditorPrivate(VariablePropertyBrowserPrivate *editorPrivate)
    {
        m_editorPrivate = editorPrivate;
    }

    QTreeWidgetItem *indexToItem(const QModelIndex &index) const
    {
        return itemFromIndex(index);
    }

  protected:
    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

  private:
    VariablePropertyBrowserPrivate *m_editorPrivate;
};

VariablePropertyBrowserView::VariablePropertyBrowserView(QWidget *parent) : QTreeWidget(parent), m_editorPrivate(0)
{
    connect(header(), SIGNAL(sectionDoubleClicked(int)), this, SLOT(resizeColumnToContents(int)));
}

void VariablePropertyBrowserView::drawRow(
    QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index
) const
{
    QStyleOptionViewItem opt = option;
    bool hasValue = true;
    if (m_editorPrivate)
    {
        VariableProperty *property = m_editorPrivate->indexToProperty(index);
        if (property)
            hasValue = property->hasValue();
    }
    if (!hasValue && m_editorPrivate->markPropertiesWithoutValue())
    {
        const QColor c = option.palette.color(QPalette::Dark);
        painter->fillRect(option.rect, c);
        opt.palette.setColor(QPalette::AlternateBase, c);
    }
    else
    {
        const QColor c = m_editorPrivate->calculatedBackgroundColor(m_editorPrivate->indexToBrowserItem(index));
        if (c.isValid())
        {
            painter->fillRect(option.rect, c);
            opt.palette.setColor(QPalette::AlternateBase, c.lighter(112));
        }
    }
    QTreeWidget::drawRow(painter, opt, index);
    QColor color = static_cast<QRgb>(QApplication::style()->styleHint(QStyle::SH_Table_GridLineColor, &opt));
    painter->save();
    painter->setPen(QPen(color));
    painter->drawLine(opt.rect.x(), opt.rect.bottom(), opt.rect.right(), opt.rect.bottom());
    painter->restore();
}

void VariablePropertyBrowserView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: // Trigger Edit
        if (!m_editorPrivate->editedItem())
            if (const QTreeWidgetItem *item = currentItem())
                if (item->columnCount() >= 2 && ((item->flags() & (Qt::ItemIsEditable | Qt::ItemIsEnabled)) ==
                                                 (Qt::ItemIsEditable | Qt::ItemIsEnabled)))
                {
                    event->accept();
                    // If the current position is at column 0, move to 1.
                    QModelIndex index = currentIndex();
                    if (index.column() == 0)
                    {
                        index = index.sibling(index.row(), 1);
                        setCurrentIndex(index);
                    }
                    edit(index);
                    return;
                }
        break;
    default:
        break;
    }
    QTreeWidget::keyPressEvent(event);
}

void VariablePropertyBrowserView::mousePressEvent(QMouseEvent *event)
{
    QTreeWidget::mousePressEvent(event);
    QTreeWidgetItem *item = itemAt(event->pos());

    if (item)
    {
        if ((item != m_editorPrivate->editedItem()) && (event->button() == Qt::LeftButton) &&
            (header()->logicalIndexAt(event->pos().x()) == 1) &&
            ((item->flags() & (Qt::ItemIsEditable | Qt::ItemIsEnabled)) == (Qt::ItemIsEditable | Qt::ItemIsEnabled)))
        {
            editItem(item, 1);
        }
        else if (!m_editorPrivate->hasValue(item) && m_editorPrivate->markPropertiesWithoutValue() && !rootIsDecorated())
        {
            if (event->pos().x() + header()->offset() < 20)
                item->setExpanded(!item->isExpanded());
        }
    }
}

// ------------ VariablePropertyBrowserDelegate
class VariablePropertyBrowserDelegate : public QItemDelegate
{
    Q_OBJECT
  public:
    VariablePropertyBrowserDelegate(QObject *parent = 0)
        : QItemDelegate(parent), m_editorPrivate(0), m_editedItem(0), m_editedWidget(0)
    {
    }

    void setEditorPrivate(VariablePropertyBrowserPrivate *editorPrivate)
    {
        m_editorPrivate = editorPrivate;
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    void setModelData(QWidget *, QAbstractItemModel *, const QModelIndex &) const {}

    void setEditorData(QWidget *, const QModelIndex &) const {}

    bool eventFilter(QObject *object, QEvent *event);
    void closeEditor(VariableProperty *property);

    QTreeWidgetItem *editedItem() const
    {
        return m_editedItem;
    }

  private slots:
    void slotEditorDestroyed(QObject *object);

  private:
    int indentation(const QModelIndex &index) const;

    typedef QMap<QWidget *, VariableProperty *> EditorToPropertyMap;
    mutable EditorToPropertyMap m_editorToProperty;

    typedef QMap<VariableProperty *, QWidget *> PropertyToEditorMap;
    mutable PropertyToEditorMap m_propertyToEditor;
    VariablePropertyBrowserPrivate *m_editorPrivate;
    mutable QTreeWidgetItem *m_editedItem;
    mutable QWidget *m_editedWidget;
};

int VariablePropertyBrowserDelegate::indentation(const QModelIndex &index) const
{
    if (!m_editorPrivate)
        return 0;

    QTreeWidgetItem *item = m_editorPrivate->indexToItem(index);
    int indent = 0;
    while (item->parent())
    {
        item = item->parent();
        ++indent;
    }
    if (m_editorPrivate->treeWidget()->rootIsDecorated())
        ++indent;
    return indent * m_editorPrivate->treeWidget()->indentation();
}

void VariablePropertyBrowserDelegate::slotEditorDestroyed(QObject *object)
{
    if (QWidget *w = qobject_cast<QWidget *>(object))
    {
        const EditorToPropertyMap::iterator it = m_editorToProperty.find(w);
        if (it != m_editorToProperty.end())
        {
            m_propertyToEditor.remove(it.value());
            m_editorToProperty.erase(it);
        }
        if (m_editedWidget == w)
        {
            m_editedWidget = 0;
            m_editedItem = 0;
        }
    }
}

void VariablePropertyBrowserDelegate::closeEditor(VariableProperty *property)
{
    if (QWidget *w = m_propertyToEditor.value(property, 0))
        w->deleteLater();
}

QWidget *VariablePropertyBrowserDelegate::createEditor(
    QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index
) const
{
    if (index.column() == 1 && m_editorPrivate)
    {
        VariableProperty *property = m_editorPrivate->indexToProperty(index);
        QTreeWidgetItem *item = m_editorPrivate->indexToItem(index);
        if (property && item && (item->flags() & Qt::ItemIsEnabled))
        {
            QWidget *editor = m_editorPrivate->createEditor(property, parent);
            if (editor)
            {
                editor->setAutoFillBackground(true);
                editor->installEventFilter(const_cast<VariablePropertyBrowserDelegate *>(this));
                connect(editor, SIGNAL(destroyed(QObject *)), this, SLOT(slotEditorDestroyed(QObject *)));
                m_propertyToEditor[property] = editor;
                m_editorToProperty[editor] = property;
                m_editedItem = item;
                m_editedWidget = editor;
            }
            return editor;
        }
    }
    return 0;
}

void VariablePropertyBrowserDelegate::updateEditorGeometry(
    QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index
) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect.adjusted(0, 0, 0, -1));
}

void VariablePropertyBrowserDelegate::paint(
    QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index
) const
{
    bool hasValue = true;
    if (m_editorPrivate)
    {
        VariableProperty *property = m_editorPrivate->indexToProperty(index);
        if (property)
            hasValue = property->hasValue();
    }
    QStyleOptionViewItem opt = option;
    if ((m_editorPrivate && index.column() == 0) || !hasValue)
    {
        VariableProperty *property = m_editorPrivate->indexToProperty(index);
        if (property && property->isModified())
        {
            opt.font.setBold(true);
            opt.fontMetrics = QFontMetrics(opt.font);
        }
    }
    QColor c;
    if (!hasValue && m_editorPrivate->markPropertiesWithoutValue())
    {
        c = opt.palette.color(QPalette::Dark);
        opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::BrightText));
    }
    else
    {
        c = m_editorPrivate->calculatedBackgroundColor(m_editorPrivate->indexToBrowserItem(index));
        if (c.isValid() && (opt.features & QStyleOptionViewItem::Alternate))
            c = c.lighter(112);
    }
    if (c.isValid())
        painter->fillRect(option.rect, c);
    opt.state &= ~QStyle::State_HasFocus;
    QItemDelegate::paint(painter, opt, index);

    opt.palette.setCurrentColorGroup(QPalette::Active);
    QColor color = static_cast<QRgb>(QApplication::style()->styleHint(QStyle::SH_Table_GridLineColor, &opt));
    painter->save();
    painter->setPen(QPen(color));
    if (!m_editorPrivate || (!m_editorPrivate->lastColumn(index.column()) && hasValue))
    {
        int right = (option.direction == Qt::LeftToRight) ? option.rect.right() : option.rect.left();
        painter->drawLine(right, option.rect.y(), right, option.rect.bottom());
    }
    painter->restore();
}

QSize VariablePropertyBrowserDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QItemDelegate::sizeHint(option, index) + QSize(3, 4);
}

bool VariablePropertyBrowserDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusOut)
    {
        QFocusEvent *fe = static_cast<QFocusEvent *>(event);
        if (fe->reason() == Qt::ActiveWindowFocusReason)
            return false;
    }
    return QItemDelegate::eventFilter(object, event);
}

//  -------- VariablePropertyBrowserPrivate implementation
VariablePropertyBrowserPrivate::VariablePropertyBrowserPrivate()
    : m_treeWidget(0),
      m_headerVisible(true),
      m_delegate(0),
      m_markPropertiesWithoutValue(false),
      m_browserChangedBlocked(false)
{
}

// Draw an icon indicating opened/closing branches
static QIcon drawIndicatorIcon(const QPalette &palette, QStyle *style)
{
    QPixmap pix(14, 14);
    pix.fill(Qt::transparent);
    QStyleOption branchOption;
    branchOption.rect = QRect(2, 2, 9, 9); // ### hardcoded in qcommonstyle.cpp
    branchOption.palette = palette;
    branchOption.state = QStyle::State_Children;

    QPainter p;
    // Draw closed state
    p.begin(&pix);
    style->drawPrimitive(QStyle::PE_IndicatorBranch, &branchOption, &p);
    p.end();
    QIcon rc = pix;
    rc.addPixmap(pix, QIcon::Selected, QIcon::Off);
    // Draw opened state
    branchOption.state |= QStyle::State_Open;
    pix.fill(Qt::transparent);
    p.begin(&pix);
    style->drawPrimitive(QStyle::PE_IndicatorBranch, &branchOption, &p);
    p.end();

    rc.addPixmap(pix, QIcon::Normal, QIcon::On);
    rc.addPixmap(pix, QIcon::Selected, QIcon::On);
    return rc;
}

void VariablePropertyBrowserPrivate::init(QWidget *parent)
{
    QHBoxLayout *layout = new QHBoxLayout(parent);
    layout->setContentsMargins(QMargins());
    m_treeWidget = new VariablePropertyBrowserView(parent);
    m_treeWidget->setEditorPrivate(this);
    m_treeWidget->setIconSize(QSize(18, 18));
    layout->addWidget(m_treeWidget);

    m_treeWidget->setColumnCount(3);
    QStringList labels;
    labels.append(QCoreApplication::translate("VariablePropertyBrowser", "Name"));
    labels.append(QCoreApplication::translate("VariablePropertyBrowser", "Value"));
    labels.append(QCoreApplication::translate("VariablePropertyBrowser", "Type"));
    m_treeWidget->setHeaderLabels(labels);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_delegate = new VariablePropertyBrowserDelegate(parent);
    m_delegate->setEditorPrivate(this);
    m_treeWidget->setItemDelegate(m_delegate);
    m_treeWidget->header()->setSectionsMovable(false);
    m_treeWidget->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_expandIcon = drawIndicatorIcon(q_ptr->palette(), q_ptr->style());

    QObject::connect(m_treeWidget, SIGNAL(collapsed(QModelIndex)), q_ptr, SLOT(slotCollapsed(QModelIndex)));
    QObject::connect(m_treeWidget, SIGNAL(expanded(QModelIndex)), q_ptr, SLOT(slotExpanded(QModelIndex)));
    QObject::connect(
        m_treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), q_ptr,
        SLOT(slotCurrentTreeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *))
    );
    QObject::connect(
        m_treeWidget->header(), SIGNAL(sectionResized(int, int, int)), q_ptr, SLOT(slotHeaderResized(int, int, int))
    );
}

QtBrowserItem *VariablePropertyBrowserPrivate::currentItem() const
{
    if (QTreeWidgetItem *treeItem = m_treeWidget->currentItem())
        return m_itemToIndex.value(treeItem);
    return 0;
}

void VariablePropertyBrowserPrivate::setCurrentItem(QtBrowserItem *browserItem, bool block)
{
    const bool blocked = block ? m_treeWidget->blockSignals(true) : false;
    if (browserItem == 0)
        m_treeWidget->setCurrentItem(0);
    else
        m_treeWidget->setCurrentItem(m_indexToItem.value(browserItem));
    if (block)
        m_treeWidget->blockSignals(blocked);
}

VariableProperty *VariablePropertyBrowserPrivate::indexToProperty(const QModelIndex &index) const
{
    QTreeWidgetItem *item = m_treeWidget->indexToItem(index);
    QtBrowserItem *idx = m_itemToIndex.value(item);
    if (idx)
        return idx->property();
    return 0;
}

QtBrowserItem *VariablePropertyBrowserPrivate::indexToBrowserItem(const QModelIndex &index) const
{
    QTreeWidgetItem *item = m_treeWidget->indexToItem(index);
    return m_itemToIndex.value(item);
}

QTreeWidgetItem *VariablePropertyBrowserPrivate::indexToItem(const QModelIndex &index) const
{
    return m_treeWidget->indexToItem(index);
}

bool VariablePropertyBrowserPrivate::lastColumn(int column) const
{
    return m_treeWidget->header()->visualIndex(column) == m_treeWidget->columnCount() - 1;
}

void VariablePropertyBrowserPrivate::disableItem(QTreeWidgetItem *item) const
{
    Qt::ItemFlags flags = item->flags();
    if (flags & Qt::ItemIsEnabled)
    {
        flags &= ~Qt::ItemIsEnabled;
        item->setFlags(flags);
        m_delegate->closeEditor(m_itemToIndex[item]->property());
        const int childCount = item->childCount();
        for (int i = 0; i < childCount; i++)
        {
            QTreeWidgetItem *child = item->child(i);
            disableItem(child);
        }
    }
}

void VariablePropertyBrowserPrivate::enableItem(QTreeWidgetItem *item) const
{
    Qt::ItemFlags flags = item->flags();
    flags |= Qt::ItemIsEnabled;
    item->setFlags(flags);
    const int childCount = item->childCount();
    for (int i = 0; i < childCount; i++)
    {
        QTreeWidgetItem *child = item->child(i);
        VariableProperty *property = m_itemToIndex[child]->property();
        if (property->isEnabled())
        {
            enableItem(child);
        }
    }
}

bool VariablePropertyBrowserPrivate::hasValue(QTreeWidgetItem *item) const
{
    QtBrowserItem *browserItem = m_itemToIndex.value(item);
    if (browserItem)
        return browserItem->property()->hasValue();
    return false;
}

void VariablePropertyBrowserPrivate::propertyInserted(QtBrowserItem *index, QtBrowserItem *afterIndex)
{
    QTreeWidgetItem *afterItem = m_indexToItem.value(afterIndex);
    QTreeWidgetItem *parentItem = m_indexToItem.value(index->parent());

    QTreeWidgetItem *newItem = 0;
    if (parentItem)
    {
        newItem = new QTreeWidgetItem(parentItem, afterItem);
    }
    else
    {
        newItem = new QTreeWidgetItem(m_treeWidget, afterItem);
    }
    m_itemToIndex[newItem] = index;
    m_indexToItem[index] = newItem;

    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
    newItem->setExpanded(true);

    updateItem(newItem);
}

void VariablePropertyBrowserPrivate::propertyRemoved(QtBrowserItem *index)
{
    QTreeWidgetItem *item = m_indexToItem.value(index);

    if (m_treeWidget->currentItem() == item)
    {
        m_treeWidget->setCurrentItem(0);
    }

    delete item;

    m_indexToItem.remove(index);
    m_itemToIndex.remove(item);
    m_indexToBackgroundColor.remove(index);
}

void VariablePropertyBrowserPrivate::propertyChanged(QtBrowserItem *index)
{
    QTreeWidgetItem *item = m_indexToItem.value(index);

    updateItem(item);
}

void VariablePropertyBrowserPrivate::updateItem(QTreeWidgetItem *item)
{
    VariableProperty *property = m_itemToIndex[item]->property();
    QIcon expandIcon;
    if (property->hasValue())
    {
        const QString valueToolTip = property->valueToolTip();
        const QString valueText = property->valueText();
        item->setToolTip(1, valueToolTip.isEmpty() ? valueText : valueToolTip);
        item->setIcon(1, property->valueIcon());
        item->setText(1, valueText);
    }
    else if (markPropertiesWithoutValue() && !m_treeWidget->rootIsDecorated())
    {
        expandIcon = m_expandIcon;
    }
    if (dynamic_cast<VariablePropertyManager *>(property->propertyManager()))
    {
        VariablePropertyManager *manager = static_cast<VariablePropertyManager *>(property->propertyManager());
        QString type = manager->type(property);
        item->setToolTip(2, type);
        item->setText(2, type);
    }
    item->setIcon(0, expandIcon);
    item->setFirstColumnSpanned(!property->hasValue());
    const QString descriptionToolTip = property->descriptionToolTip();
    const QString propertyName = property->propertyName();
    item->setToolTip(0, descriptionToolTip.isEmpty() ? propertyName : descriptionToolTip);
    item->setStatusTip(0, property->statusTip());
    item->setWhatsThis(0, property->whatsThis());
    item->setText(0, propertyName);
    bool wasEnabled = item->flags() & Qt::ItemIsEnabled;
    bool isEnabled = wasEnabled;
    if (property->isEnabled())
    {
        QTreeWidgetItem *parent = item->parent();
        if (!parent || (parent->flags() & Qt::ItemIsEnabled))
            isEnabled = true;
        else
            isEnabled = false;
    }
    else
    {
        isEnabled = false;
    }
    if (wasEnabled != isEnabled)
    {
        if (isEnabled)
            enableItem(item);
        else
            disableItem(item);
    }

    bool wasVisible = !item->isHidden();
    bool isVisible = wasVisible;
    if (property->isVisible())
    {
        QTreeWidgetItem *parent = item->parent();
        if (!parent || !parent->isHidden())
            isVisible = true;
        else
            isVisible = false;
    }
    else
    {
        isVisible = false;
    }
    if (wasVisible != isVisible)
    {
        if (isVisible)
            item->setHidden(false);
        else
            item->setHidden(true);
    }

    m_treeWidget->viewport()->update();
}

QColor VariablePropertyBrowserPrivate::calculatedBackgroundColor(QtBrowserItem *item) const
{
    QtBrowserItem *i = item;
    const QMap<QtBrowserItem *, QColor>::const_iterator itEnd = m_indexToBackgroundColor.constEnd();
    while (i)
    {
        QMap<QtBrowserItem *, QColor>::const_iterator it = m_indexToBackgroundColor.constFind(i);
        if (it != itEnd)
            return it.value();
        i = i->parent();
    }
    return QColor();
}

void VariablePropertyBrowserPrivate::resizeHeaderSections()
{
    int totalWidth = m_treeWidget->viewport()->width();

    QMap<int, double> ratios;
    double totalRatio = 0.0;
    for (int i = 0; i < m_treeWidget->columnCount(); ++i)
    {
        ratios[i] = m_sectionResizeRatios.value(i, 1.0);
        totalRatio += ratios[i];
    }

    for (int i = 0; i < ratios.size(); ++i)
    {
        m_treeWidget->setColumnWidth(i, totalWidth * ratios[i] / totalRatio);
    }
}

void VariablePropertyBrowserPrivate::slotHeaderResized(int logicalIndex, int oldSize, int newSize)
{
    if (m_headerInteractiveDragging == false)
        return;
    int sizeDiff = newSize - oldSize;
    if (logicalIndex + 1 < m_treeWidget->header()->count())
    {
        m_treeWidget->header()->blockSignals(true);
        int nextSectionSize = m_treeWidget->header()->sectionSize(logicalIndex + 1);
        int minSize = m_treeWidget->header()->minimumSectionSize();
        if (nextSectionSize - sizeDiff > minSize)
        {
            m_treeWidget->header()->resizeSection(logicalIndex + 1, nextSectionSize - sizeDiff);
        }
        else
        {
            m_treeWidget->header()->resizeSection(logicalIndex + 1, minSize);
            m_treeWidget->header()->resizeSection(logicalIndex, oldSize + nextSectionSize - minSize);
        }
        m_treeWidget->header()->blockSignals(false);
    }

    double minSize = std::numeric_limits<double>::max();
    QMap<int, double> sizes;

    for (int i = 0; i < m_treeWidget->columnCount(); ++i)
    {
        double sectionSize = m_treeWidget->header()->sectionSize(i);
        sizes[i] = sectionSize;
        if (sectionSize < minSize)
            minSize = sectionSize;
    }

    if (minSize > 0)
    {
        for (int i = 0; i < m_treeWidget->columnCount(); ++i)
        {
            m_sectionResizeRatios[i] = sizes[i] / minSize;
        }
    }
}

void VariablePropertyBrowserPrivate::slotCollapsed(const QModelIndex &index)
{
    QTreeWidgetItem *item = indexToItem(index);
    QtBrowserItem *idx = m_itemToIndex.value(item);
    if (item)
        emit q_ptr->collapsed(idx);
}

void VariablePropertyBrowserPrivate::slotExpanded(const QModelIndex &index)
{
    QTreeWidgetItem *item = indexToItem(index);
    QtBrowserItem *idx = m_itemToIndex.value(item);
    if (item)
        emit q_ptr->expanded(idx);
}

void VariablePropertyBrowserPrivate::slotCurrentBrowserItemChanged(QtBrowserItem *item)
{
    if (!m_browserChangedBlocked && item != currentItem())
        setCurrentItem(item, true);
}

void VariablePropertyBrowserPrivate::slotCurrentTreeItemChanged(QTreeWidgetItem *newItem, QTreeWidgetItem *)
{
    QtBrowserItem *browserItem = newItem ? m_itemToIndex.value(newItem) : 0;
    m_browserChangedBlocked = true;
    q_ptr->setCurrentItem(browserItem);
    m_browserChangedBlocked = false;
}

QTreeWidgetItem *VariablePropertyBrowserPrivate::editedItem() const
{
    return m_delegate->editedItem();
}

void VariablePropertyBrowserPrivate::editItem(QtBrowserItem *browserItem)
{
    if (QTreeWidgetItem *treeItem = m_indexToItem.value(browserItem, 0))
    {
        m_treeWidget->setCurrentItem(treeItem, 1);
        m_treeWidget->editItem(treeItem, 1);
    }
}

/*!
    \class VariablePropertyBrowser
    \internal
    \inmodule QtDesigner
    \since 4.4

    \brief The VariablePropertyBrowser class provides QTreeWidget based
    property browser.

    A property browser is a widget that enables the user to edit a
    given set of properties. Each property is represented by a label
    specifying the property's name, and an editing widget (e.g. a line
    edit or a combobox) holding its value. A property can have zero or
    more subproperties.

    VariablePropertyBrowser provides a tree based view for all nested
    properties, i.e. properties that have subproperties can be in an
    expanded (subproperties are visible) or collapsed (subproperties
    are hidden) state. For example:

    \image VariablePropertyBrowser.png

    Use the QtAbstractPropertyBrowser API to add, insert and remove
    properties from an instance of the VariablePropertyBrowser class.
    The properties themselves are created and managed by
    implementations of the QtAbstractPropertyManager class.

    \sa QtGroupBoxPropertyBrowser, QtAbstractPropertyBrowser
*/

/*!
    \fn void VariablePropertyBrowser::collapsed(QtBrowserItem *item)

    This signal is emitted when the \a item is collapsed.

    \sa expanded(), setExpanded()
*/

/*!
    \fn void VariablePropertyBrowser::expanded(QtBrowserItem *item)

    This signal is emitted when the \a item is expanded.

    \sa collapsed(), setExpanded()
*/

/*!
    Creates a property browser with the given \a parent.
*/
VariablePropertyBrowser::VariablePropertyBrowser(QWidget *parent)
    : QtAbstractPropertyBrowser(parent), d_ptr(new VariablePropertyBrowserPrivate)
{
    d_ptr->q_ptr = this;

    d_ptr->init(this);
    connect(
        this, SIGNAL(currentItemChanged(QtBrowserItem *)), this, SLOT(slotCurrentBrowserItemChanged(QtBrowserItem *))
    );
    d_ptr->m_treeWidget->header()->viewport()->installEventFilter(this);
}

/*!
    Destroys this property browser.

    Note that the properties that were inserted into this browser are
    \e not destroyed since they may still be used in other
    browsers. The properties are owned by the manager that created
    them.

    \sa VariableProperty, QtAbstractPropertyManager
*/
VariablePropertyBrowser::~VariablePropertyBrowser() {}

/*!
    \property VariablePropertyBrowser::indentation
    \brief indentation of the items in the tree view.
*/
int VariablePropertyBrowser::indentation() const
{
    return d_ptr->m_treeWidget->indentation();
}

void VariablePropertyBrowser::setIndentation(int i)
{
    d_ptr->m_treeWidget->setIndentation(i);
}

/*!
  \property VariablePropertyBrowser::rootIsDecorated
  \brief whether to show controls for expanding and collapsing root items.
*/
bool VariablePropertyBrowser::rootIsDecorated() const
{
    return d_ptr->m_treeWidget->rootIsDecorated();
}

void VariablePropertyBrowser::setRootIsDecorated(bool show)
{
    d_ptr->m_treeWidget->setRootIsDecorated(show);
    for (auto it = d_ptr->m_itemToIndex.cbegin(), end = d_ptr->m_itemToIndex.cend(); it != end; ++it)
    {
        VariableProperty *property = it.value()->property();
        if (!property->hasValue())
            d_ptr->updateItem(it.key());
    }
}

/*!
  \property VariablePropertyBrowser::alternatingRowColors
  \brief whether to draw the background using alternating colors.
  By default this property is set to true.
*/
bool VariablePropertyBrowser::alternatingRowColors() const
{
    return d_ptr->m_treeWidget->alternatingRowColors();
}

void VariablePropertyBrowser::setAlternatingRowColors(bool enable)
{
    d_ptr->m_treeWidget->setAlternatingRowColors(enable);
}

/*!
  \property VariablePropertyBrowser::headerVisible
  \brief whether to show the header.
*/
bool VariablePropertyBrowser::isHeaderVisible() const
{
    return d_ptr->m_headerVisible;
}

void VariablePropertyBrowser::setHeaderVisible(bool visible)
{
    if (d_ptr->m_headerVisible == visible)
        return;

    d_ptr->m_headerVisible = visible;
    d_ptr->m_treeWidget->header()->setVisible(visible);
}

QMap<int, double> VariablePropertyBrowser::headerSectionResizeRatios() const
{
    return d_ptr->m_sectionResizeRatios;
}

void VariablePropertyBrowser::setHeaderSectionResizeRatios(const QMap<int, double> &ratios)
{
    for (int i = 0; i < ratios.keys().size(); ++i)
    {
        d_ptr->m_sectionResizeRatios[ratios.keys().at(i)] = ratios.values().at(i);
    }
}

void VariablePropertyBrowser::setHeaderSectionResizeRatios(const QVector<double> &ratios)
{
    for (int i = 0; i < ratios.size(); ++i)
    {
        d_ptr->m_sectionResizeRatios[i] = ratios.at(i);
    }
}

void VariablePropertyBrowser::setHeaderSectionResizeRatios(const QList<double> &ratios)
{
    for (int i = 0; i < ratios.size(); ++i)
    {
        d_ptr->m_sectionResizeRatios[i] = ratios.at(i);
    }
}

void VariablePropertyBrowser::setHeaderSectionResizeRatio(int section, double ratio)
{
    d_ptr->m_sectionResizeRatios[section] = ratio;
}

/*!
    Sets the \a item to either collapse or expanded, depending on the value of \a expanded.

    \sa isExpanded(), expanded(), collapsed()
*/

void VariablePropertyBrowser::setExpanded(QtBrowserItem *item, bool expanded)
{
    QTreeWidgetItem *treeItem = d_ptr->m_indexToItem.value(item);
    if (treeItem)
        treeItem->setExpanded(expanded);
}

/*!
    Returns true if the \a item is expanded; otherwise returns false.

    \sa setExpanded()
*/

bool VariablePropertyBrowser::isExpanded(QtBrowserItem *item) const
{
    QTreeWidgetItem *treeItem = d_ptr->m_indexToItem.value(item);
    if (treeItem)
        return treeItem->isExpanded();
    return false;
}

/*!
    Returns true if the \a item is visible; otherwise returns false.

    \sa setItemVisible()
    \since 4.5
*/

bool VariablePropertyBrowser::isItemVisible(QtBrowserItem *item) const
{
    if (const QTreeWidgetItem *treeItem = d_ptr->m_indexToItem.value(item))
        return !treeItem->isHidden();
    return false;
}

/*!
    Sets the \a item to be visible, depending on the value of \a visible.

   \sa isItemVisible()
   \since 4.5
*/

void VariablePropertyBrowser::setItemVisible(QtBrowserItem *item, bool visible)
{
    if (QTreeWidgetItem *treeItem = d_ptr->m_indexToItem.value(item))
        treeItem->setHidden(!visible);
}

/*!
    Sets the \a item's background color to \a color. Note that while item's background
    is rendered every second row is being drawn with alternate color (which is a bit lighter than items \a color)

    \sa backgroundColor(), calculatedBackgroundColor()
*/

void VariablePropertyBrowser::setBackgroundColor(QtBrowserItem *item, const QColor &color)
{
    if (!d_ptr->m_indexToItem.contains(item))
        return;
    if (color.isValid())
        d_ptr->m_indexToBackgroundColor[item] = color;
    else
        d_ptr->m_indexToBackgroundColor.remove(item);
    d_ptr->m_treeWidget->viewport()->update();
}

/*!
    Returns the \a item's color. If there is no color set for item it returns invalid color.

    \sa calculatedBackgroundColor(), setBackgroundColor()
*/

QColor VariablePropertyBrowser::backgroundColor(QtBrowserItem *item) const
{
    return d_ptr->m_indexToBackgroundColor.value(item);
}

/*!
    Returns the \a item's color. If there is no color set for item it returns parent \a item's
    color (if there is no color set for parent it returns grandparent's color and so on). In case
    the color is not set for \a item and it's top level item it returns invalid color.

    \sa backgroundColor(), setBackgroundColor()
*/

QColor VariablePropertyBrowser::calculatedBackgroundColor(QtBrowserItem *item) const
{
    return d_ptr->calculatedBackgroundColor(item);
}

/*!
    \property VariablePropertyBrowser::propertiesWithoutValueMarked
    \brief whether to enable or disable marking properties without value.

    When marking is enabled the item's background is rendered in dark color and item's
    foreground is rendered with light color.

    \sa propertiesWithoutValueMarked()
*/
void VariablePropertyBrowser::setPropertiesWithoutValueMarked(bool mark)
{
    if (d_ptr->m_markPropertiesWithoutValue == mark)
        return;

    d_ptr->m_markPropertiesWithoutValue = mark;
    for (auto it = d_ptr->m_itemToIndex.cbegin(), end = d_ptr->m_itemToIndex.cend(); it != end; ++it)
    {
        VariableProperty *property = it.value()->property();
        if (!property->hasValue())
            d_ptr->updateItem(it.key());
    }
    d_ptr->m_treeWidget->viewport()->update();
}

bool VariablePropertyBrowser::propertiesWithoutValueMarked() const
{
    return d_ptr->m_markPropertiesWithoutValue;
}

/*!
    \reimp
*/
void VariablePropertyBrowser::itemInserted(QtBrowserItem *item, QtBrowserItem *afterItem)
{
    d_ptr->propertyInserted(item, afterItem);
}

/*!
    \reimp
*/
void VariablePropertyBrowser::itemRemoved(QtBrowserItem *item)
{
    d_ptr->propertyRemoved(item);
}

/*!
    \reimp
*/
void VariablePropertyBrowser::itemChanged(QtBrowserItem *item)
{
    d_ptr->propertyChanged(item);
}

/*!
    \reimp
*/
void VariablePropertyBrowser::resizeEvent(QResizeEvent *event)
{
    QtAbstractPropertyBrowser::resizeEvent(event);
    d_ptr->resizeHeaderSections();
}

/*!
    \reimp
*/
void VariablePropertyBrowser::showEvent(QShowEvent *event)
{
    QtAbstractPropertyBrowser::showEvent(event);
    d_ptr->resizeHeaderSections();
}

bool VariablePropertyBrowser::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == d_ptr->m_treeWidget->header()->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            d_ptr->m_headerInteractiveDragging = true;
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            d_ptr->m_headerInteractiveDragging = false;
        }
        else if (event->type() == QEvent::MouseButtonDblClick)
        {
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

/*!
    Sets the current item to \a item and opens the relevant editor for it.
*/
void VariablePropertyBrowser::editItem(QtBrowserItem *item)
{
    d_ptr->editItem(item);
}

/*!
    Expands all items in the tree.
*/
void VariablePropertyBrowser::expandAll()
{
    d_ptr->m_treeWidget->expandAll();
}

/*!
    Collapses all items in the tree.
*/
void VariablePropertyBrowser::collapseAll()
{
    d_ptr->m_treeWidget->collapseAll();
}

} // namespace gui
} // namespace xequation

#include "moc_variable_property_browser.cpp"
#include "variable_property_browser.moc"
