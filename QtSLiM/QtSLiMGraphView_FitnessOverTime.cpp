//
//  QtSLiMGraphView_FitnessOverTime.cpp
//  SLiM
//
//  Created by Ben Haller on 3/30/2020.
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

#include "QtSLiMGraphView_FitnessOverTime.h"

#include <QAction>
#include <QMenu>
#include <QPixmap>
#include <QPainterPath>
#include <QDebug>

#include "QtSLiMWindow.h"
#include "subpopulation.h"


QtSLiMGraphView_FitnessOverTime::QtSLiMGraphView_FitnessOverTime(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    setXAxisRangeFromGeneration();
    setDefaultYAxisRange();
    
    xAxisLabel_ = "Generation";
    yAxisLabel_ = "Fitness (rescaled absolute)";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    showSubpopulations_ = true;
    drawLines_ = true;
    
    updateAfterTick();
}

void QtSLiMGraphView_FitnessOverTime::setDefaultYAxisRange(void)
{
    yAxisMin_ = 0.9;
	yAxisMax_ = 1.1;		// dynamic
	yAxisMajorTickInterval_ = 0.1;
	yAxisMinorTickInterval_ = 0.02;
	yAxisMajorTickModulus_ = 5;
	yAxisTickValuePrecision_ = 1;
}

QtSLiMGraphView_FitnessOverTime::~QtSLiMGraphView_FitnessOverTime()
{
}

void QtSLiMGraphView_FitnessOverTime::invalidateDrawingCache(void)
{
    delete drawingCache_;
	drawingCache_ = nullptr;
	drawingCacheGeneration_ = 0;
}

void QtSLiMGraphView_FitnessOverTime::controllerRecycled(void)
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

void QtSLiMGraphView_FitnessOverTime::controllerSelectionChanged(void)
{
    invalidateDrawingCache();
    update();
	
	QtSLiMGraphView::controllerSelectionChanged();
}

QString QtSLiMGraphView_FitnessOverTime::graphTitle(void)
{
    return "Fitness ~ Time";
}

QString QtSLiMGraphView_FitnessOverTime::aboutString(void)
{
    return "The Fitness ~ Time graph shows mean fitness as a function of time.  The mean fitness "
           "of the population is shown with a thick black line, while those of subpopulations "
           "are shown with thinner colored lines.  Fixation events during the model run are "
           "shown with light blue vertical lines at the generation in which they occurred.  The "
           "fitness shown is 'rescaled absolute', meaning that it is absolute fitness, not "
           "relative fitness, but that when non-neutral mutations fix and are 'substituted' by "
           "SLiM they are no longer included in fitness calculations, so the y axis is 'rescaled'; "
           "this is mainly relevant to WF models.  It is also 'rescaled' in the sense that it "
           "excludes subpopulation fitnessScaling values (to emphasize individual fitness effects "
           "over density-dependence); this is mainly relevant to nonWF models.";
}

void QtSLiMGraphView_FitnessOverTime::updateAfterTick(void)
{
	if (!controller_->invalidSimulation() && !yAxisIsUserRescaled_)
	{
		SLiMSim *sim = controller_->sim;
		Population &pop = sim->population_;
		double minHistory = std::numeric_limits<double>::infinity();
		double maxHistory = -std::numeric_limits<double>::infinity();
		bool showSubpops = showSubpopulations_ && (pop.fitness_histories_.size() > 2);
		
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (showSubpops || (history_record_iter.first == -1))
			{
				FitnessHistory &history_record = history_record_iter.second;
				double *history = history_record.history_;
				slim_generation_t historyLength = history_record.history_length_;
				
				// find the min and max history value
				for (int i = 0; i < historyLength; ++i)
				{
					double historyEntry = history[i];
					
					if (!std::isnan(historyEntry))
					{
						if (historyEntry > maxHistory)
							maxHistory = historyEntry;
						if (historyEntry < minHistory)
							minHistory = historyEntry;
					}
				}
			}
		}
		
		// set axis range to encompass the data
		if (!std::isinf(minHistory) && !std::isinf(maxHistory))
		{
			if ((minHistory < 0.9) || (maxHistory > 1.1))	// if we're outside our original axis range...
			{
				double axisMin = (minHistory < 0.5 ? 0.0 : 0.5);	// either 0.0 or 0.5
				double axisMax = ceil(maxHistory * 2.0) / 2.0;		// 1.5, 2.0, 2.5, ...
				
				if (axisMax < 1.5)
					axisMax = 1.5;
				
				if ((fabs(axisMin - yAxisMin_) > 0.0000001) || (fabs(axisMax - yAxisMax_) > 0.0000001))
				{
					yAxisMin_ = axisMin;
					yAxisMax_ = axisMax;
					yAxisMajorTickInterval_ = 0.5;
					yAxisMinorTickInterval_ = 0.25;
					yAxisMajorTickModulus_ = 2;
					yAxisTickValuePrecision_ = 1;
					
					invalidateDrawingCache();
				}
			}
		}
	}
	
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_FitnessOverTime::drawPointGraph(QPainter &painter, QRect interiorRect)
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
    
	// Draw fixation events
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_generation_t fixation_gen = substitution->fixation_generation_;
		
		// If we are caching, draw all events; if we are not, draw only those that are not already in the cache
		if (!cachingNow_ && (fixation_gen < drawingCacheGeneration_))
			continue;
		
        double substitutionX = plotToDeviceX(fixation_gen, interiorRect);
		QRectF substitutionRect(substitutionX - 0.5, interiorRect.x(), 1.0, interiorRect.height());
		
        painter.fillRect(substitutionRect, QtSLiMColorWithRGB(0.2, 0.2, 1.0, 0.2));
	}
	
	// Draw the fitness history as a scatter plot; better suited to caching of the image
    bool showSubpops = showSubpopulations_ && (pop.fitness_histories_.size() > 2);
	bool drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
	
	// First draw subpops, then draw the mean population fitness
    for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
    {
        QColor pointColor = ((iter == 0) ? QtSLiMColorWithWhite(0.5, 1.0) : Qt::black);
        
        for (auto history_record_iter : pop.fitness_histories_)
        {
            if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
            {
                FitnessHistory &history_record = history_record_iter.second;
                double *history = history_record.history_;
                slim_generation_t historyLength = history_record.history_length_;
                
                // If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
                slim_generation_t firstHistoryEntryToDraw = (cachingNow_ ? 0 : (drawingCache_ ? drawingCacheGeneration_ : 0));
                
                for (slim_generation_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedGenerations); ++i)
                {
                    double historyEntry = history[i];
                    
                    if (!std::isnan(historyEntry))
                    {
                        QPointF historyPoint(plotToDeviceX(i, interiorRect), plotToDeviceY(historyEntry, interiorRect));
                        
                        if ((iter == 0) && !drawSubpopsGray)
                            pointColor = controller_->whiteContrastingColorForIndex(history_record_iter.first);
                        
                        painter.fillRect(QRectF(historyPoint.x() - 0.5, historyPoint.y() - 0.5, 1.0, 1.0), pointColor);
                    }
                }
            }
        }
    }
}

