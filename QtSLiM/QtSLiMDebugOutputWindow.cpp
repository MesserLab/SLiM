//
//  QtSLiMDebugOutputWindow.cpp
//  SLiM
//
//  Created by Ben Haller on 02/06/2021.
//  Copyright (c) 2021-2024 Philipp Messer.  All rights reserved.
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


#include "QtSLiMDebugOutputWindow.h"
#include "ui_QtSLiMDebugOutputWindow.h"

#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>

#include <utility>
#include <string>
#include <vector>
#include <limits>

#include "QtSLiMWindow.h"
#include "QtSLiMAppDelegate.h"
#include "QtSLiMExtras.h"

#include "log_file.h"


//
//  QtSLiMDebugOutputWindow
//

QtSLiMDebugOutputWindow::QtSLiMDebugOutputWindow(QtSLiMWindow *p_parent) :
    QWidget(p_parent, Qt::Window),    // the console window has us as a parent, but is still a standalone window
    parentSLiMWindow(p_parent),
    ui(new Ui::QtSLiMDebugOutputWindow)
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
    
    settings.beginGroup("QtSLiMDebugOutputWindow");
    resize(settings.value("size", QSize(400, 300)).toSize());
    move(settings.value("pos", QPoint(25, 445)).toPoint());
    settings.endGroup();
    
    // set up the tab bar; annoyingly, this cannot be configured in Designer at all!
    ui->tabBar->setAcceptDrops(false);
    ui->tabBar->setDocumentMode(false);
    ui->tabBar->setDrawBase(false);
    ui->tabBar->setExpanding(false);
    ui->tabBar->setMovable(false);
    ui->tabBar->setShape(QTabBar::RoundedNorth);
    ui->tabBar->setTabsClosable(false);
    ui->tabBar->setUsesScrollButtons(false);
    ui->tabBar->setIconSize(QSize(15, 15));
    
    ui->tabBar->addTab("Debug Output");
    ui->tabBar->setTabToolTip(0, "Debug Output");
    ui->tabBar->addTab("Run Output");
    ui->tabBar->setTabToolTip(1, "Run Output");
    ui->tabBar->addTab("Scheduling");
    ui->tabBar->setTabToolTip(2, "Scheduling");
    resetTabIcons();
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, [this]() { resetTabIcons(); }); // adjust to dark mode change
    
    connect(ui->tabBar, &QTabBar::currentChanged, this, &QtSLiMDebugOutputWindow::selectedTabChanged);
    
    // glue UI; no separate file since this is very simple
    connect(ui->clearOutputButton, &QPushButton::clicked, this, &QtSLiMDebugOutputWindow::clearOutputClicked);
    ui->clearOutputButton->qtslimSetBaseName("delete");
    connect(ui->clearOutputButton, &QPushButton::pressed, this, &QtSLiMDebugOutputWindow::clearOutputPressed);
    connect(ui->clearOutputButton, &QPushButton::released, this, &QtSLiMDebugOutputWindow::clearOutputReleased);
    
    // fix the layout of the window
    ui->outputHeaderLayout->setSpacing(4);
    ui->outputHeaderLayout->setMargin(0);
    
    // QtSLiMTextEdit attributes
    ui->debugOutputTextEdit->setOptionClickEnabled(false);
    ui->debugOutputTextEdit->setCodeCompletionEnabled(false);
    ui->debugOutputTextEdit->setScriptType(QtSLiMTextEdit::NoScriptType);
    ui->debugOutputTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::OutputHighlighting);
    ui->debugOutputTextEdit->setReadOnly(true);
    
    ui->runOutputTextEdit->setOptionClickEnabled(false);
    ui->runOutputTextEdit->setCodeCompletionEnabled(false);
    ui->runOutputTextEdit->setScriptType(QtSLiMTextEdit::NoScriptType);
    ui->runOutputTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::OutputHighlighting);
    ui->runOutputTextEdit->setReadOnly(true);
    
    ui->schedulingOutputTextEdit->setOptionClickEnabled(false);
    ui->schedulingOutputTextEdit->setCodeCompletionEnabled(false);
    ui->schedulingOutputTextEdit->setScriptType(QtSLiMTextEdit::NoScriptType);
    ui->schedulingOutputTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::OutputHighlighting);
    ui->schedulingOutputTextEdit->setReadOnly(true);
    
    showDebugOutput();
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMDebugOutputWindow::~QtSLiMDebugOutputWindow()
{
    delete ui;
}

