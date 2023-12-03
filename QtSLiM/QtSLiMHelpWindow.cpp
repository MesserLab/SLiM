//
//  QtSLiMHelpWindow.cpp
//  SLiM
//
//  Created by Ben Haller on 11/19/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
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

#include "QtSLiMHelpWindow.h"
#include "ui_QtSLiMHelpWindow.h"

#include <QLineEdit>
#include <QIcon>
#include <QDebug>
#include <QSettings>
#include <QCloseEvent>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QRegularExpression>
#include <QTextCursor>

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "community.h"

#include "QtSLiMExtras.h"
#include "QtSLiMAppDelegate.h"

#include <vector>
#include <algorithm>
#include <utility>
#include <string>


//
// This is our custom outline item class, which can hold a QTextDocumentFragment
//
QtSLiMHelpItem::~QtSLiMHelpItem()
{
}

QVariant QtSLiMHelpItem::data(int column, int role) const
{
    if (is_top_level && (role == Qt::ForegroundRole))
    {
        bool inDarkMode = QtSLiMInDarkMode();
        
        if (inDarkMode)
            return QBrush(QtSLiMColorWithWhite(0.8, 1.0));
        else
            return QBrush(QtSLiMColorWithWhite(0.4, 1.0));
    }
    
    return QTreeWidgetItem::data(column, role);
}


//
// This subclass of QStyledItemDelegate provides custom drawing for the outline view.
//
QtSLiMHelpOutlineDelegate::~QtSLiMHelpOutlineDelegate(void)
{
}

void QtSLiMHelpOutlineDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString itemString = index.data().toString();
    
    itemString = itemString.replace(QChar::Nbsp, ' ');  // standardize down to regular spaces
    
    bool topLevel = !index.parent().isValid();
    QRect fullRect = option.rect;
    fullRect.setLeft(0);             // we are not clipped; we will draw outside our rect to occupy the full width of the view
    
    // if we're a top-level item, wash a background color over our row; we use alpha so the disclosure triangle remains visible
    if (topLevel)
    {
        bool inDarkMode = QtSLiMInDarkMode();
        bool isEidos = index.data().toString().startsWith("Eidos ");
        
        if (isEidos)
            painter->fillRect(fullRect, QBrush(inDarkMode ? QtSLiMColorWithRGB(0.0, 1.0, 0.0, 0.10) : QtSLiMColorWithRGB(0.0, 1.0, 0.0, 0.04)));       // pale green background for Eidos docs
        else
            painter->fillRect(fullRect, QBrush(inDarkMode ? QtSLiMColorWithRGB(0.0, 0.0, 1.0, 0.15) : QtSLiMColorWithRGB(0.0, 0.0, 1.0, 0.04)));       // pale blue background for SLiM docs
    }
    
    // On Ubuntu, items get shown as having "focus" even when they're not selectable, which I dislike; this disables that appearance
    // See https://stackoverflow.com/a/2061871/2752221
    QStyleOptionViewItem modifiedOption(option);
    if (modifiedOption.state & QStyle::State_HasFocus)
        modifiedOption.state = modifiedOption.state ^ QStyle::State_HasFocus;
    
    // then let super draw
    QStyledItemDelegate::paint(painter, modifiedOption, index);
    
    // custom overdraw
    if (topLevel)
    {
        // if an item is top-level, we want to frame it to look heavier, like a "group item" on macOS
        bool inDarkMode = QtSLiMInDarkMode();
        
        painter->fillRect(QRect(fullRect.left(), fullRect.top(), fullRect.width(), 1), QtSLiMColorWithWhite(inDarkMode ? 0.15 : 0.85, 1.0));        // top edge in light gray
        painter->fillRect(QRect(fullRect.left(), fullRect.top() + fullRect.height() - 1, fullRect.width(), 1), QtSLiMColorWithWhite(inDarkMode ? 0.05 : 0.65, 1.0));     // bottom edge in medium gray
    }
    else
    {
        // otherwise, add a color box on the right for the items that need them
        static QStringList *stringsWF = nullptr;
        static QStringList *stringsNonWF = nullptr;
        static QStringList *stringsNucmut = nullptr;
        
        if (!stringsWF)
            stringsWF = new QStringList({"– addSubpopSplit()",
                                         "– registerMateChoiceCallback()",
                                         "cloningRate =>",
                                         "immigrantSubpopFractions =>",
                                         "immigrantSubpopIDs =>",
                                         "selfingRate =>",
                                         "sexRatio =>",
                                         "– setCloningRate()",
                                         "– setMigrationRates()",
                                         "– setSelfingRate()",
                                         "– setSexRatio()",
                                         "– setSubpopulationSize()",
                                         "mateChoice() callbacks"
                                        });
        
        if (!stringsNonWF)
            stringsNonWF = new QStringList({"initializeSLiMModelType()",
                              "age =>",
                              "modelType =>",
                              "– registerReproductionCallback()",
                              "– registerSurvivalCallback()",
                              "– addCloned()",
                              "– addCrossed()",
                              "– addEmpty()",
                              "– addRecombinant()",
                              "– addSelfed()",
                              "– removeSubpopulation()",
                              "– killIndividuals()",
                              "– takeMigrants()",
                              "reproduction() callbacks",
                              "survival() callbacks"
                              });
        
        if (!stringsNucmut)
            stringsNucmut = new QStringList({"initializeAncestralNucleotides()",
                               "initializeMutationTypeNuc()",
                               "initializeHotspotMap()",
                               "codonsToAminoAcids()",
                               "randomNucleotides()",
                               "mm16To256()",
                               "mmJukesCantor()",
                               "mmKimura()",
                               "nucleotideCounts()",
                               "nucleotideFrequencies()",
                               "nucleotidesToCodons()",
                               "codonsToNucleotides()",
                               "nucleotideBased =>",
                               "nucleotide <–>",
                               "nucleotideValue <–>",
                               "mutationMatrix =>",
                               "– setMutationMatrix()",
                               "– ancestralNucleotides()",
                               "– setAncestralNucleotides()",
                               "– nucleotides()",
                               "hotspotEndPositions =>",
                               "hotspotEndPositionsF =>",
                               "hotspotEndPositionsM =>",
                               "hotspotMultipliers =>",
                               "hotspotMultipliersF =>",
                               "hotspotMultipliersM =>",
                               "– setHotspotMap()"
                               });
        
        bool draw_WF_box = stringsWF->contains(itemString);
        bool draw_nonWF_box = stringsNonWF->contains(itemString);
        bool draw_nucmut_box = stringsNucmut->contains(itemString);
        
        if (draw_WF_box || draw_nonWF_box || draw_nucmut_box)
        {
            QRect boxRect = QRect(fullRect.left() + fullRect.width() - 14, fullRect.top() + 4, 8, 8);
            QColor boxColor;
            
            if (draw_WF_box)
                boxColor = QColor(66, 255, 53);         // WF-only color (green)
            else if (draw_nonWF_box)
                boxColor = QColor(88, 148, 255);        // nonWF-only color (blue)
            else //if (draw_nucmut_box)
                boxColor = QColor(228, 118, 255);       // nucmut color (purple)
            
            painter->fillRect(boxRect, boxColor);
            QtSLiMFrameRect(boxRect, Qt::black, *painter);
        }
    }
}


