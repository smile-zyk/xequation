#include "equation_debugger_browser.h"
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
class EquationDebuggerBrowserView;
class EquationDebuggerBrowserPrivate
{
    EquationDebuggerBrowser *q_ptr;
    Q_DECLARE_PUBLIC(EquationDebuggerBrowser)

  public:
    EquationDebuggerBrowserPrivate();
    void init(QWidget *parent);

    void propertyInserted(QtBrowserItem *index, QtBrowserItem *afterIndex);
    void propertyRemoved(QtBrowserItem *index);
    void propertyChanged(QtBrowserItem *index);
    QWidget *createEditor(QtProperty *property, QWidget *parent) const
    {
        return q_ptr->createEditor(property, parent);
    }
    QtProperty *indexToProperty(const QModelIndex &index) const;
    QTreeWidgetItem *indexToItem(const QModelIndex &index) const;
    QtBrowserItem *indexToBrowserItem(const QModelIndex &index) const;
    bool lastColumn(int column) const;
    void disableItem(QTreeWidgetItem *item) const;
    void enableItem(QTreeWidgetItem *item) const;
    bool hasValue(QTreeWidgetItem *item) const;

    void slotCollapsed(const QModelIndex &index);
    void slotExpanded(const QModelIndex &index);

    QColor calculatedBackgroundColor(QtBrowserItem *item) const;

    EquationDebuggerBrowserView *treeWidget() const
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

    QMap<QtBrowserItem *, QColor> m_indexToBackgroundColor;

    EquationDebuggerBrowserView *m_treeWidget;

    bool m_headerVisible;
    EquationDebuggerBrowser::ResizeMode m_resizeMode;
    class EquationDebuggerBrowserDelegate *m_delegate;
    bool m_markPropertiesWithoutValue;
    bool m_browserChangedBlocked;
    QIcon m_expandIcon;
};

// ------------ EquationDebuggerBrowserView
class EquationDebuggerBrowserView : public QTreeWidget
{
    Q_OBJECT
  public:
    EquationDebuggerBrowserView(QWidget *parent = 0);
    void setEditorPrivate(EquationDebuggerBrowserPrivate *editorPrivate)
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
    EquationDebuggerBrowserPrivate *m_editorPrivate;
};

EquationDebuggerBrowserView::EquationDebuggerBrowserView(QWidget *parent) : QTreeWidget(parent), m_editorPrivate(0)
{
    connect(header(), SIGNAL(sectionDoubleClicked(int)), this, SLOT(resizeColumnToContents(int)));
}

void EquationDebuggerBrowserView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index)
    const
{
    QStyleOptionViewItem opt = option;
    bool hasValue = true;
    if (m_editorPrivate)
    {
        QtProperty *property = m_editorPrivate->indexToProperty(index);
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

void EquationDebuggerBrowserView::keyPressEvent(QKeyEvent *event)
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

void EquationDebuggerBrowserView::mousePressEvent(QMouseEvent *event)
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

// ------------ EquationDebuggerBrowserDelegate
class EquationDebuggerBrowserDelegate : public QItemDelegate
{
    Q_OBJECT
  public:
    EquationDebuggerBrowserDelegate(QObject *parent = 0)
        : QItemDelegate(parent), m_editorPrivate(0), m_editedItem(0), m_editedWidget(0)
    {
    }

    void setEditorPrivate(EquationDebuggerBrowserPrivate *editorPrivate)
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
    void closeEditor(QtProperty *property);

    QTreeWidgetItem *editedItem() const
    {
        return m_editedItem;
    }

  private slots:
    void slotEditorDestroyed(QObject *object);

  private:
    int indentation(const QModelIndex &index) const;

    typedef QMap<QWidget *, QtProperty *> EditorToPropertyMap;
    mutable EditorToPropertyMap m_editorToProperty;

    typedef QMap<QtProperty *, QWidget *> PropertyToEditorMap;
    mutable PropertyToEditorMap m_propertyToEditor;
    EquationDebuggerBrowserPrivate *m_editorPrivate;
    mutable QTreeWidgetItem *m_editedItem;
    mutable QWidget *m_editedWidget;
};

int EquationDebuggerBrowserDelegate::indentation(const QModelIndex &index) const
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

void EquationDebuggerBrowserDelegate::slotEditorDestroyed(QObject *object)
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

void EquationDebuggerBrowserDelegate::closeEditor(QtProperty *property)
{
    if (QWidget *w = m_propertyToEditor.value(property, 0))
        w->deleteLater();
}

QWidget *
EquationDebuggerBrowserDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.column() == 1 && m_editorPrivate)
    {
        QtProperty *property = m_editorPrivate->indexToProperty(index);
        QTreeWidgetItem *item = m_editorPrivate->indexToItem(index);
        if (property && item && (item->flags() & Qt::ItemIsEnabled))
        {
            QWidget *editor = m_editorPrivate->createEditor(property, parent);
            if (editor)
            {
                editor->setAutoFillBackground(true);
                editor->installEventFilter(const_cast<EquationDebuggerBrowserDelegate *>(this));
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

void EquationDebuggerBrowserDelegate::updateEditorGeometry(
    QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index
) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect.adjusted(0, 0, 0, -1));
}

void EquationDebuggerBrowserDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index)
    const
{
    bool hasValue = true;
    if (m_editorPrivate)
    {
        QtProperty *property = m_editorPrivate->indexToProperty(index);
        if (property)
            hasValue = property->hasValue();
    }
    QStyleOptionViewItem opt = option;
    if ((m_editorPrivate && index.column() == 0) || !hasValue)
    {
        QtProperty *property = m_editorPrivate->indexToProperty(index);
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

QSize EquationDebuggerBrowserDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QItemDelegate::sizeHint(option, index) + QSize(3, 4);
}

bool EquationDebuggerBrowserDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusOut)
    {
        QFocusEvent *fe = static_cast<QFocusEvent *>(event);
        if (fe->reason() == Qt::ActiveWindowFocusReason)
            return false;
    }
    return QItemDelegate::eventFilter(object, event);
}

