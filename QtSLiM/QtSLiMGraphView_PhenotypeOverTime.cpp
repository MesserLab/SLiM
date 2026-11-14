//
//  QtSLiMGraphView_PhenotypeOverTime.cpp
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

#include "QtSLiMGraphView_PhenotypeOverTime.h"

#include <QAction>
#include <QMenu>
#include <QPixmap>
#include <QPainterPath>
#include <QComboBox>
#include <QDebug>

#include <vector>

#include "QtSLiMWindow.h"


QtSLiMGraphView_PhenotypeOverTime::QtSLiMGraphView_PhenotypeOverTime(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
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
    
    // try to figure out our focal trait from the focal display species
    focalTrait();
    
    QtSLiMGraphView_PhenotypeOverTime::updateAfterTick();
}

void QtSLiMGraphView_PhenotypeOverTime::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *button_layout = buttonLayout();
    
    if (button_layout)
    {
        traitButton_ = newButtonInLayout(button_layout);
        connect(traitButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_PhenotypeOverTime::traitPopupChanged);
        
        addTraitsToMenu(traitButton_, trait_name_);
    }
}

void QtSLiMGraphView_PhenotypeOverTime::focalTraitChanged(void)
{
    // called by both traitPopupChanged() and setFocalTrait(); this assumes
    // that the trait popup is already correct, to avoid circularity, and
    // just handles the consequence of the focal trait change for display
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies || graphSpecies->has_implicit_trait_)
        yAxisLabel_ = "Phenotype";
    else
        yAxisLabel_ = QString("Phenotype (%1)").arg(QString::fromStdString(trait_name_));
    
    invalidateDrawingCache();
    QtSLiMGraphView_PhenotypeOverTime::updateAfterTick();
}

void QtSLiMGraphView_PhenotypeOverTime::traitPopupChanged(int /* index */)
{
    std::string newTraitName = traitButton_->currentData().toString().toStdString();
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (trait_name_ != newTraitName) && (newTraitName.length() > 0))
    {
        trait_name_ = newTraitName;
        focalTraitChanged();
    }
}

void QtSLiMGraphView_PhenotypeOverTime::setFocalTrait(Trait *trait)
{
    if (!trait)
        return;
    
    trait_name_ = trait->Name();
    addTraitsToMenu(traitButton_, trait_name_);
    
    focalTraitChanged();
}

Trait *QtSLiMGraphView_PhenotypeOverTime::focalTrait(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies)
        return nullptr;
    
    // if we have a trait name, try to use it, and don't change it (so we work across a recycle)
    if (trait_name_.length())
    {
        Trait *trait = graphSpecies->TraitFromName(trait_name_);
        
        return trait;   // might be nullptr
    }
    
    // with no trait name, try the controller's focal display trait
    if (controller_)
    {
        Trait *trait = controller_->focalTraitForSpecies(graphSpecies);
        
        if (trait)
        {
            setFocalTrait(trait);
            return trait;
        }
    }
    
    // with no focal display trait, try the first trait of the species
    if (graphSpecies && (graphSpecies->TraitCount() > 0))
    {
        Trait *trait = graphSpecies->Traits()[0];
        
        if (trait)
        {
            setFocalTrait(trait);
            return trait;
        }
    }
    
    return nullptr;
}

void QtSLiMGraphView_PhenotypeOverTime::setDefaultYAxisRange(void)
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

QtSLiMGraphView_PhenotypeOverTime::~QtSLiMGraphView_PhenotypeOverTime()
{
    // We are responsible for our own destruction
    QtSLiMGraphView_PhenotypeOverTime::invalidateDrawingCache();
}

void QtSLiMGraphView_PhenotypeOverTime::invalidateDrawingCache(void)
{
    delete drawingCache_;
	drawingCache_ = nullptr;
	drawingCacheTick_ = 0;
}

void QtSLiMGraphView_PhenotypeOverTime::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
	{
		if (!yAxisIsUserRescaled_)
			setDefaultYAxisRange();
		//if (!xAxisIsUserRescaled_)
		//	setXAxisRangeFromTick();	// the end tick is not yet known
		
		update();
	}
    
    addTraitsToMenu(traitButton_, trait_name_);
	
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_PhenotypeOverTime::graphTitle(void)
{
    return "Phenotype ~ Time";
}

QString QtSLiMGraphView_PhenotypeOverTime::aboutString(void)
{
    return "The Phenotype ~ Time graph shows the mean phenotype for one trait, "
           "for the focal species (and subpopulations), as a function of time.  The species-"
           "level mean phenotype is shown with a thick bright line, while those for "
           "subpopulations are shown with thinner pastel lines.";
}

