//
//  QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime.cpp
//  SLiM
//
//  Created by Ben Haller on 7/26/2026.
//  Copyright (c) 2026 Benjamin C. Haller.  All rights reserved.
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

#include "QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime.h"

#include <QAction>
#include <QMenu>
#include <QPixmap>
#include <QPainterPath>
#include <QDebug>

#include <vector>
#include <algorithm>

#include "QtSLiMWindow.h"


QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    // super assumes that we are species-specific; we tell it we are not
    setFocalDisplaySpecies(nullptr);
    
    //setXAxisRangeFromTick();	// the end tick is not yet known
    setDefaultYAxisRange();
    
    xAxisLabel_ = "Tick";
    yAxisLabel_ = "Phenotype";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    showSubpopulations_ = true;
    drawLines_ = true;
    
    QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::updateAfterTick();
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::setDefaultYAxisRange(void)
{
    original_y0_ = -1.0;
    original_y1_ = 1.0;		// dynamic
    
    y0_ = original_y0_;
    y1_ = original_y1_;
    
    yAxisMin_ = y0_;
	yAxisMax_ = y1_;
	yAxisMajorTickInterval_ = 10;
	yAxisMinorTickInterval_ = 1;
	yAxisMajorTickModulus_ = 5;
	yAxisTickValuePrecision_ = 0;
}

QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::~QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime()
{
    // We are responsible for our own destruction
    QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::invalidateDrawingCache();
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::invalidateDrawingCache(void)
{
    delete drawingCache_;
	drawingCache_ = nullptr;
	drawingCacheTick_ = 0;
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
	{
		if (!yAxisIsUserRescaled_)
			setDefaultYAxisRange();
		//if (!xAxisIsUserRescaled_)
		//	setXAxisRangeFromTick();	// the end tick is not yet known
		
		update();
	}
	
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::graphTitle(void)
{
    return "Multispecies Multitrait Phenotype ~ Time";
}

QString QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::aboutString(void)
{
    return "The Multispecies Multitrait Phenotype ~ Time graph shows mean phenotypes for all traits, "
           "for every species (and subpopulation), as a function of time.  The species-"
           "level mean phenotypes are shown with thick bright lines, while those for "
           "subpopulations are shown with thinner pastel lines.";
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::updateAfterTick(void)
{
    // BCH 3/20/2024: We set the x axis range each tick, because the end tick is now invalid until after initialize() callbacks
    if (!controller_->invalidSimulation() && !xAxisIsUserRescaled_)
        setXAxisRangeFromTick();
    
    if (!controller_->invalidSimulation() && !yAxisIsUserRescaled_)
    {
        Community *community = controller_->community;
        double ymin = std::numeric_limits<double>::infinity();
        double ymax = -std::numeric_limits<double>::infinity();
        
        for (Species *species : community->all_species_)
        {
            Population &pop = species->population_;
            
            if (pop.subpop_trait_histories_.size() == 0)
                continue;
            
            for (slim_trait_index_t trait_index = 0; trait_index < species->TraitCount(); ++trait_index)
            {
                std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
                bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
                
                for (const auto &history_record_iter : one_trait_histories)
                {
                    if (showSubpops || (history_record_iter.first == -1))
                    {
						const SubpopTraitHistory &history_record = history_record_iter.second;
						const double *history = history_record.history_;
                        slim_tick_t historyLength = history_record.history_length_;
                        
                        // find the min and max history value
                        for (int i = 0; i < historyLength; ++i)
                        {
                            double history_value = history[i];
                            
                            ymin = std::min(ymin, history_value);
                            ymax = std::max(ymax, history_value);
                        }
                    }
                }
            }
        }
		
		// set axis range to encompass the data
        if (std::isfinite(ymin) && std::isfinite(ymax))
        {
            original_y0_ = ymin;
            original_y1_ = ymax;
            
            y0_ = original_y0_;
            y1_ = original_y1_;
            
            configureAxisForRange(y0_, y1_, yAxisMin_, yAxisMax_, yAxisMajorTickInterval_, yAxisMinorTickInterval_,
                                  yAxisMajorTickModulus_, yAxisTickValuePrecision_);

#warning invalidates every tick, I think; might be a flaw with other cached drawing schemes too!
            std::cout << "invalidating drawing!" << std::endl;
            QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::invalidateDrawingCache();
        }
    }
	
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::drawPointGraph(QPainter &painter, QRect interiorRect)
{
    Community *community = controller_->community;
	slim_tick_t completedTicks = community->Tick() - 1;
	
	// The tick counter can get set backwards, in which case our drawing cache is invalid – it contains drawing of things in the
	// future that may no longer happen.  So we need to detect that case and invalidate our cache.
	if (!cachingNow_ && drawingCache_ && (drawingCacheTick_ > completedTicks))
	{
		//qDebug() << "backward tick change detected, invalidating drawing cache";
		invalidateDrawingCache();
	}
	
	// If we're not caching, then: if our cache is invalid OR we have crossed a 1000-tick boundary since we last cached, cache an image
	if (!cachingNow_ && (!drawingCache_ || ((completedTicks / 1000) > (drawingCacheTick_ / 1000))))
	{
        invalidateDrawingCache();
		
        //qDebug() << "making new cache at tick " << community->Tick();
		cachingNow_ = true;
        
		QPixmap *cache = new QPixmap(interiorRect.size());
        cache->fill(Qt::transparent);   // transparent so grid lines don't get overwritten by drawPixmap()
        
        QPainter cachePainter(cache);
        drawGraph(cachePainter, cache->rect());
        
        drawingCache_ = cache;
		drawingCacheTick_ = completedTicks;
		cachingNow_ = false;
	}
	
	// Now draw our cache, if we have one
	if (drawingCache_)
    {
        //qDebug() << "drawing cache:" << drawingCache_->rect() << ", drawingCacheTick_ == " << drawingCacheTick_;
        painter.drawPixmap(interiorRect, *drawingCache_, drawingCache_->rect());
    }
    
    // Count the number of distinct species/trait histories we have, for coloring purposes
    int species_history_count = 0;
    
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        
        if (pop.subpop_trait_histories_.size() == 0)
            continue;
        
        species_history_count += species->TraitCount();
    }
    
	// Draw the trait history as a scatter plot; better suited to caching of the image
    int species_history_index = 0;
    
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        
        if (pop.subpop_trait_histories_.size() == 0)
            continue;
        
        for (slim_trait_index_t trait_index = 0; trait_index < species->TraitCount(); ++trait_index)
        {
            std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
            bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
            
            // First draw subpops, then draw the population mean
            for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
            {
                QColor historyColor = controller_->qcolorForIndexInSeries(species_history_index, species_history_count);
                
                if (iter == 0)
                    historyColor.setAlphaF(0.6);
                
                for (const auto &history_record_iter : one_trait_histories)
                {
                    if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
                    {
						const SubpopTraitHistory &history_record = history_record_iter.second;
						const double *history = history_record.history_;
                        slim_tick_t historyLength = history_record.history_length_;
                        
                        // If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
                        slim_tick_t firstHistoryEntryToDraw = (cachingNow_ ? 0 : (drawingCache_ ? drawingCacheTick_ : 0));
                        
                        for (slim_tick_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedTicks); ++i)
                        {
                            double historyEntry = history[i];
                            
                            if (!std::isnan(historyEntry))
                            {
                                QPointF historyPoint(plotToDeviceX(i, interiorRect), plotToDeviceY(historyEntry, interiorRect));
                                
                                painter.fillRect(QRectF(historyPoint.x() - 0.5, historyPoint.y() - 0.5, 1.0, 1.0), historyColor);
                            }
                        }
                    }
                }
            }
            
            species_history_index++;
        }
    }
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::drawLineGraph(QPainter &painter, QRect interiorRect)
{
    Community *community = controller_->community;
    
    // Count the number of distinct species/trait histories we have, for coloring purposes
    int species_history_count = 0;
    
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        
        if (pop.subpop_trait_histories_.size() == 0)
            continue;
        
        species_history_count += species->TraitCount();
    }
    
    // Draw the trait history as a line plot, without image caching
    int species_history_index = 0;
    
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        slim_tick_t completedTicks = community->Tick() - 1;
        
        if (pop.subpop_trait_histories_.size() == 0)
            continue;
        
        for (slim_trait_index_t trait_index = 0; trait_index < species->TraitCount(); ++trait_index)
        {
            std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
            bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
            
            // First draw subpops, then draw the population mean
            for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
            {
                QColor historyColor = controller_->qcolorForIndexInSeries(species_history_index, species_history_count);
                double lineWidth = 1.5;
                
                if (iter == 0)
                {
                    historyColor.setAlphaF(0.6);
                    lineWidth = 1.0;
                }
                
                for (const auto &history_record_iter : one_trait_histories)
                {
                    if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
                    {
						const SubpopTraitHistory &history_record = history_record_iter.second;
						const double *history = history_record.history_;
                        slim_tick_t historyLength = history_record.history_length_;
                        QPainterPath linePath;
                        bool startedLine = false;
                        
                        for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
                        {
                            double historyEntry = history[i];
                            
                            if (std::isnan(historyEntry))
                            {
                                startedLine = false;
                            }
                            else
                            {
                                QPointF historyPoint(plotToDeviceX(i, interiorRect), plotToDeviceY(historyEntry, interiorRect));
                                
                                if (startedLine)    linePath.lineTo(historyPoint);
                                else                linePath.moveTo(historyPoint);
                                
                                startedLine = true;
                            }
                        }
                        
                        painter.strokePath(linePath, QPen(historyColor, lineWidth));
                    }
                }
            }
            
            species_history_index++;
        }
    }
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::drawGraph(QPainter &painter, QRect interiorRect)
{
    if (drawLines_)
		drawLineGraph(painter, interiorRect);
	else
		drawPointGraph(painter, interiorRect);
}

bool QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::appendStringForData(QString &string)
{
    Community *community = controller_->community;
	slim_tick_t completedTicks = community->Tick() - 1;
	
    // Phenotype history
    for (Species *species : community->all_species_)
    {
        QString speciesName = QString::fromStdString(species->name_);
        Population &pop = species->population_;
        
        if (pop.subpop_trait_histories_.size() == 0)
            continue;
        
        for (slim_trait_index_t trait_index = 0; trait_index < species->TraitCount(); ++trait_index)
        {
            Trait *trait = species->Traits()[trait_index];
            QString traitName = QString::fromStdString(trait->Name());
            std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
            bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
            
            string.append(QString("\n\n# Phenotype history (species %1, trait %2):\n").arg(speciesName, traitName));
            
            for (int iter = 0; iter <= (showSubpops ? 1 : 0); ++iter)
            {
                for (const auto &history_record_iter : one_trait_histories)
                {
                    if (((iter == 0) && (history_record_iter.first == -1)) || ((iter == 1) && (history_record_iter.first != -1)))
                    {
						const SubpopTraitHistory &history_record = history_record_iter.second;
						const double *history = history_record.history_;
                        slim_tick_t historyLength = history_record.history_length_;
                        
                        if (iter == 1)
                            string.append(QString("\n\n# Phenotype history (subpopulation p%1):\n").arg(history_record_iter.first));
                        
                        for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
                            string.append(QString("%1, ").arg(history[i]));
                        
                        string.append("\n");
                    }
                }
            }
        }
    }
}

QtSLiMLegendSpec QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::legendKey(void)
{
    Community *community = controller_->community;
    QtSLiMLegendSpec legend_key;
    
    // Count the number of distinct species/trait histories we have, for coloring purposes
    int species_history_count = 0;
    
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        
        if (pop.subpop_trait_histories_.size() == 0)
            continue;
        
        species_history_count += species->TraitCount();
    }
    
    if (species_history_count <= 1)
        return QtSLiMLegendSpec();
    
    // Choose colors for each distinct species/trait history
    int species_history_index = 0;
    
    for (Species *species : community->all_species_)
    {
        QString speciesName = QString::fromStdString(species->name_);
        
        for (slim_trait_index_t trait_index = 0; trait_index < species->TraitCount(); ++trait_index)
        {
            Trait *trait = species->Traits()[trait_index];
            QString traitName = QString::fromStdString(trait->Name());
            QColor historyColor = controller_->qcolorForIndexInSeries(species_history_index, species_history_count);
            QString legendName;
            
            if (community->all_species_.size() > 1)
                legendName = QString("%1 (%2)").arg(traitName, speciesName);
            else
                legendName = QString("%1").arg(traitName);
            
            legend_key.emplace_back(legendName, historyColor);
            species_history_index++;
        }
    }
    
    return legend_key;
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::toggleShowSubpopulations(void)
{
    showSubpopulations_ = !showSubpopulations_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::toggleDrawLines(void)
{
    drawLines_ = !drawLines_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction(showSubpopulations_ ? "Hide Subpopulations" : "Show Subpopulations", this, &QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::toggleShowSubpopulations);
    contextMenu.addAction(drawLines_ ? "Draw Points (Faster)" : "Draw Lines (Slower)", this, &QtSLiMGraphView_MultispeciesMultitraitPhenotypeOverTime::toggleDrawLines);
}





























