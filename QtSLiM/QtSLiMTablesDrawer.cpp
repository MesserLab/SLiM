//
//  QtSLiMTablesDrawer.cpp
//  SLiM
//
//  Created by Ben Haller on 2/22/2020.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
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
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <QWindow>
#include <QDebug>

#include <algorithm>
#include <utility>
#include <string>
#include <vector>
#include <map>

#include "QtSLiMWindow.h"
#include "QtSLiMExtras.h"
#include "QtSLiMAppDelegate.h"

#include "mutation_type.h"
#include "interaction_type.h"
#include "eidos_rng.h"

#include <ctime>


// a helper function for making the tooltip images in the mutation and interaction type tables
// this corresponds to SLiMgui's -[SLiMFunctionGraphToolTipView drawRect:]
static QImage imageForMutationOrInteractionType(MutationType *mut_type, InteractionType *interaction_type)
{
    QImage image = QImage(154, 100, QImage::Format_ARGB32);     // double resolution, for high-resolution displays
    QPainter painter(&image);
    
    painter.scale(2.0, 2.0);    // compensate for high resolution
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect bounds(0, 0, 77, 50);
    
    // Flip coordinate system to match that in SLiMgui, for easy porting
    painter.translate(0, 50);
    painter.scale(1.0, -1.0);
    
    // Frame and fill our tooltip rect
    painter.fillRect(bounds, QtSLiMColorWithWhite(0.95, 1.0));
    //QtSLiMFrameRect(bounds, QtSLiMColorWithWhite(0.75, 1.0), painter);  // not used, since Qt gives our tooltip a frame
    
    // Plan our plotting
	if ((!mut_type && !interaction_type) || (mut_type && interaction_type))
		return image;
	
	size_t sample_size;
	std::vector<double> draws;
	bool draw_positive = false, draw_negative = false;
	bool heights_negative = false;
	double axis_min, axis_max;
	bool draw_axis_midpoint = true, custom_axis_max = false;
	
	if (mut_type)
	{
		// Generate draws for a mutation type; this case is stochastic, based upon a large number of DFE samples.
		// Draw all the values we will plot; we need our own private RNG so we don't screw up the simulation's.
		// Drawing selection coefficients could raise, if they are type "s" and there is an error in the script,
		// so we run the sampling inside a try/catch block; if we get a raise, we just show a "?" in the plot.
		static bool rng_initialized = false;
		static Eidos_RNG_State local_rng;
		
		sample_size = (mut_type->dfe_type_ == DFEType::kScript) ? 100000 : 1000000;	// large enough to make curves pretty smooth, small enough to be reasonably fast
		draws.reserve(sample_size);
		
		if (!rng_initialized)
		{
			_Eidos_InitializeOneRNG(local_rng);
			rng_initialized = true;
		}
		
		_Eidos_SetOneRNGSeed(local_rng, 10);		// arbitrary seed, but the same seed every time
		
		std::swap(local_rng, gEidos_RNG_SINGLE);	// swap in our local RNG for DrawSelectionCoefficient()
		
		//std::clock_t start = std::clock();
		
		try
		{
			for (size_t sample_count = 0; sample_count < sample_size; ++sample_count)
			{
				double draw = mut_type->DrawSelectionCoefficient();
				
				draws.emplace_back(draw);
				
				if (draw < 0.0)			draw_negative = true;
				else if (draw > 0.0)	draw_positive = true;
			}
		}
		catch (...)
		{
			draws.clear();
			draw_negative = true;
			draw_positive = true;
		}
		
		//NSLog(@"Draws took %f seconds", (std::clock() - start) / (double)CLOCKS_PER_SEC);
		
		std::swap(local_rng, gEidos_RNG_SINGLE);	// swap out our local RNG; restore the standard RNG
		
		// figure out axis limits
		if (draw_negative && !draw_positive)
		{
			axis_min = -1.0;
			axis_max = 0.0;
		}
		else if (draw_positive && !draw_negative)
		{
			axis_min = 0.0;
			axis_max = 1.0;
		}
		else
		{
			axis_min = -1.0;
			axis_max = 1.0;
		}
	}
	else // if (interaction_type)
	{
		// Since interaction types are deterministic, we don't need draws; we will just calculate our
		// bin heights directly below.
		sample_size = 0;
		draw_negative = false;
		draw_positive = true;
		axis_min = 0.0;
		if ((interaction_type->max_distance_ < 1.0) || std::isinf(interaction_type->max_distance_))
		{
			axis_max = 1.0;
		}
		else
		{
			axis_max = interaction_type->max_distance_;
			draw_axis_midpoint = false;
			custom_axis_max = true;
		}
		heights_negative = (interaction_type->if_param1_ < 0.0);	// this is a negative-strength interaction, if T
	}
	
	// Draw the graph axes and ticks
    QRect graphRect(bounds.x() + 6, bounds.y() + (heights_negative ? 5 : 14), bounds.width() - 12, bounds.height() - 20);
	int axis_y = (heights_negative ? graphRect.y() + graphRect.height() - 1 : graphRect.y());
	int tickoff3 = (heights_negative ? 1 : -3);
	int tickoff1 = (heights_negative ? 1 : -1);
	QColor axisColor = QtSLiMColorWithWhite(0.2, 1.0);
    
    painter.fillRect(QRect(graphRect.x(), axis_y, graphRect.width(), 1), axisColor);
	
	painter.fillRect(QRect(graphRect.x(), axis_y + tickoff3, 1, 3), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.125), axis_y + tickoff1, 1, 1), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.25), axis_y + tickoff1, 1, 1), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.375), axis_y + tickoff1, 1, 1), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.5), axis_y + tickoff3, 1, 3), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.625), axis_y + tickoff1, 1, 1), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.75), axis_y + tickoff1, 1, 1), axisColor);
	painter.fillRect(QRect(graphRect.x() + qRound((graphRect.width() - 1) * 0.875), axis_y + tickoff1, 1, 1), axisColor);
	painter.fillRect(QRect(graphRect.x() + graphRect.width() - 1, axis_y + tickoff3, 1, 3), axisColor);
    
    // Draw the axis labels