void QtSLiMGraphView_PhenotypeOverTime::updateAfterTick(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    
    // BCH 3/20/2024: We set the x axis range each tick, because the end tick is now invalid until after initialize() callbacks
    if (!controller_->invalidSimulation() && graphSpecies && !xAxisIsUserRescaled_)
        setXAxisRangeFromTick();
    
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections
    addTraitsToMenu(traitButton_, trait_name_);
    
    if (!controller_->invalidSimulation() && graphSpecies && !yAxisIsUserRescaled_)
    {
        Population &pop = graphSpecies->population_;
        double ymin = std::numeric_limits<double>::infinity();
        double ymax = -std::numeric_limits<double>::infinity();
        
        if (pop.subpop_trait_histories_.size() == 0)
            return;
        
        Trait *trait = focalTrait();
        
        if (!trait)
            return;
        
        slim_trait_index_t trait_index = trait->Index();
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
            QtSLiMGraphView_PhenotypeOverTime::invalidateDrawingCache();
        }
    }
	
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_PhenotypeOverTime::drawPointGraph(QPainter &painter, QRect interiorRect)
{
    Community *community = controller_->community;
    Species *graphSpecies = focalDisplaySpecies();
    Population &pop = graphSpecies->population_;
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
    if (pop.subpop_trait_histories_.size() == 0)
        return;
    
    Trait *trait = focalTrait();
    
    if (!trait)
        return;
    
    slim_trait_index_t trait_index = trait->Index();
    std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
    bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
    bool drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
    
    // First draw subpops, then draw the population mean
    for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
    {
        QColor pointColor = ((iter == 0) ? QtSLiMColorWithWhite(0.5, 1.0) : Qt::black);
        
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
                        
                        if ((iter == 0) && !drawSubpopsGray)
                            pointColor = controller_->whiteContrastingColorForIndex(history_record_iter.first);
                        
                        painter.fillRect(QRectF(historyPoint.x() - 0.5, historyPoint.y() - 0.5, 1.0, 1.0), pointColor);
                    }
                }
            }
        }
    }
}

void QtSLiMGraphView_PhenotypeOverTime::drawLineGraph(QPainter &painter, QRect interiorRect)
{
    Community *community = controller_->community;
    Species *graphSpecies = focalDisplaySpecies();
    Population &pop = graphSpecies->population_;
    slim_tick_t completedTicks = community->Tick() - 1;
    
    // Draw the size history as a line plot, without image caching
    if (pop.subpop_trait_histories_.size() == 0)
        return;
    
    Trait *trait = focalTrait();
    
    if (!trait)
        return;
    
    slim_trait_index_t trait_index = trait->Index();
    std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
    bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
    bool drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
    
    // First draw subpops, then draw the population mean
    for (int iter = (showSubpops ? 0 : 1); iter <= 1; ++iter)
    {
        QColor lineColor = (iter == 0) ? QtSLiMColorWithWhite(0.5, 1.0) : Qt::black;
        double lineWidth = (iter == 0) ? 1.0 : 1.5;
        
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
                
                if ((iter == 0) && !drawSubpopsGray)
                    lineColor = controller_->whiteContrastingColorForIndex(history_record_iter.first);
                
                painter.strokePath(linePath, QPen(lineColor, lineWidth));
            }
        }
    }
}

void QtSLiMGraphView_PhenotypeOverTime::drawGraph(QPainter &painter, QRect interiorRect)
{
    if (drawLines_)
		drawLineGraph(painter, interiorRect);
	else
		drawPointGraph(painter, interiorRect);
}

bool QtSLiMGraphView_PhenotypeOverTime::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_PhenotypeOverTime::appendStringForData(QString &string)
{
    Community *community = controller_->community;
    Species *graphSpecies = focalDisplaySpecies();
    Population &pop = graphSpecies->population_;
	slim_tick_t completedTicks = community->Tick() - 1;
	
    // Phenotype history
    if (pop.subpop_trait_histories_.size() == 0)
        return;
    
    Trait *trait = focalTrait();
    
    if (!trait)
        return;
    
    slim_trait_index_t trait_index = trait->Index();
    std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
    QString traitName = QString::fromStdString(trait->Name());
    bool showSubpops = showSubpopulations_ && (one_trait_histories.size() > 2);
    
    string.append(QString("\n\n# Phenotype history (trait %1):\n").arg(traitName));
    
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

QtSLiMLegendSpec QtSLiMGraphView_PhenotypeOverTime::legendKey(void)
{
    if (!showSubpopulations_)
        return QtSLiMLegendSpec();
    
    Species *graphSpecies = focalDisplaySpecies();
    Population &pop = graphSpecies->population_;
    if (pop.subpop_trait_histories_.size() == 0)
        return QtSLiMLegendSpec();
    
    Trait *trait = focalTrait();
    
    if (!trait)
        return QtSLiMLegendSpec();
    
    slim_trait_index_t trait_index = trait->Index();
    std::map<slim_objectid_t,SubpopTraitHistory> &one_trait_histories = pop.subpop_trait_histories_[trait_index];
    
    if (one_trait_histories.size() <= 2)
        return QtSLiMLegendSpec();
    
    std::vector<slim_objectid_t> subpopsToDisplay;
    
    for (const auto &history_record_iter : one_trait_histories)
        subpopsToDisplay.emplace_back(history_record_iter.first);
    
    return subpopulationLegendKey(subpopsToDisplay, subpopsToDisplay.size() > 8);
}

void QtSLiMGraphView_PhenotypeOverTime::toggleShowSubpopulations(void)
{
    showSubpopulations_ = !showSubpopulations_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_PhenotypeOverTime::toggleDrawLines(void)
{
    drawLines_ = !drawLines_;
    invalidateDrawingCache();
    update();
}

void QtSLiMGraphView_PhenotypeOverTime::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction(showSubpopulations_ ? "Hide Subpopulations" : "Show Subpopulations", this, &QtSLiMGraphView_PhenotypeOverTime::toggleShowSubpopulations);
    contextMenu.addAction(drawLines_ ? "Draw Points (Faster)" : "Draw Lines (Slower)", this, &QtSLiMGraphView_PhenotypeOverTime::toggleDrawLines);
}





