void QtSLiMDebugOutputWindow::resetTabIcons(void)
{
    // No icons for now, see how it goes
    /*
    QIcon debugTabIcon(QtSLiMImagePath("debug", false));
    
    ui->tabBar->setTabIcon(0, debugTabIcon);
    ui->tabBar->setTabIcon(1, qtSLiMAppDelegate->applicationIcon());
    */
}

void QtSLiMDebugOutputWindow::hideAllViews(void)
{
    ui->debugOutputTextEdit->setVisible(false);
    ui->runOutputTextEdit->setVisible(false);
    ui->schedulingOutputTextEdit->setVisible(false);
    
    for (QTableWidget *logtable : logfileViews)
        logtable->setVisible(false);
    
    for (QtSLiMTextEdit *fileview : fileViews)
        fileview->setVisible(false);
}

void QtSLiMDebugOutputWindow::showDebugOutput()
{
    hideAllViews();
    ui->debugOutputTextEdit->setVisible(true);
}

void QtSLiMDebugOutputWindow::showRunOutput()
{
    hideAllViews();
    ui->runOutputTextEdit->setVisible(true);
}

void QtSLiMDebugOutputWindow::showSchedulingOutput()
{
    hideAllViews();
    ui->schedulingOutputTextEdit->setVisible(true);
}

void QtSLiMDebugOutputWindow::showLogFile(int logFileIndex)
{
    QTableWidget *table = logfileViews[logFileIndex];
    
    hideAllViews();
    table->setVisible(true);
}

void QtSLiMDebugOutputWindow::showFile(int fileIndex)
{
    QtSLiMTextEdit *fileview = fileViews[fileIndex];
    
    hideAllViews();
    fileview->setVisible(true);
}

void QtSLiMDebugOutputWindow::tabReceivedInput(int tabIndex)
{
    // set the tab's text color to red when new input is received, if it is not the current tab
    // the color depends on dark mode; FIXME we should fix it on dark mode change but we don't
    if (tabIndex != ui->tabBar->currentIndex())
    {
        QColor color = (QtSLiMInDarkMode() ? QColor(255, 128, 128, 255) : QColor(192, 0, 0, 255));
        
        ui->tabBar->setTabTextColor(tabIndex, color);
    }
}

void QtSLiMDebugOutputWindow::takeDebugOutput(QString str)
{
    ui->debugOutputTextEdit->moveCursor(QTextCursor::End);
    ui->debugOutputTextEdit->insertPlainText(str);
    ui->debugOutputTextEdit->moveCursor(QTextCursor::End);
    
    tabReceivedInput(0);
}

void QtSLiMDebugOutputWindow::takeRunOutput(QString str)
{
    ui->runOutputTextEdit->moveCursor(QTextCursor::End);
    ui->runOutputTextEdit->insertPlainText(str);
    ui->runOutputTextEdit->moveCursor(QTextCursor::End);
    
    tabReceivedInput(1);
}

void QtSLiMDebugOutputWindow::takeSchedulingOutput(QString str)
{
    ui->schedulingOutputTextEdit->moveCursor(QTextCursor::End);
    ui->schedulingOutputTextEdit->insertPlainText(str);
    ui->schedulingOutputTextEdit->moveCursor(QTextCursor::End);
    
    tabReceivedInput(2);
}

QTableWidget *QtSLiMDebugOutputWindow::logFileTableForPath(const std::string &path)
{
    auto pathIter = std::find(logfilePaths.begin(), logfilePaths.end(), path);
    QTableWidget *table = nullptr;
    
    if (pathIter != logfilePaths.end())
    {
        size_t tableIndex = std::distance(logfilePaths.begin(), pathIter);
        
        table = logfileViews[tableIndex];
    }
    
    return table;
}