//
// This is the QWidget subclass for the help window, which does the heavy lifting of building the doc outline from HTML files
//
QtSLiMHelpWindow &QtSLiMHelpWindow::instance(void)
{
    static QtSLiMHelpWindow *inst = nullptr;
    
    if (!inst)
        inst = new QtSLiMHelpWindow(nullptr);
    
    return *inst;
}

QtSLiMHelpWindow::QtSLiMHelpWindow(QWidget *p_parent) : QWidget(p_parent, Qt::Window), ui(new Ui::QtSLiMHelpWindow)
{
    ui->setupUi(this);
    interpolateSplitters();
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // Configure the search field to look like a search field
    ui->searchField->setClearButtonEnabled(true);
    ui->searchField->setPlaceholderText("Search...");
    
    connect(ui->searchField, &QLineEdit::returnPressed, this, &QtSLiMHelpWindow::searchFieldChanged);
    connect(ui->searchScopeButton, &QPushButton::clicked, this, &QtSLiMHelpWindow::searchScopeToggled);
    
    // Change the search scope button's appearance based on the app palette
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, &QtSLiMHelpWindow::applicationPaletteChanged);
    applicationPaletteChanged();
    
    // Configure the outline view to behave as we wish
    connect(ui->topicOutlineView, &QTreeWidget::itemSelectionChanged, this, &QtSLiMHelpWindow::outlineSelectionChanged);
    connect(ui->topicOutlineView, &QTreeWidget::itemClicked, this, &QtSLiMHelpWindow::itemClicked);
    connect(ui->topicOutlineView, &QTreeWidget::itemCollapsed, this, &QtSLiMHelpWindow::itemCollapsed);
    connect(ui->topicOutlineView, &QTreeWidget::itemExpanded, this, &QtSLiMHelpWindow::itemExpanded);
    
    QAbstractItemDelegate *outlineDelegate = new QtSLiMHelpOutlineDelegate(ui->topicOutlineView);
    ui->topicOutlineView->setItemDelegate(outlineDelegate);
    
    // tweak appearance on Linux; the form is adjusted for macOS
#if defined(__linux__)
    {
        // use a smaller font for the outline
        QFont outlineFont(ui->topicOutlineView->font());
        outlineFont.setPointSizeF(outlineFont.pointSizeF() - 1);
        ui->topicOutlineView->setFont(outlineFont);
        
        // the headers/content button needs somewhat different metrics
        ui->searchScopeButton->setMinimumWidth(75);
        ui->searchScopeButton->setMaximumWidth(75);
    }
#endif
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMHelpWindow");
    resize(settings.value("size", QSize(550, 400)).toSize());
    move(settings.value("pos", QPoint(25, 45)).toPoint());
    settings.endGroup();
    
    // Add Eidos topics
    std::vector<EidosPropertySignature_CSP> builtin_properties = EidosClass::RegisteredClassProperties(true, false);
    std::vector<EidosMethodSignature_CSP> builtin_methods = EidosClass::RegisteredClassMethods(true, false);
    
    addTopicsFromRTFFile("EidosHelpFunctions", "Eidos Functions", &EidosInterpreter::BuiltInFunctions(), nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpClasses", "Eidos Classes", &EidosInterpreter::BuiltInFunctions(), &builtin_methods, &builtin_properties);    // constructors are in BuiltInFunctions()
    addTopicsFromRTFFile("EidosHelpOperators", "Eidos Operators", nullptr, nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpStatements", "Eidos Statements", nullptr, nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpTypes", "Eidos Types", nullptr, nullptr, nullptr);
    
    // Check for completeness of the Eidos documentation
    checkDocumentationOfFunctions(&EidosInterpreter::BuiltInFunctions());
    
    for (EidosClass *class_object : EidosClass::RegisteredClasses(true, false))
    {
        const std::string &element_type = class_object->ClassName();
        
        if (!Eidos_string_hasPrefix(element_type, "_") && (element_type != "DictionaryBase"))		// internal classes are undocumented
        {
            checkDocumentationOfClass(class_object);
            addSuperclassItemForClass(class_object);
        }
    }
    
    // Add SLiM topics
    std::vector<EidosPropertySignature_CSP> context_properties = EidosClass::RegisteredClassProperties(false, true);
    std::vector<EidosMethodSignature_CSP> context_methods = EidosClass::RegisteredClassMethods(false, true);
    const std::vector<EidosFunctionSignature_CSP> *zg_functions = Community::ZeroTickFunctionSignatures();
    const std::vector<EidosFunctionSignature_CSP> *slim_functions = Community::SLiMFunctionSignatures();
    std::vector<EidosFunctionSignature_CSP> all_slim_functions;
    
    all_slim_functions.insert(all_slim_functions.end(), zg_functions->begin(), zg_functions->end());
    all_slim_functions.insert(all_slim_functions.end(), slim_functions->begin(), slim_functions->end());
    
    addTopicsFromRTFFile("SLiMHelpFunctions", "SLiM Functions", &all_slim_functions, nullptr, nullptr);
    addTopicsFromRTFFile("SLiMHelpClasses", "SLiM Classes", nullptr, &context_methods, &context_properties);
    addTopicsFromRTFFile("SLiMHelpCallbacks", "SLiM Events and Callbacks", nullptr, nullptr, nullptr);
    
    // Check for completeness of the SLiM documentation
    checkDocumentationOfFunctions(&all_slim_functions);
    
    for (EidosClass *class_object : EidosClass::RegisteredClasses(false, true))
    {
        const std::string &element_type = class_object->ClassName();
        
        if (!Eidos_string_hasPrefix(element_type, "_"))		// internal classes are undocumented
        {
            checkDocumentationOfClass(class_object);
            addSuperclassItemForClass(class_object);
        }
    }
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

void QtSLiMHelpWindow::applicationPaletteChanged(void)
{
    bool inDarkMode = QtSLiMInDarkMode();
    
    // Custom colors for the search mode button for dark mode vs. light mode; note that this completely overrides the style sheet in the .ui file!
    if (inDarkMode)
        ui->searchScopeButton->setStyleSheet("QPushButton { border: 1px solid #888; border-radius: 20px; border-style: outset; margin: 0px; padding: 2px; background: rgb(125, 125, 125); } QPushButton:pressed { background: rgb(105, 105, 105); }");
    else
        ui->searchScopeButton->setStyleSheet("QPushButton { border: 1px solid #888; border-radius: 20px; border-style: outset; margin: 0px; padding: 2px; background: rgb(245, 245, 245); } QPushButton:pressed { background: rgb(195, 195, 195); }");
}

QtSLiMHelpWindow::~QtSLiMHelpWindow()
{
    delete ui;
}

void QtSLiMHelpWindow::interpolateSplitters(void)
{
#if 1
    // add a top-level horizontal splitter
    
    QLayout *parentLayout = ui->horizontalLayout;
    QWidget *firstWidget = ui->topicOutlineView;
    QWidget *secondWidget = ui->descriptionTextEdit;
    
    // force geometry calculation, which is lazy
    setAttribute(Qt::WA_DontShowOnScreen, true);
    show();
    hide();
    setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // change fixed-size views to be flexible, so they cooperate with the splitter
    firstWidget->setMinimumWidth(200);
    firstWidget->setMaximumWidth(400);
    
    // empty out parentLayout
    QLayoutItem *child;
    while ((child = parentLayout->takeAt(0)) != nullptr);
    
    // make the QSplitter between the left and right and add the subsidiary widgets to it
    splitter = new QSplitter(Qt::Horizontal, this);
    
    splitter->addWidget(firstWidget);
    splitter->addWidget(secondWidget);
    splitter->setHandleWidth(splitter->handleWidth() + 3);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);    // initially, give 2/3 of the width to the description textedit
    splitter->setCollapsible(0, true);
    splitter->setCollapsible(1, false);
    
    // and finally, add the splitter to the parent layout
    parentLayout->addWidget(splitter);
    parentLayout->setContentsMargins(0, 0, 0, 0);
#endif
}

