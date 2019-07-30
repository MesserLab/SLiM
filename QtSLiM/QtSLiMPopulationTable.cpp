#include "QtSLiMPopulationTable.h"
#include "QtSLiMWindow.h"
#include "subpopulation.h"


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
    else if (role == Qt::DecorationRole)
    {
        switch (section)
        {
        case 2: return QVariant::fromValue(QIcon(":/buttons/play_H.png"));
        case 3: return QVariant::fromValue(QIcon(":/buttons/play_H.png"));
        case 4: return QVariant::fromValue(QIcon(":/buttons/play_H.png"));
        case 5: return QVariant::fromValue(QIcon(":/buttons/play_H.png"));
        }
    }
    return QVariant();
}

void QtSLiMPopulationTableModel::reloadTable(void)
{
    beginResetModel();
    endResetModel();
}



































