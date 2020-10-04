//
//  QtSLiMFindPanel.h
//  SLiM
//
//  Created by Ben Haller on 3/24/2020.
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

#ifndef QTSLIMFINDPANEL_H
#define QTSLIMFINDPANEL_H

#include <QDialog>

class QCloseEvent;
class QTextEdit;


namespace Ui {
class QtSLiMFindPanel;
}

class QtSLiMFindPanel : public QDialog
{
    Q_OBJECT
    
public:
    static QtSLiMFindPanel &instance(void);
    
    QTextEdit *targetTextEditRequireModifiable(bool requireModifiable); // public for menu enabling
    
public slots:
    void showFindPanel(void);
    void findNext(void);
    void findPrevious(void);
    void replaceAndFind(void);
    void replace(void);
    void replaceAll(void);
    void useSelectionForFind(void);
    void useSelectionForReplace(void);
    void jumpToSelection(void);
    void jumpToLine(void);
    void fixEnableState(void);
    
private:
    // singleton pattern
    explicit QtSLiMFindPanel(QWidget *parent = nullptr);
    QtSLiMFindPanel(void) = delete;
    virtual ~QtSLiMFindPanel(void) override;
    QtSLiMFindPanel(const QtSLiMFindPanel&) = delete;
    QtSLiMFindPanel& operator=(const QtSLiMFindPanel&) = delete;
    
    virtual void closeEvent(QCloseEvent *e) override;
    
    bool findForwardWrapBeep(QTextEdit *target, bool forward, bool wrap, bool beepIfNotFound);
    
    bool changingFindText = false;
    
private slots:
    void findBufferChanged(void);
    void findTextChanged(void);
    void replaceTextChanged(void);
    void optionsChanged(void);
    
private:
    Ui::QtSLiMFindPanel *ui;
};


#endif // QTSLIMFINDPANEL_H


