bool QtSLiMHelpWindow::findItemsMatchingSearchString(QTreeWidgetItem *root, const QString searchString, bool titlesOnly, std::vector<QTreeWidgetItem *> &matchKeys, std::vector<QTreeWidgetItem *> &expandItems)
{
	bool anyChildMatches = false;
	
    for (int child_index = 0; child_index < root->childCount(); ++child_index)
	{
        QTreeWidgetItem *childItem = root->child(child_index);
        
		if (childItem->childCount() > 0)
		{
			// Recurse through the child's children
			bool result = findItemsMatchingSearchString(childItem, searchString, titlesOnly, matchKeys, expandItems);
			
			if (result)
				anyChildMatches = true;
		}
		else if (childItem->childIndicatorPolicy() == QTreeWidgetItem::DontShowIndicatorWhenChildless)
		{
			// If the item has no children, and is not showing an indicator, it is a leaf and should be searched
			bool isMatch = false;
            const QString itemText = childItem->text(0);
            
            if (itemText.contains(searchString, Qt::CaseInsensitive))
				isMatch = true;
			
			if (!titlesOnly)
			{
                QtSLiMHelpItem *helpItem = dynamic_cast<QtSLiMHelpItem *>(childItem);
                
                if (helpItem)
                {
                    QTextDocumentFragment *helpFragment = helpItem->doc_fragment;
                    
                    if (helpFragment)
                    {
                        const QString helpText = helpFragment->toPlainText();
                        
                        if (helpText.contains(searchString, Qt::CaseInsensitive))
                            isMatch = true;
                    }
                }
			}
			
			if (isMatch)
			{
				matchKeys.emplace_back(childItem);
				anyChildMatches = true;
			}
		}
	}
	
	if (anyChildMatches)
		expandItems.emplace_back(root);
	
	return anyChildMatches;
}

void QtSLiMHelpWindow::expandToShowItems(const std::vector<QTreeWidgetItem *> &expandItems, const std::vector<QTreeWidgetItem *> &matchKeys)
{
    // Coalesce the selection change to avoid obsessively re-generating the documentation textedit
    doingProgrammaticSelection = true;
    doingProgrammaticCollapseExpand = true;
    
    // Deselect and collapse everything, as an initial state
    ui->topicOutlineView->setCurrentItem(nullptr, 0, QItemSelectionModel::Clear);
    recursiveCollapse(ui->topicOutlineView->invisibleRootItem());   // collapseAll() only collapses items that are visible!
    
    // Expand all nodes that have a search hit; reverse order so parents expand before their children
    for (auto iter = expandItems.rbegin(); iter != expandItems.rend(); ++iter)
        ui->topicOutlineView->expandItem(*iter);
    
    // Select all of the items that matched
    for (auto item : matchKeys)
        ui->topicOutlineView->setCurrentItem(item, 0, QItemSelectionModel::Select);
    
    // Finish coalescing selection changes
    doingProgrammaticCollapseExpand = false;
    doingProgrammaticSelection = false;
    outlineSelectionChanged();
}

void QtSLiMHelpWindow::searchFieldChanged(void)
{
    QString searchString = ui->searchField->text();
    
    ui->searchField->selectAll();
    
    if (searchString.length())
    {
        // Do a depth-first search under the topic root that matches the search pattern, and gather tasks to perform
        std::vector<QTreeWidgetItem *> matchKeys;
        std::vector<QTreeWidgetItem *> expandItems;
        
        findItemsMatchingSearchString(ui->topicOutlineView->invisibleRootItem(), searchString, (searchType == 0), matchKeys, expandItems);
        
        if (matchKeys.size())
            expandToShowItems(expandItems, matchKeys);
        else
            qApp->beep();
    }
}

void QtSLiMHelpWindow::searchScopeToggled(void)
{
    searchType = 1 - searchType;
    
    if (searchType == 0)
        ui->searchScopeButton->setText("🔍  headers");
    else if (searchType == 1)
        ui->searchScopeButton->setText("🔍  content");
    
    searchFieldChanged();
}

