//
//  QtSLiMTablesDrawer.cpp
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


#include "QtSLiMTablesDrawer.h"
#include "ui_QtSLiMTablesDrawer.h"

#include <QPainter>
#include <QKeyEvent>
#include <QDebug>

#include "QtSLiMWindow.h"


//
//  QtSLiMTablesDrawer
//

QtSLiMTablesDrawer::QtSLiMTablesDrawer(QtSLiMWindow *parent) :
    QDialog(parent),
    parentSLiMWindow(parent),
    ui(new Ui::QtSLiMTablesDrawer)
{
    ui->setupUi(this);
    initializeUI();
}

QtSLiMTablesDrawer::~QtSLiMTablesDrawer()
{
    delete ui;
}

QHeaderView *QtSLiMTablesDrawer::configureTableView(QTableView *tableView)
{
    QHeaderView *tableHHeader = tableView->horizontalHeader();
    QHeaderView *tableVHeader = tableView->verticalHeader();
    
    tableHHeader->setMinimumSectionSize(1);
    tableVHeader->setMinimumSectionSize(1);
    
    tableHHeader->setSectionsClickable(false);
    tableHHeader->setSectionsMovable(false);
    
    QFont headerFont = tableHHeader->font();
    QFont cellFont = tableView->font();
#ifdef __APPLE__
    headerFont.setPointSize(11);
    cellFont.setPointSize(11);
#else
    headerFont.setPointSize(8);
    cellFont.setPointSize(8);
#endif
    tableHHeader->setFont(headerFont);
    tableView->setFont(cellFont);
    
    tableVHeader->setSectionResizeMode(QHeaderView::Fixed);
    tableVHeader->setDefaultSectionSize(18);
    
    return tableHHeader;
}

void QtSLiMTablesDrawer::initializeUI(void)
{
    // no window icon
    setWindowIcon(QIcon());
    
    // Make the models for the tables; this is a sort of datasource concept, except
    // that because C++ is not sufficiently dynamic it has to be a separate object
    mutTypeTableModel_ = new QtSLiMMutTypeTableModel(parentSLiMWindow);
    ui->mutationTypeTable->setModel(mutTypeTableModel_);
    
    geTypeTableModel_ = new QtSLiMGETypeTypeTableModel(parentSLiMWindow);
    ui->genomicElementTypeTable->setModel(geTypeTableModel_);

    interactionTypeTableModel_ = new QtSLiMInteractionTypeTableModel(parentSLiMWindow);
    ui->interactionTypeTable->setModel(interactionTypeTableModel_);

    eidosBlockTableModel_ = new QtSLiMEidosBlockTableModel(parentSLiMWindow);
    ui->eidosBlockTable->setModel(eidosBlockTableModel_);
    
    // Configure the table views, then set column widths and sizing behavior
    {
        QHeaderView *mutTypeTableHHeader = configureTableView(ui->mutationTypeTable);
        
        mutTypeTableHHeader->resizeSection(0, 43);
        mutTypeTableHHeader->resizeSection(1, 43);
        mutTypeTableHHeader->resizeSection(2, 53);
        //mutTypeTableHHeader->resizeSection(3, ?);
        mutTypeTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        mutTypeTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        mutTypeTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
        mutTypeTableHHeader->setSectionResizeMode(3, QHeaderView::Stretch);
    }
    {
        QHeaderView *geTypeTableHHeader = configureTableView(ui->genomicElementTypeTable);
        
        geTypeTableHHeader->resizeSection(0, 43);
        geTypeTableHHeader->resizeSection(1, 43);
        //geTypeTableHHeader->resizeSection(2, ?);
        geTypeTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        geTypeTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        geTypeTableHHeader->setSectionResizeMode(2, QHeaderView::Stretch);
        
        QAbstractItemDelegate *tableDelegate = new QtSLiMGETypeTypeTableDelegate();
        ui->genomicElementTypeTable->setItemDelegate(tableDelegate);
    }
    {
        QHeaderView *interactionTypeTableHHeader = configureTableView(ui->interactionTypeTable);
        
        interactionTypeTableHHeader->resizeSection(0, 43);
        interactionTypeTableHHeader->resizeSection(1, 43);
        interactionTypeTableHHeader->resizeSection(2, 53);
        //interactionTypeTableHHeader->resizeSection(3, ?);
        interactionTypeTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        interactionTypeTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        interactionTypeTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
        interactionTypeTableHHeader->setSectionResizeMode(3, QHeaderView::Stretch);
    }
    {
        QHeaderView *eidosBlockTableHHeader = configureTableView(ui->eidosBlockTable);
        
        eidosBlockTableHHeader->resizeSection(0, 43);
        eidosBlockTableHHeader->resizeSection(1, 63);
        eidosBlockTableHHeader->resizeSection(2, 63);
        //eidosBlockTableHHeader->resizeSection(3, ?);
        eidosBlockTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        eidosBlockTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        eidosBlockTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
        eidosBlockTableHHeader->setSectionResizeMode(3, QHeaderView::Stretch);
    }
}

