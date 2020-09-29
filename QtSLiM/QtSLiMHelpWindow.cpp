//
//  QtSLiMHelpWindow.cpp
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
#include "slim_sim.h"
#include "chromosome.h"
#include "genome.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "individual.h"
#include "subpopulation.h"

#include "QtSLiMExtras.h"
#include "QtSLiM_SLiMgui.h"
#include "QtSLiMAppDelegate.h"

#include <vector>
#include <algorithm>


//
// This is our custom outline item class, which can hold a QTextDocumentFragment
//
QtSLiMHelpItem::~QtSLiMHelpItem()
{
}


//
// This subclass of QStyledItemDelegate provides custom drawing for the outline view.
//
QtSLiMHelpOutlineDelegate::~QtSLiMHelpOutlineDelegate(void)
{
}

void QtSLiMHelpOutlineDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    bool topLevel = !index.parent().isValid();
    QRect fullRect = option.rect;
    fullRect.setLeft(0);             // we are not clipped; we will draw outside our rect to occupy the full width of the view
    
    // if we're a top-level item, wash a background color over our row; we use alpha so the disclosure triangle remains visible
    if (topLevel)
    {
        bool isEidos = index.data().toString().startsWith("Eidos ");
        
        if (isEidos)
            painter->fillRect(fullRect, QBrush(QtSLiMColorWithRGB(0.0, 1.0, 0.0, 0.04)));       // pale green background for Eidos docs
        else
            painter->fillRect(fullRect, QBrush(QtSLiMColorWithRGB(0.0, 0.0, 1.0, 0.04)));       // pale blue background for SLiM docs
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
        painter->fillRect(QRect(fullRect.left(), fullRect.top(), fullRect.width(), 1), QtSLiMColorWithWhite(0.85, 1.0));        // top edge in light gray
        painter->fillRect(QRect(fullRect.left(), fullRect.top() + fullRect.height() - 1, fullRect.width(), 1), QtSLiMColorWithWhite(0.65, 1.0));     // bottom edge in medium gray
    }
    else
    {
        // otherwise, add a color box on the right for the items that need them
        QString itemString = index.data().toString();
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
                                         "4. mateChoice() callbacks"
                                        });
        
        if (!stringsNonWF)
            stringsNonWF = new QStringList({"initializeSLiMModelType()",
                              "age =>",
                              "modelType =>",
                              "– registerReproductionCallback()",
                              "– addCloned()",
                              "– addCrossed()",
                              "– addEmpty()",
                              "– addRecombinant()",
                              "– addSelfed()",
                              "– removeSubpopulation()",
                              "– takeMigrants()",
                              "8. reproduction() callbacks"
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
                boxColor = QtSLiMColorWithRGB(66/255.0, 255/255.0, 53/255.0, 1.0);      // WF-only color (green)
            else if (draw_nonWF_box)
                boxColor = QtSLiMColorWithRGB(88/255.0, 148/255.0, 255/255.0, 1.0);     // nonWF-only color (blue)
            else //if (draw_nucmut_box)
                boxColor = QtSLiMColorWithRGB(228/255.0, 118/255.0, 255/255.0, 1.0);	// nucmut color (purple)
            
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

QtSLiMHelpWindow::QtSLiMHelpWindow(QWidget *parent) : QWidget(parent, Qt::Window), ui(new Ui::QtSLiMHelpWindow)
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
    
    // Configure the outline view to behave as we wish
    connect(ui->topicOutlineView, &QTreeWidget::itemSelectionChanged, this, &QtSLiMHelpWindow::outlineSelectionChanged);
    connect(ui->topicOutlineView, &QTreeWidget::itemClicked, this, &QtSLiMHelpWindow::itemClicked);
    connect(ui->topicOutlineView, &QTreeWidget::itemCollapsed, this, &QtSLiMHelpWindow::itemCollapsed);
    connect(ui->topicOutlineView, &QTreeWidget::itemExpanded, this, &QtSLiMHelpWindow::itemExpanded);
    
    QAbstractItemDelegate *outlineDelegate = new QtSLiMHelpOutlineDelegate(ui->topicOutlineView);
    ui->topicOutlineView->setItemDelegate(outlineDelegate);
    
    // tweak appearance on Linux; the form is adjusted for macOS
#if !defined(__APPLE__)
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
    addTopicsFromRTFFile("EidosHelpFunctions", "Eidos Functions", &EidosInterpreter::BuiltInFunctions(), nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpMethods", "Eidos Methods", nullptr, gEidos_UndefinedClassObject->Methods(), nullptr);
    addTopicsFromRTFFile("EidosHelpOperators", "Eidos Operators", nullptr, nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpStatements", "Eidos Statements", nullptr, nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpTypes", "Eidos Types", nullptr, nullptr, nullptr);
    
    // Check for completeness of the Eidos documentation
    checkDocumentationOfFunctions(&EidosInterpreter::BuiltInFunctions());
    checkDocumentationOfClass(gEidos_UndefinedClassObject);
    
    // Add SLiM topics
    const std::vector<EidosFunctionSignature_CSP> *zg_functions = SLiMSim::ZeroGenerationFunctionSignatures();
    const std::vector<EidosFunctionSignature_CSP> *slim_functions = SLiMSim::SLiMFunctionSignatures();
    std::vector<EidosFunctionSignature_CSP> all_slim_functions;
    
    all_slim_functions.insert(all_slim_functions.end(), zg_functions->begin(), zg_functions->end());
    all_slim_functions.insert(all_slim_functions.end(), slim_functions->begin(), slim_functions->end());
    
    addTopicsFromRTFFile("SLiMHelpFunctions", "SLiM Functions", &all_slim_functions, nullptr, nullptr);
    addTopicsFromRTFFile("SLiMHelpClasses", "SLiM Classes", nullptr, slimguiAllMethodSignatures(), slimguiAllPropertySignatures());
    addTopicsFromRTFFile("SLiMHelpCallbacks", "SLiM Events and Callbacks", nullptr, nullptr, nullptr);
    
    // Check for completeness of the SLiM documentation
    checkDocumentationOfFunctions(&all_slim_functions);
    
    checkDocumentationOfClass(gSLiM_Chromosome_Class);
    checkDocumentationOfClass(gSLiM_Genome_Class);
    checkDocumentationOfClass(gSLiM_GenomicElement_Class);
    checkDocumentationOfClass(gSLiM_GenomicElementType_Class);
    checkDocumentationOfClass(gSLiM_Individual_Class);
    checkDocumentationOfClass(gSLiM_InteractionType_Class);
    checkDocumentationOfClass(gSLiM_Mutation_Class);
    checkDocumentationOfClass(gSLiM_MutationType_Class);
    checkDocumentationOfClass(gSLiM_SLiMEidosBlock_Class);
    checkDocumentationOfClass(gSLiM_SLiMSim_Class);
    checkDocumentationOfClass(gSLiM_Subpopulation_Class);
    checkDocumentationOfClass(gSLiM_Substitution_Class);
    checkDocumentationOfClass(gSLiM_SLiMgui_Class);
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
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
				matchKeys.push_back(childItem);
				anyChildMatches = true;
			}
		}
	}
	
	if (anyChildMatches)
		expandItems.push_back(root);
	
	return anyChildMatches;
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
        else
        {
            qApp->beep();
        }
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