//  -------- EquationDebuggerBrowserPrivate implementation
EquationDebuggerBrowserPrivate::EquationDebuggerBrowserPrivate()
    : m_treeWidget(0),
      m_headerVisible(true),
      m_resizeMode(EquationDebuggerBrowser::Stretch),
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

void EquationDebuggerBrowserPrivate::init(QWidget *parent)
{
    QHBoxLayout *layout = new QHBoxLayout(parent);
    layout->setContentsMargins(QMargins());
    m_treeWidget = new EquationDebuggerBrowserView(parent);
    m_treeWidget->setEditorPrivate(this);
    m_treeWidget->setIconSize(QSize(18, 18));
    layout->addWidget(m_treeWidget);

    m_treeWidget->setColumnCount(2);
    QStringList labels;
    labels.append(QCoreApplication::translate("EquationDebuggerBrowser", "Property"));
    labels.append(QCoreApplication::translate("EquationDebuggerBrowser", "Value"));
    m_treeWidget->setHeaderLabels(labels);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_delegate = new EquationDebuggerBrowserDelegate(parent);
    m_delegate->setEditorPrivate(this);
    m_treeWidget->setItemDelegate(m_delegate);
    m_treeWidget->header()->setSectionsMovable(false);
    m_treeWidget->header()->setSectionResizeMode(QHeaderView::Stretch);

    m_expandIcon = drawIndicatorIcon(q_ptr->palette(), q_ptr->style());

    QObject::connect(m_treeWidget, SIGNAL(collapsed(QModelIndex)), q_ptr, SLOT(slotCollapsed(QModelIndex)));
    QObject::connect(m_treeWidget, SIGNAL(expanded(QModelIndex)), q_ptr, SLOT(slotExpanded(QModelIndex)));
    QObject::connect(
        m_treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), q_ptr,
        SLOT(slotCurrentTreeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *))
    );
}

QtBrowserItem *EquationDebuggerBrowserPrivate::currentItem() const
{
    if (QTreeWidgetItem *treeItem = m_treeWidget->currentItem())
        return m_itemToIndex.value(treeItem);
    return 0;
}

void EquationDebuggerBrowserPrivate::setCurrentItem(QtBrowserItem *browserItem, bool block)
{
    const bool blocked = block ? m_treeWidget->blockSignals(true) : false;
    if (browserItem == 0)
        m_treeWidget->setCurrentItem(0);
    else
        m_treeWidget->setCurrentItem(m_indexToItem.value(browserItem));
    if (block)
        m_treeWidget->blockSignals(blocked);
}

QtProperty *EquationDebuggerBrowserPrivate::indexToProperty(const QModelIndex &index) const
{
    QTreeWidgetItem *item = m_treeWidget->indexToItem(index);
    QtBrowserItem *idx = m_itemToIndex.value(item);
    if (idx)
        return idx->property();
    return 0;
}

QtBrowserItem *EquationDebuggerBrowserPrivate::indexToBrowserItem(const QModelIndex &index) const
{
    QTreeWidgetItem *item = m_treeWidget->indexToItem(index);
    return m_itemToIndex.value(item);
}

