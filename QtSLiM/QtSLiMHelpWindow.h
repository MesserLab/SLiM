//
//  QtSLiMHelpWindow.h
//  SLiM
//
//  Created by Ben Haller on 11/19/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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

#ifndef QTSLIMHELPWINDOW_H
#define QTSLIMHELPWINDOW_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextDocumentFragment>
#include <QMap>
#include <QStyledItemDelegate>

#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

class QCloseEvent;
class QSplitter;
class EidosObjectClass;


// SLiMgui uses an NSDictionary-based design to hold the documentation tree data structure.  Here we
// instead leverage the fact that QTreeWidgetItem already represents a tree structure, and simply
// use it directly to represent the documentation tree.  We use a custom subclass so we can keep
// associated doc, which is held as a QTextDocumentFragment; this is used only by leaves

class QtSLiMHelpItem : public QTreeWidgetItem
{
    // no Q_OBJECT; QTreeWidgetItem is not a QObject subclass!
    
public:
    QTextDocumentFragment *doc_fragment = nullptr;
    
    QtSLiMHelpItem(QTreeWidget *parent) : QTreeWidgetItem(parent) {}
    QtSLiMHelpItem(QTreeWidgetItem *parent) : QTreeWidgetItem(parent) {}
    virtual ~QtSLiMHelpItem() override;
};


// This subclass of QStyledItemDelegate provides custom drawing for the outline view.

class QtSLiMHelpOutlineDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    
public:
    QtSLiMHelpOutlineDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    virtual ~QtSLiMHelpOutlineDelegate(void) override;
    
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


// We keep a QMap of topics in the currently building hierarchy so we can find the right parent
// for each new item.  This is a temporary data structure used only during the build of each section.
typedef QMap<QString, QtSLiMHelpItem *> QtSLiMTopicMap;


// This class provides a singleton help window

namespace Ui {
class QtSLiMHelpWindow;
}

class QtSLiMHelpWindow : public QWidget
{
    Q_OBJECT
    
public:
    static QtSLiMHelpWindow &instance(void);
    
    void enterSearchForString(QString searchString, bool titlesOnly = true);
    
private:
    // singleton pattern
    explicit QtSLiMHelpWindow(QWidget *parent = nullptr);
    QtSLiMHelpWindow(void) = delete;
    virtual ~QtSLiMHelpWindow(void) override;
    QtSLiMHelpWindow(const QtSLiMHelpWindow&) = delete;
    QtSLiMHelpWindow& operator=(const QtSLiMHelpWindow&) = delete;
    
    // Add topics and items drawn from a specially formatted HTML file, under a designated top-level heading.
    // The signatures found for functions, methods, and prototypes will be checked against the supplied lists,
    // if they are not NULL, and if matches are found, colorized versions will be substituted.
    void addTopicsFromRTFFile(const QString &htmlFile,
                              const QString &topLevelHeading,
                              const std::vector<EidosFunctionSignature_CSP> *functionList,
                              const std::vector<EidosMethodSignature_CSP> *methodList,
                              const std::vector<EidosPropertySignature_CSP> *propertyList);
    
    const std::vector<EidosPropertySignature_CSP> *slimguiAllPropertySignatures(void);
    const std::vector<EidosMethodSignature_CSP> *slimguiAllMethodSignatures(void);
    
    // Searching
    bool findItemsMatchingSearchString(QTreeWidgetItem *root, const QString searchString, bool titlesOnly, std::vector<QTreeWidgetItem *> &matchKeys, std::vector<QTreeWidgetItem *> &expandItems);
    void searchFieldChanged(void);
    void searchScopeToggled(void);
    
    // Smart expand/contract, with the option key making it apply to all children as well
    void recursiveExpand(QTreeWidgetItem *item);
    void recursiveCollapse(QTreeWidgetItem *item);
    void itemClicked(QTreeWidgetItem *item, int column);
    void itemCollapsed(QTreeWidgetItem *item);
    void itemExpanded(QTreeWidgetItem *item);
    
    // Check for complete documentation
    QtSLiMHelpItem *findObjectWithKeySuffix(const QString searchKeySuffix, QTreeWidgetItem *searchItem);
    QtSLiMHelpItem *findObjectForKeyEqualTo(const QString searchKey, QTreeWidgetItem *searchItem);
    void checkDocumentationOfFunctions(const std::vector<EidosFunctionSignature_CSP> *functions);
    void checkDocumentationOfClass(EidosObjectClass *classObject);
    
    // responding to events
    virtual void closeEvent(QCloseEvent *e) override;
    void outlineSelectionChanged(void);
    
    // Internals
    QTreeWidgetItem *parentItemForSection(const QString &sectionString, QtSLiMTopicMap &topics, QtSLiMHelpItem *topItem);
    QtSLiMHelpItem *createItemForSection(const QString &sectionString, QString title, QtSLiMTopicMap &topics, QtSLiMHelpItem *topItem);
    
private:
    int searchType = 0;		// 0 == Title, 1 == Content; equal to the tags on the search type menu items
    bool doingProgrammaticCollapseExpand = false;   // used to distinguish user actions from programmatic ones
    bool doingProgrammaticSelection = false;        // used to distinguish user actions from programmatic ones
    
    void interpolateSplitters(void);
    QSplitter *splitter = nullptr;
    
    Ui::QtSLiMHelpWindow *ui;
};


#endif // QTSLIMHELPWINDOW_H