void QtSLiMHelpWindow::enterSearchForString(QString searchString, bool titlesOnly)
{
	// Show our window and bring it front
    show();
    raise();
    activateWindow();
	
	// Set the search string per the request
    ui->searchField->setText(searchString);
	
	// Set the search type per the request
    int desiredSearchType = titlesOnly ? 0 : 1;
	
    if (searchType != desiredSearchType)
        searchScopeToggled();   // re-runs the search as a side effect
	else
        searchFieldChanged();   // re-run explicitly
}

void QtSLiMHelpWindow::closeEvent(QCloseEvent *p_event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMHelpWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // use super's default behavior
    QWidget::closeEvent(p_event);
}

// This is a helper method for addTopicsFromRTFFile:... that finds the right parent item to insert a given section index under.
// This method makes a lot of assumptions about the layout of the RTF file, such as that section number proceeds in sorted order.
QTreeWidgetItem *QtSLiMHelpWindow::parentItemForSection(const QString &sectionString, QtSLiMTopicMap &topics, QtSLiMHelpItem *topItem)
{
    QStringList sectionComponents = sectionString.split('.');
	int sectionCount = sectionComponents.size();
	
	if (sectionCount <= 1)
	{
        // With an empty section string, or a whole-number section like "3", the parent is the top item
		return topItem;
	}
	else
	{
		// We have a section string like "3.1" or "3.1.2"; we want to look for a parent to add it to – like "3" or "3.1", respectively
		sectionComponents.pop_back();
		
		QString parentSectionString = sectionComponents.join('.');
		auto parentTopicDictIter = topics.find(parentSectionString);
		
		if (parentTopicDictIter != topics.end())
			return parentTopicDictIter.value();     // Found a parent to add to
		else
			return topItem;                         // Couldn't find a parent to add to, so the parent is the top item
	}
}

// This is a helper method for addTopicsFromRTFFile:... that creates a new QTreeWidgetItem under which items will be placed, and finds the right parent
// item to insert it under.  This method makes a lot of assumptions about the layout of the RTF file, such as that section number proceeds in sorted order.
QtSLiMHelpItem *QtSLiMHelpWindow::createItemForSection(const QString &sectionString, QString title, QtSLiMTopicMap &topics, QtSLiMHelpItem *topItem)
{
	static const QString functions(" functions");
	
    if (title.endsWith(functions))
        title.chop(functions.length());
	
    QStringList sectionComponents = sectionString.split('.');
	QTreeWidgetItem *parentItem = parentItemForSection(sectionString, topics, topItem);
    QtSLiMHelpItem *newItem = new QtSLiMHelpItem(parentItem);
	
    newItem->setText(0, title);
    newItem->setFlags(Qt::ItemIsEnabled);
    newItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    
    topics.insert(sectionString, newItem);
	
	return newItem;
}

