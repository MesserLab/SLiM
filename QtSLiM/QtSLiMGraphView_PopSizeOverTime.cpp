//
//  QtSLiMGraphView_PopSizeOverTime.cpp
//  SLiM
//
//  Created by Ben Haller on 8/30/2020.
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

#include "QtSLiMGraphView_PopSizeOverTime.h"

#include <QAction>
#include <QMenu>
#include <QPixmap>
#include <QPainterPath>
#include <QDebug>

#include "QtSLiMWindow.h"
#include "subpopulation.h"


QtSLiMGraphView_PopSizeOverTime::QtSLiMGraphView_PopSizeOverTime(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    setXAxisRangeFromGeneration();
    setDefaultYAxisRange();
    
    xAxisLabel_ = "Generation";
    yAxisLabel_ = "Number of individuals";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    showSubpopulations_ = true;
    drawLines_ = true;
    
    updateAfterTick();
}

void QtSLiMGraphView_PopSizeOverTime::setDefaultYAxisRange(void)
{
    yAxisMin_ = 0.0;
	yAxisMax_ = 100.0;		// dynamic
	yAxisMajorTickInterval_ = 50;
	yAxisMinorTickInterval_ = 10;
	yAxisMajorTickModulus_ = 5;
	yAxisTickValuePrecision_ = 0;
}

QtSLiMGraphView_PopSizeOverTime::~QtSLiMGraphView_PopSizeOverTime()
{
}

void QtSLiMGraphView_PopSizeOverTime::invalidateDrawingCache(void)
{
    delete drawingCache_;
	drawingCache_ = nullptr;
	drawingCacheGeneration_ = 0;
}

void QtSLiMGraphView_PopSizeOverTime::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
	{
		if (!yAxisIsUserRescaled_)
			setDefaultYAxisRange();
		if (!xAxisIsUserRescaled_)
			setXAxisRangeFromGeneration();
		
		update();
	}
	
	QtSLiMGraphView::controllerRecycled();
}

void QtSLiMGraphView_PopSizeOverTime::controllerSelectionChanged(void)
{
    invalidateDrawingCache();
    update();
	
	QtSLiMGraphView::controllerSelectionChanged();
}

QString QtSLiMGraphView_PopSizeOverTime::graphTitle(void)
{
    return "Population Size ~ Time";
}

QString QtSLiMGraphView_PopSizeOverTime::aboutString(void)
{
    return "The Population Size ~ Time graph shows population (and subpopulation) size as a "
           "function of time.  The size of the population is shown with a thick black line, "
           "while those of subpopulations are shown with thinner colored lines.";
}

