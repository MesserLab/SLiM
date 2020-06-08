//
//  QtSLiMVariableBrowser.h
//  SLiM
//
//  Created by Ben Haller on 4/17/2019.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "QtSLiMVariableBrowser.h"
#include "ui_QtSLiMVariableBrowser.h"

#include <QSettings>
#include <QStringList>
#include <QVariant>
#include <QTreeWidgetItem>
#include <QScrollBar>
#include <QDebug>

#include "QtSLiMWindow.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMAppDelegate.h"

#include "eidos_symbol_table.h"


static int QtSLiMVarBrowserRowHeight = 0;  // a global, for simplicity; 0 is uninitialized, -1 is failed to init


//
// This subclass of QStyledItemDelegate provides custom drawing for the outline view.
//

QtSLiMVariableBrowserDelegate::~QtSLiMVariableBrowserDelegate(void)
{
}

void QtSLiMVariableBrowserDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // On Ubuntu, items get shown as having "focus" even when they're not selectable, which I dislike; this disables that appearance
    // See https://stackoverflow.com/a/2061871/2752221
    QStyleOptionViewItem modifiedOption(option);
    if (modifiedOption.state & QStyle::State_HasFocus)
        modifiedOption.state = modifiedOption.state ^ QStyle::State_HasFocus;
    
    // then let super draw
    QStyledItemDelegate::paint(painter, modifiedOption, index);
}


//
//  QtSLiMBrowserItem
//

QtSLiMBrowserItem::QtSLiMBrowserItem(QString name, EidosValue_SP value, int elementIndex, bool isEllipsis) :
    QTreeWidgetItem(), symbol_name(name), eidos_value(value), element_index(elementIndex), is_ellipsis(isEllipsis)
{
    // We want to display Eidos constants in gray text, to de-emphasize them.  For now, we just hard-code them
    // as a hack, because we *don't* want SLiM constants (sim, g1, p1, etc.) to display dimmed
    is_eidos_constant = false;
    if ((name == "T") || (name == "F") || (name == "E") || (name == "PI") || (name == "INF") || (name == "NAN") || (name == "NULL"))
        is_eidos_constant = true;
    
    // If we contain children, they won't be added under us until we get expanded, so force the indicator on
    // Note this applies both to the row for the vector (count > 0) and rows for elements (count > 0)
    if (eidos_value && (eidos_value->Type() == EidosValueType::kValueObject) && (eidos_value->Count() > 0))
    {
        setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        has_children = true;
    }
    else
    {
        has_children = false;
    }
    
    // Precompute the hash value for this item.  This is done up front and cached, so that it is available
    // even if our eidos_value object element gets deallocated.  We use it to confirm that two browser items
    // truly refer to the same Eidos object.  The hash encapsulates the symbol name, the element index, and
    // the object-element type.  It does not encapsulate the number of elements; we want matches to carry
    // over across changes in vector length.  Note that of course hash collisions can occur.  That will
    // result in the expansion of the wrong item during a browser reload; it is not fatal, so as long as it
    // is very rare, it's OK.  Since the same symbol might provide a different EidosValue object every time
    // it is accessed (as properties often would), and we can't look inside the elements, this is the best
    // we can do.  It should be quite reliable.
    item_hash = qHash(symbol_name) ^ static_cast<unsigned int>(element_index);
    
    if (eidos_value && (eidos_value->Type() == EidosValueType::kValueObject))
        item_hash ^= (std::hash<std::string>{}(eidos_value->ElementType()) << 16);
}

QtSLiMBrowserItem::~QtSLiMBrowserItem(void)
{
    //qDebug() << "QtSLiMBrowserItem::~QtSLiMBrowserItem";
    eidos_value.reset();
}