void QtSLiMTablesDrawer::closeEvent(QCloseEvent *event)
{
    // send our close signal
    emit willClose();
    
    // use super's default behavior
    QDialog::closeEvent(event);
}

void QtSLiMTablesDrawer::keyPressEvent(QKeyEvent *event)
{
    // Prevent escape from closing the window
    if (event->key() == Qt::Key_Escape)
        return;
    
    QDialog::keyPressEvent(event);
}


//
//  Define models for the four table views
//

QtSLiMMutTypeTableModel::QtSLiMMutTypeTableModel(QObject *parent) : QAbstractTableModel(parent)
{
    // parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(parent) == nullptr)
        throw parent;
}

QtSLiMMutTypeTableModel::~QtSLiMMutTypeTableModel()
{
}

int QtSLiMMutTypeTableModel::rowCount(const QModelIndex & /* parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller && !controller->invalidSimulation())
        return static_cast<int>(controller->sim->mutation_types_.size());
    
    return 0;
}

int QtSLiMMutTypeTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return 4;
}

QVariant QtSLiMMutTypeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (!controller || controller->invalidSimulation())
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        SLiMSim *sim = controller->sim;
        std::map<slim_objectid_t,MutationType*> &mutationTypes = sim->mutation_types_;
        int mutationTypeCount = static_cast<int>(mutationTypes.size());
        
        if (index.row() < mutationTypeCount)
        {
            auto mutTypeIter = mutationTypes.begin();
            
            std::advance(mutTypeIter, index.row());
            slim_objectid_t mutTypeID = mutTypeIter->first;
            MutationType *mutationType = mutTypeIter->second;
            
            if (index.column() == 0)
            {
                return QVariant(QString("m%1").arg(mutTypeID));
            }
            else if (index.column() == 1)
            {
                return QVariant(QString("%1").arg(static_cast<double>(mutationType->dominance_coeff_), 0, 'f', 3));
            }
            else if (index.column() == 2)
            {
                switch (mutationType->dfe_type_)
                {
                    case DFEType::kFixed:			return QVariant(QString("fixed"));
                    case DFEType::kGamma:			return QVariant(QString("gamma"));
                    case DFEType::kExponential:		return QVariant(QString("exp"));
                    case DFEType::kNormal:			return QVariant(QString("normal"));
                    case DFEType::kWeibull:			return QVariant(QString("Weibull"));
                    case DFEType::kScript:			return QVariant(QString("script"));
                }
            }
            else if (index.column() == 3)
            {
                QString paramString;
                
                if (mutationType->dfe_type_ == DFEType::kScript)
                {
                    // DFE type 's' has parameters of type string
                    for (unsigned int paramIndex = 0; paramIndex < mutationType->dfe_strings_.size(); ++paramIndex)
                    {
                        QString dfe_string = QString::fromStdString(mutationType->dfe_strings_[paramIndex]);
                        
                        paramString += ("\"" + dfe_string + "\"");
                        
                        if (paramIndex < mutationType->dfe_strings_.size() - 1)
                            paramString += ", ";
                    }
                }
                else
                {
                    // All other DFEs have parameters of type double
                    for (unsigned int paramIndex = 0; paramIndex < mutationType->dfe_parameters_.size(); ++paramIndex)
                    {
                        QString paramSymbol;
                        
                        switch (mutationType->dfe_type_)
                        {
                            case DFEType::kFixed:			paramSymbol = "s"; break;
                            case DFEType::kGamma:			paramSymbol = (paramIndex == 0 ? "s̄" : "α"); break;
                            case DFEType::kExponential:		paramSymbol = "s̄"; break;
                            case DFEType::kNormal:			paramSymbol = (paramIndex == 0 ? "s̄" : "σ"); break;
                            case DFEType::kWeibull:			paramSymbol = (paramIndex == 0 ? "λ" : "k"); break;
                            case DFEType::kScript:			break;
                        }
                        
                        paramString += QString("%1=%2").arg(paramSymbol).arg(mutationType->dfe_parameters_[paramIndex], 0, 'f', 3);
                        
                        if (paramIndex < mutationType->dfe_parameters_.size() - 1)
                            paramString += ", ";
                    }
                }
                
                return QVariant(paramString);
            }
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    
    return QVariant();
}

QVariant QtSLiMMutTypeTableModel::headerData(int section,
                                             Qt::Orientation /* orientation */,
                                             int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QVariant("ID");
        case 1: return QVariant("h");
        case 2: return QVariant("DFE");
        case 3: return QVariant("Params");
        default: return QVariant("");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case 0: return QVariant("the ID for the mutation type");
        case 1: return QVariant("the dominance coefficient");
        case 2: return QVariant("the distribution of fitness effects");
        case 3: return QVariant("the DFE parameters");
        default: return QVariant("");
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (section)
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    
    return QVariant();
}
    