void QtSLiMDebugOutputWindow::takeLogFileOutput(std::vector<std::string> &lineElements, const std::string &path)
{
    // First, find the index of the log file view we're taking input into
    // If we didn't find one, make a new one
    auto pathIter = std::find(logfilePaths.begin(), logfilePaths.end(), path);
    QTableWidget *table;
    size_t tableIndex;
    
    if (pathIter != logfilePaths.end())
    {
        tableIndex = std::distance(logfilePaths.begin(), pathIter);
        table = logfileViews[tableIndex];
    }
    else
    {
        QLayout *windowLayout = this->layout();
        
        table = new QTableWidget();
        table->setSortingEnabled(false);
        table->setAlternatingRowColors(true);
        table->setDragEnabled(false);
        table->setAcceptDrops(false);
        table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->horizontalHeader()->setResizeContentsPrecision(100);     // look at the first 100 rows to determine sizing
        table->horizontalHeader()->setMinimumSectionSize(100);          // wide enough to fit most floating-point output
        table->horizontalHeader()->setMaximumSectionSize(400);          // don't let super-wide output push the table width too far
        table->horizontalHeader()->setSectionsClickable(false);
        table->setVisible(false);
        windowLayout->addWidget(table);
        
        // Get right-clicks on table widget headers and items, to run our context menu
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table, &QTableWidget::customContextMenuRequested, this, &QtSLiMDebugOutputWindow::logFileRightClick);
        table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &QtSLiMDebugOutputWindow::logFileRightClick);
        
        // Make a new tab and insert it at the correct position in the tab bar
        QString filename = QString::fromStdString(Eidos_LastPathComponent(path));
        
        tableIndex = logfilePaths.size();
        ui->tabBar->insertTab(tableIndex + 3, filename);
        ui->tabBar->setTabToolTip(tableIndex + 3, filename);
        
        // Add the new view's info
        logfilePaths.emplace_back(path);
        logfileViews.emplace_back(table);
        logfileLineNumbers.emplace_back(-1);
    }
    
    // Then create a new QTableWidgetItem for each item in the row
    // If this is the first row we have taken, then it's the header row
    bool firstTime = !table->horizontalHeaderItem(0);
    
    if (firstTime)
    {
        int col = 0;
        
        table->setColumnCount(lineElements.size());
        
        for (const std::string &str : lineElements)
        {
            QString qstr = QString::fromStdString(str);
            QTableWidgetItem *colItem = new QTableWidgetItem(qstr);
            colItem->setFlags(Qt::ItemIsEnabled);
            
            QFont itemFont = colItem->font();
            itemFont.setBold(true);
            colItem->setFont(itemFont);
            
            table->setHorizontalHeaderItem(col, colItem);
            col++;
        }
    }
    else
    {
        int rowIndex = table->rowCount();
        int lineNumber = logfileLineNumbers[tableIndex] + 1;
        int col = 0;
        
        table->setRowCount(rowIndex + 1);
        
        QTableWidgetItem *rowItem = new QTableWidgetItem(QString("%1").arg(lineNumber));
        rowItem->setFlags(Qt::NoItemFlags);
        rowItem->setTextAlignment(Qt::AlignRight);
        rowItem->setForeground(Qt::darkGray);
        table->setVerticalHeaderItem(rowIndex, rowItem);
        
        logfileLineNumbers[tableIndex] = lineNumber;
        
        for (const std::string &str : lineElements)
        {
            QString qstr = QString::fromStdString(str);
            QTableWidgetItem *item = new QTableWidgetItem(qstr);
            item->setFlags(Qt::ItemIsEnabled);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(rowIndex, col, item);
            col++;
        }
        
        table->scrollToBottom();
    }
    
    // adjust to fit the headers and the initial rows; this is probably rather expensive,
    // but LogFile usually only fires once per tick or less, so hopefully not a big deal
    table->resizeColumnsToContents();
    
    tabReceivedInput(tableIndex + 3);
}