void QtSLiMGraphView_FitnessOverTime::drawLineGraph(QPainter &painter, QRect interiorRect)
{
    SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	// Draw fixation events
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_generation_t fixation_gen = substitution->fixation_generation_;
        double substitutionX = plotToDeviceX(fixation_gen, interiorRect);
		QRectF substitutionRect(substitutionX - 0.5, interiorRect.x(), 1.0, interiorRect.height());
		
        painter.fillRect(substitutionRect, QtSLiMColorWithRGB(0.2, 0.2, 1.0, 0.2));
	}
	
	// Draw the fitness history as a line plot
	bool showSubpops = showSubpopulations_ && (pop.fitness_histories_.size() > 2);
	bool drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
	
    // First draw subpops, then draw the mean population fitness
    for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
    {
        QColor lineColor = (iter == 0) ? QtSLiMColorWithWhite(0.5, 1.0) : Qt::black;
        double lineWidth = (iter == 0) ? 1.0 : 1.5;
        
        for (auto history_record_iter : pop.fitness_histories_)
        {
            if (((iter == 0) && (history_record_iter.first != -1)) || ((iter == 1) && (history_record_iter.first == -1)))
            {
                FitnessHistory &history_record = history_record_iter.second;
                double *history = history_record.history_;
                slim_generation_t historyLength = history_record.history_length_;
                QPainterPath linePath;
                bool startedLine = false;
                
                for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
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
                
                if ((iter == 0) && !drawSubpopsGray)
                    lineColor = controller_->whiteContrastingColorForIndex(history_record_iter.first);
                
                painter.strokePath(linePath, QPen(lineColor, lineWidth));
            }
        }
    }
}

void QtSLiMGraphView_FitnessOverTime::drawGraph(QPainter &painter, QRect interiorRect)
{
    if (drawLines_)
		drawLineGraph(painter, interiorRect);
	else
		drawPointGraph(painter, interiorRect);
}

bool QtSLiMGraphView_FitnessOverTime::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_FitnessOverTime::appendStringForData(QString &string)
{
	SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	// Fixation events
	string.append("# Fixation generations:\n");
	
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_generation_t fixation_gen = substitution->fixation_generation_;
		
		string.append(QString("%1, ").arg(fixation_gen));
	}
	
	// Fitness history
    bool showSubpops = showSubpopulations_ && (pop.fitness_histories_.size() > 2);
    
	string.append("\n\n# Fitness history:\n");
	
    for (int iter = 0; iter <= (showSubpops ? 1 : 0); ++iter)
    {
        for (auto history_record_iter : pop.fitness_histories_)
        {
            if (((iter == 0) && (history_record_iter.first == -1)) || ((iter == 1) && (history_record_iter.first != -1)))
            {
                FitnessHistory &history_record = history_record_iter.second;
                double *history = history_record.history_;
                slim_generation_t historyLength = history_record.history_length_;
                
                if (iter == 1)
                    string.append(QString("\n\n# Fitness history (subpopulation p%1):\n").arg(history_record_iter.first));
                
                for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
                    string.append(QString("%1, ").arg(history[i], 0, 'f', 4));
                
                string.append("\n");
			}
		}
	}
}

QtSLiMLegendSpec QtSLiMGraphView_FitnessOverTime::legendKey(void)
{
    if (!showSubpopulations_)
        return QtSLiMLegendSpec();
    
    std::vector<slim_objectid_t> subpopsToDisplay;
    
    for (auto history_record_iter : controller_->sim->population_.fitness_histories_)
        subpopsToDisplay.push_back(history_record_iter.first);

    return subpopulationLegendKey(subpopsToDisplay, subpopsToDisplay.size() > 8);
}

void QtSLiMGraphView_FitnessOverTime::toggleShowSubpopulations(void)
{
    showSubpopulations_ = !showSubpopulations_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_FitnessOverTime::toggleDrawLines(void)
{
    drawLines_ = !drawLines_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_FitnessOverTime::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction(showSubpopulations_ ? "Hide Subpopulations" : "Show Subpopulations", this, &QtSLiMGraphView_FitnessOverTime::toggleShowSubpopulations);
    contextMenu.addAction(drawLines_ ? "Draw Points (Faster)" : "Draw Lines (Slower)", this, &QtSLiMGraphView_FitnessOverTime::toggleDrawLines);
}





