#ifdef __linux__
    painter.setFont(QFont("Times New Roman", 14));  // 7, but double scale
#else
    painter.setFont(QFont("Times New Roman", 18));  // 9, but double scale
#endif
    
    std::ostringstream ss;
	ss << axis_max;
	std::string ss_str = ss.str();
	QString axis_max_pretty_string = QString::fromStdString(ss_str);
	QString axis_min_label = (axis_min == 0.0 ? "0" : "−1");
	QString axis_half_label = (axis_min == 0.0 ? "0.5" : (axis_max == 0.0 ? "−0.5" : "0"));
	QString axis_max_label = (custom_axis_max ? axis_max_pretty_string : (axis_max == 0.0 ? "0" : "1"));
    double min_label_width = painter.boundingRect(QRectF(), 0, axis_min_label).width() / 2.0;       // /2.0 to compensate for scaled font size
    double half_label_width = painter.boundingRect(QRectF(), 0, axis_half_label).width() / 2.0;
    double max_label_width = painter.boundingRect(QRectF(), 0, axis_max_label).width() / 2.0;
    double min_label_halfwidth = min_label_width / 2.0;
    double half_label_halfwidth = half_label_width / 2.0;
    double max_label_halfwidth = max_label_width / 2.0;
    double label_y = (heights_negative ? bounds.y() + bounds.height() - 11 : bounds.y() + 2);
    QPointF min_label_point = painter.transform().map(QPointF(bounds.x() + 7 - min_label_halfwidth, label_y));
    QPointF half_label_point = painter.transform().map(QPointF(bounds.x() + 39 - half_label_halfwidth, label_y));
    QPointF max_label_point = painter.transform().map(QPointF(custom_axis_max ? bounds.x() + 72 - max_label_width : bounds.x() + 71 - max_label_halfwidth, label_y));
    
    painter.setWorldMatrixEnabled(false);
    painter.drawText(min_label_point, axis_min_label);
    if (draw_axis_midpoint)
        painter.drawText(half_label_point, axis_half_label);
    painter.drawText(max_label_point, axis_max_label);
    painter.setWorldMatrixEnabled(true);
    
    // If we had an exception while drawing values, just show a question mark and return
	if (mut_type && !draws.size())
	{
#ifdef __linux__
        painter.setFont(QFont("Times New Roman", 28));  // 14, but double scale
#else
        painter.setFont(QFont("Times New Roman", 36));  // 18, but double scale
#endif
        
        QString labelText("?");
        int labelWidth = painter.boundingRect(QRect(), 0, labelText).width();
        double labelX = bounds.x() + qRound((bounds.width() - labelWidth / 2.0) / 2.0); // inner /2.0 compensates for the double-scaled font size, which QPainter does not do, oddly
        double labelY = bounds.y() + 22;
        QPointF labelPoint = painter.transform().map(QPointF(labelX, labelY));
        
        painter.setWorldMatrixEnabled(false);
        painter.drawText(labelPoint, labelText);
        painter.setWorldMatrixEnabled(true);
	}
    
    QRect interiorRect(graphRect.x(), graphRect.y() + (heights_negative ? 0 : 2), graphRect.width(), graphRect.height() - 2);
	
	// Tabulate the distribution from the samples we took; the math here is a bit subtle, because when we are doing a -1 to +1 axis
	// we want those values to fall at bin centers, but when we're doing 0 to +1 or -1 to 0 we want 0 to fall at the bin edge.
	int half_bin_count = interiorRect.width();
	int bin_count = half_bin_count * 2;								// 2x bins to look nice on Retina displays
	double *bins = static_cast<double *>(calloc(static_cast<size_t>(bin_count), sizeof(double)));
	
	if (sample_size)
	{
		// sample-based tabulation into a histogram; mutation types only, right now
		for (size_t sample_count = 0; sample_count < sample_size; ++sample_count)
		{
			double sel_coeff = draws[sample_count];
			int bin_index;
			
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
			if ((axis_min == -1.0) && (axis_max == 1.0))
				bin_index = static_cast<int>(floor(((sel_coeff + 1.0) / 2.0) * (bin_count - 1) + 0.5));
			else if ((axis_min == -1.0) && (axis_max == 0.0))
				bin_index = static_cast<int>(ceil((sel_coeff + 1.0) * (bin_count - 1 - 0.5) + 0.5));		// 0.0 maps to bin_count - 1, -1.0 maps to the center of bin 0
			else // if ((axis_min == 0.0) && (axis_max == 1.0))
				bin_index = static_cast<int>(floor(sel_coeff * (bin_count - 1 + 0.5)));					// 0.0 maps to 0, 1.0 maps to the center of bin_count - 1
#pragma clang diagnostic pop
#pragma GCC diagnostic pop
			
			if ((bin_index >= 0) && (bin_index < bin_count))
				bins[bin_index]++;
		}
	}
	else
	{
		// non-sample-based construction of a function by evaluation; interaction types only, right now
		double max_x = interaction_type->max_distance_;
		
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
		{
			double bin_left = (bin_index / static_cast<double>(bin_count)) * axis_max;
			double bin_right = ((bin_index + 1) / static_cast<double>(bin_count)) * axis_max;
			double total_value = 0.0;
			
			for (int evaluate_index = 0; evaluate_index <= 999; ++evaluate_index)
			{
				double evaluate_x = bin_left + (bin_right - bin_left) / 999;
				
				if (evaluate_x < max_x)
					total_value += interaction_type->CalculateStrengthNoCallbacks(evaluate_x);
			}
			
			bins[bin_index] = total_value / 1000.0;
		}
	}
	
    // If we only have samples equal to zero, replicate the center column for symmetry
	if (!draw_positive && !draw_negative)
	{
		double zero_count = std::max(bins[half_bin_count - 1], bins[half_bin_count]);	// whichever way it rounds...
		
		bins[half_bin_count - 1] = zero_count;
		bins[half_bin_count] = zero_count;
	}
	
	// Find the maximum-magnitude bin count
	double max_bin = 0;
	
	if (heights_negative)
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
			max_bin = std::min(max_bin, bins[bin_index]);
	}
	else
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
			max_bin = std::max(max_bin, bins[bin_index]);
	}
	
    // Plot the bins
    QColor plotColor = Qt::black;
	
	if (heights_negative)
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
		{
			if (bins[bin_index] < 0)
			{
				double height = interiorRect.height() * (bins[bin_index] / max_bin);
				
                painter.fillRect(QRectF(interiorRect.x() + bin_index * 0.5, interiorRect.y() + interiorRect.height() - height, 0.5, height), plotColor);
			}
		}
	}
	else
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
		{
			if (bins[bin_index] > 0)
                painter.fillRect(QRectF(interiorRect.x() + bin_index * 0.5, interiorRect.y(), 0.5, interiorRect.height() * (bins[bin_index] / max_bin)), plotColor);
		}
	}
	
	free(bins);
    return image;
}
    