// This is the main RTF doc file reading method; it finds an RTF file of a given name in the main bundle, reads it into an attributed string, and then scans
// that string for topic headings, function/method/property signature lines, etc., and creates a hierarchy of help topics from the results.  This process
// assumes that the RTF doc file is laid out in a standard way that fits the regex patterns used here; it is designed to work directly with content copied
// and pasted out of our Word documentation files into RTF in TextEdit.
void QtSLiMHelpWindow::addTopicsFromRTFFile(const QString &htmlFile,
                                            const QString &topLevelHeading,
                                            const std::vector<EidosFunctionSignature_CSP> *functionList,
                                            const std::vector<EidosMethodSignature_CSP> *methodList,
                                            const std::vector<EidosPropertySignature_CSP> *propertyList)
{
    QString topicFilePath = QString(":/help/") + htmlFile + QString(".html");
    QTextDocument topicFileTextDocument;
    
    // Read the HTML resource in and create a QTextDocument
    {
        QFile topicFile(topicFilePath);
        QString topicFileData;
        
        if (!topicFile.open(QIODevice::ReadOnly | QIODevice::Text)) {   // QIODevice::Text converts line endings to \n, making Windows happy; harmless
            qDebug() << "QtSLiMHelpWindow::addTopicsFromRTFFile(): could not find HTML file " << htmlFile;
            return;
        }
        
        topicFileData = topicFile.readAll();
        topicFile.close();
        
        // OK, so this is gross and probably fragile.  Our HTML content has colors set on the text in some places: sometimes for syntax coloring
        // of code snippets, but also just setting text explicitly to black for no apparent reason.  This doesn't translate well to dark mode;
        // the syntax coloring uses colors that are fairly illegible against black, and the black text of course ends up black-on-black.  We thus
        // need to strip off all color information.  Ideally, we'd do this on the QTextDocument, but I don't know how to do that (see question at
        // https://stackoverflow.com/questions/65779196/how-to-remove-all-text-color-attributes-from-a-qtextdocument).  So instead, here we scan
        // the HTML source code for color attributes and remove them, with a regex.  (What could possibly go wrong???)
        // BCH 5/7/2021: This regex worked well, but a better solution appears to be the code below.  We'll see whether it causes any problems.
        // BCH 12/12/2021: Well, that code below did cause problems: it caused the formatting to be lost from "SLiM Events and Callbacks" and
        // "Eidos Statements" for no apparent reason.  So, reverting to this regex, which has had no observed problems thus far.
        static const QRegularExpression htmlColorRegex("(;? ?color: ?#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f])");
        
        topicFileData.replace(htmlColorRegex, "");
        
        // FIXME while on the topic of dark mode, there is a subtle bug in the help window's handling of it.  We call ColorizeCallSignature() and
        // ColorizePropertySignature() below to colorize property/function/method signatures.  Those produce colorized strings tailored to the
        // mode we're currently in: light mode or dark mode.  If the user then switches modes, those strings are incorrectly colored.  This is
        // not a disaster - the color schemes are not that different - but it's a bug.  Ideally, we'd either do the colorizing dynamically at
        // display time, or we'd reload the whole help document when the display mode switches.  Both are rather difficult to do, however.
        
        // Create the QTextDocument from the HTML
        topicFileTextDocument.setHtml(topicFileData);
        
        // Get rid of foreground and background colors set on the text; the original solution was the regex commented out above, but this seems
        // to work just as well, and is less icky/scary.  This solution is from https://stackoverflow.com/a/67384128/2752221.
        // BCH 12/12/2021: This solution caused loss of text formatting in some cases; reverting to the regex solution above.  Oh well.
        /*QTextCharFormat defaultFormat = QTextCharFormat();
        QTextCursor tc(&topicFileTextDocument);
        tc.select(QTextCursor::Document);
        QTextCharFormat docFormat = tc.charFormat();
        docFormat.setBackground(defaultFormat.background());
        docFormat.setForeground(defaultFormat.foreground());
        tc.mergeCharFormat(docFormat);*/
    }
    
    // Create the topic map for the section being parsed; this keeps track of numbered sections so we can find where children go
    QtSLiMTopicMap topics;				// keys are strings like 3.1 or 3.1.2 or whatever
    
    // Create the top-level item for the section we're parsing; note that QtSLiMHelpOutlineDelegate does additional display customization
    QtSLiMHelpItem *topItem = new QtSLiMHelpItem(ui->topicOutlineView);
    topItem->setText(0, topLevelHeading);
    topItem->setFlags(Qt::ItemIsEnabled);
    topItem->setForeground(0, QBrush(QtSLiMColorWithWhite(0.4, 1.0)));
    topItem->setSizeHint(0, QSize(20.0, 20.0));
    topItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    QFont itemFont(topItem->font(0));
    itemFont.setBold(true);
    topItem->setFont(0, itemFont);
    topItem->is_top_level = true;
    
    // Set up the current topic item that we are appending content into
    QtSLiMHelpItem *currentTopicItem = topItem;
	QString topicItemKey;
	QTextCursor *topicItemCursor = nullptr;
    
    // Make regular expressions that we will use below
    static const QRegularExpression topicHeaderRegex("^((?:[0-9]+\\.)*[0-9]+)\\.?[\u00A0 ] (.+)$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression topicGenericItemRegex("^((?:[0-9]+\\.)*[0-9]+)\\.?[\u00A0 ] ITEM: ((?:[0-9]+\\.? )?)(.+)$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression topicFunctionRegex("^\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression topicMethodRegex("^([-–+])[\u00A0 ]\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression topicPropertyRegex("^([a-zA-Z_0-9]+)[\u00A0 ]((?:<[-–]>)|(?:=>)) \\([a-zA-Z<>\\*+$]+\\)$", QRegularExpression::CaseInsensitiveOption);
	
    if (!topicHeaderRegex.isValid() || !topicGenericItemRegex.isValid() || !topicFunctionRegex.isValid() || !topicMethodRegex.isValid() || !topicPropertyRegex.isValid())
        qDebug() << "QtSLiMHelpWindow::addTopicsFromRTFFile(): invalid regex";
    
    // Scan through the file one line at a time, parsing out topic headers
	QString topicFileString = topicFileTextDocument.toRawText();
	QStringList topicFileLineArray = topicFileString.split(QChar::ParagraphSeparator);  // this is what Qt's HTML rendering uses between blocks
	int lineCount = topicFileLineArray.size();
	int lineStartIndex = 0;		// the character index of the current line in topicFileAttrString
	
	for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
	{
		const QString &line = topicFileLineArray.at(lineIndex);
		int lineLength = line.length();
        QTextCursor lineCursor(&topicFileTextDocument);
        
        lineCursor.setPosition(lineStartIndex);
        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, lineLength);
        
        // figure out what kind of line we have and handle it
        QRegularExpressionMatch match_topicHeaderRegex = topicHeaderRegex.match(line);
        QRegularExpressionMatch match_topicGenericItemRegex = topicGenericItemRegex.match(line);
        QRegularExpressionMatch match_topicFunctionRegex = topicFunctionRegex.match(line);
        QRegularExpressionMatch match_topicMethodRegex = topicMethodRegex.match(line);
        QRegularExpressionMatch match_topicPropertyRegex = topicPropertyRegex.match(line);
		int isTopicHeaderLine = match_topicHeaderRegex.hasMatch();
		int isTopicGenericItemLine = match_topicGenericItemRegex.hasMatch();
		int isTopicFunctionLine = match_topicFunctionRegex.hasMatch();
		int isTopicMethodLine = match_topicMethodRegex.hasMatch();
		int isTopicPropertyLine = match_topicPropertyRegex.hasMatch();
		int matchCount = isTopicHeaderLine + isTopicFunctionLine + isTopicMethodLine + isTopicPropertyLine;	// excludes isTopicGenericItemLine, which is a subtype of isTopicHeaderLine
		
		if (matchCount > 1)
		{
			qDebug() << "*** line matched more than one regex type: %@" << line;
			return;
		}
        
        if (matchCount == 0)
        {
            if (lineLength)
            {
                // If we have a topic, this is a content line, to be appended to the current topic item's content
                if (topicItemCursor)
                    topicItemCursor->movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, lineLength + 1);     // 1 for the QChar::ParagraphSeparator
                else if (line.trimmed().length())
                    qDebug() << "orphan line while reading for top level heading " << topLevelHeading << ": " << line;
            }
        }
        
        if ((matchCount == 1) || ((matchCount == 0) && (lineIndex == lineCount - 1)))
		{
			// This line starts a new header or item or ends the file, so we need to terminate the current item
			if (topicItemCursor && topicItemKey.length())
			{
                if (!currentTopicItem)
                {
                    qDebug() << "no current topic item for text to be placed into";
                    return;
                }
                
                QtSLiMHelpItem *childItem = new QtSLiMHelpItem(currentTopicItem);
                QTextDocumentFragment *topicFragment = new QTextDocumentFragment(topicItemCursor->selection());
                
                childItem->setText(0, topicItemKey);
                childItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                childItem->doc_fragment = topicFragment;
                
                delete topicItemCursor;
                topicItemCursor = nullptr;
                topicItemKey.clear();
			}
		}
        
        if (isTopicHeaderLine)
		{
			// We have hit a new topic header.  This might be a subtopic of the current topic, or a sibling, or a sibling of one of our ancestors
            QString sectionString = match_topicHeaderRegex.captured(1);
            QString titleString = match_topicHeaderRegex.captured(2);
			
			//qDebug() << "topic header section " << sectionString << ", title " << titleString << ", line: " << line;
			
			if (isTopicGenericItemLine)
			{
				// This line plays two roles: it is both a header (with a period-separated section index at the beginning) and a
				// topic item starter like function/method/property lines, with item content following it immediately.  First we
				// use the header-line section string to find the right parent section to place it.
				currentTopicItem = dynamic_cast<QtSLiMHelpItem *>(parentItemForSection(sectionString, topics, topItem));
				
				// Then we extract the item name and create a new item under the parent dict.
                //QString itemOrder = match_topicGenericItemRegex.captured(2);
                QString itemName = match_topicGenericItemRegex.captured(3);
                int itemName_pos = match_topicGenericItemRegex.capturedStart(3);
                int itemName_len = match_topicGenericItemRegex.capturedLength(3);
                
                topicItemCursor = new QTextCursor(&topicFileTextDocument);
                
                topicItemCursor->setPosition(lineStartIndex + itemName_pos);
                topicItemCursor->movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, itemName_len);
                
				topicItemKey = itemName;
			}
            else
            {
                // This header line is not also an item line, so we want to create a new expandable category and await items
                currentTopicItem = createItemForSection(sectionString, titleString, topics, topItem);
            }
        }
        else if (isTopicFunctionLine)
		{
            // This topic item is a function declaration
            QString callName = match_topicFunctionRegex.captured(1);
			
			//qDebug() << "topic function name: " << callName << ", line: " << line;
			
			// Check for a built-in function signature that matches and substitute it in
			if (functionList)
			{
				std::string function_name = callName.toStdString();
				const EidosFunctionSignature *function_signature = nullptr;
				
				for (const auto &signature_iter : *functionList)
					if (signature_iter->call_name_.compare(function_name) == 0)
					{
						function_signature = signature_iter.get();
						break;
					}
				
				if (function_signature)
                    ColorizeCallSignature(function_signature, 11.0, lineCursor);
				else
					qDebug() << "*** no function signature found for function name " << callName;
			}
			
			topicItemKey = callName + "()";
            topicItemCursor = new QTextCursor(lineCursor);
        }
        else if (isTopicMethodLine)
		{
            // This topic item is a method declaration
            QString classMethodString = match_topicMethodRegex.captured(1);
            QString callName = match_topicMethodRegex.captured(2);
			
			//qDebug() << "topic method name: " << callName << ", line: " << line;
			
			// Check for a built-in method signature that matches and substitute it in
            // BCH 3 April 2022: I don't think there's any reason why we can't have more than one method with the same name,
            // but with different signatures, as long as they are not in the same class; we can't handle overloading, but
            // method lookup is within-class.  So this code could be generalized as the property lookup code below was; I just
            // haven't bothered to do so.
			if (methodList)
			{
				std::string method_name(callName.toStdString());
				const EidosMethodSignature *method_signature = nullptr;
				
				for (const auto &signature_iter : *methodList)
					if (signature_iter->call_name_.compare(method_name) == 0)
					{
						method_signature = signature_iter.get();
						break;
					}
				
				if (method_signature)
                    ColorizeCallSignature(method_signature, 11.0, lineCursor);
				else
					qDebug() << "*** no method signature found for method name " << callName;
			}
			
            topicItemKey = classMethodString + "\u00A0" + callName + "()";
            topicItemCursor = new QTextCursor(lineCursor);
        }
        else if (isTopicPropertyLine)
		{
            // This topic item is a method declaration
            QString callName = match_topicPropertyRegex.captured(1);
            QString readOnlyName = match_topicPropertyRegex.captured(2);
			
            //qDebug() << "topic property name: " << callName << ", line: " << line;
            
            // Check for a built-in property signature that matches and substitute it in.  Note that we accept a match from any property in any class
			// API as long as the signature matches; we do not rigorously check that the API within a given class matches between signature and doc.
			// This is mostly not a problem because it is quite rare for the same property name to be used with more than one signature.
            if (propertyList)
            {
                std::string property_name(callName.toStdString());
                bool found_match = false, found_mismatch = false;
                QString oldSignatureString, newSignatureString;
                
                for (const auto &signature_iter : *propertyList)
                    if (signature_iter->property_name_.compare(property_name) == 0)
                    {
                        const EidosPropertySignature *property_signature = signature_iter.get();
                        
                        oldSignatureString = lineCursor.selectedText();
                        std::ostringstream ss;
                        ss << *property_signature;
                        newSignatureString = QString::fromStdString(ss.str());
                        
                        if (newSignatureString == oldSignatureString)
                        {
                            //qDebug() << "signature match for method" << callName;
                            
                            // Replace the signature line with the syntax-colored version
                            ColorizePropertySignature(property_signature, 11.0, lineCursor);
                            found_match = true;
                            break;
                        }
                        else
                        {
                            // If we find a mismatched signature but no matching signature, that's probably an error in either the doc or
                            // the signature, unless we find a match later on with a different signature for the same property name.
                            found_mismatch = true;
                        }
                    }
                
                if (found_mismatch && !found_match)
                    qDebug() << "*** property signature mismatch:\nold: " << oldSignatureString << "\nnew: " << newSignatureString;
				else if (!found_match)
					qDebug() << "*** no property signature found for property name" << callName;
			}
			
			topicItemKey = callName + "\u00A0" + readOnlyName;
            topicItemCursor = new QTextCursor(lineCursor);
        }
        
        lineStartIndex += (lineLength + 1);		// +1 to jump over the newline
    }
}