void QtSLiMDebugOutputWindow::logFileRightClick(const QPoint &pos)
{
    for (QTableWidget *table : logfileViews)
    {
        if (table->isVisible())
        {
            // found the visible/active table
            int clickedColumnIndex = table->horizontalHeader()->logicalIndexAt(pos);
            
            if (clickedColumnIndex != -1)
            {
                int rowCount = table->rowCount();
                int columnCount = table->columnCount();
                
                if ((rowCount > 0) && (columnCount > 0))
                {
                    std::vector<uint8_t> columnIsNumeric;        // were any conversion errors to double detected?
                    
                    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
                    {
                        columnIsNumeric.push_back(true);
                        
                        for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                        {
                            QTableWidgetItem *item = table->item(rowIndex, columnIndex);
                            
                            if (item)
                            {
                                QString itemText = item->text();
                                
                                if ((itemText == "NAN") || (itemText == "INF") || (itemText == "-INF"))
                                {
                                    // NAN / INF / -INF are acceptable as double values, although
                                    // whatever parser the data ends up in might not like it...
                                }
                                else
                                {
                                    bool ok;
                                    
                                    itemText.toDouble(&ok);
                                    if (!ok)
                                    {
                                        columnIsNumeric.back() = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    // make a context menu
                    QMenu contextMenu("logfile_menu", this);
                    QString columnName = table->horizontalHeaderItem(clickedColumnIndex)->text();
                    
                    QString copyDataTitle("Copy Data for ");
                    copyDataTitle.append(columnName);
                    QAction *copyDataAction = contextMenu.addAction(copyDataTitle);
                    copyDataAction->setEnabled(true);
                    
                    QString graphData1DTitle("Graph ");
                    graphData1DTitle.append(columnName);
                    QAction *graphData1DAction = contextMenu.addAction(graphData1DTitle);
                    graphData1DAction->setEnabled(columnIsNumeric[clickedColumnIndex]);
                    
                    if (columnIsNumeric[clickedColumnIndex])
                    {
                        // if the clicked column is numeric, we can potentially plot it as the
                        // dependent variable with a different column as the independent variable
                        bool has2DGraphActions = false;
                        
                        for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
                        {
                            if ((columnIndex != clickedColumnIndex) && (columnIsNumeric[columnIndex]))
                            {
                                if (!has2DGraphActions)
                                {
                                    contextMenu.addSeparator();
                                    
                                    {
                                        QString graphData2DTitle("Graph ");
                                        graphData2DTitle.append(columnName);
                                        graphData2DTitle.append(" ~ ");
                                        graphData2DTitle.append(table->horizontalHeaderItem(columnIndex)->text());
                                        graphData2DTitle.append(" (line plot)");
                                        QAction *graphData2DAction = contextMenu.addAction(graphData2DTitle);
                                        graphData2DAction->setEnabled(true);
                                        graphData2DAction->setData(columnIndex);
                                    }
                                    {
                                        QString graphData2DTitle("Graph ");
                                        graphData2DTitle.append(columnName);
                                        graphData2DTitle.append(" ~ ");
                                        graphData2DTitle.append(table->horizontalHeaderItem(columnIndex)->text());
                                        graphData2DTitle.append(" (scatter plot)");
                                        QAction *graphData2DAction = contextMenu.addAction(graphData2DTitle);
                                        graphData2DAction->setEnabled(true);
                                        graphData2DAction->setData(columnIndex + 10000);    // 10000 is "scatter plot"
                                    }
                                    
                                    has2DGraphActions = true;
                                }
                            }
                        }
                    }
                    
                    // could also add items for, e.g., Graph Data for FST ~ cycle
                    // just need to vet all columns to determine which are double/integer
                    
                    // run the context menu synchronously
                    QPoint globalPoint = table->mapToGlobal(pos);
                    QAction *action = contextMenu.exec(globalPoint);
                    
                    if (action == copyDataAction)
                    {
                        QString string("# LogFile data: ");
                        
                        string.append(columnName);
                        string.append("\n");
                        string.append(slimDateline());
                        string.append("\n\n");
                        
                        if (columnIsNumeric[clickedColumnIndex])
                        {
                            // if the values are all double, we can just dump
                            // the value strings unquoted, as numeric data
                            for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                            {
                                QTableWidgetItem *item = table->item(rowIndex, clickedColumnIndex);
                                
                                if (rowIndex > 0)
                                    string.append(QString(", "));
                                string.append(item->text());
                            }
                        }
                        else
                        {
                            // otherwise, we dump the value strings quoted
                            // FIXME should worry about quoting issues here
                            for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                            {
                                QTableWidgetItem *item = table->item(rowIndex, clickedColumnIndex);
                                
                                if (rowIndex > 0)
                                    string.append(QString(", \""));
                                else
                                    string.append("\"");
                                
                                string.append(item->text());
                                string.append("\"");
                            }
                        }
                        
                        QClipboard *clipboard = QGuiApplication::clipboard();
                        clipboard->setText(string);
                    }
                    else if (action == graphData1DAction)
                    {
                        // all values are convertible to double; guaranteed above
                        double *y_values = (double *)malloc(rowCount * sizeof(double));
                        
                        for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                        {
                            QTableWidgetItem *item = table->item(rowIndex, clickedColumnIndex);
                            QString text = item->text();
                            double value;
                            
                            if (text == "NAN")
                                value = std::numeric_limits<double>::quiet_NaN();
                            else if (text == "INF")
                                value = std::numeric_limits<double>::infinity();
                            else if (text == "-INF")
                                value = -std::numeric_limits<double>::infinity();
                            else
                                value = text.toDouble();
                            
                            y_values[rowIndex] = value;
                        }
                        
                        parentSLiMWindow->plotLogFileData_1D(columnName, columnName, y_values, rowCount);
                    }
                    else if (action)    // graphData2DAction
                    {
                        // all values are convertible to double; guaranteed above
                        QVariant actiondata = action->data();
                        int xColumnIndex = actiondata.toInt();
                        bool makeScatterPlot = false;
                        
                        if (xColumnIndex >= 10000)
                        {
                            xColumnIndex -= 10000;
                            makeScatterPlot = true;
                        }
                        
                        QString xColumnName = table->horizontalHeaderItem(xColumnIndex)->text();
                        
                        double *x_values = (double *)malloc(rowCount * sizeof(double));
                        double *y_values = (double *)malloc(rowCount * sizeof(double));
                        
                        for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                        {
                            QTableWidgetItem *x_item = table->item(rowIndex, xColumnIndex);
                            QTableWidgetItem *y_item = table->item(rowIndex, clickedColumnIndex);
                            QString x_text = x_item->text();
                            QString y_text = y_item->text();
                            double x_value, y_value;
                            
                            if (x_text == "NAN")
                                x_value = std::numeric_limits<double>::quiet_NaN();
                            else if (x_text == "INF")
                                x_value = std::numeric_limits<double>::infinity();
                            else if (x_text == "-INF")
                                x_value = -std::numeric_limits<double>::infinity();
                            else
                                x_value = x_text.toDouble();
                            
                            if (y_text == "NAN")
                                y_value = std::numeric_limits<double>::quiet_NaN();
                            else if (y_text == "INF")
                                y_value = std::numeric_limits<double>::infinity();
                            else if (y_text == "-INF")
                                y_value = -std::numeric_limits<double>::infinity();
                            else
                                y_value = y_text.toDouble();
                            
                            x_values[rowIndex] = x_value;
                            y_values[rowIndex] = y_value;
                        }
                        
                        QString title = QString("%1 ~ %2").arg(columnName).arg(xColumnName);
                        
                        parentSLiMWindow->plotLogFileData_2D(title, xColumnName, columnName, x_values, y_values, rowCount, makeScatterPlot);
                    }
                }
            }
        }
    }
}

EidosValue_SP QtSLiMDebugOutputWindow::dataForColumn(LogFile *logFile, int64_t columnIndex)
{
    const std::string &logfile_path = logFile->UserFilePath();
    QTableWidget *table = logFileTableForPath(logfile_path);
    
    if (table)
    {
        int rowCount = table->rowCount();
        int columnCount = table->columnCount();
        
        if ((rowCount > 0) && (columnCount > 0) && (columnIndex < columnCount))
        {
            bool columnIsNumeric = true;        // were any conversion errors to double detected?
            
            for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
            {
                QTableWidgetItem *item = table->item(rowIndex, columnIndex);
                
                if (item)
                {
                    QString itemText = item->text();
                    
                    if ((itemText == "NAN") || (itemText == "INF") || (itemText == "-INF"))
                    {
                        // NAN / INF / -INF are acceptable as double values, although
                        // whatever parser the data ends up in might not like it...
                    }
                    else
                    {
                        bool ok;
                        
                        itemText.toDouble(&ok);
                        if (!ok)
                        {
                            columnIsNumeric = false;
                            break;
                        }
                    }
                }
            }
            
            if (columnIsNumeric)
            {
                EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(rowCount);
                
                for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                {
                    QTableWidgetItem *item = table->item(rowIndex, columnIndex);
                    QString text = item->text();
                    double value;
                    
                    if (text == "NAN")
                        value = std::numeric_limits<double>::quiet_NaN();
                    else if (text == "INF")
                        value = std::numeric_limits<double>::infinity();
                    else if (text == "-INF")
                        value = -std::numeric_limits<double>::infinity();
                    else
                        value = text.toDouble();
                    
                    float_result->set_float_no_check(value, rowIndex);
                }
                
                return EidosValue_SP(float_result);
            }
            else
            {
                EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(rowCount);
                
                for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
                {
                    QTableWidgetItem *item = table->item(rowIndex, columnIndex);
                    QString text = item->text();
                    
                    string_result->PushString(text.toStdString());
                }
                
                return EidosValue_SP(string_result);
            }
        }
    }
    
    return gStaticEidosValueNULL;
}

EidosValue_SP QtSLiMDebugOutputWindow::dataForColumn(LogFile *logFile, const std::string &columnName)
{
    std::vector<std::string> columnNames = logFile->SortedKeys_StringKeys();
    auto columnIter = std::find(columnNames.begin(), columnNames.end(), columnName);
    
    if (columnIter != columnNames.end())
        return dataForColumn(logFile, std::distance(columnNames.begin(), columnIter));
    
    return gStaticEidosValueNULL;
}

void QtSLiMDebugOutputWindow::takeFileOutput(std::vector<std::string> &lines, bool append, const std::string &path)
{
    // First, find the index of the file view we're taking input into
    // If we didn't find one, make a new one
    auto pathIter = std::find(filePaths.begin(), filePaths.end(), path);
    QtSLiMTextEdit *fileview;
    size_t fileIndex;
    
    if (pathIter != filePaths.end())
    {
        fileIndex = std::distance(filePaths.begin(), pathIter);
        fileview = fileViews[fileIndex];
    }
    else
    {
        QLayout *windowLayout = this->layout();
        
        fileview = new QtSLiMTextEdit();
        fileview->setUndoRedoEnabled(false);
        fileview->setOptionClickEnabled(false);
        fileview->setCodeCompletionEnabled(false);
        fileview->setScriptType(QtSLiMTextEdit::NoScriptType);
        fileview->setSyntaxHighlightType(QtSLiMTextEdit::NoHighlighting);
        fileview->setReadOnly(true);
        fileview->setVisible(false);
        windowLayout->addWidget(fileview);
        
        // Make a new tab and add it at the end of the tab bar
        QString filename = QString::fromStdString(Eidos_LastPathComponent(path));
        
        fileIndex = filePaths.size();
        ui->tabBar->insertTab(ui->tabBar->count(), filename);
        ui->tabBar->setTabToolTip(ui->tabBar->count() - 1, filename);
        
        // Add the new view's info
        filePaths.emplace_back(path);
        fileViews.emplace_back(fileview);
    }
    
    // Then append the new text to the view, as with the built in textviews
    if (!append)
        fileview->setPlainText("");
    
    fileview->moveCursor(QTextCursor::End);
    
    bool hasPrecedingLines = (fileview->textCursor().position() != 0);
    
    for (const std::string &str : lines)
    {
        if (hasPrecedingLines)
            fileview->insertPlainText("\n");
        fileview->insertPlainText(QString::fromStdString(str));
        hasPrecedingLines = true;
    }
    
    fileview->moveCursor(QTextCursor::End);
    
    tabReceivedInput(fileIndex + 3 + logfilePaths.size());
}

void QtSLiMDebugOutputWindow::clearAllOutput(void)
{
    ui->debugOutputTextEdit->setPlainText("");
    ui->runOutputTextEdit->setPlainText("");
    ui->schedulingOutputTextEdit->setPlainText("");
    
    // Remove all tabs but the base three completely; they may not exist again after recycling
    while (ui->tabBar->count() > 3)
        ui->tabBar->removeTab(3);
    
    // Reset the base three tabs to the default text color
    ui->tabBar->setTabTextColor(0, ui->tabBar->tabTextColor(-1));
    ui->tabBar->setTabTextColor(1, ui->tabBar->tabTextColor(-1));
    ui->tabBar->setTabTextColor(2, ui->tabBar->tabTextColor(-1));
    
    logfilePaths.clear();
    logfileViews.clear();
    logfileLineNumbers.clear();
    filePaths.clear();
    fileViews.clear();
}

void QtSLiMDebugOutputWindow::clearOutputClicked(void)
{
    if (ui->debugOutputTextEdit->isVisible())
        ui->debugOutputTextEdit->setPlainText("");
    
    if (ui->runOutputTextEdit->isVisible())
        ui->runOutputTextEdit->setPlainText("");
    
    if (ui->schedulingOutputTextEdit->isVisible())
        ui->schedulingOutputTextEdit->setPlainText("");
    
    for (QTableWidget *table : logfileViews)
        if (table->isVisible())
            table->setRowCount(0);
    
    for (QtSLiMTextEdit *file : fileViews)
        if (file->isVisible())
            file->setPlainText("");
}

void QtSLiMDebugOutputWindow::selectedTabChanged(void)
{
    int tabIndex = ui->tabBar->currentIndex();
    
    ui->tabBar->setTabTextColor(tabIndex, ui->tabBar->tabTextColor(-1));  // set to an invalid color to clear to the default color
    
    if (tabIndex == 0)
    {
        showDebugOutput();
        return;
    }
    else if (tabIndex == 1)
    {
        showRunOutput();
        return;
    }
    else if (tabIndex == 2)
    {
        showSchedulingOutput();
        return;
    }
    else
    {
        tabIndex -= 3;                      // zero-base the index for logfilePaths
        
        if ((tabIndex >= 0) && (tabIndex < (int)logfilePaths.size()))
        {
            showLogFile(tabIndex);
            return;
        }
        
        tabIndex -= logfilePaths.size();    // zero-base the index for filePaths
        
        if ((tabIndex >= 0) && (tabIndex < (int)fileViews.size()))
        {
            showFile(tabIndex);
            return;
        }
        
        qDebug() << "unexpected current tab index" << ui->tabBar->currentIndex() << "in selectedTabChanged()";
    }
}

void QtSLiMDebugOutputWindow::clearOutputPressed(void)
{
    ui->clearOutputButton->qtslimSetHighlight(true);
}

void QtSLiMDebugOutputWindow::clearOutputReleased(void)
{
    ui->clearOutputButton->qtslimSetHighlight(false);
}

void QtSLiMDebugOutputWindow::closeEvent(QCloseEvent *p_event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMDebugOutputWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // send our close signal
    emit willClose();
    
    // use super's default behavior
    QWidget::closeEvent(p_event);
}

