//
//  QtSLiMTablesDrawer
//

QtSLiMTablesDrawer::QtSLiMTablesDrawer(QtSLiMWindow *p_parent) :
    QWidget(p_parent, Qt::Window),
    parentSLiMWindow(p_parent),
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
#ifdef __linux__
    headerFont.setPointSize(8);
    cellFont.setPointSize(8);
#else
    headerFont.setPointSize(11);
    cellFont.setPointSize(11);
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
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
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
        
        mutTypeTableHHeader->resizeSection(0, 53);
        mutTypeTableHHeader->resizeSection(1, 43);
        mutTypeTableHHeader->resizeSection(2, 53);
        //mutTypeTableHHeader->resizeSection(3, ?);
        mutTypeTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        mutTypeTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        mutTypeTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
        mutTypeTableHHeader->setSectionResizeMode(3, QHeaderView::Stretch);
        
        // pre-configure for our image tooltips with an off-white background
        ui->mutationTypeTable->setStyleSheet("QToolTip{border: 0px; padding: 0px; margin-top: 1px; background-color: '#F2F2F2'; opacity: 255;}");
    }
    {
        QHeaderView *geTypeTableHHeader = configureTableView(ui->genomicElementTypeTable);
        
        geTypeTableHHeader->resizeSection(0, 53);
        geTypeTableHHeader->resizeSection(1, 43);
        //geTypeTableHHeader->resizeSection(2, ?);
        geTypeTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        geTypeTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        geTypeTableHHeader->setSectionResizeMode(2, QHeaderView::Stretch);
        
        QAbstractItemDelegate *tableDelegate = new QtSLiMGETypeTypeTableDelegate(ui->genomicElementTypeTable);
        ui->genomicElementTypeTable->setItemDelegate(tableDelegate);
    }
    {
        QHeaderView *interactionTypeTableHHeader = configureTableView(ui->interactionTypeTable);
        
        interactionTypeTableHHeader->resizeSection(0, 53);
        interactionTypeTableHHeader->resizeSection(1, 43);
        interactionTypeTableHHeader->resizeSection(2, 53);
        //interactionTypeTableHHeader->resizeSection(3, ?);
        interactionTypeTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        interactionTypeTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        interactionTypeTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
        interactionTypeTableHHeader->setSectionResizeMode(3, QHeaderView::Stretch);
        
        // pre-configure for our image tooltips with an off-white background
        ui->interactionTypeTable->setStyleSheet("QToolTip{border: 0px; padding: 0px; margin-top: 1px; background-color: '#F2F2F2'; opacity: 255;}");
    }
    {
        QHeaderView *eidosBlockTableHHeader = configureTableView(ui->eidosBlockTable);
        
        eidosBlockTableHHeader->resizeSection(0, 53);
        eidosBlockTableHHeader->resizeSection(1, 63);
        eidosBlockTableHHeader->resizeSection(2, 63);
        //eidosBlockTableHHeader->resizeSection(3, ?);
        eidosBlockTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
        eidosBlockTableHHeader->setSectionResizeMode(1, QHeaderView::Fixed);
        eidosBlockTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
        eidosBlockTableHHeader->setSectionResizeMode(3, QHeaderView::Stretch);
    }
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