void QtSLiMHelpWindow::checkDocumentationOfFunctions(const std::vector<EidosFunctionSignature_CSP> *functions)
{
    for (const EidosFunctionSignature_CSP &functionSignature : *functions)
	{
		QString functionNameString = QString::fromStdString(functionSignature->call_name_);
		
		if (!functionNameString.startsWith("_"))
			if (!findObjectForKeyEqualTo(functionNameString + "()", ui->topicOutlineView->invisibleRootItem()))
				qDebug() << "*** no documentation found for function " << functionNameString << "()";
	}
}

void QtSLiMHelpWindow::checkDocumentationOfClass(EidosClass *classObject)
{
    const EidosClass *superclassObject = classObject->Superclass();
	
	// We're hiding DictionaryBase, where the Dictionary stuff is actually defined, so we have Dictionary pretend that its superclass is Object, so the Dictionary stuff gets checked
	if (classObject == gEidosDictionaryRetained_Class)
		superclassObject = gEidosObject_Class;
	
    const QString className = QString::fromStdString(classObject->ClassName());
	const QString classKey = "Class " + className;
	QtSLiMHelpItem *classDocumentation = findObjectWithKeySuffix(classKey, ui->topicOutlineView->invisibleRootItem());
    int classDocChildCount = classDocumentation ? classDocumentation->childCount() : 0;
	
	if (classDocumentation && (classDocumentation->doc_fragment == nullptr) && (classDocChildCount > 0))
	{
		QString propertiesKey = /*QString("1. ") +*/ className + QString(" properties");
		QString methodsKey = /*QString("2. ") +*/ className + QString(" methods");
        QtSLiMHelpItem *classPropertyItem = findObjectForKeyEqualTo(propertiesKey, classDocumentation);
        QtSLiMHelpItem *classMethodsItem = findObjectForKeyEqualTo(methodsKey, classDocumentation);
        
		if ((classDocChildCount == 2) || (classDocChildCount == 3))    // 3 if there is a constructor function, which we don't presently check
		{
			// Check for complete documentation of all properties defined by the class
			{
				const std::vector<EidosPropertySignature_CSP> *classProperties = classObject->Properties();
                const std::vector<EidosPropertySignature_CSP> *superclassProperties = superclassObject ? superclassObject->Properties() : nullptr;
				QStringList docProperties;
				
                if (classPropertyItem)
                    for (int child_index = 0; child_index < classPropertyItem->childCount(); ++child_index)
                        docProperties.push_back(classPropertyItem->child(child_index)->text(0));
                
				for (const EidosPropertySignature_CSP &propertySignature : *classProperties)
				{
                    const std::string &&connector_string = propertySignature->PropertySymbol();
                    const std::string &property_name_string = propertySignature->property_name_;
                    QString property_string = QString::fromStdString(property_name_string) + QString("\u00A0") + QString::fromStdString(connector_string);
                    int docIndex = docProperties.indexOf(property_string);
                    
                    if (docIndex != -1)
                    {
                        // If the property is defined in this class doc, consider it documented
                        docProperties.removeAt(docIndex);
                    }
                    else
                    {
                        // If the property is not defined in this class doc, then that is an error unless it is a superclass property
                        bool isSuperclassProperty = superclassProperties && (std::find(superclassProperties->begin(), superclassProperties->end(), propertySignature) != superclassProperties->end());
					
                        if (!isSuperclassProperty)
                            qDebug() << "*** no documentation found for class " << className << " property " << property_string;
                    }
				}
				
				if (docProperties.size())
					qDebug() << "*** excess documentation found for class " << className << " properties " << docProperties;
			}
			
			// Check for complete documentation of all methods defined by the class
			{
				const std::vector<EidosMethodSignature_CSP> *classMethods = classObject->Methods();
				const std::vector<EidosMethodSignature_CSP> *superclassMethods = superclassObject ? superclassObject->Methods() : nullptr;
				QStringList docMethods;
				
                if (classMethodsItem)
                    for (int child_index = 0; child_index < classMethodsItem->childCount(); ++child_index)
                        docMethods.push_back(classMethodsItem->child(child_index)->text(0));
                
				for (const EidosMethodSignature_CSP &methodSignature : *classMethods)
				{
                    const std::string &&prefix_string = methodSignature->CallPrefix();
                    const std::string &method_name_string = methodSignature->call_name_;
                    QString method_string = QString::fromStdString(prefix_string) + QString::fromStdString(method_name_string) + QString("()");
                    int docIndex = docMethods.indexOf(method_string);
                    
                    if (docIndex != -1)
                    {
						// If the method is defined in this class doc, consider it documented
                        docMethods.removeAt(docIndex);
                    }
                    else
                    {
                        // If the method is not defined in this class doc, then that is an error unless it is a superclass method
                        bool isSuperclassMethod = superclassMethods && (std::find(superclassMethods->begin(), superclassMethods->end(), methodSignature) != superclassMethods->end());
					
                        if (!isSuperclassMethod)
							qDebug() << "*** no documentation found for class " << className << " method " << method_string;
					}
				}
				
				if (docMethods.size())
					qDebug() << "*** excess documentation found for class " << className << " methods " << docMethods;
			}
		}
		else
		{
			qDebug() << "*** documentation for class " << className << " in unexpected format";
		}
	}
	else
	{
		qDebug() << "*** no documentation found for class " << className;
	}
}