void QtSLiMMutTypeTableModel::reloadTable(void)
{
    beginResetModel();
    endResetModel();
}


QtSLiMGETypeTypeTableModel::QtSLiMGETypeTypeTableModel(QObject *parent) : QAbstractTableModel(parent)
{
    // parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(parent) == nullptr)
        throw parent;
}

QtSLiMGETypeTypeTableModel::~QtSLiMGETypeTypeTableModel()
{
}

int QtSLiMGETypeTypeTableModel::rowCount(const QModelIndex & /* parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller && !controller->invalidSimulation())
        return static_cast<int>(controller->sim->genomic_element_types_.size());
    
    return 0;
}

int QtSLiMGETypeTypeTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return 3;
}

QVariant QtSLiMGETypeTypeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (!controller || controller->invalidSimulation())
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        SLiMSim *sim = controller->sim;
        std::map<slim_objectid_t,GenomicElementType*> &genomicElementTypes = sim->genomic_element_types_;
        int genomicElementTypeCount = static_cast<int>(genomicElementTypes.size());
        
        if (index.row() < genomicElementTypeCount)
        {
            auto genomicElementTypeIter = genomicElementTypes.begin();
            
            std::advance(genomicElementTypeIter, index.row());
            slim_objectid_t genomicElementTypeID = genomicElementTypeIter->first;
            GenomicElementType *genomicElementType = genomicElementTypeIter->second;
            
            if (index.column() == 0)
            {
                return QVariant(QString("g%1").arg(genomicElementTypeID));
            }
            else if (index.column() == 1)
            {
                float red, green, blue, alpha;
                
                controller->colorForGenomicElementType(genomicElementType, genomicElementTypeID, &red, &green, &blue, &alpha);
                
                QColor geTypeColor = QColor::fromRgbF(static_cast<qreal>(red), static_cast<qreal>(green), static_cast<qreal>(blue), static_cast<qreal>(alpha));
                QRgb geTypeRGB = geTypeColor.rgb();
                
                return QVariant(geTypeRGB); // return the color as an unsigned int
            }
            else if (index.column() == 2)
            {
                QString paramString;
                
                for (unsigned int mutTypeIndex = 0; mutTypeIndex < genomicElementType->mutation_fractions_.size(); ++mutTypeIndex)
                {
                    MutationType *mutType = genomicElementType->mutation_type_ptrs_[mutTypeIndex];
                    double mutTypeFraction = genomicElementType->mutation_fractions_[mutTypeIndex];
                    
                    paramString += QString("m%1=%2").arg(mutType->mutation_type_id_).arg(mutTypeFraction, 0, 'f', 3);
                    
                    if (mutTypeIndex < genomicElementType->mutation_fractions_.size() - 1)
                        paramString += ", ";
                }
                
                return QVariant(paramString);
            }
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    
    return QVariant();
}

QVariant QtSLiMGETypeTypeTableModel::headerData(int section,
                                                Qt::Orientation /* orientation */,
                                                int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QVariant("ID");
        case 1: return QVariant("Color");
        case 2: return QVariant("Mutation types");
        default: return QVariant("");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case 0: return QVariant("the ID for the genomic element type");
        case 1: return QVariant("the color used in QtSLiM");
        case 2: return QVariant("the mutation types drawn from");
        default: return QVariant("");
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (section)
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}
    
