//
//  QtSLiMTablesDrawer.h
//  SLiM
//
//  Created by Ben Haller on 2/22/2020.
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

#ifndef QTSLIMTABLESDRAWER_H
#define QTSLIMTABLESDRAWER_H

#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QWidget>

class QCloseEvent;
class QtSLiMWindow;
class QTableView;
class QHeaderView;

class QtSLiMMutTypeTableModel;
class QtSLiMGETypeTypeTableModel;
class QtSLiMInteractionTypeTableModel;
class QtSLiMEidosBlockTableModel;


namespace Ui {
class QtSLiMTablesDrawer;
}

class QtSLiMTablesDrawer : public QWidget
{
    Q_OBJECT
    
public:
    QtSLiMWindow *parentSLiMWindow = nullptr;     // a copy of parent with the correct class, for convenience
    
    explicit QtSLiMTablesDrawer(QtSLiMWindow *parent = nullptr);
    virtual ~QtSLiMTablesDrawer() override;
    
signals:
    void willClose(void);
    
private slots:
    virtual void closeEvent(QCloseEvent *event) override;
    
private:
    Ui::QtSLiMTablesDrawer *ui;
    
    QtSLiMMutTypeTableModel *mutTypeTableModel_ = nullptr;
    QtSLiMGETypeTypeTableModel *geTypeTableModel_ = nullptr;
    QtSLiMInteractionTypeTableModel *interactionTypeTableModel_ = nullptr;
    QtSLiMEidosBlockTableModel *eidosBlockTableModel_ = nullptr;
    
    QHeaderView *configureTableView(QTableView *tableView);
    void initializeUI(void);
    
    friend QtSLiMWindow;
};


//
//  Declare models for the four table views; has to be in header file for MOC
//

class QtSLiMMutTypeTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMMutTypeTableModel(QObject *parent = nullptr);
    virtual ~QtSLiMMutTypeTableModel() override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void reloadTable(void);
};

class QtSLiMGETypeTypeTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMGETypeTypeTableModel(QObject *parent = nullptr);
    virtual ~QtSLiMGETypeTypeTableModel() override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void reloadTable(void);
};

class QtSLiMInteractionTypeTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMInteractionTypeTableModel(QObject *parent = nullptr);
    virtual ~QtSLiMInteractionTypeTableModel() override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void reloadTable(void);
};

class QtSLiMEidosBlockTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMEidosBlockTableModel(QObject *parent = nullptr);
    virtual ~QtSLiMEidosBlockTableModel() override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void reloadTable(void);
};


//
//  Drawing delegates for custom drawing in the table views
//

class QtSLiMGETypeTypeTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    
public:
    QtSLiMGETypeTypeTableDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    virtual ~QtSLiMGETypeTypeTableDelegate(void) override;
    
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


#endif // QTSLIMTABLESDRAWER_H






