void QtSLiMGraphView_PopSizeOverTime::updateAfterTick(void)
{
	if (!controller_->invalidSimulation() && !yAxisIsUserRescaled_)
	{
		SLiMSim *sim = controller_->sim;
		Population &pop = sim->population_;
		slim_popsize_t maxHistory = 0;
		bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
		
		for (auto history_record_iter : pop.subpop_size_histories_)
		{
			if (showSubpops || (history_record_iter.first == -1))
			{
				SubpopSizeHistory &history_record = history_record_iter.second;
				slim_popsize_t *history = history_record.history_;
				slim_generation_t historyLength = history_record.history_length_;
				
				// find the min and max history value
				for (int i = 0; i < historyLength; ++i)
                    maxHistory = std::max(maxHistory, history[i]);
			}
		}
		
		// set axis range to encompass the data
        if (maxHistory > yAxisMax_)
        {
            if (maxHistory <= 1000)
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 100.0) * 100.0);
                yAxisMax_ = maxHistory;
                yAxisMajorTickInterval_ = 200;
                yAxisMinorTickInterval_ = 100;
                yAxisMajorTickModulus_ = 2;
            }
            else if (maxHistory <= 10000)
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 1000.0) * 1000.0);
                yAxisMax_ = maxHistory;
                yAxisMajorTickInterval_ = 2000;
                yAxisMinorTickInterval_ = 1000;
                yAxisMajorTickModulus_ = 2;
            }
            else if (maxHistory <= 100000)
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 10000.0) * 10000.0);
                yAxisMax_ = maxHistory;
                yAxisMajorTickInterval_ = 20000;
                yAxisMinorTickInterval_ = 10000;
                yAxisMajorTickModulus_ = 2;
            }
            else
            {
                maxHistory = (slim_popsize_t)(std::ceil(maxHistory / 100000.0) * 100000.0);
                yAxisMax_ = maxHistory;
                yAxisMajorTickInterval_ = 200000;
                yAxisMinorTickInterval_ = 100000;
                yAxisMajorTickModulus_ = 2;
            }
            
            invalidateDrawingCache();
        }
    }
	
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_PopSizeOverTime::drawPointGraph(QPainter &painter, QRect interiorRect)
{
    SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	// The generation counter can get set backwards, in which case our drawing cache is invalid – it contains drawing of things in the
	// future that may no longer happen.  So we need to detect that case and invalidate our cache.
	if (!cachingNow_ && drawingCache_ && (drawingCacheGeneration_ > completedGenerations))
	{
		//qDebug() << "backward generation change detected, invalidating drawing cache";
		invalidateDrawingCache();
	}
	
	// If we're not caching, then: if our cache is invalid OR we have crossed a 1000-generation boundary since we last cached, cache an image
	if (!cachingNow_ && (!drawingCache_ || ((completedGenerations / 1000) > (drawingCacheGeneration_ / 1000))))
	{
        invalidateDrawingCache();
		
        //qDebug() << "making new cache at generation " << sim->generation_;
		cachingNow_ = true;
        
		QPixmap *cache = new QPixmap(interiorRect.size());
        cache->fill(Qt::transparent);   // transparent so grid lines don't get overwritten by drawPixmap()
        
        QPainter cachePainter(cache);
        drawGraph(cachePainter, cache->rect());
        
        drawingCache_ = cache;
		drawingCacheGeneration_ = completedGenerations;
		cachingNow_ = false;
	}
	
	// Now draw our cache, if we have one
	if (drawingCache_)
    {
        //qDebug() << "drawing cache:" << drawingCache_->rect() << ", drawingCacheGeneration_ == " << drawingCacheGeneration_;
        painter.drawPixmap(interiorRect, *drawingCache_, drawingCache_->rect());
    }
    
	// Draw the size history as a scatter plot; better suited to caching of the image
    bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
	bool drawSubpopsGray = (showSubpops && (pop.subpop_size_histories_.size() > 8));	// 7 subpops + pop
	
	// First draw subpops, then draw the population size
    for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
    {
        QColor pointColor = ((iter == 0) ? QtSLiMColorWithWhite(0.5, 1.0) : Qt::black);
        
        for (auto history_record_iter : pop.subpop_size_histories_)
        {
            if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
            {
                SubpopSizeHistory &history_record = history_record_iter.second;
                slim_popsize_t *history = history_record.history_;
                slim_generation_t historyLength = history_record.history_length_;
                
                if ((iter == 0) && !drawSubpopsGray)
                    pointColor = controller_->whiteContrastingColorForIndex(history_record_iter.first);
                
                // If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
                slim_generation_t firstHistoryEntryToDraw = (cachingNow_ ? 0 : (drawingCache_ ? drawingCacheGeneration_ : 0));
                
                for (slim_generation_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedGenerations); ++i)
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

void QtSLiMGraphView_PopSizeOverTime::drawLineGraph(QPainter &painter, QRect interiorRect)
{
    SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	// Draw the size history as a line plot
	bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
	bool drawSubpopsGray = (showSubpops && (pop.subpop_size_histories_.size() > 8));	// 7 subpops + pop
	
    // First draw subpops, then draw the population size
    for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
    {
        QColor lineColor = (iter == 0) ? QtSLiMColorWithWhite(0.5, 1.0) : Qt::black;
        double lineWidth = (iter == 0) ? 1.0 : 1.5;
        
        for (auto history_record_iter : pop.subpop_size_histories_)
        {
            if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
            {
                SubpopSizeHistory &history_record = history_record_iter.second;
                slim_popsize_t *history = history_record.history_;
                slim_generation_t historyLength = history_record.history_length_;
                QPainterPath linePath;
                bool startedLine = false;
                
                for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
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
                
                if ((iter == 0) && !drawSubpopsGray)
                    lineColor = controller_->whiteContrastingColorForIndex(history_record_iter.first);
                
                painter.strokePath(linePath, QPen(lineColor, lineWidth));
            }
        }
    }
}

void QtSLiMGraphView_PopSizeOverTime::drawGraph(QPainter &painter, QRect interiorRect)
{
    if (drawLines_)
		drawLineGraph(painter, interiorRect);
	else
		drawPointGraph(painter, interiorRect);
}

bool QtSLiMGraphView_PopSizeOverTime::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_PopSizeOverTime::appendStringForData(QString &string)
{
	SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
    // Size history
    bool showSubpops = showSubpopulations_ && (pop.subpop_size_histories_.size() > 2);
    
	string.append("\n\n# Size history:\n");
	
    for (int iter = 0; iter <= (showSubpops ? 1 : 0); ++iter)
    {
        for (auto history_record_iter : pop.subpop_size_histories_)
        {
            if (((iter == 0) && (history_record_iter.first == -1)) || ((iter == 1) && (history_record_iter.first != -1)))
            {
                SubpopSizeHistory &history_record = history_record_iter.second;
                slim_popsize_t *history = history_record.history_;
                slim_generation_t historyLength = history_record.history_length_;
                
                if (iter == 1)
                    string.append(QString("\n\n# Size history (subpopulation p%1):\n").arg(history_record_iter.first));
                
                for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
                    string.append(QString("%1, ").arg(history[i]));
                
                string.append("\n");
            }
        }
    }
}

QtSLiMLegendSpec QtSLiMGraphView_PopSizeOverTime::legendKey(void)
{
    if (!showSubpopulations_)
        return QtSLiMLegendSpec();
    
    std::vector<slim_objectid_t> subpopsToDisplay;
    
    for (auto history_record_iter : controller_->sim->population_.subpop_size_histories_)
        subpopsToDisplay.push_back(history_record_iter.first);

    return subpopulationLegendKey(subpopsToDisplay, subpopsToDisplay.size() > 8);
}

void QtSLiMGraphView_PopSizeOverTime::toggleShowSubpopulations(void)
{
    showSubpopulations_ = !showSubpopulations_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_PopSizeOverTime::toggleDrawLines(void)
{
    drawLines_ = !drawLines_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_PopSizeOverTime::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction(showSubpopulations_ ? "Hide Subpopulations" : "Show Subpopulations", this, &QtSLiMGraphView_PopSizeOverTime::toggleShowSubpopulations);
    contextMenu.addAction(drawLines_ ? "Draw Points (Faster)" : "Draw Lines (Slower)", this, &QtSLiMGraphView_PopSizeOverTime::toggleDrawLines);
}





























