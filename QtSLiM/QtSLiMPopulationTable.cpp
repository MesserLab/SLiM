//
//  QtSLiMPopulationTable.cpp
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
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


#include "QtSLiMPopulationTable.h"
#include "QtSLiMWindow.h"
#include "subpopulation.h"

#include <QDebug>


QtSLiMPopulationTableModel::QtSLiMPopulationTableModel(QObject *parent) : QAbstractTableModel(parent)
{
    // parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(parent) == nullptr)
        throw parent;
}

QtSLiMPopulationTableModel::~QtSLiMPopulationTableModel() 
{
}

int QtSLiMPopulationTableModel::rowCount(const QModelIndex & /* parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller && !controller->invalidSimulation())
        return static_cast<int>(controller->sim->population_.subpops_.size());
    
    return 0;
}

int QtSLiMPopulationTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return 6;
}

QVariant QtSLiMPopulationTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (!controller || controller->invalidSimulation())
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        SLiMSim *sim = controller->sim;
        Population &population = sim->population_;
        int subpopCount = static_cast<int>(population.subpops_.size());
        
        if (index.row() < subpopCount)
        {
            auto popIter = population.subpops_.begin();
            
            std::advance(popIter, index.row());
            slim_objectid_t subpop_id = popIter->first;
            Subpopulation *subpop = popIter->second;
            
            if (index.column() == 0)
            {
                return QVariant(QString("p%1").arg(subpop_id));
            }
            else if (index.column() == 1)
            {
                return QVariant(QString("%1").arg(subpop->parent_subpop_size_));
            }
            else if (sim->ModelType() == SLiMModelType::kModelTypeNonWF)
            {
                // in nonWF models selfing/cloning/sex rates/ratios are emergent, calculated from collected metrics
                double total_offspring = subpop->gui_offspring_cloned_M_ + subpop->gui_offspring_crossed_ + subpop->gui_offspring_empty_ + subpop->gui_offspring_selfed_;
                
                if (subpop->sex_enabled_)
                    total_offspring += subpop->gui_offspring_cloned_F_;		// avoid double-counting clones when we are modeling hermaphrodites
                
                if (index.column() == 2)
                {
                    if (!subpop->sex_enabled_ && (total_offspring > 0))
                        return QVariant(QString("%1").arg(subpop->gui_offspring_selfed_ / total_offspring, 0, 'f', 2));
                }
                else if (index.column() == 3)
                {
                    if (total_offspring > 0)
                        return QVariant(QString("%1").arg(subpop->gui_offspring_cloned_F_ / total_offspring, 0, 'f', 2));
                }
                else if (index.column() == 4)
                {
                    if (total_offspring > 0)
                        return QVariant(QString("%1").arg(subpop->gui_offspring_cloned_M_ / total_offspring, 0, 'f', 2));
                }
                else if (index.column() == 5)
                {
                    if (subpop->sex_enabled_ && (subpop->parent_subpop_size_ > 0))
                        return QVariant(QString("%1").arg(1.0 - subpop->parent_first_male_index_ / static_cast<double>(subpop->parent_subpop_size_), 0, 'f', 2));
                }
                
                return QVariant("—");
            }
            else	// sim->ModelType() == SLiMModelType::kModelTypeWF
            {
                if (index.column() == 2)
                {
                    if (subpop->sex_enabled_)
                        return QVariant("—");
                    else
                        return QVariant(QString("%1").arg(subpop->selfing_fraction_, 0, 'f', 2));
                }
                else if (index.column() == 3)
                {
                    return QVariant(QString("%1").arg(subpop->female_clone_fraction_, 0, 'f', 2));
                }
                else if (index.column() == 4)
                {
                    return QVariant(QString("%1").arg(subpop->male_clone_fraction_, 0, 'f', 2));
                }
                else if (index.column() == 5)
                {
                    if (subpop->sex_enabled_)
                        return QVariant(QString("%1").arg(subpop->parent_sex_ratio_, 0, 'f', 2));
                    else
                        return QVariant("—");
                }
            }
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 4: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 5: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    
    return QVariant();
}

QVariant QtSLiMPopulationTableModel::headerData(int section,
                                Qt::Orientation /* orientation */,
                                int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QVariant("ID");
        case 1: return QVariant("N");
        //case 2: return QVariant("self");
        //case 3: return QVariant("clF");
        //case 4: return QVariant("clM");
        //case 5: return QVariant("SR");
        default: return QVariant("");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case 0: return QVariant("the Eidos identifier for the subpopulation");
        case 1: return QVariant("the subpopulation size");
        case 2: return QVariant("the selfing rate of the subpopulation");
        case 3: return QVariant("the cloning rate of the subpopulation, for females");
        case 4: return QVariant("the cloning rate of the subpopulation, for males");
        case 5: return QVariant("the sex ratio of the subpopulation, M:(M+F)");
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (section)
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 4: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 5: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
//    else if (role == Qt::DecorationRole)
//    {
//        switch (section)
//        {
//        case 2: return QVariant::fromValue(QIcon(":/buttons/Qt_selfing_rate.png"));
//        case 3: return QVariant::fromValue(QIcon(":/buttons/Qt_female_symbol.png"));
//        case 4: return QVariant::fromValue(QIcon(":/buttons/Qt_male_symbol.png"));
//        case 5: return QVariant::fromValue(QIcon(":/buttons/Qt_sex_ratio.png"));
//        }
//    }
    return QVariant();
}

