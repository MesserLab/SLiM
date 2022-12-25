//
//  QtSLiMPopulationTable.cpp
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
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


#include "QtSLiMPopulationTable.h"
#include "QtSLiMWindow.h"
#include "QtSLiMAppDelegate.h"
#include "subpopulation.h"

#include <QDebug>


QtSLiMPopulationTableModel::QtSLiMPopulationTableModel(QObject *p_parent) : QAbstractTableModel(p_parent)
{
    // p_parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(p_parent) == nullptr)
        throw p_parent;
}

QtSLiMPopulationTableModel::~QtSLiMPopulationTableModel() 
{
}

int QtSLiMPopulationTableModel::rowCount(const QModelIndex & /* parent */) const
{
    return static_cast<int>(displaySubpops.size());
}

int QtSLiMPopulationTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return 6;
}

QVariant QtSLiMPopulationTableModel::data(const QModelIndex &p_index, int role) const
{
    if (!p_index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    int subpopCount = static_cast<int>(displaySubpops.size());
    
    if (subpopCount == 0)
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        if (p_index.row() < subpopCount)
        {
            auto popIter = displaySubpops.begin();
            
            std::advance(popIter, p_index.row());
            Subpopulation *subpop = *popIter;
            
            if (p_index.column() == 0)
            {
                QString idString = QString("p%1").arg(subpop->subpopulation_id_);
                
                if (community->all_species_.size() > 1)
                    idString.append(" ").append(QString::fromStdString(subpop->species_.avatar_));
                
                return QVariant(idString);
            }
            else if (p_index.column() == 1)
            {
                return QVariant(QString("%1").arg(subpop->parent_subpop_size_));
            }
            else if (community->ModelType() == SLiMModelType::kModelTypeNonWF)
            {
                // in nonWF models selfing/cloning/sex rates/ratios are emergent, calculated from collected metrics
                double total_offspring = subpop->gui_offspring_cloned_M_ + subpop->gui_offspring_crossed_ + subpop->gui_offspring_empty_ + subpop->gui_offspring_selfed_;
                
                if (subpop->sex_enabled_)
                    total_offspring += subpop->gui_offspring_cloned_F_;		// avoid double-counting clones when we are modeling hermaphrodites
                
                if (p_index.column() == 2)
                {
                    if (!subpop->sex_enabled_ && (total_offspring > 0))
                        return QVariant(QString("%1").arg(subpop->gui_offspring_selfed_ / total_offspring, 0, 'f', 2));
                }
                else if (p_index.column() == 3)
                {
                    if (total_offspring > 0)
                        return QVariant(QString("%1").arg(subpop->gui_offspring_cloned_F_ / total_offspring, 0, 'f', 2));
                }
                else if (p_index.column() == 4)
                {
                    if (total_offspring > 0)
                        return QVariant(QString("%1").arg(subpop->gui_offspring_cloned_M_ / total_offspring, 0, 'f', 2));
                }
                else if (p_index.column() == 5)
                {
                    if (subpop->sex_enabled_ && (subpop->parent_subpop_size_ > 0))
                        return QVariant(QString("%1").arg(1.0 - subpop->parent_first_male_index_ / static_cast<double>(subpop->parent_subpop_size_), 0, 'f', 2));
                }
                
                return QVariant("—");
            }
            else	// sim->ModelType() == SLiMModelType::kModelTypeWF
            {
                if (p_index.column() == 2)
                {
                    if (subpop->sex_enabled_)
                        return QVariant("—");
                    else
                        return QVariant(QString("%1").arg(subpop->selfing_fraction_, 0, 'f', 2));
                }
                else if (p_index.column() == 3)
                {
                    return QVariant(QString("%1").arg(subpop->female_clone_fraction_, 0, 'f', 2));
                }
                else if (p_index.column() == 4)
                {
                    return QVariant(QString("%1").arg(subpop->male_clone_fraction_, 0, 'f', 2));
                }
                else if (p_index.column() == 5)
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
        switch (p_index.column())
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
                                Qt::Orientation /* p_orientation */,
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
//        case 2: return QVariant::fromValue(QIcon(QtSLiMImagePath("Qt_selfing_rate", false)));
//        case 3: return QVariant::fromValue(QIcon(QtSLiMImagePath("Qt_female_symbol", false)));
//        case 4: return QVariant::fromValue(QIcon(QtSLiMImagePath("Qt_male_symbol", false)));
//        case 5: return QVariant::fromValue(QIcon(QtSLiMImagePath("Qt_sex_ratio", false)));
//        }
//    }
    return QVariant();
}

void QtSLiMPopulationTableModel::reloadTable(void)
{
    beginResetModel();
    
    // recache the list of subpopulations we display
    displaySubpops.clear();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller)
        displaySubpops = controller->listedSubpopulations();
    
    endResetModel();
}

QtSLiMPopulationTableHeaderView::QtSLiMPopulationTableHeaderView(Qt::Orientation p_orientation, QWidget *p_parent) : QHeaderView(p_orientation, p_parent)
{
    cacheIcons();
    
    // Recache our icons if the light mode  / dark mode setting changes
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, [this]() { freeCachedIcons(); cacheIcons(); });
}

void QtSLiMPopulationTableHeaderView::freeCachedIcons(void)
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

void QtSLiMPopulationTableHeaderView::cacheIcons(void)
{
    // Note that this caches the icons for the current light mode / dark mode setting; they will be recached if the mode changes
    icon_cloning_rate = new QIcon(QtSLiMImagePath("Qt_cloning_rate", false));
    icon_selfing_rate = new QIcon(QtSLiMImagePath("Qt_selfing_rate", false));
    icon_sex_ratio = new QIcon(QtSLiMImagePath("Qt_sex_ratio", false));
    icon_female_symbol = new QIcon(QtSLiMImagePath("Qt_female_symbol", false));
    icon_male_symbol = new QIcon(QtSLiMImagePath("Qt_male_symbol", false));    
}

QtSLiMPopulationTableHeaderView::~QtSLiMPopulationTableHeaderView()
{
    freeCachedIcons();
}

void QtSLiMPopulationTableHeaderView::paintSection(QPainter *painter, const QRect &p_rect, int p_logicalIndex) const
{
    painter->save();
    QHeaderView::paintSection(painter, p_rect, p_logicalIndex);
    painter->restore();
    
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    
    switch (p_logicalIndex)
    {
    case 2:
    case 5:
    {
        QIcon *icon = (p_logicalIndex == 2 ? icon_selfing_rate : icon_sex_ratio);
        QPoint center = p_rect.center();
        
        icon->paint(painter, center.x() - 5, center.y() - 6, 12, 12);
        break;
    }
    case 3:
    {
        QIcon *icon1 = icon_cloning_rate;
        QIcon *icon2 = icon_female_symbol;
        QPoint center = p_rect.center();
        
        icon1->paint(painter, center.x() - 11, center.y() - 6, 12, 12);
        icon2->paint(painter, center.x() + 1, center.y() - 6, 12, 12);
        break;
    }
    case 4:
    {
        QIcon *icon1 = icon_cloning_rate;
        QIcon *icon2 = icon_male_symbol;
        QPoint center = p_rect.center();
        
        icon1->paint(painter, center.x() - 13, center.y() - 6, 12, 12);
        icon2->paint(painter, center.x() + 1, center.y() - 6, 12, 12);
        break;
    }
    default: break;
    }
    
    painter->restore();
}


