void QtSLiMHelpWindow::closeEvent(QCloseEvent *event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMHelpWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // use super's default behavior
    QWidget::closeEvent(event);
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
	QString numberedTitle = sectionComponents.back().append(". ").append(title);
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
        
        if (!topicFile.open(QIODevice::ReadOnly)) {
            qDebug() << "QtSLiMHelpWindow::addTopicsFromRTFFile(): could not find HTML file " << htmlFile;
            return;
        }
        
        topicFileData = topicFile.readAll();
        topicFile.close();
        topicFileTextDocument.setHtml(topicFileData);
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
    QFont font(topItem->font(0));
    font.setBold(true);
    topItem->setFont(0, font);
    
    // Set up the current topic item that we are appending content into
    QtSLiMHelpItem *currentTopicItem = topItem;
	QString topicItemKey;
	QTextCursor *topicItemCursor = nullptr;
    
    // Make regular expressions that we will use below
    QRegularExpression topicHeaderRegex("^((?:[0-9]+\\.)*[0-9]+)\\.?[\u00A0 ] (.+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression topicGenericItemRegex("^((?:[0-9]+\\.)*[0-9]+)\\.?[\u00A0 ] ITEM: ((?:[0-9]+\\.? )?)(.+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression topicFunctionRegex("^\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression topicMethodRegex("^([-–+])[\u00A0 ]\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression topicPropertyRegex("^([a-zA-Z_0-9]+)[\u00A0 ]((?:<[-–]>)|(?:=>)) \\([a-zA-Z<>\\*+$]+\\)$", QRegularExpression::CaseInsensitiveOption);
	
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
			
			// check for a built-in function signature that matches and substitute it in
			if (functionList)
			{
				std::string function_name = callName.toStdString();
				const EidosFunctionSignature *function_signature = nullptr;
				
				for (auto signature_iter : *functionList)
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
			
			// check for a built-in method signature that matches and substitute it in
			if (methodList)
			{
				std::string method_name(callName.toStdString());
				const EidosMethodSignature *method_signature = nullptr;
				
				for (auto signature_iter : *methodList)
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
            
			// check for a built-in property signature that matches and substitute it in
			if (propertyList)
			{
				std::string property_name(callName.toStdString());
				const EidosPropertySignature *property_signature = nullptr;
				
				for (auto signature_iter : *propertyList)
					if (signature_iter->property_name_.compare(property_name) == 0)
					{
						property_signature = signature_iter.get();
						break;
					}
				
				if (property_signature)
                    ColorizePropertySignature(property_signature, 11.0, lineCursor);
				else
					qDebug() << "*** no property signature found for property name " << callName;
			}
			
			topicItemKey = callName + "\u00A0" + readOnlyName;
            topicItemCursor = new QTextCursor(lineCursor);
        }
        
        lineStartIndex += (lineLength + 1);		// +1 to jump over the newline
    }
}

const std::vector<EidosPropertySignature_CSP> *QtSLiMHelpWindow::slimguiAllPropertySignatures(void)
{
    // This adds the properties belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<EidosPropertySignature_CSP> *propertySignatures = nullptr;
	
	if (!propertySignatures)
	{
		const std::vector<EidosPropertySignature_CSP> *slimProperties =					SLiMSim::AllPropertySignatures();
		const std::vector<EidosPropertySignature_CSP> *propertiesSLiMgui =				gSLiM_SLiMgui_Class->Properties();
		
		propertySignatures = new std::vector<EidosPropertySignature_CSP>(*slimProperties);
		
		propertySignatures->insert(propertySignatures->end(), propertiesSLiMgui->begin(), propertiesSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllPropertySignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(propertySignatures->begin(), propertySignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(propertySignatures->begin(), propertySignatures->end());
		propertySignatures->resize(static_cast<size_t>(std::distance(propertySignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(propertySignatures->begin(), propertySignatures->end(), CompareEidosPropertySignatures);
		
		EidosPropertySignature_CSP previous_sig = nullptr;
		
		for (EidosPropertySignature_CSP &sig : *propertySignatures)
		{
			if (previous_sig && (sig->property_name_.compare(previous_sig->property_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the property signatures are identical.
				if ((sig->property_id_ != previous_sig->property_id_) ||
					(sig->read_only_ != previous_sig->read_only_) ||
					(sig->value_mask_ != previous_sig->value_mask_) ||
					(sig->value_class_ != previous_sig->value_class_))
				std::cout << "Duplicate property name with different signature: " << sig->property_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return propertySignatures;
}

const std::vector<EidosMethodSignature_CSP> *QtSLiMHelpWindow::slimguiAllMethodSignatures(void)
{
    // This adds the methods belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<EidosMethodSignature_CSP> *methodSignatures = nullptr;
	
	if (!methodSignatures)
	{
		const std::vector<EidosMethodSignature_CSP> *slimMethods =					SLiMSim::AllMethodSignatures();
		const std::vector<EidosMethodSignature_CSP> *methodsSLiMgui =				gSLiM_SLiMgui_Class->Methods();
		
		methodSignatures = new std::vector<EidosMethodSignature_CSP>(*slimMethods);
		
		methodSignatures->insert(methodSignatures->end(), methodsSLiMgui->begin(), methodsSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllMethodSignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(methodSignatures->begin(), methodSignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(methodSignatures->begin(), methodSignatures->end());
		methodSignatures->resize(static_cast<size_t>(std::distance(methodSignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(methodSignatures->begin(), methodSignatures->end(), CompareEidosCallSignatures);
		
		EidosMethodSignature_CSP previous_sig = nullptr;
		
		for (EidosMethodSignature_CSP &sig : *methodSignatures)
		{
			if (previous_sig && (sig->call_name_.compare(previous_sig->call_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
                const EidosMethodSignature *sig1 = sig.get();
				const EidosMethodSignature *sig2 = previous_sig.get();
				
				if ((typeid(*sig1) != typeid(*sig2)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->call_name_ != previous_sig->call_name_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
				std::cout << "Duplicate method name with a different signature: " << sig->call_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
		
		// log a full list
		//std::cout << "----------------" << std::endl;
		//for (const EidosMethodSignature *sig : *methodSignatures)
		//	std::cout << sig->call_name_ << " (" << sig << ")" << std::endl;
	}
	
	return methodSignatures;
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

void QtSLiMHelpWindow::checkDocumentationOfClass(EidosObjectClass *classObject)
{
    bool classIsUndefinedClass = (classObject == gEidos_UndefinedClassObject);
    const QString className = QString::fromStdString(classObject->ElementType());
	const QString classKey = (classIsUndefinedClass ? "Eidos Methods" : "Class " + className);
	QtSLiMHelpItem *classDocumentation = findObjectWithKeySuffix(classKey, ui->topicOutlineView->invisibleRootItem());
	
	if (classDocumentation && (classDocumentation->doc_fragment == nullptr) && (classDocumentation->childCount() > 0))
	{
		QString propertiesKey = /*QString("1. ") +*/ className + QString(" properties");
		QString methodsKey = /*QString("2. ") +*/ className + QString(" methods");
        QtSLiMHelpItem *classPropertyItem = findObjectForKeyEqualTo(propertiesKey, classDocumentation);
        QtSLiMHelpItem *classMethodsItem = findObjectForKeyEqualTo(methodsKey, classDocumentation);
        
        if (classIsUndefinedClass && !classMethodsItem)
            classMethodsItem = classDocumentation;
        
		if (classIsUndefinedClass ||
			((classDocumentation->childCount() == 2) && classPropertyItem && classMethodsItem))
		{
			// Check for complete documentation of all properties defined by the class
			if (!classIsUndefinedClass)
			{
				const std::vector<EidosPropertySignature_CSP> *classProperties = classObject->Properties();
				QStringList docProperties;
				
                for (int child_index = 0; child_index < classPropertyItem->childCount(); ++child_index)
                    docProperties.push_back(classPropertyItem->child(child_index)->text(0));
				
				for (const EidosPropertySignature_CSP &propertySignature : *classProperties)
				{
					const std::string &&connector_string = propertySignature->PropertySymbol();
					const std::string &property_name_string = propertySignature->property_name_;
					QString property_string = QString::fromStdString(property_name_string) + QString("\u00A0") + QString::fromStdString(connector_string);
                    int docIndex = docProperties.indexOf(property_string);
					
					if (docIndex != -1)
						docProperties.removeAt(docIndex);
					else
						qDebug() << "*** no documentation found for class " << className << " property " << property_string;
				}
				
				if (docProperties.size())
					qDebug() << "*** excess documentation found for class " << className << " properties " << docProperties;
			}
			
			// Check for complete documentation of all methods defined by the class
			{
				const std::vector<EidosMethodSignature_CSP> *classMethods = classObject->Methods();
				const std::vector<EidosMethodSignature_CSP> *baseMethods = gEidos_UndefinedClassObject->Methods();
				QStringList docMethods;
				
                for (int child_index = 0; child_index < classMethodsItem->childCount(); ++child_index)
                    docMethods.push_back(classMethodsItem->child(child_index)->text(0));
				
				for (const EidosMethodSignature_CSP &methodSignature : *classMethods)
				{
					bool isBaseMethod = (std::find(baseMethods->begin(), baseMethods->end(), methodSignature) != baseMethods->end());
					
					if (!isBaseMethod || classIsUndefinedClass)
					{
						const std::string &&prefix_string = methodSignature->CallPrefix();
						const std::string &method_name_string = methodSignature->call_name_;
						QString method_string = QString::fromStdString(prefix_string) + QString::fromStdString(method_name_string) + QString("()");
                        int docIndex = docMethods.indexOf(method_string);
						
						if (docIndex != -1)
							docMethods.removeAt(docIndex);
						else
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

void QtSLiMHelpWindow::outlineSelectionChanged(void)
{
    if (!doingProgrammaticSelection)
    {
        QList<QTreeWidgetItem *> &&selection = ui->topicOutlineView->selectedItems();
        QTextEdit *textedit = ui->descriptionTextEdit;
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




