void QtSLiMPopulationTableModel::reloadTable(void)
{
    beginResetModel();
    endResetModel();
}

QtSLiMPopulationTableHeaderView::QtSLiMPopulationTableHeaderView(Qt::Orientation orientation, QWidget *parent) : QHeaderView(orientation, parent)
{
    icon_cloning_rate = new QIcon(":/buttons/Qt_cloning_rate.png");
    icon_selfing_rate = new QIcon(":/buttons/Qt_selfing_rate.png");
    icon_sex_ratio = new QIcon(":/buttons/Qt_sex_ratio.png");
    icon_female_symbol = new QIcon(":/buttons/Qt_female_symbol.png");
    icon_male_symbol = new QIcon(":/buttons/Qt_male_symbol.png");    
}

QtSLiMPopulationTableHeaderView::~QtSLiMPopulationTableHeaderView()
{
    if (icon_cloning_rate)
    {
        delete icon_cloning_rate;
        icon_cloning_rate = nullptr;
    }
    if (icon_selfing_rate)
    {
        delete icon_selfing_rate;
        icon_selfing_rate = nullptr;
    }
    if (icon_sex_ratio)
    {
        delete icon_sex_ratio;
        icon_sex_ratio = nullptr;
    }
    if (icon_female_symbol)
    {
        delete icon_female_symbol;
        icon_female_symbol = nullptr;
    }
    if (icon_male_symbol)
    {
        delete icon_male_symbol;
        icon_male_symbol = nullptr;
    }
}

void QtSLiMPopulationTableHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    painter->save();
    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();
    
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    
    switch (logicalIndex)
    {
    case 2:
    case 5:
    {
        QIcon *icon = (logicalIndex == 2 ? icon_selfing_rate : icon_sex_ratio);
        QPoint center = rect.center();
        
        icon->paint(painter, center.x() - 5, center.y() - 6, 12, 12);
        break;
    }
    case 3:
    {
        QIcon *icon1 = icon_cloning_rate;
        QIcon *icon2 = icon_female_symbol;
        QPoint center = rect.center();
        
        icon1->paint(painter, center.x() - 11, center.y() - 6, 12, 12);
        icon2->paint(painter, center.x() + 1, center.y() - 6, 12, 12);
        break;
    }
    case 4:
    {
        QIcon *icon1 = icon_cloning_rate;
        QIcon *icon2 = icon_male_symbol;
        QPoint center = rect.center();
        
        icon1->paint(painter, center.x() - 13, center.y() - 6, 12, 12);
        icon2->paint(painter, center.x() + 1, center.y() - 6, 12, 12);
        break;
    }
    default: break;
    }
    
    painter->restore();
}


































