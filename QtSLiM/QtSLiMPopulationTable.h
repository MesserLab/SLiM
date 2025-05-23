//
//  QtSLiMPopulationTable.h
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
//  Copyright (c) 2019-2025 Benjamin C. Haller.  All rights reserved.
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

#ifndef QTSLIMPOPULATIONTABLE_H
#define QTSLIMPOPULATIONTABLE_H

#include <QObject>
#include <QAbstractTableModel>
#include <QHeaderView>

class QPainter;
class Subpopulation;


class QtSLiMPopulationTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMPopulationTableModel(QObject *p_parent = nullptr);
    virtual ~QtSLiMPopulationTableModel() override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation p_orientation, int role = Qt::DisplayRole) const override;
    
    bool needsUpdateForDisplaySubpops(std::vector<Subpopulation *> &newDisplayList);
    void reloadTable(std::vector<Subpopulation *> &newDisplayList);
    
    Subpopulation *subpopAtIndex(int i) const { return displaySubpops[i]; }
    
protected:
    // We cache a list of the subpopulations we are displaying, for more efficient display
    std::vector<Subpopulation *> displaySubpops;
};

class QtSLiMPopulationTableHeaderView : public QHeaderView
{
    Q_OBJECT
    
    QIcon *icon_cloning_rate = nullptr;
    QIcon *icon_selfing_rate = nullptr;
    QIcon *icon_sex_ratio = nullptr;
    QIcon *icon_female_symbol = nullptr;
    QIcon *icon_male_symbol = nullptr;
    
public:
    QtSLiMPopulationTableHeaderView(Qt::Orientation p_orientation, QWidget *p_parent = nullptr);
    virtual ~QtSLiMPopulationTableHeaderView() override;
    
    virtual void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    
protected:
    void freeCachedIcons(void);
    void cacheIcons(void);
};


#endif // QTSLIMPOPULATIONTABLE_H









