void QtSLiMHelpWindow::addSuperclassItemForClass(EidosClass *classObject)
{
    const EidosClass *superclassObject = classObject->Superclass();
	
	// We're hiding DictionaryBase, where the Dictionary stuff is actually defined, so we have Dictionary pretend that its superclass is Object
	if (classObject == gEidosDictionaryRetained_Class)
		superclassObject = gEidosObject_Class;
	
    const QString className = QString::fromStdString(classObject->ClassName());
	const QString classKey = "Class " + className;
	QtSLiMHelpItem *classDocumentation = findObjectWithKeySuffix(classKey, ui->topicOutlineView->invisibleRootItem());
    int classDocChildCount = classDocumentation ? classDocumentation->childCount() : 0;
	
	if (classDocumentation && (classDocumentation->doc_fragment == nullptr) && (classDocChildCount > 0))
	{
        QString superclassName = superclassObject ? QString::fromStdString(superclassObject->ClassName()) : QString("none");
        
        // Hide DictionaryBase by pretending our superclass is Dictionary
        if (superclassName == "DictionaryBase")
            superclassName = "Dictionary";
        
        bool inDarkMode = QtSLiMInDarkMode();
        QtSLiMHelpItem *superclassItem = new QtSLiMHelpItem((QTreeWidgetItem *)nullptr);
        
        superclassItem->setText(0, QString("Superclass: " + superclassName));
        superclassItem->setFlags(Qt::ItemIsEnabled);
        
        if (superclassObject)
        {
            // Hyperlink appearance, with blue underlined text
            superclassItem->setForeground(0, QBrush(inDarkMode ? QtSLiMColorWithHSV(0.65, 0.6, 1.0, 1.0) : QtSLiMColorWithHSV(0.65, 1.0, 0.8, 1.0)));
            
            QFont itemFont(superclassItem->font(0));
            itemFont.setUnderline(true);
            superclassItem->setFont(0, itemFont);
        }
        else
        {
            // Dimmed appearance
            superclassItem->setForeground(0, QBrush(inDarkMode ? QtSLiMColorWithWhite(0.5, 1.0) : QtSLiMColorWithWhite(0.3, 1.0)));
        }
        
        classDocumentation->insertChild(0, superclassItem);     // takes ownership
	}
}