QTreeWidgetItem *EquationDebuggerBrowserPrivate::indexToItem(const QModelIndex &index) const
{
    return m_treeWidget->indexToItem(index);
}

bool EquationDebuggerBrowserPrivate::lastColumn(int column) const
{
    return m_treeWidget->header()->visualIndex(column) == m_treeWidget->columnCount() - 1;
}

void EquationDebuggerBrowserPrivate::disableItem(QTreeWidgetItem *item) const
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

void EquationDebuggerBrowserPrivate::enableItem(QTreeWidgetItem *item) const
{
    Qt::ItemFlags flags = item->flags();
    flags |= Qt::ItemIsEnabled;
    item->setFlags(flags);
    const int childCount = item->childCount();
    for (int i = 0; i < childCount; i++)
    {
        QTreeWidgetItem *child = item->child(i);
        QtProperty *property = m_itemToIndex[child]->property();
        if (property->isEnabled())
        {
            enableItem(child);
        }
    }
}

bool EquationDebuggerBrowserPrivate::hasValue(QTreeWidgetItem *item) const
{
    QtBrowserItem *browserItem = m_itemToIndex.value(item);
    if (browserItem)
        return browserItem->property()->hasValue();
    return false;
}

void EquationDebuggerBrowserPrivate::propertyInserted(QtBrowserItem *index, QtBrowserItem *afterIndex)
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

void EquationDebuggerBrowserPrivate::propertyRemoved(QtBrowserItem *index)
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

void EquationDebuggerBrowserPrivate::propertyChanged(QtBrowserItem *index)
{
    QTreeWidgetItem *item = m_indexToItem.value(index);

    updateItem(item);
}

