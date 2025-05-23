//
//  QtSLiMGraphView_MultispeciesPopSizeOverTime.cpp
//  SLiM
//
//  Created by Ben Haller on 4/25/2022.
//  Copyright (c) 2022-2025 Benjamin C. Haller.  All rights reserved.
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

#include "QtSLiMGraphView_MultispeciesPopSizeOverTime.h"

#include <QAction>
#include <QMenu>
#include <QPixmap>
#include <QPainterPath>
#include <QDebug>

#include <string>
#include <vector>
#include <algorithm>

#include "QtSLiMWindow.h"


QtSLiMGraphView_MultispeciesPopSizeOverTime::QtSLiMGraphView_MultispeciesPopSizeOverTime(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    // super assumes that we are species-specific; we tell it we are not
    setFocalDisplaySpecies(nullptr);
    
    //setXAxisRangeFromTick();	// the end tick is not yet known
    setDefaultYAxisRange();
    
    xAxisLabel_ = "Tick";
    yAxisLabel_ = "Number of individuals";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    showSubpopulations_ = true;
    drawLines_ = true;
    
    QtSLiMGraphView_MultispeciesPopSizeOverTime::updateAfterTick();
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::setDefaultYAxisRange(void)
{
    y0_ = 0.0;
    y1_ = 100.0;		// dynamic
    
    yAxisMin_ = y0_;
	yAxisMax_ = y1_;
	yAxisMajorTickInterval_ = 50;
	yAxisMinorTickInterval_ = 10;
	yAxisMajorTickModulus_ = 5;
	yAxisTickValuePrecision_ = 0;
}

QtSLiMGraphView_MultispeciesPopSizeOverTime::~QtSLiMGraphView_MultispeciesPopSizeOverTime()
{
    // We are responsible for our own destruction
    QtSLiMGraphView_MultispeciesPopSizeOverTime::invalidateDrawingCache();
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::invalidateDrawingCache(void)
{
    delete drawingCache_;
	drawingCache_ = nullptr;
	drawingCacheTick_ = 0;
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::controllerRecycled(void)
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

QString QtSLiMGraphView_MultispeciesPopSizeOverTime::graphTitle(void)
{
    return "Multispecies Population Size ~ Time";
}

QString QtSLiMGraphView_MultispeciesPopSizeOverTime::aboutString(void)
{
    return "The Multispecies Population Size ~ Time graph shows species (and subpopulation) "
           "size as a function of time.  The size of each species is shown with a thick "
           "bright line, while those of subpopulations are shown with thinner pastel lines.";
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::updateAfterTick(void)
{
    // BCH 3/20/2024: We set the x axis range each tick, because the end tick is now invalid until after initialize() callbacks
    if (!controller_->invalidSimulation() && !xAxisIsUserRescaled_)
        setXAxisRangeFromTick();
    
    if (!controller_->invalidSimulation() && !yAxisIsUserRescaled_)
    {
        Community *community = controller_->community;
        slim_popsize_t maxHistory = 0;
        
        for (Species *species : community->all_species_)
        {
            Population &pop = species->population_;
            bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
            
            for (auto history_record_iter : pop.subpop_size_histories_)
            {
                if (showSubpops || (history_record_iter.first == -1))
                {
                    SubpopSizeHistory &history_record = history_record_iter.second;
                    slim_popsize_t *history = history_record.history_;
                    slim_tick_t historyLength = history_record.history_length_;
                    
                    // find the min and max history value
                    for (int i = 0; i < historyLength; ++i)
                        maxHistory = std::max(maxHistory, history[i]);
                }
            }
        }
		
		// set axis range to encompass the data
        if (maxHistory > yAxisMax_)
        {
            if (maxHistory <= 1000)
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 100.0) * 100.0);
                yAxisMax_ = maxHistory;
                y1_ = yAxisMax_;               // the same as yAxisMax_, for base plots
                yAxisMajorTickInterval_ = 200;
                yAxisMinorTickInterval_ = 100;
                yAxisMajorTickModulus_ = 2;
            }
            else if (maxHistory <= 10000)
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 1000.0) * 1000.0);
                yAxisMax_ = maxHistory;
                y1_ = yAxisMax_;               // the same as yAxisMax_, for base plots
                yAxisMajorTickInterval_ = 2000;
                yAxisMinorTickInterval_ = 1000;
                yAxisMajorTickModulus_ = 2;
            }
            else if (maxHistory <= 100000)
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 10000.0) * 10000.0);
                yAxisMax_ = maxHistory;
                y1_ = yAxisMax_;               // the same as yAxisMax_, for base plots
                yAxisMajorTickInterval_ = 20000;
                yAxisMinorTickInterval_ = 10000;
                yAxisMajorTickModulus_ = 2;
            }
            else
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 100000.0) * 100000.0);
                yAxisMax_ = maxHistory;
                y1_ = yAxisMax_;               // the same as yAxisMax_, for base plots
                yAxisMajorTickInterval_ = 200000;
                yAxisMinorTickInterval_ = 100000;
                yAxisMajorTickModulus_ = 2;
            }
            
            QtSLiMGraphView_MultispeciesPopSizeOverTime::invalidateDrawingCache();
        }
    }
	
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::drawPointGraph(QPainter &painter, QRect interiorRect)
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
    
	// Draw the size history as a scatter plot; better suited to caching of the image
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
        
        // First draw subpops, then draw the population size
        for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
        {
            QColor speciesColor = controller_->qcolorForSpecies(species);
            QColor pointColor = (iter == 0) ? speciesColor.lighter(150) : speciesColor;
            
            for (auto history_record_iter : pop.subpop_size_histories_)
            {
                if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
                {
                    SubpopSizeHistory &history_record = history_record_iter.second;
                    slim_popsize_t *history = history_record.history_;
                    slim_tick_t historyLength = history_record.history_length_;
                    
                    // If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
                    slim_tick_t firstHistoryEntryToDraw = (cachingNow_ ? 0 : (drawingCache_ ? drawingCacheTick_ : 0));
                    
                    for (slim_tick_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedTicks); ++i)
                    {
                        slim_popsize_t historyEntry = history[i];
                        
                        if (historyEntry != 0)
                        {
                            QPointF historyPoint(plotToDeviceX(i, interiorRect), plotToDeviceY(historyEntry, interiorRect));
                            
                            painter.fillRect(QRectF(historyPoint.x() - 0.5, historyPoint.y() - 0.5, 1.0, 1.0), pointColor);
                        }
                    }
                }
            }
        }
    }
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::drawLineGraph(QPainter &painter, QRect interiorRect)
{
    Community *community = controller_->community;
    
    for (Species *species : community->all_species_)
    {
        Population &pop = species->population_;
        slim_tick_t completedTicks = community->Tick() - 1;
        
        // Draw the size history as a line plot
        bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
        
        // First draw subpops, then draw the population size
        for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
        {
            QColor speciesColor = controller_->qcolorForSpecies(species);
            QColor lineColor = (iter == 0) ? speciesColor.lighter(150) : speciesColor;
            double lineWidth = (iter == 0) ? 1.0 : 1.5;
            
            for (auto history_record_iter : pop.subpop_size_histories_)
            {
                if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
                {
                    SubpopSizeHistory &history_record = history_record_iter.second;
                    slim_popsize_t *history = history_record.history_;
                    slim_tick_t historyLength = history_record.history_length_;
                    QPainterPath linePath;
                    bool startedLine = false;
                    
                    for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
                    {
                        slim_popsize_t historyEntry = history[i];
                        
                        if (historyEntry == 0)
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
                    
                    painter.strokePath(linePath, QPen(lineColor, lineWidth));
                }
            }
        }
    }
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::drawGraph(QPainter &painter, QRect interiorRect)
{
    if (drawLines_)
		drawLineGraph(painter, interiorRect);
	else
		drawPointGraph(painter, interiorRect);
}

bool QtSLiMGraphView_MultispeciesPopSizeOverTime::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::appendStringForData(QString &string)
{
    Community *community = controller_->community;
	slim_tick_t completedTicks = community->Tick() - 1;
	
    // Size history
    for (Species *species : community->all_species_)
    {
        QString speciesName = QString::fromStdString(species->name_);
        Population &pop = species->population_;
        bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
        
        string.append(QString("\n\n# Size history (species %1):\n").arg(speciesName));
        
        for (int iter = 0; iter <= (showSubpops ? 1 : 0); ++iter)
        {
            for (auto history_record_iter : pop.subpop_size_histories_)
            {
                if (((iter == 0) && (history_record_iter.first == -1)) || ((iter == 1) && (history_record_iter.first != -1)))
                {
                    SubpopSizeHistory &history_record = history_record_iter.second;
                    slim_popsize_t *history = history_record.history_;
                    slim_tick_t historyLength = history_record.history_length_;
                    
                    if (iter == 1)
                        string.append(QString("\n\n# Size history (subpopulation p%1):\n").arg(history_record_iter.first));
                    
                    for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
                        string.append(QString("%1, ").arg(history[i]));
                    
                    string.append("\n");
                }
            }
        }
    }
}

QtSLiMLegendSpec QtSLiMGraphView_MultispeciesPopSizeOverTime::legendKey(void)
{
    Community *community = controller_->community;
    QtSLiMLegendSpec legend_key;
    
    for (Species *species : community->all_species_)
    {
        QString speciesName = QString::fromStdString(species->name_);
        QColor speciesColor = controller_->qcolorForSpecies(species);
        
        legend_key.emplace_back(speciesName, speciesColor);
    }
    
    return legend_key;
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::toggleShowSubpopulations(void)
{
    showSubpopulations_ = !showSubpopulations_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::toggleDrawLines(void)
{
    drawLines_ = !drawLines_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_MultispeciesPopSizeOverTime::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction(showSubpopulations_ ? "Hide Subpopulations" : "Show Subpopulations", this, &QtSLiMGraphView_MultispeciesPopSizeOverTime::toggleShowSubpopulations);
    contextMenu.addAction(drawLines_ ? "Draw Points (Faster)" : "Draw Lines (Slower)", this, &QtSLiMGraphView_MultispeciesPopSizeOverTime::toggleDrawLines);
}





