void QtSLiMHelpWindow::outlineSelectionChanged(void)
{
    if (!doingProgrammaticSelection)
    {
        QList<QTreeWidgetItem *> &&selection = ui->topicOutlineView->selectedItems();
        QPlainTextEdit *textedit = ui->descriptionTextEdit;
        QTextDocument *textdoc = textedit->document();
        bool firstItem = true;
        
        textedit->clear();
        
        QTextCursor insertion(textdoc);
        insertion.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        
        QTextBlockFormat defaultBlockFormat;
        QTextBlockFormat hrBlockFormat;
        hrBlockFormat.setTopMargin(10.0);
        hrBlockFormat.setBottomMargin(10.0);
        hrBlockFormat.setAlignment(Qt::AlignHCenter);
        
        for (QTreeWidgetItem *selwidget : selection)
        {
            if (!firstItem)
            {
                insertion.insertBlock(hrBlockFormat);
                // inserting an HR doesn't work properly, but the bug I filed got a useful response if I ever want to revisit this: https://bugreports.qt.io/browse/QTBUG-80473
                //            insertion.insertHtml("<HR>");
                insertion.insertText("––––––––––––––––––––");
                insertion.insertBlock(defaultBlockFormat);
            }
            firstItem = false;
            
            QtSLiMHelpItem *helpItem = dynamic_cast<QtSLiMHelpItem *>(selwidget);
            
            if (helpItem && helpItem->doc_fragment)
                insertion.insertFragment(*helpItem->doc_fragment);
        }
        
        //qDebug() << textdoc->toHtml();
    }
}

void QtSLiMHelpWindow::recursiveExpand(QTreeWidgetItem *item)
{
    // Expand pre-order; I don't think this matters, but it seems safer
    if (!item->isExpanded())
        ui->topicOutlineView->expandItem(item);
    
    for (int child_index = 0; child_index < item->childCount(); child_index++)
        recursiveExpand(item->child(child_index));
}

void QtSLiMHelpWindow::recursiveCollapse(QTreeWidgetItem *item)
{
    // Collapse post-order; I don't think this matters, but it seems safer
    for (int child_index = 0; child_index < item->childCount(); child_index++)
        recursiveCollapse(item->child(child_index));
    
    if (item->isExpanded())
        ui->topicOutlineView->collapseItem(item);
}

void QtSLiMHelpWindow::itemClicked(QTreeWidgetItem *item, int __attribute__((__unused__)) column)
{
    if ((item->childCount() == 0) && (item->childIndicatorPolicy() == QTreeWidgetItem::DontShowIndicatorWhenChildless))
    {
        // If the item has no children, and is not showing an indicator, it is a leaf and might be a hyperlink item
        QString itemText = item->text(0);
        
        if (itemText.startsWith("Superclass: "))
        {
            QString superclassName = itemText.right(itemText.length() - QString("Superclass: ").length());
            
            if (superclassName != "none")
            {
                QString sectionTitle = "Class " + superclassName;
                
                // Open disclosure triangles to show the section
                std::vector<QTreeWidgetItem *> matchKeys;
                std::vector<QTreeWidgetItem *> expandItems;
                
                QTreeWidgetItem *classDocumentation = findObjectWithKeySuffix(sectionTitle, ui->topicOutlineView->invisibleRootItem());
                
                while (classDocumentation)
                {
                    expandItems.emplace_back(classDocumentation);
                    classDocumentation = classDocumentation->parent();
                }
                
                if (expandItems.size())
                    expandToShowItems(expandItems, matchKeys);
                else
                    qApp->beep();
            }
        }
    }
    else
    {
        // Otherwise, it has a disclosure triangle and needs to expand/collapse
        bool optionPressed = QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
        
        doingProgrammaticCollapseExpand = true;
        
        if (optionPressed)
        {
            // recursively expand/collapse items below this item
            if (item->isExpanded())
                recursiveCollapse(item);
            else
                recursiveExpand(item);
        }
        else
        {
            // expand/collapse just this item
            if (item->isExpanded())
                ui->topicOutlineView->collapseItem(item);
            else
                ui->topicOutlineView->expandItem(item);
        }
        
        doingProgrammaticCollapseExpand = false;
    }
}

void QtSLiMHelpWindow::itemCollapsed(QTreeWidgetItem *item)
{
    // If the user has collapsed an item by clicking, we want to implement option-clicking on top of that
    if (!doingProgrammaticCollapseExpand)
    {
        bool optionPressed = QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
        
        if (optionPressed)
        {
            doingProgrammaticCollapseExpand = true;
            recursiveCollapse(item);
            doingProgrammaticCollapseExpand = false;
        }
    }
}

void QtSLiMHelpWindow::itemExpanded(QTreeWidgetItem *item)
{
    // If the user has expanded an item by clicking, we want to implement option-clicking on top of that
    if (!doingProgrammaticCollapseExpand)
    {
        bool optionPressed = QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
        
        if (optionPressed)
        {
            doingProgrammaticCollapseExpand = true;
            recursiveExpand(item);
            doingProgrammaticCollapseExpand = false;
        }
    }
}

QtSLiMHelpItem *QtSLiMHelpWindow::findObjectWithKeySuffix(const QString searchKeySuffix, QTreeWidgetItem *searchItem)
{
	QtSLiMHelpItem *value = nullptr;
    int childCount = searchItem->childCount();
	
    for (int childIndex = 0; childIndex < childCount; ++childIndex)
	{
        QTreeWidgetItem *child = searchItem->child(childIndex);
        QtSLiMHelpItem *our_child = dynamic_cast<QtSLiMHelpItem *>(child);
        
        if (our_child)
        {
            QString childTitle = child->text(0);
            
            // Search by substring matching; we have to be careful to use this method to search only for unique keys
            if (childTitle.endsWith(searchKeySuffix))
                value = our_child;
            else if (our_child->childCount())
                value = findObjectWithKeySuffix(searchKeySuffix, our_child);
            
            if (value)
                break;
        }
	}
	
	return value;
}

QtSLiMHelpItem *QtSLiMHelpWindow::findObjectForKeyEqualTo(const QString searchKey, QTreeWidgetItem *searchItem)
{
	QtSLiMHelpItem *value = nullptr;
	int childCount = searchItem->childCount();
    
	for (int childIndex = 0; childIndex < childCount; ++childIndex)
	{
        QTreeWidgetItem *child = searchItem->child(childIndex);
        QtSLiMHelpItem *our_child = dynamic_cast<QtSLiMHelpItem *>(child);
        
        if (our_child)
        {
            QString childTitle = child->text(0);
            
            // Search using string equality; we have to be careful to use this method to search only for unique keys
            if (childTitle == searchKey)
                value = our_child;
            else if (our_child->childCount())
                value = findObjectForKeyEqualTo(searchKey, our_child);
            
            if (value)
                break;
        }
	}
	
	return value;
}




