void QtSLiMTablesDrawer::closeEvent(QCloseEvent *p_event)
{
    // send our close signal
    emit willClose();
    
    // use super's default behavior
    QWidget::closeEvent(p_event);
}


//
//  Define models for the four table views
//

QtSLiMMutTypeTableModel::QtSLiMMutTypeTableModel(QObject *p_parent) : QAbstractTableModel(p_parent)
{
    // p_parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(p_parent) == nullptr)
        throw p_parent;
}

QtSLiMMutTypeTableModel::~QtSLiMMutTypeTableModel()
{
}

int QtSLiMMutTypeTableModel::rowCount(const QModelIndex & /* p_parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    if (community)
        return static_cast<int>(community->AllMutationTypes().size());
    
    return 0;
}

int QtSLiMMutTypeTableModel::columnCount(const QModelIndex & /* p_parent */) const
{
    return 4;
}

QVariant QtSLiMMutTypeTableModel::data(const QModelIndex &p_index, int role) const
{
    if (!p_index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    if (!community)
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        const std::map<slim_objectid_t,MutationType*> &mutationTypes = community->AllMutationTypes();
        int mutationTypeCount = static_cast<int>(mutationTypes.size());
        
        if (p_index.row() < mutationTypeCount)
        {
            auto mutTypeIter = mutationTypes.begin();
            
            std::advance(mutTypeIter, p_index.row());
            slim_objectid_t mutTypeID = mutTypeIter->first;
            MutationType *mutationType = mutTypeIter->second;
            
            if (p_index.column() == 0)
            {
                QString idString = QString("m%1").arg(mutTypeID);
                
                if (community->all_species_.size() > 1)
                    idString.append(" ").append(QString::fromStdString(mutationType->species_.avatar_));
                
                return QVariant(idString);
            }
            else if (p_index.column() == 1)
            {
                return QVariant(QString("%1").arg(static_cast<double>(mutationType->dominance_coeff_), 0, 'f', 3));
            }
            else if (p_index.column() == 2)
            {
                switch (mutationType->dfe_type_)
                {
                    case DFEType::kFixed:			return QVariant(QString("fixed"));
                    case DFEType::kGamma:			return QVariant(QString("gamma"));
                    case DFEType::kExponential:		return QVariant(QString("exp"));
                    case DFEType::kNormal:			return QVariant(QString("normal"));
                    case DFEType::kWeibull:			return QVariant(QString("Weibull"));
                    case DFEType::kLaplace:			return QVariant(QString("Laplace"));
                    case DFEType::kScript:			return QVariant(QString("script"));
                }
            }
            else if (p_index.column() == 3)
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
                            case DFEType::kLaplace:			paramSymbol = (paramIndex == 0 ? "s̄" : "b"); break;
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
    else if (role == Qt::ToolTipRole)
    {
        const std::map<slim_objectid_t,MutationType*> &mutationTypes = community->AllMutationTypes();
        int mutationTypeCount = static_cast<int>(mutationTypes.size());
        
        if (p_index.row() < mutationTypeCount)
        {
            auto mutTypeIter = mutationTypes.begin();
            std::advance(mutTypeIter, p_index.row());
            
            // Display an image in the tooltip; thanks to https://stackoverflow.com/a/34300771/2752221
            QImage image = imageForMutationOrInteractionType(mutTypeIter->second, nullptr);
            
            // the image is high-dpi; smoothly downscale the image to standard DPI if we're not on a high-DPI screen
            QWidget *tablesWindow = controller->TablesDrawerController();
            QWindow *tablesWindowHandle = tablesWindow ? tablesWindow->windowHandle() : nullptr;
            double dpi = tablesWindowHandle ? tablesWindowHandle->devicePixelRatio() : 1.0;
            
            if (dpi < 1.5)
                image = image.scaled(QSize(77, 50), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            
            QByteArray image_data;
            QBuffer buffer(&image_data);
            image.save(&buffer, "PNG", 100);
            return QString("<img width=77 height=50 src='data:image/png;base64, %0'>").arg(QString(image_data.toBase64()));
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (p_index.column())
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


QtSLiMGETypeTypeTableModel::QtSLiMGETypeTypeTableModel(QObject *p_parent) : QAbstractTableModel(p_parent)
{
    // p_parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(p_parent) == nullptr)
        throw p_parent;
}

QtSLiMGETypeTypeTableModel::~QtSLiMGETypeTypeTableModel()
{
}

int QtSLiMGETypeTypeTableModel::rowCount(const QModelIndex & /* p_parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    if (community)
        return static_cast<int>(community->AllGenomicElementTypes().size());
    
    return 0;
}

int QtSLiMGETypeTypeTableModel::columnCount(const QModelIndex & /* p_parent */) const
{
    return 3;
}

QVariant QtSLiMGETypeTypeTableModel::data(const QModelIndex &p_index, int role) const
{
    if (!p_index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    if (!community)
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        const std::map<slim_objectid_t,GenomicElementType*> &genomicElementTypes = community->AllGenomicElementTypes();
        int genomicElementTypeCount = static_cast<int>(genomicElementTypes.size());
        
        if (p_index.row() < genomicElementTypeCount)
        {
            auto genomicElementTypeIter = genomicElementTypes.begin();
            
            std::advance(genomicElementTypeIter, p_index.row());
            slim_objectid_t genomicElementTypeID = genomicElementTypeIter->first;
            GenomicElementType *genomicElementType = genomicElementTypeIter->second;
            
            if (p_index.column() == 0)
            {
                QString idString = QString("g%1").arg(genomicElementTypeID);
                
                if (community->all_species_.size() > 1)
                    idString.append(" ").append(QString::fromStdString(genomicElementType->species_.avatar_));
                
                return QVariant(idString);
            }
            else if (p_index.column() == 1)
            {
                float red, green, blue, alpha;
                
                controller->colorForGenomicElementType(genomicElementType, genomicElementTypeID, &red, &green, &blue, &alpha);
                
                QColor geTypeColor = QColor::fromRgbF(static_cast<qreal>(red), static_cast<qreal>(green), static_cast<qreal>(blue), static_cast<qreal>(alpha));
                QRgb geTypeRGB = geTypeColor.rgb();
                
                return QVariant(geTypeRGB); // return the color as an unsigned int
            }
            else if (p_index.column() == 2)
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
        switch (p_index.column())
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
        case 1: return QVariant("the color used in SLiMgui");
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


QtSLiMInteractionTypeTableModel::QtSLiMInteractionTypeTableModel(QObject *p_parent) : QAbstractTableModel(p_parent)
{
    // p_parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(p_parent) == nullptr)
        throw p_parent;
}

QtSLiMInteractionTypeTableModel::~QtSLiMInteractionTypeTableModel()
{
}

int QtSLiMInteractionTypeTableModel::rowCount(const QModelIndex & /* p_parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    if (community)
        return static_cast<int>(community->AllInteractionTypes().size());
    
    return 0;
}

int QtSLiMInteractionTypeTableModel::columnCount(const QModelIndex & /* p_parent */) const
{
    return 4;
}

QVariant QtSLiMInteractionTypeTableModel::data(const QModelIndex &p_index, int role) const
{
    if (!p_index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    Community *community = controller->community;
    
    if (!community)
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        const std::map<slim_objectid_t,InteractionType*> &interactionTypes = community->AllInteractionTypes();
        int interactionTypeCount = static_cast<int>(interactionTypes.size());
        
        if (p_index.row() < interactionTypeCount)
        {
            auto interactionTypeIter = interactionTypes.begin();
            
            std::advance(interactionTypeIter, p_index.row());
            slim_objectid_t interactionTypeID = interactionTypeIter->first;
            InteractionType *interactionType = interactionTypeIter->second;
            
            if (p_index.column() == 0)
            {
                QString idString = QString("i%1").arg(interactionTypeID);
                
                return QVariant(idString);
            }
            else if (p_index.column() == 1)
            {
                return QVariant(QString("%1").arg(interactionType->max_distance_, 0, 'f', 3));
            }
            else if (p_index.column() == 2)
            {
                switch (interactionType->if_type_)
                {
                    case SpatialKernelType::kFixed:				return QVariant(QString("fixed"));
                    case SpatialKernelType::kLinear:			return QVariant(QString("linear"));
                    case SpatialKernelType::kExponential:		return QVariant(QString("exp"));
                    case SpatialKernelType::kNormal:			return QVariant(QString("normal"));
                    case SpatialKernelType::kCauchy:			return QVariant(QString("Cauchy"));
                    case SpatialKernelType::kStudentsT:			return QVariant(QString("Student's t"));
                }
            }
            else if (p_index.column() == 3)
            {
                QString paramString;
                
                // the first parameter is always the maximum interaction strength
                paramString += QString("f=%1").arg(interactionType->if_param1_, 0, 'f', 3);
                
                // append second parameters where applicable
                switch (interactionType->if_type_)
                {
                    case SpatialKernelType::kFixed:
                    case SpatialKernelType::kLinear:
                        break;
                    case SpatialKernelType::kExponential:
                        paramString += QString(", β=%1").arg(interactionType->if_param2_, 0, 'f', 3);
                        break;
                    case SpatialKernelType::kNormal:
                        paramString += QString(", σ=%1").arg(interactionType->if_param2_, 0, 'f', 3);
                        break;
                    case SpatialKernelType::kCauchy:
                        paramString += QString(", γ=%1").arg(interactionType->if_param2_, 0, 'f', 3);
                        break;
                    case SpatialKernelType::kStudentsT:
                        paramString += QString(", ν=%1, σ=%2").arg(interactionType->if_param2_, 0, 'f', 3).arg(interactionType->if_param3_, 0, 'f', 3);
                        break;
                }
                
                return QVariant(paramString);
            }
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        const std::map<slim_objectid_t,InteractionType*> &interactionTypes = community->AllInteractionTypes();
        int interactionTypeCount = static_cast<int>(interactionTypes.size());
        
        if (p_index.row() < interactionTypeCount)
        {
            auto interactionTypeIter = interactionTypes.begin();
            std::advance(interactionTypeIter, p_index.row());
            
            // Display an image in the tooltip; thanks to https://stackoverflow.com/a/34300771/2752221
            QImage image = imageForMutationOrInteractionType(nullptr, interactionTypeIter->second);
            
            // the image is high-dpi; smoothly downscale the image to standard DPI if we're not on a high-DPI screen
            QWidget *tablesWindow = controller->TablesDrawerController();
            QWindow *tablesWindowHandle = tablesWindow ? tablesWindow->windowHandle() : nullptr;
            double dpi = tablesWindowHandle ? tablesWindowHandle->devicePixelRatio() : 1.0;
            
            if (dpi < 1.5)
                image = image.scaled(QSize(77, 50), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            
            QByteArray image_data;
            QBuffer buffer(&image_data);
            image.save(&buffer, "PNG", 100);
            return QString("<img width=77 height=50 src='data:image/png;base64, %0'>").arg(QString(image_data.toBase64()));
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (p_index.column())
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


QtSLiMEidosBlockTableModel::QtSLiMEidosBlockTableModel(QObject *p_parent) : QAbstractTableModel(p_parent)
{
    // p_parent must be a pointer to QtSLiMWindow, which holds our model information
    if (dynamic_cast<QtSLiMWindow *>(p_parent) == nullptr)
        throw p_parent;
}

QtSLiMEidosBlockTableModel::~QtSLiMEidosBlockTableModel()
{
}

int QtSLiMEidosBlockTableModel::rowCount(const QModelIndex & /* p_parent */) const
{
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (controller && !controller->invalidSimulation())
        return static_cast<int>(controller->community->AllScriptBlocks().size());
    
    return 0;
}

int QtSLiMEidosBlockTableModel::columnCount(const QModelIndex & /* p_parent */) const
{
    return 4;
}

QVariant QtSLiMEidosBlockTableModel::data(const QModelIndex &p_index, int role) const
{
    if (!p_index.isValid())
        return QVariant();
    
    QtSLiMWindow *controller = static_cast<QtSLiMWindow *>(parent());
    
    if (!controller || controller->invalidSimulation())
        return QVariant();
    
    if (role == Qt::DisplayRole)
    {
        Community *community = controller->community;
        std::vector<SLiMEidosBlock*> &scriptBlocks = community->AllScriptBlocks();
        int scriptBlockCount = static_cast<int>(scriptBlocks.size());
        
        if (p_index.row() < scriptBlockCount)
        {
            SLiMEidosBlock *scriptBlock = scriptBlocks[static_cast<size_t>(p_index.row())];
            
            if (p_index.column() == 0)
            {
                slim_objectid_t block_id = scriptBlock->block_id_;
                QString idString;
                
                if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                    idString = "—";
                else if (block_id == -1)
                    idString = "—";
                else
                    idString = QString("s%1").arg(block_id);
                
                if ((community->all_species_.size() > 1) && scriptBlock->species_spec_)
                    idString.append(" ").append(QString::fromStdString(scriptBlock->species_spec_->avatar_));
                else if ((community->all_species_.size() > 1) && scriptBlock->ticks_spec_)
                    idString.append(" ").append(QString::fromStdString(scriptBlock->ticks_spec_->avatar_));
                
                return QVariant(idString);
            }
            else if (p_index.column() == 1)
            {
                if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                    return QVariant("—");
                else if (scriptBlock->start_tick_ == -1)
                    return QVariant("MIN");
                else
                    return QVariant(QString("%1").arg(scriptBlock->start_tick_));
            }
            else if (p_index.column() == 2)
            {
                if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                    return QVariant("—");
                else if (scriptBlock->end_tick_ == SLIM_MAX_TICK + 1)
                    return QVariant("MAX");
                else
                    return QVariant(QString("%1").arg(scriptBlock->end_tick_));
            }
            else if (p_index.column() == 3)
            {
                switch (scriptBlock->type_)
                {
                    case SLiMEidosBlockType::SLiMEidosEventFirst:				return QVariant("first()");
                    case SLiMEidosBlockType::SLiMEidosEventEarly:				return QVariant("early()");
                    case SLiMEidosBlockType::SLiMEidosEventLate:				return QVariant("late()");
                    case SLiMEidosBlockType::SLiMEidosInitializeCallback:		return QVariant("initialize()");
                    case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	return QVariant("mutationEffect()");
                    case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	return QVariant("fitnessEffect()");
                    case SLiMEidosBlockType::SLiMEidosInteractionCallback:		return QVariant("interaction()");
                    case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		return QVariant("mateChoice()");
                    case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		return QVariant("modifyChild()");
                    case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	return QVariant("recombination()");
                    case SLiMEidosBlockType::SLiMEidosMutationCallback:			return QVariant("mutation()");
                    case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			return QVariant("survival()");
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
    else if (role == Qt::ToolTipRole)
    {
        Community *community = controller->community;
        std::vector<SLiMEidosBlock*> &scriptBlocks = community->AllScriptBlocks();
        int scriptBlockCount = static_cast<int>(scriptBlocks.size());
        
        if (p_index.row() < scriptBlockCount)
        {
            SLiMEidosBlock *scriptBlock = scriptBlocks[static_cast<size_t>(p_index.row())];
            const char *script_string = scriptBlock->compound_statement_node_->token_->token_string_.c_str();
            QString q_script_string = QString::fromStdString(script_string);
            
            q_script_string.replace('\t', "   ");
            
            return q_script_string;
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (p_index.column())
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
        case 1: return QVariant("the start tick");
        case 2: return QVariant("the end tick");
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

