void EquationDebuggerBrowserPrivate::updateItem(QTreeWidgetItem *item)
{
    QtProperty *property = m_itemToIndex[item]->property();
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

QColor EquationDebuggerBrowserPrivate::calculatedBackgroundColor(QtBrowserItem *item) const
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

void EquationDebuggerBrowserPrivate::slotCollapsed(const QModelIndex &index)
{
    QTreeWidgetItem *item = indexToItem(index);
    QtBrowserItem *idx = m_itemToIndex.value(item);
    if (item)
        emit q_ptr->collapsed(idx);
}

void EquationDebuggerBrowserPrivate::slotExpanded(const QModelIndex &index)
{
    QTreeWidgetItem *item = indexToItem(index);
    QtBrowserItem *idx = m_itemToIndex.value(item);
    if (item)
        emit q_ptr->expanded(idx);
}

void EquationDebuggerBrowserPrivate::slotCurrentBrowserItemChanged(QtBrowserItem *item)
{
    if (!m_browserChangedBlocked && item != currentItem())
        setCurrentItem(item, true);
}

void EquationDebuggerBrowserPrivate::slotCurrentTreeItemChanged(QTreeWidgetItem *newItem, QTreeWidgetItem *)
{
    QtBrowserItem *browserItem = newItem ? m_itemToIndex.value(newItem) : 0;
    m_browserChangedBlocked = true;
    q_ptr->setCurrentItem(browserItem);
    m_browserChangedBlocked = false;
}

QTreeWidgetItem *EquationDebuggerBrowserPrivate::editedItem() const
{
    return m_delegate->editedItem();
}

void EquationDebuggerBrowserPrivate::editItem(QtBrowserItem *browserItem)
{
    if (QTreeWidgetItem *treeItem = m_indexToItem.value(browserItem, 0))
    {
        m_treeWidget->setCurrentItem(treeItem, 1);
        m_treeWidget->editItem(treeItem, 1);
    }
}

/*!
    \class EquationDebuggerBrowser
    \internal
    \inmodule QtDesigner
    \since 4.4

    \brief The EquationDebuggerBrowser class provides QTreeWidget based
    property browser.

    A property browser is a widget that enables the user to edit a
    given set of properties. Each property is represented by a label
    specifying the property's name, and an editing widget (e.g. a line
    edit or a combobox) holding its value. A property can have zero or
    more subproperties.

    EquationDebuggerBrowser provides a tree based view for all nested
    properties, i.e. properties that have subproperties can be in an
    expanded (subproperties are visible) or collapsed (subproperties
    are hidden) state. For example:

    \image EquationDebuggerBrowser.png

    Use the QtAbstractPropertyBrowser API to add, insert and remove
    properties from an instance of the EquationDebuggerBrowser class.
    The properties themselves are created and managed by
    implementations of the QtAbstractPropertyManager class.

    \sa QtGroupBoxPropertyBrowser, QtAbstractPropertyBrowser
*/

/*!
    \fn void EquationDebuggerBrowser::collapsed(QtBrowserItem *item)

    This signal is emitted when the \a item is collapsed.

    \sa expanded(), setExpanded()
*/

/*!
    \fn void EquationDebuggerBrowser::expanded(QtBrowserItem *item)

    This signal is emitted when the \a item is expanded.

    \sa collapsed(), setExpanded()
*/

/*!
    Creates a property browser with the given \a parent.
*/
EquationDebuggerBrowser::EquationDebuggerBrowser(QWidget *parent)
    : QtAbstractPropertyBrowser(parent), d_ptr(new EquationDebuggerBrowserPrivate)
{
    d_ptr->q_ptr = this;

    d_ptr->init(this);
    connect(
        this, SIGNAL(currentItemChanged(QtBrowserItem *)), this, SLOT(slotCurrentBrowserItemChanged(QtBrowserItem *))
    );
}

/*!
    Destroys this property browser.

    Note that the properties that were inserted into this browser are
    \e not destroyed since they may still be used in other
    browsers. The properties are owned by the manager that created
    them.

    \sa QtProperty, QtAbstractPropertyManager
*/
EquationDebuggerBrowser::~EquationDebuggerBrowser() {}

/*!
    \property EquationDebuggerBrowser::indentation
    \brief indentation of the items in the tree view.
*/
int EquationDebuggerBrowser::indentation() const
{
    return d_ptr->m_treeWidget->indentation();
}

void EquationDebuggerBrowser::setIndentation(int i)
{
    d_ptr->m_treeWidget->setIndentation(i);
}

/*!
  \property EquationDebuggerBrowser::rootIsDecorated
  \brief whether to show controls for expanding and collapsing root items.
*/
bool EquationDebuggerBrowser::rootIsDecorated() const
{
    return d_ptr->m_treeWidget->rootIsDecorated();
}

void EquationDebuggerBrowser::setRootIsDecorated(bool show)
{
    d_ptr->m_treeWidget->setRootIsDecorated(show);
    for (auto it = d_ptr->m_itemToIndex.cbegin(), end = d_ptr->m_itemToIndex.cend(); it != end; ++it)
    {
        QtProperty *property = it.value()->property();
        if (!property->hasValue())
            d_ptr->updateItem(it.key());
    }
}

/*!
  \property EquationDebuggerBrowser::alternatingRowColors
  \brief whether to draw the background using alternating colors.
  By default this property is set to true.
*/
bool EquationDebuggerBrowser::alternatingRowColors() const
{
    return d_ptr->m_treeWidget->alternatingRowColors();
}

void EquationDebuggerBrowser::setAlternatingRowColors(bool enable)
{
    d_ptr->m_treeWidget->setAlternatingRowColors(enable);
}

/*!
  \property EquationDebuggerBrowser::headerVisible
  \brief whether to show the header.
*/
bool EquationDebuggerBrowser::isHeaderVisible() const
{
    return d_ptr->m_headerVisible;
}

void EquationDebuggerBrowser::setHeaderVisible(bool visible)
{
    if (d_ptr->m_headerVisible == visible)
        return;

    d_ptr->m_headerVisible = visible;
    d_ptr->m_treeWidget->header()->setVisible(visible);
}

/*!
  \enum EquationDebuggerBrowser::ResizeMode

  The resize mode specifies the behavior of the header sections.

  \value Interactive The user can resize the sections.
  The sections can also be resized programmatically using setSplitterPosition().

  \value Fixed The user cannot resize the section.
  The section can only be resized programmatically using setSplitterPosition().

  \value Stretch QHeaderView will automatically resize the section to fill the available space.
  The size cannot be changed by the user or programmatically.

  \value ResizeToContents QHeaderView will automatically resize the section to its optimal
  size based on the contents of the entire column.
  The size cannot be changed by the user or programmatically.

  \sa setResizeMode()
*/

/*!
    \property EquationDebuggerBrowser::resizeMode
    \brief the resize mode of setions in the header.
*/

EquationDebuggerBrowser::ResizeMode EquationDebuggerBrowser::resizeMode() const
{
    return d_ptr->m_resizeMode;
}

void EquationDebuggerBrowser::setResizeMode(EquationDebuggerBrowser::ResizeMode mode)
{
    if (d_ptr->m_resizeMode == mode)
        return;

    d_ptr->m_resizeMode = mode;
    QHeaderView::ResizeMode m = QHeaderView::Stretch;
    switch (mode)
    {
    case EquationDebuggerBrowser::Interactive:
        m = QHeaderView::Interactive;
        break;
    case EquationDebuggerBrowser::Fixed:
        m = QHeaderView::Fixed;
        break;
    case EquationDebuggerBrowser::ResizeToContents:
        m = QHeaderView::ResizeToContents;
        break;
    case EquationDebuggerBrowser::Stretch:
    default:
        m = QHeaderView::Stretch;
        break;
    }
    d_ptr->m_treeWidget->header()->setSectionResizeMode(m);
}

/*!
    \property EquationDebuggerBrowser::splitterPosition
    \brief the position of the splitter between the colunms.
*/

int EquationDebuggerBrowser::splitterPosition() const
{
    return d_ptr->m_treeWidget->header()->sectionSize(0);
}

void EquationDebuggerBrowser::setSplitterPosition(int position)
{
    d_ptr->m_treeWidget->header()->resizeSection(0, position);
}

/*!
    Sets the \a item to either collapse or expanded, depending on the value of \a expanded.

    \sa isExpanded(), expanded(), collapsed()
*/

void EquationDebuggerBrowser::setExpanded(QtBrowserItem *item, bool expanded)
{
    QTreeWidgetItem *treeItem = d_ptr->m_indexToItem.value(item);
    if (treeItem)
        treeItem->setExpanded(expanded);
}

/*!
    Returns true if the \a item is expanded; otherwise returns false.

    \sa setExpanded()
*/

bool EquationDebuggerBrowser::isExpanded(QtBrowserItem *item) const
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

bool EquationDebuggerBrowser::isItemVisible(QtBrowserItem *item) const
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

void EquationDebuggerBrowser::setItemVisible(QtBrowserItem *item, bool visible)
{
    if (QTreeWidgetItem *treeItem = d_ptr->m_indexToItem.value(item))
        treeItem->setHidden(!visible);
}

/*!
    Sets the \a item's background color to \a color. Note that while item's background
    is rendered every second row is being drawn with alternate color (which is a bit lighter than items \a color)

    \sa backgroundColor(), calculatedBackgroundColor()
*/

void EquationDebuggerBrowser::setBackgroundColor(QtBrowserItem *item, const QColor &color)
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

QColor EquationDebuggerBrowser::backgroundColor(QtBrowserItem *item) const
{
    return d_ptr->m_indexToBackgroundColor.value(item);
}

/*!
    Returns the \a item's color. If there is no color set for item it returns parent \a item's
    color (if there is no color set for parent it returns grandparent's color and so on). In case
    the color is not set for \a item and it's top level item it returns invalid color.

    \sa backgroundColor(), setBackgroundColor()
*/

QColor EquationDebuggerBrowser::calculatedBackgroundColor(QtBrowserItem *item) const
{
    return d_ptr->calculatedBackgroundColor(item);
}

/*!
    \property EquationDebuggerBrowser::propertiesWithoutValueMarked
    \brief whether to enable or disable marking properties without value.

    When marking is enabled the item's background is rendered in dark color and item's
    foreground is rendered with light color.

    \sa propertiesWithoutValueMarked()
*/
void EquationDebuggerBrowser::setPropertiesWithoutValueMarked(bool mark)
{
    if (d_ptr->m_markPropertiesWithoutValue == mark)
        return;

    d_ptr->m_markPropertiesWithoutValue = mark;
    for (auto it = d_ptr->m_itemToIndex.cbegin(), end = d_ptr->m_itemToIndex.cend(); it != end; ++it)
    {
        QtProperty *property = it.value()->property();
        if (!property->hasValue())
            d_ptr->updateItem(it.key());
    }
    d_ptr->m_treeWidget->viewport()->update();
}

bool EquationDebuggerBrowser::propertiesWithoutValueMarked() const
{
    return d_ptr->m_markPropertiesWithoutValue;
}

/*!
    \reimp
*/
void EquationDebuggerBrowser::itemInserted(QtBrowserItem *item, QtBrowserItem *afterItem)
{
    d_ptr->propertyInserted(item, afterItem);
}

/*!
    \reimp
*/
void EquationDebuggerBrowser::itemRemoved(QtBrowserItem *item)
{
    d_ptr->propertyRemoved(item);
}

/*!
    \reimp
*/
void EquationDebuggerBrowser::itemChanged(QtBrowserItem *item)
{
    d_ptr->propertyChanged(item);
}

/*!
    Sets the current item to \a item and opens the relevant editor for it.
*/
void EquationDebuggerBrowser::editItem(QtBrowserItem *item)
{
    d_ptr->editItem(item);
}
} // namespace gui
} // namespace xequation

#include "equation_debugger_browser.moc"
#include "moc_equation_debugger_browser.cpp"
