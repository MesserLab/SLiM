//
//  QtSLiMDebugOutputWindow.h
//  SLiM
//
//  Created by Ben Haller on 02/06/2021.
//  Copyright (c) 2021-2023 Philipp Messer.  All rights reserved.
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

#ifndef QTSLIMDEBUGOUTPUTWINDOW_H
#define QTSLIMDEBUGOUTPUTWINDOW_H

#include <QWidget>


class QCloseEvent;
class QtSLiMWindow;
class QtSLiMTextEdit;
class QTableWidget;


namespace Ui {
class QtSLiMDebugOutputWindow;
}

class QtSLiMDebugOutputWindow : public QWidget
{
    Q_OBJECT
    
public:
    QtSLiMWindow *parentSLiMWindow = nullptr;     // a copy of parent with the correct class, for convenience
    
    explicit QtSLiMDebugOutputWindow(QtSLiMWindow *p_parent = nullptr);
    virtual ~QtSLiMDebugOutputWindow() override;
    
    // Our various output views, which each collect output independently
    void takeDebugOutput(QString str);
    void takeRunOutput(QString str);
    void takeSchedulingOutput(QString str);
    void takeLogFileOutput(std::vector<std::string> &lineElements, const std::string &path);
    void takeFileOutput(std::vector<std::string> &lines, bool append, const std::string &path);
    
public slots:
    void clearAllOutput(void);
    void clearOutputClicked(void);
    
    void showDebugOutput(void);
    void showRunOutput(void);
    void showSchedulingOutput(void);
    void showLogFile(int logFileIndex);
    void showFile(int fileIndex);
    
signals:
    void willClose(void);
    
private slots:
    void tabReceivedInput(int tabIndex);
    void selectedTabChanged(void);
    virtual void closeEvent(QCloseEvent *p_event) override;
    void clearOutputPressed(void);
    void clearOutputReleased(void);
    
private:
    Ui::QtSLiMDebugOutputWindow *ui;
    
    // all the LogFile paths we have seen, views containing their output, and the last line number emitted
    std::vector<std::string> logfilePaths;
    std::vector<QTableWidget *> logfileViews;
    std::vector<size_t> logfileLineNumbers;
    
    // all the ordinary file paths we have seen, from writeFile() and similar, and output views
    std::vector<std::string> filePaths;
    std::vector<QtSLiMTextEdit *> fileViews;
    
    void resetTabIcons(void);
    void hideAllViews(void);
};


#endif // QTSLIMDEBUGOUTPUTWINDOW_H


