void QtSLiMGETypeTypeTableModel::reloadTable(void)
{
    beginResetModel();
    endResetModel();
}


QtSLiMInteractionTypeTableModel::QtSLiMInteractionTypeTableModel(QObject *parent) : QAbstractTableModel(parent)
{
    // parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(parent) == nullptr)
        throw parent;
}

QtSLiMInteractionTypeTableModel::~QtSLiMInteractionTypeTableModel()
{
}

int QtSLiMInteractionTypeTableModel::rowCount(const QModelIndex & /* parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller && !controller->invalidSimulation())
        return static_cast<int>(controller->sim->interaction_types_.size());
    
    return 0;
}

int QtSLiMInteractionTypeTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return 4;
}

QVariant QtSLiMInteractionTypeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (!controller || controller->invalidSimulation())
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        SLiMSim *sim = controller->sim;
        std::map<slim_objectid_t,InteractionType*> &interactionTypes = sim->interaction_types_;
        int interactionTypeCount = static_cast<int>(interactionTypes.size());
        
        if (index.row() < interactionTypeCount)
        {
            auto interactionTypeIter = interactionTypes.begin();
            
            std::advance(interactionTypeIter, index.row());
            slim_objectid_t interactionTypeID = interactionTypeIter->first;
            InteractionType *interactionType = interactionTypeIter->second;
            
            if (index.column() == 0)
            {
                return QVariant(QString("i%1").arg(interactionTypeID));
            }
            else if (index.column() == 1)
            {
                return QVariant(QString("%1").arg(interactionType->max_distance_, 0, 'f', 3));
            }
            else if (index.column() == 2)
            {
                switch (interactionType->if_type_)
                {
                    case IFType::kFixed:			return QVariant(QString("fixed"));
                    case IFType::kLinear:			return QVariant(QString("linear"));
                    case IFType::kExponential:		return QVariant(QString("exp"));
                    case IFType::kNormal:			return QVariant(QString("normal"));
                    case IFType::kCauchy:			return QVariant(QString("Cauchy"));
                }
            }
            else if (index.column() == 3)
            {
                QString paramString;
                
                // the first parameter is always the maximum interaction strength
                paramString += QString("f=%1").arg(interactionType->if_param1_, 0, 'f', 3);
                
                // append second parameters where applicable
                switch (interactionType->if_type_)
                {
                    case IFType::kFixed:
                    case IFType::kLinear:
                        break;
                    case IFType::kExponential:
                        paramString += QString(", β=%1").arg(interactionType->if_param2_, 0, 'f', 3);
                        break;
                    case IFType::kNormal:
                        paramString += QString(", σ=%1").arg(interactionType->if_param2_, 0, 'f', 3);
                        break;
                    case IFType::kCauchy:
                        paramString += QString(", γ=%1").arg(interactionType->if_param2_, 0, 'f', 3);
                        break;
                }
                
                return QVariant(paramString);
            }
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    
    return QVariant();
}

QVariant QtSLiMInteractionTypeTableModel::headerData(int section,
                                                     Qt::Orientation /* orientation */,
                                                     int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QVariant("ID");
        case 1: return QVariant("max");
        case 2: return QVariant("IF");
        case 3: return QVariant("Params");
        default: return QVariant("");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case 0: return QVariant("the ID for the interaction type");
        case 1: return QVariant("the maximum interaction distance");
        case 2: return QVariant("the interaction function");
        case 3: return QVariant("the interaction function parameters");
        default: return QVariant("");
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (section)
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}
    
void QtSLiMInteractionTypeTableModel::reloadTable(void)
{
    beginResetModel();
    endResetModel();
}


QtSLiMEidosBlockTableModel::QtSLiMEidosBlockTableModel(QObject *parent) : QAbstractTableModel(parent)
{
    // parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(parent) == nullptr)
        throw parent;
}

QtSLiMEidosBlockTableModel::~QtSLiMEidosBlockTableModel()
{
}

