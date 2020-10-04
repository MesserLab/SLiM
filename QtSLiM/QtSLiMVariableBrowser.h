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

#ifndef QTSLIMVARIABLEBROWSER_H
#define QTSLIMVARIABLEBROWSER_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QStyledItemDelegate>

#include "eidos_value.h"

class QCloseEvent;
class QtSLiMEidosConsole;


// A QTreeWidgetItem subclass that keeps associated information

class QtSLiMBrowserItem : public QTreeWidgetItem
{
    // no Q_OBJECT; QTreeWidgetItem is not a QObject subclass!
    
public:
    QtSLiMBrowserItem(QString name, EidosValue_SP value) : QtSLiMBrowserItem(name, value, -1) {}
    QtSLiMBrowserItem(QString name, EidosValue_SP value, int index) : QtSLiMBrowserItem(name, value, index, false) {}
    QtSLiMBrowserItem(QString name, EidosValue_SP value, int index, bool isEllipsis);
    virtual ~QtSLiMBrowserItem(void) override;
    
    virtual QVariant data(int column, int role) const override;
    
    QString symbol_name;            // the name as displayed in the browser
    EidosValue_SP eidos_value;      // the EidosValue referred to by this item (perhaps just one element of it)
    int element_index;              // -1 if this item refers to the whole value; otherwise, an element index
    uint item_hash;                 // a precomputed hash value that can be used to confirm that items match
    bool is_eidos_constant;         // true if this is one of the built-in Eidos constants; cached for speed
    bool is_ellipsis;               // true if this item is a "..." representing more undisplayed elements
    bool has_children;              // true if this item has children (which might not be created yet)
};


// This subclass of QStyledItemDelegate provides custom drawing for the outline view.

class QtSLiMVariableBrowserDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    
public:
    QtSLiMVariableBrowserDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    virtual ~QtSLiMVariableBrowserDelegate(void) override;
    
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


namespace Ui {
class QtSLiMVariableBrowser;
}

class QtSLiMVariableBrowser : public QWidget
{
    Q_OBJECT
    
public:
    QtSLiMEidosConsole *parentEidosConsole = nullptr;     // a copy of parent with the correct class, for convenience
    
    explicit QtSLiMVariableBrowser(QtSLiMEidosConsole *parent = nullptr);
    virtual ~QtSLiMVariableBrowser() override;
    
    void reloadBrowser(bool nowValidState);
    
public slots:
    void itemExpanded(QTreeWidgetItem *item);
    void itemCollapsed(QTreeWidgetItem *item);
    void itemClicked(QTreeWidgetItem *item, int column);
    
signals:
    void willClose(void);
    
private slots:
    virtual void closeEvent(QCloseEvent *event) override;
    
private:
    Ui::QtSLiMVariableBrowser *ui;
    
    void appendIndexedItemsToItem(QtSLiMBrowserItem *browserItem, int startIndex);
    void matchExpansionOfOldItem(QTreeWidgetItem *itemToMatch, QTreeWidgetItem *parentToSearch);
    void expandEllipsisItem(QtSLiMBrowserItem *browserItem);
    void clearSavedExpansionState(void);
    void wipeEidosValuesFromSubtree(QTreeWidgetItem *item);
    void scrollerChanged(void);
    
    QList<QTreeWidgetItem *> old_children;  // a saved tree that we will try to match next reload
    int old_scroll_position = 0;            // a saved scroll position, parallel to old_children
    bool doingMatching = false;
};


#endif // QTSLIMVARIABLEBROWSER_H


































