//
//  QtSLiMDebugOutputWindow.h
//  SLiM
//
//  Created by Ben Haller on 02/06/2021.
//  Copyright (c) 2021 Philipp Messer.  All rights reserved.
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
    
    QtSLiMTextEdit *debugOutputTextView(void);
    
public slots:
    void clearOutputClicked(void);
    
signals:
    void willClose(void);
    
private slots:
    virtual void closeEvent(QCloseEvent *p_event) override;
    void clearOutputPressed(void);
    void clearOutputReleased(void);
    
private:
    Ui::QtSLiMDebugOutputWindow *ui;
};


#endif // QTSLIMDEBUGOUTPUTWINDOW_H


