int QtSLiMEidosBlockTableModel::rowCount(const QModelIndex & /* parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller && !controller->invalidSimulation())
        return static_cast<int>(controller->sim->AllScriptBlocks().size());
    
    return 0;
}

int QtSLiMEidosBlockTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return 4;
}

QVariant QtSLiMEidosBlockTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (!controller || controller->invalidSimulation())
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        SLiMSim *sim = controller->sim;
        std::vector<SLiMEidosBlock*> &scriptBlocks = sim->AllScriptBlocks();
        int scriptBlockCount = static_cast<int>(scriptBlocks.size());
        
        if (index.row() < scriptBlockCount)
        {
            SLiMEidosBlock *scriptBlock = scriptBlocks[static_cast<size_t>(index.row())];
            
            if (index.column() == 0)
            {
                slim_objectid_t block_id = scriptBlock->block_id_;
                
                if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                    return QVariant("—");
                else if (block_id == -1)
                    return QVariant("—");
                else
                    return QVariant(QString("s%1").arg(block_id));
            }
            else if (index.column() == 1)
            {
                if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                    return QVariant("—");
                else if (scriptBlock->start_generation_ == -1)
                    return QVariant("MIN");
                else
                    return QVariant(QString("%1").arg(scriptBlock->start_generation_));
            }
            else if (index.column() == 2)
            {
                if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                    return QVariant("—");
                else if (scriptBlock->end_generation_ == SLIM_MAX_GENERATION + 1)
                    return QVariant("MAX");
                else
                    return QVariant(QString("%1").arg(scriptBlock->end_generation_));
            }
            else if (index.column() == 3)
            {
                switch (scriptBlock->type_)
                {
                    case SLiMEidosBlockType::SLiMEidosEventEarly:				return QVariant("early()");
                    case SLiMEidosBlockType::SLiMEidosEventLate:				return QVariant("late()");
                    case SLiMEidosBlockType::SLiMEidosInitializeCallback:		return QVariant("initialize()");
                    case SLiMEidosBlockType::SLiMEidosFitnessCallback:			return QVariant("fitness()");
                    case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:	return QVariant("fitness()");
                    case SLiMEidosBlockType::SLiMEidosInteractionCallback:		return QVariant("interaction()");
                    case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		return QVariant("mateChoice()");
                    case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		return QVariant("modifyChild()");
                    case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	return QVariant("recombination()");
                    case SLiMEidosBlockType::SLiMEidosMutationCallback:			return QVariant("mutation()");
                    case SLiMEidosBlockType::SLiMEidosReproductionCallback:		return QVariant("reproduction()");
                    case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:
                    {
                        EidosASTNode *function_decl_node = scriptBlock->root_node_->children_[0];
                        EidosASTNode *function_name_node = function_decl_node->children_[1];
                        QString function_name = QString::fromStdString(function_name_node->token_->token_string_);
                        
                        return function_name + "()";
                    }
                    case SLiMEidosBlockType::SLiMEidosNoBlockType:				return QVariant("");	// never hit
                }
            }
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    
    return QVariant();
}

QVariant QtSLiMEidosBlockTableModel::headerData(int section,
                                                Qt::Orientation /* orientation */,
                                                int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QVariant("ID");
        case 1: return QVariant("Start");
        case 2: return QVariant("End");
        case 3: return QVariant("Type");
        default: return QVariant("");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case 0: return QVariant("the ID for the script block");
        case 1: return QVariant("the start generation");
        case 2: return QVariant("the end generation");
        case 3: return QVariant("the script block type");
        default: return QVariant("");
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (section)
        {
        case 0: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 2: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 3: return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}
    
void QtSLiMEidosBlockTableModel::reloadTable(void)
{
    beginResetModel();
    endResetModel();
}


//
//  Drawing delegates for custom drawing in the table views
//

QtSLiMGETypeTypeTableDelegate::~QtSLiMGETypeTypeTableDelegate(void)
{
}

void QtSLiMGETypeTypeTableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == 1)
    {
        // Get the color for the genomic element type, which has been encoded as an unsigned int in a QVariant
        QVariant data = index.data();
        QRgb rgbData = static_cast<QRgb>(data.toUInt());
        QColor boxColor(rgbData);
        
        // Calculate a rect for the color swatch in the center of the item's field
        QRect itemRect = option.rect;
        int centerX = static_cast<int>(itemRect.center().x());
        int halfSide = static_cast<int>((itemRect.height() - 8) / 2);
        QRect boxRect = QRect(centerX - halfSide, itemRect.top() + 5, halfSide * 2, halfSide * 2);
        
        // Fill and frame
        painter->fillRect(boxRect, boxColor);
        QtSLiMFrameRect(boxRect, Qt::black, *painter);
    }
    else
    {
        // Let super draw
        QStyledItemDelegate::paint(painter, option, index);
    }
}

