QVariant QtSLiMBrowserItem::data(int column, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (column == 0)            return symbol_name;
        if (is_ellipsis)            return QVariant();
        if (!eidos_value)           return (column == 3) ? "<inaccessible>" : QVariant();
        if (element_index != -1)    return QVariant();
        
        // the remainder of this assumes the above conditions: the column is not 0, the item is not an ellipsis item,
        // there is an associated eidos_value, and the element_index is -1 (we're representing a whole vector)
        if (column == 1)
        {
            EidosValueType type = eidos_value->Type();
            std::string type_string = StringForEidosValueType(type);
            QString typeString = QString::fromStdString(type_string);
            
            if (type == EidosValueType::kValueObject)
            {
                EidosValue_Object *object_value = static_cast<EidosValue_Object *>(eidos_value.get());
                const std::string &element_string = object_value->ElementType();
                QString elementString = QString::fromStdString(element_string);
                
                typeString.append('<');
                typeString.append(elementString);
                typeString.append('>');
            }
            
            return typeString;
        }
        else if (column == 2)
        {
            return eidos_value->Count();
        }
        else if (column == 3)
        {
            int value_count = eidos_value->Count();
            std::ostringstream outstream;
            
            // print values as a comma-separated list with strings quoted; halfway between print() and cat()
            for (int value_index = 0; value_index < value_count; ++value_index)
            {
                EidosValue_SP element_value = eidos_value->GetValueAtIndex(value_index, nullptr);
                
                if (value_index > 0)
                {
                    outstream << ", ";
                    
                    // terminate the list at some reasonable point, otherwise we generate massively long strings for large vectors...
                    if (value_index > 50)
                    {
                        outstream << ", ...";
                        break;
                    }
                }
                outstream << *element_value;
            }
            
            return QString::fromStdString(outstream.str()).simplified();
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if (column == 2)
            return static_cast<Qt::Alignment::Int>(Qt::AlignHCenter | Qt::AlignVCenter);
        else
            return static_cast<Qt::Alignment::Int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else if (role == Qt::ForegroundRole)
    {
        return QBrush(is_eidos_constant ? Qt::darkGray : Qt::black);
    }
    else if (role == Qt::FontRole)
    {
        static QFont *defaultFont = nullptr;
        static QFont *italicFont = nullptr;
        
        if (!defaultFont)
        {
            defaultFont = new QFont(QTreeWidgetItem::data(column, Qt::FontRole).value<QFont>());
            italicFont = new QFont(*defaultFont);
            italicFont->setItalic(true);
        }
        
        return (element_index == -1) ? *defaultFont : *italicFont;
    }
    else if (role == Qt::SizeHintRole)
    {
        if (QtSLiMVarBrowserRowHeight > 0)
            return QSize(0, QtSLiMVarBrowserRowHeight);
        else
            return QTreeWidgetItem::data(column, Qt::SizeHintRole);
    }
    
    return QVariant();
}


//
//  QtSLiMVariableBrowser
//

QtSLiMVariableBrowser::QtSLiMVariableBrowser(QtSLiMEidosConsole *parent) :
    QWidget(parent, Qt::Window),    // the console window has us as a parent, but is still a standalone window
    parentEidosConsole(parent),
    ui(new Ui::QtSLiMVariableBrowser)
{
    ui->setupUi(this);
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMVariableBrowser");
    resize(settings.value("size", QSize(400, 300)).toSize());
    move(settings.value("pos", QPoint(25, 445)).toPoint());
    settings.endGroup();
    
    // tree widget settings
    QTreeWidget *browserTree = ui->browserTreeWidget;
    
    QAbstractItemDelegate *outlineDelegate = new QtSLiMVariableBrowserDelegate(browserTree);
    browserTree->setItemDelegate(outlineDelegate);
    
#if !defined(__APPLE__)
    {
        // use a smaller font for the outline on Linux
        QFont browserFont(browserTree->font());
        browserFont.setPointSizeF(browserFont.pointSizeF() - 1);
        browserTree->setFont(browserFont);
    }
#endif
    
    browserTree->setHeaderLabels(QStringList{"Symbol", "Type", "Size", "Values"});
#if defined(__APPLE__)
    browserTree->headerItem()->setTextAlignment(0, Qt::AlignVCenter);
    browserTree->headerItem()->setTextAlignment(1, Qt::AlignVCenter);
    browserTree->headerItem()->setTextAlignment(2, Qt::AlignCenter);
    browserTree->headerItem()->setTextAlignment(3, Qt::AlignVCenter);
#else
    browserTree->headerItem()->setTextAlignment(0, Qt::AlignTop);
    browserTree->headerItem()->setTextAlignment(1, Qt::AlignTop);
    browserTree->headerItem()->setTextAlignment(2, Qt::AlignHCenter | Qt::AlignTop);
    browserTree->headerItem()->setTextAlignment(3, Qt::AlignTop);
#endif
    browserTree->setColumnWidth(0, 180);
    browserTree->setColumnWidth(1, 180);
    browserTree->setColumnWidth(2, 75);
    browserTree->header()->setMinimumHeight(21);
    browserTree->header()->setSectionResizeMode(QHeaderView::Fixed);
    browserTree->header()->setSectionsMovable(false);
    browserTree->setMinimumWidth(500);
    browserTree->setUniformRowHeights(true);
    
    // handle expand/collapse events
    connect(browserTree, &QTreeWidget::itemExpanded, this, &QtSLiMVariableBrowser::itemExpanded);
    connect(browserTree, &QTreeWidget::itemCollapsed, this, &QtSLiMVariableBrowser::itemCollapsed);
    connect(browserTree, &QTreeWidget::itemClicked, this, &QtSLiMVariableBrowser::itemClicked);
    
    // watch the tree widget's vertical scroller, to restore the scroll position
    connect(ui->browserTreeWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &QtSLiMVariableBrowser::scrollerChanged);
    
    // initial state
    reloadBrowser(true);
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMVariableBrowser::~QtSLiMVariableBrowser()
{
    clearSavedExpansionState();
    
    delete ui;
}

void QtSLiMVariableBrowser::closeEvent(QCloseEvent *event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMVariableBrowser");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // send our close signal
    emit willClose();
    
    // use super's default behavior
    QWidget::closeEvent(event);
}

void QtSLiMVariableBrowser::reloadBrowser(bool nowValidState)
{
    QTreeWidget *browserTree = ui->browserTreeWidget;
    
    // Take over the old browser tree so we can consult it for things to expand
    QTreeWidgetItem *root = browserTree->invisibleRootItem();
    
    //qDebug() << "   RELOAD: root ==" << root << "and has" << root->childCount() << "children";
    
    if (root->childCount() > 0)
    {
        if (old_children.count() == 0)
        {
            // The root currently has items under it; if we have no valid saved state already,
            // the current state of the tree represents a state we want to save
            old_children = root->takeChildren();
            old_scroll_position = browserTree->verticalScrollBar()->value();
            //qDebug() << "   SAVED OLD TREE ITEMS FOR MATCHING";
            
            // We don't use the EidosValues in the saved tree at all, since they will potentially be stale;
            // to free up the memory involved, we go through the saved tree and wipe the values to nullptr
            for (QTreeWidgetItem *old_root_child : old_children)
                wipeEidosValuesFromSubtree(old_root_child);
        }
        else
        {
            // We didn't want to save the current state, since we already have a saved state that
            // has not been invalidated by a more recent user action; so just clear the root
            browserTree->clear();
        }
    }
    
    // Make the new root items; we do not attempt to reuse the old items, not worth the complication
    if (parentEidosConsole)
    {
        EidosSymbolTable *symbols = parentEidosConsole->symbolTable();
        
        if (symbols)
        {
            // Add constants first, then non-constants
            for (int isConstant = 1; isConstant >= 0; --isConstant)
            {
                std::vector<std::string> symbolNamesVec = isConstant ? symbols->ReadOnlySymbols() : symbols->ReadWriteSymbols();
                size_t symbolNamesCount = symbolNamesVec.size();
                
                for (size_t index = 0; index < symbolNamesCount;++ index)
                {
                    const std::string &symbolName = symbolNamesVec[index];
                    EidosValue_SP symbolValue = symbols->GetValueOrRaiseForSymbol(Eidos_GlobalStringIDForString(symbolName));
                    QtSLiMBrowserItem *item = new QtSLiMBrowserItem(QString::fromStdString(symbolName), std::move(symbolValue));
                    browserTree->addTopLevelItem(item);
                    
                    // figure out the correct row height, which we have to do after making a row item
                    if (QtSLiMVarBrowserRowHeight == 0)
                    {
                        QRect rect = browserTree->visualItemRect(item);
                        int defaultHeight = rect.height();
                        QtSLiMVarBrowserRowHeight = (defaultHeight > 0) ? defaultHeight + 2 : -1;
                    }
                }
            }
        }
    }
    
    // If we're now in a valid state, we'll try to match our old expanded state; otherwise, we have
    // just emptied out the tree, but are in an invalid state where we cannot repopulate it
    if (nowValidState)
    {
        // Analyze the old children and try to expand items to match the previous state
        doingMatching = true;
        
        for (QTreeWidgetItem *old_root_child : old_children)
            matchExpansionOfOldItem(old_root_child, root);
        
        // Try to restore the scroll position to where it was when we saved the tree
        // We use visualItemRect() for its side effect of forcing relayout, giving the
        // vertical scroller the right value range; I don't see an API for that
        if (browserTree->invisibleRootItem()->childCount() > 0)
        {
            browserTree->visualItemRect(browserTree->invisibleRootItem()->child(0));
            browserTree->verticalScrollBar()->setValue(old_scroll_position);
        }
        
        doingMatching = false;
        
        // Note that we hang on to the old expanded state; we will continue to use it until it is invalidated
    }
}

void QtSLiMVariableBrowser::matchExpansionOfOldItem(QTreeWidgetItem *itemToMatch, QTreeWidgetItem *parentToSearch)
{
    // The old tree item itemToMatch was expanded; we have been asked to find a matching item in parentToSearch and expand it
    QtSLiMBrowserItem *browserItemToMatch = dynamic_cast<QtSLiMBrowserItem *>(itemToMatch);
    
    if (browserItemToMatch && browserItemToMatch->childCount() > 0)
    {
        //qDebug() << "Old symbol" << browserItemToMatch->symbol_name << "was expanded, looking for match...";
        //qDebug() << "   parentToSearch ==" << parentToSearch << "and has" << parentToSearch->childCount() << "children";
        
        // Old state we're trying to match; note that the EidosValue here may contain freed objects!
        // We thus can't look inside the EidosValue to establish a match, nor do we want to use pointer
        // equality (the underlying object might have been dealloced and a new object alloced at the
        // same address; and in any case the same symbol might have changed address, as properties
        // would unless their value is cached).  Instead, to establish a match we use a precomputed
        // hash value that includes symbol name, element index, and object-element type.
        uint item_hash_to_match = browserItemToMatch->item_hash;
        
        // We consider a "match" to have an identical hash; it must also have children to expand
        for (int parentToSearchIndex = 0; parentToSearchIndex < parentToSearch->childCount(); parentToSearchIndex++)
        {
            QTreeWidgetItem *childItemToCheck = parentToSearch->child(parentToSearchIndex);
            QtSLiMBrowserItem *childBrowserItemToCheck = dynamic_cast<QtSLiMBrowserItem *>(childItemToCheck);
            
            if (childBrowserItemToCheck && childBrowserItemToCheck->has_children)
            {
                //qDebug() << "   checking child item with symbol" << childBrowserItemToCheck->symbol_name << "...";
                
                if (childBrowserItemToCheck->item_hash == item_hash_to_match)
                {
                    // We have a match, so we expand it
                    //qDebug() << "      MATCH: expanding...";
                    ui->browserTreeWidget->expandItem(childBrowserItemToCheck);
                    
                    // If the items involved are vectors with >1 child, expand to the right number of children
                    int targetChildCount = browserItemToMatch->childCount();
                    int currentChildCount = childBrowserItemToCheck->childCount();
                    
                    while (currentChildCount < targetChildCount)
                    {
                        QTreeWidgetItem *lastExpanded = childBrowserItemToCheck->child(currentChildCount - 1);
                        QtSLiMBrowserItem *lastBrowserItem = dynamic_cast<QtSLiMBrowserItem *>(lastExpanded);
                        
                        if (!lastBrowserItem || !lastBrowserItem->is_ellipsis)
                            break;
                        
                        expandEllipsisItem(lastBrowserItem);
                        currentChildCount = childBrowserItemToCheck->childCount();
                    }
                    
                    // Since this item was expanded, we might have further matches in need of expansion under it
                    for (int old_child_index = 0; old_child_index < browserItemToMatch->childCount(); old_child_index++)
                    {
                        QTreeWidgetItem *old_item_child = browserItemToMatch->child(old_child_index);
                        matchExpansionOfOldItem(old_item_child, childBrowserItemToCheck);
                    }
                }
                else
                {
                    //qDebug() << "      no match :  hashes are" << item_hash_to_match << "and" << childBrowserItemToCheck->item_hash;
                }
            }
        }
    }
}

void QtSLiMVariableBrowser::wipeEidosValuesFromSubtree(QTreeWidgetItem *item)
{
    QtSLiMBrowserItem *browserItem = dynamic_cast<QtSLiMBrowserItem *>(item);
    
    if (browserItem)
    {
        browserItem->eidos_value.reset();
        
        for (int child_index = 0; child_index < browserItem->childCount(); child_index++)
            wipeEidosValuesFromSubtree(browserItem->child(child_index));
    }
}

void QtSLiMVariableBrowser::appendIndexedItemsToItem(QtSLiMBrowserItem *browserItem, int startIndex)
{
    EidosValue *eidos_value = browserItem->eidos_value.get();
    
    if (eidos_value && (eidos_value->Type() == EidosValueType::kValueObject))
    {
        int elementCount = eidos_value->Count();
        int appendCount = std::max(10, startIndex);           // reveal twice as many items with each expansion
        int lastIndex = ((startIndex + appendCount - 1) > (elementCount - 1)) ? (elementCount - 1) : (startIndex + appendCount - 1);
        int index;
        
        for (index = startIndex; index <= lastIndex; ++index)
        {
            QString childName = QString("%1[%2]").arg(browserItem->symbol_name).arg(index);
            QtSLiMBrowserItem *childItem = new QtSLiMBrowserItem(childName, browserItem->eidos_value, index);   // elements refer to the main vector
            
            browserItem->addChild(childItem);
        }
        
        if (index < elementCount - 1)
        {
            QtSLiMBrowserItem *ellipsisItem = new QtSLiMBrowserItem("...", EidosValue_SP(nullptr), index, true);
            
            browserItem->addChild(ellipsisItem);
        }
    }
}

void QtSLiMVariableBrowser::itemExpanded(QTreeWidgetItem *item)
{
    clearSavedExpansionState();     // invalidate our saved expansion state
    
    QtSLiMBrowserItem *browserItem = dynamic_cast<QtSLiMBrowserItem *>(item);
    EidosValue *eidos_value = browserItem->eidos_value.get();
    int element_index = browserItem->element_index;
    
    if (eidos_value && (eidos_value->Type() == EidosValueType::kValueObject))
    {
        int elementCount = eidos_value->Count();
		
		// values which are of object type and contain more than one element get displayed as a list of elements
		if ((elementCount > 1) && (element_index == -1))
		{
            appendIndexedItemsToItem(browserItem, 0);
		}
        else if ((elementCount == 1) || (element_index != -1))
        {
            // display property values for either (a) a object vector of length 1, or (b) an element of an object vector
            // we used to display zero-length property values for zero-length object vectors, but don't any more
            int display_index = (element_index != -1) ? element_index : 0;
            EidosValue_Object *eidos_object_vector = static_cast<EidosValue_Object *>(eidos_value);
            EidosObjectElement *eidos_object = eidos_object_vector->ObjectElementAtIndex(display_index, nullptr);
			const EidosObjectClass *object_class = eidos_object->Class();
			const std::vector<EidosPropertySignature_CSP> *properties = object_class->Properties();
			size_t propertyCount = properties->size();
			bool oldSuppressWarnings = gEidosSuppressWarnings, inaccessibleCaught = false;
			
			gEidosSuppressWarnings = true;		// prevent warnings from questionable property accesses from producing warnings in the user's output pane
			
			for (size_t index = 0; index < propertyCount; ++index)
			{
				const EidosPropertySignature_CSP &propertySig = (*properties)[index];
				const std::string &symbolName = propertySig->property_name_;
				EidosGlobalStringID symbolID = propertySig->property_id_;
                QString symbolString = QString::fromStdString(symbolName);
				EidosValue_SP symbolValue;
				
				// protect against raises in property accesses due to inaccessible properties
				try {
					symbolValue = eidos_object->GetProperty(symbolID);
				} catch (...) {
					//std::cout << "caught inaccessible property " << symbolName << std::endl;
					inaccessibleCaught = true;
				}
				
                QtSLiMBrowserItem *childItem = new QtSLiMBrowserItem(symbolString, std::move(symbolValue));
				
                item->addChild(childItem);
			}
			
			gEidosSuppressWarnings = oldSuppressWarnings;
			
			if (inaccessibleCaught)
			{
				// throw away the raise message(s) so they don't confuse us
				gEidosTermination.clear();
				gEidosTermination.str(gEidosStr_empty_string);
			}
        }
    }
}

void QtSLiMVariableBrowser::itemCollapsed(QTreeWidgetItem *item)
{
    clearSavedExpansionState();     // invalidate our saved expansion state
    
    // Take the children from item and free them
    QList<QTreeWidgetItem *> children = item->takeChildren();
    
    while (!children.isEmpty())
        delete children.takeFirst();
}

void QtSLiMVariableBrowser::expandEllipsisItem(QtSLiMBrowserItem *browserItem)
{
    clearSavedExpansionState();     // invalidate our saved expansion state
    
    QTreeWidgetItem *parentItem = browserItem->parent();
    QtSLiMBrowserItem *parentBrowserItem = dynamic_cast<QtSLiMBrowserItem *>(parentItem);
    
    if (parentBrowserItem)
    {
        appendIndexedItemsToItem(parentBrowserItem, browserItem->element_index);
        parentBrowserItem->removeChild(browserItem);
        delete browserItem;
    }
}

void QtSLiMVariableBrowser::itemClicked(QTreeWidgetItem *item, int /* column */)
{
    // If the item is an ellipsis item, expand it to show more items
    QtSLiMBrowserItem *browserItem = dynamic_cast<QtSLiMBrowserItem *>(item);
    
    if (browserItem)
    {
        if (browserItem->is_ellipsis)
        {
            expandEllipsisItem(browserItem);
        }
        else if (browserItem->isExpanded())
        {
            ui->browserTreeWidget->collapseItem(browserItem);
        }
        else    // collapsed
        {
            ui->browserTreeWidget->expandItem(browserItem);
        }
    }
}

void QtSLiMVariableBrowser::clearSavedExpansionState(void)
{
    // We want to save the expansion state only when the user changes it.  That way,
    // we will re-expand correctly even after a recycle and several steps; we'll
    // remember what used to be open until the user changes it.
    
    // Matching involves doing various expansions; that should not trigger a discard
    // of the saved state, because it doesn't come from a user action
    if (doingMatching)
        return;
    
    while (!old_children.isEmpty())
        delete old_children.takeFirst();
}

void QtSLiMVariableBrowser::scrollerChanged(void)
{
    if (doingMatching)
        return;
    
    // If the user drags the scroller to a new position, we want to remember and restore
    // that new position even if we're restoring a tree that was saved earlier
    old_scroll_position = ui->browserTreeWidget->verticalScrollBar()->value();
}
































