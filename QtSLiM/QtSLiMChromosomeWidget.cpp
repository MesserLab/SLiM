//
//  QtSLiMChromosomeWidget.cpp
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2024 Philipp Messer.  All rights reserved.
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


#include "QtSLiMChromosomeWidget.h"
#include "QtSLiMWindow.h"
#include "QtSLiMExtras.h"
#include "QtSLiMHaplotypeManager.h"
#include "QtSLiMPreferences.h"

#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QtDebug>

#include <map>
#include <algorithm>
#include <vector>


static const int numberOfTicksPlusOne = 4;
static const int tickLength = 5;
static const int heightForTicks = 16;
static const int selectionKnobSizeExtension = 2;	// a 5-pixel-width knob is 2: 2 + 1 + 2, an extension on each side plus the one pixel of the bar in the middle
static const int selectionKnobSize = selectionKnobSizeExtension + selectionKnobSizeExtension + 1;


QtSLiMChromosomeWidget::QtSLiMChromosomeWidget(QWidget *p_parent, QtSLiMWindow *controller, Species *displaySpecies, Qt::WindowFlags f)
#ifndef SLIM_NO_OPENGL
    : QOpenGLWidget(p_parent, f)
#else
    : QWidget(p_parent, f)
#endif
{
    controller_ = controller;
    setFocalDisplaySpecies(displaySpecies);
    
    // We support both OpenGL and non-OpenGL display, because some platforms seem
    // to have problems with OpenGL (https://github.com/MesserLab/SLiM/issues/462)
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::useOpenGLPrefChanged, this, [this]() { update(); });
}

QtSLiMChromosomeWidget::~QtSLiMChromosomeWidget()
{
    setReferenceChromosomeView(nullptr);
	
    if (haplotype_mgr_)
    {
        delete haplotype_mgr_;
        haplotype_mgr_ = nullptr;
    }
    
    if (haplotype_previous_bincounts)
    {
        free(haplotype_previous_bincounts);
        haplotype_previous_bincounts = nullptr;
    }
    
    controller_ = nullptr;
}

void QtSLiMChromosomeWidget::setController(QtSLiMWindow *controller)
{
    controller_ = controller;
}

void QtSLiMChromosomeWidget::setFocalDisplaySpecies(Species *displaySpecies)
{
    // We can have no focal species (when coming out of the nib, in particular); in that case we display empty state
    if (displaySpecies)
        focalSpeciesName_ = displaySpecies->name_;
    else
        focalSpeciesName_ = "";
}

Species *QtSLiMChromosomeWidget::focalDisplaySpecies(void)
{
    // We look up our focal species object by name every time, since keeping a pointer to it would be unsafe
    // Before initialize() is done species have not been created, so we return nullptr in that case
    if (focalSpeciesName_.length() == 0)
        return nullptr;
    
    if (controller_ && controller_->community && (controller_->community->Tick() >= 1))
        return controller_->community->SpeciesWithName(focalSpeciesName_);
    
    return nullptr;
}

void QtSLiMChromosomeWidget::stateChanged(void)
{
    // when the model state changes, we toss our cached haplotype manager to generate a new plot
    if (haplotype_mgr_)
    {
        delete haplotype_mgr_;
        haplotype_mgr_ = nullptr;
    }
    
    update();
}

#ifndef SLIM_NO_OPENGL
void QtSLiMChromosomeWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
}

void QtSLiMChromosomeWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    
	// Update the projection
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}
#endif

QRect QtSLiMChromosomeWidget::rectEncompassingBaseToBase(slim_position_t startBase, slim_position_t endBase, QRect interiorRect, QtSLiMRange displayedRange)
{
	double startFraction = (startBase - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length);
	double leftEdgeDouble = interiorRect.left() + startFraction * interiorRect.width();
	double endFraction = (endBase + 1 - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length);
	double rightEdgeDouble = interiorRect.left() + endFraction * interiorRect.width();
	int leftEdge, rightEdge;
	
	if (rightEdgeDouble - leftEdgeDouble > 1.0)
	{
		// If the range spans a width of more than one pixel, then use the maximal pixel range
		leftEdge = static_cast<int>(floor(leftEdgeDouble));
		rightEdge = static_cast<int>(ceil(rightEdgeDouble));
	}
	else
	{
		// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the left-right positions span a pixel boundary
		leftEdge = static_cast<int>(floor(leftEdgeDouble));
		rightEdge = leftEdge + 1;
	}
	
	return QRect(leftEdge, interiorRect.top(), rightEdge - leftEdge, interiorRect.height());
}

slim_position_t QtSLiMChromosomeWidget::baseForPosition(double position, QRect interiorRect, QtSLiMRange displayedRange)
{
	double fraction = (position - interiorRect.left()) / interiorRect.width();
	slim_position_t base = static_cast<slim_position_t>(floor(fraction * (displayedRange.length + 1) + displayedRange.location));
	
	return base;
}

QRect QtSLiMChromosomeWidget::getContentRect(void)
{
    QRect bounds = rect();
	
	// Two things are going on here.  The width gets inset by two pixels on each side because our frame is outset that much from our apparent frame, to
	// make room for the selection knobs to spill over a bit.  The height gets adjusted because our "content rect" does not include our ticks.
    return QRect(bounds.left(), bounds.top(), bounds.width(), bounds.height() - heightForTicks);
}

QRect QtSLiMChromosomeWidget::getInteriorRect(void)
{
    return getContentRect().marginsRemoved(QMargins(1, 1, 1, 1));
}

void QtSLiMChromosomeWidget::setReferenceChromosomeView(QtSLiMChromosomeWidget *p_ref_widget)
{
	if (referenceChromosomeView_ != p_ref_widget)
	{
        if (referenceChromosomeView_)
            disconnect(referenceChromosomeView_);
        
        referenceChromosomeView_ = p_ref_widget;
        
        if (referenceChromosomeView_)
            connect(referenceChromosomeView_, &QtSLiMChromosomeWidget::selectedRangeChanged, this, [this]() { stateChanged(); });
	}
}

QtSLiMRange QtSLiMChromosomeWidget::getSelectedRange(Species *displaySpecies)
{
    if (hasSelection_)
	{
		return QtSLiMRange(selectionFirstBase_, selectionLastBase_ - selectionFirstBase_ + 1);	// number of bases encompassed; a selection from x to x encompasses 1 base
	}
	else
	{
		Chromosome &chromosome = displaySpecies->TheChromosome();
		slim_position_t chromosomeLastPosition = chromosome.last_position_;
		
		return QtSLiMRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
	}
}

void QtSLiMChromosomeWidget::setSelectedRange(QtSLiMRange p_selectionRange)
{
    if (selectable_ && (p_selectionRange.length >= 1))
	{
		selectionFirstBase_ = static_cast<slim_position_t>(p_selectionRange.location);
		selectionLastBase_ = static_cast<slim_position_t>(p_selectionRange.location + p_selectionRange.length) - 1;
		hasSelection_ = true;
		
		// Save the selection for restoring across recycles, etc.
		savedSelectionFirstBase_ = selectionFirstBase_;
		savedSelectionLastBase_ = selectionLastBase_;
		savedHasSelection_ = hasSelection_;
	}
	else if (hasSelection_)
	{
		hasSelection_ = false;
		
		// Save the selection for restoring across recycles, etc.
		savedHasSelection_ = hasSelection_;
	}
	else
	{
		// Save the selection for restoring across recycles, etc.
		savedHasSelection_ = false;
		
		return;
	}
	
	// Our selection changed, so update and post a change notification
    update();
	
    emit selectedRangeChanged();
}

void QtSLiMChromosomeWidget::restoreLastSelection(void)
{
    if (selectable_ && savedHasSelection_)
	{
		selectionFirstBase_ = savedSelectionFirstBase_;
		selectionLastBase_ = savedSelectionLastBase_;
		hasSelection_ = savedHasSelection_;
	}
	else if (hasSelection_)
	{
		hasSelection_ = false;
	}
	else
	{
		// We want to always post the notification, to make sure updating happens correctly;
		// this ensures that correct ticks marks get drawn after a recycle, etc.
		//return;
	}
	
	// Our selection changed, so update and post a change notification
	update();
	
    emit selectedRangeChanged();
}

QtSLiMRange QtSLiMChromosomeWidget::getDisplayedRange(Species *displaySpecies)
{
	QtSLiMChromosomeWidget *reference = referenceChromosomeView_;
	
	if (reference)
		return reference->getSelectedRange(displaySpecies);
	else
	{
		Chromosome &chromosome = displaySpecies->TheChromosome();
		slim_position_t chromosomeLastPosition = chromosome.last_position_;
		
		return QtSLiMRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
	}
}

#ifndef SLIM_NO_OPENGL
void QtSLiMChromosomeWidget::paintGL()
#else
void QtSLiMChromosomeWidget::paintEvent(QPaintEvent * /* p_paint_event */)
#endif
{
    QPainter painter(this);
    bool inDarkMode = QtSLiMInDarkMode();
    
    painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    //painter.fillRect(rect(), Qt::red);
    
    painter.setPen(Qt::black);      // make sure we have our default color of black, since Qt apparently does not guarantee that
    
    Species *displaySpecies = focalDisplaySpecies();
    bool ready = (isEnabled() && controller_ && !controller_->invalidSimulation() && (displaySpecies != nullptr));
    QRect contentRect = getContentRect();
	QRect interiorRect = getInteriorRect();
    
    // if the simulation is at tick 0, it is not ready
	if (ready)
		if (controller_->community->Tick() == 0)
			ready = false;
	
    if (ready)
    {
        // erase the content area itself; done in glDrawRect() now
        //painter.fillRect(interiorRect, Qt::black);
		
		QtSLiMRange displayedRange = getDisplayedRange(displaySpecies);
        
		// draw ticks at bottom of content rect
        drawTicksInContentRect(contentRect, displaySpecies, displayedRange, painter);
        
        // do the core drawing, with or without OpenGL according to user preference
#ifndef SLIM_NO_OPENGL
        if (QtSLiMPreferencesNotifier::instance().useOpenGLPref())
        {
            painter.beginNativePainting();
            glDrawRect(displaySpecies);
            painter.endNativePainting();
        }
        else
#endif
        {
            qtDrawRect(displaySpecies, painter);
        }
        
        // frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
		QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
        
		// overlay the selection last, since it bridges over the frame
		if (hasSelection_)
			overlaySelection(interiorRect, displayedRange, painter);
    }
    else
    {
        // erase the content area itself
        painter.fillRect(interiorRect, QtSLiMColorWithWhite(inDarkMode ? 0.118 : 0.88, 1.0));
        
        // frame
        QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
    }
}

void QtSLiMChromosomeWidget::drawTicksInContentRect(QRect contentRect, __attribute__((__unused__)) Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter)
{
    bool inDarkMode = QtSLiMInDarkMode();
	QRect interiorRect = getInteriorRect();
	int64_t lastTickIndex = numberOfTicksPlusOne;
	
    painter.save();
    painter.setPen(inDarkMode ? Qt::white : Qt::black);
    
	// Display fewer ticks when we are displaying a very small number of positions
	lastTickIndex = std::min(lastTickIndex, (displayedRange.length + 1) / 3);
	
	double tickIndexDivisor = ((lastTickIndex == 0) ? 1.0 : static_cast<double>(lastTickIndex));		// avoid a divide by zero when we are displaying a single site
    static QFont *tickFont = nullptr;
    
    if (!tickFont)
    {
        tickFont = new QFont();
#ifdef __linux__
        tickFont->setPointSize(7);
#else
        tickFont->setPointSize(9);
#endif
    }
    painter.setFont(*tickFont);
    
    if (displayedRange.length == 0)
	{
		// Handle the "no genetics" case separately
		if (!selectable_)
		{
            QString tickLabel("no genetics");
            int tickLabelX = static_cast<int>(floor(contentRect.left() + contentRect.width() / 2.0));
            int tickLabelY = contentRect.bottom() + (2 + 13);
            int textFlags = (Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignBottom | Qt::AlignHCenter);
            
            painter.drawText(QRect(tickLabelX, tickLabelY, 0, 0), textFlags, tickLabel);
		}
		
        painter.restore();
		return;
	}
    
	for (int tickIndex = 0; tickIndex <= lastTickIndex; ++tickIndex)
	{
		slim_position_t tickBase = static_cast<slim_position_t>(displayedRange.location) + static_cast<slim_position_t>(ceil((displayedRange.length - 1) * (tickIndex / tickIndexDivisor)));	// -1 because we are choosing an in-between-base position that falls, at most, to the left of the last base
		QRect tickRect = rectEncompassingBaseToBase(tickBase, tickBase, interiorRect, displayedRange);
		
        tickRect.setHeight(tickLength);
        tickRect.moveBottom(contentRect.bottom() + tickLength);
		
		// if we are displaying a single site or two sites, make a tick mark one pixel wide, rather than a very wide one, which looks weird
		if (displayedRange.length <= 2)
		{
            tickRect.setLeft(static_cast<int>(floor(tickRect.left() + tickRect.width() / 2.0 - 0.5)));
			tickRect.setWidth(1);
		}
		
        painter.fillRect(tickRect, inDarkMode ? QColor(10, 10, 10, 255) : QColor(127, 127, 127, 255));  // in dark mode, 17 matches the frame, but is too light
		
		// BCH 15 May 2018: display in scientific notation for positions at or above 1e10, as it gets a bit ridiculous...
        QString tickLabel;
        int tickLabelX = static_cast<int>(floor(tickRect.left() + tickRect.width() / 2.0));
        int tickLabelY = contentRect.bottom() + (tickLength + 13);
        bool forceCenteredLabel = (displayedRange.length <= 101);	// a selected subrange is never <=101 length, so this is safe even with large chromosomes
        int textFlags = (Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignBottom);
        
        if (tickBase >= 1e10)
            tickLabel = QString::asprintf("%.6e", static_cast<double>(tickBase));
        else
            QTextStream(&tickLabel) << static_cast<int64_t>(tickBase);
        
        if ((tickIndex == lastTickIndex) && !forceCenteredLabel)
        {
            tickLabelX += 2;
            textFlags |= Qt::AlignRight;
        }
        else if ((tickIndex > 0) || forceCenteredLabel)
        {
            tickLabelX += 1;
            textFlags |= Qt::AlignHCenter;
        }
        else
            textFlags |= Qt::AlignLeft;
        
        painter.drawText(QRect(tickLabelX, tickLabelY, 0, 0), textFlags, tickLabel);
	}
    
    painter.restore();
}

void QtSLiMChromosomeWidget::updateDisplayedMutationTypes(Species *displaySpecies)
{
    // We use a flag in MutationType to indicate whether we're drawing that type or not; we update those flags here,
    // before every drawing of mutations, from the vector of mutation type IDs that we keep internally
    if (controller_)
    {
        if (displaySpecies)
        {
            std::map<slim_objectid_t,MutationType*> &muttypes = displaySpecies->mutation_types_;
            std::vector<slim_objectid_t> &displayTypes = displayMuttypes();
            
            for (auto muttype_iter : muttypes)
            {
                MutationType *muttype = muttype_iter.second;
                
                if (displayTypes.size())
                {
                    slim_objectid_t muttype_id = muttype->mutation_type_id_;
                    
                    muttype->mutation_type_displayed_ = (std::find(displayTypes.begin(), displayTypes.end(), muttype_id) != displayTypes.end());
                }
                else
                {
                    muttype->mutation_type_displayed_ = true;
                }
            }
        }
    }
}

void QtSLiMChromosomeWidget::overlaySelection(QRect interiorRect, QtSLiMRange displayedRange, QPainter &painter)
{
	if (hasSelection_)
	{
        // darken the interior of the selection slightly
        QRect selectionRect = rectEncompassingBaseToBase(selectionFirstBase_, selectionLastBase_, interiorRect, displayedRange);
        
        painter.fillRect(selectionRect, QtSLiMColorWithWhite(0.0, 0.30));
        
		// draw a bar at the start and end of the selection
		QRect selectionStartBar1 = QRect(selectionRect.left() - 1, interiorRect.top(), 1, interiorRect.height());
		QRect selectionStartBar2 = QRect(selectionRect.left(), interiorRect.top(), 1, interiorRect.height() + 5);
		//QRect selectionStartBar3 = QRect(selectionRect.left() + 1, interiorRect.top(), 1, interiorRect.height());
		//QRect selectionEndBar1 = QRect(selectionRect.left() + selectionRect.width() - 2, interiorRect.top(), 1, interiorRect.height());
		QRect selectionEndBar2 = QRect(selectionRect.left() + selectionRect.width() - 1, interiorRect.top(), 1, interiorRect.height() + 5);
		QRect selectionEndBar3 = QRect(selectionRect.left() + selectionRect.width(), interiorRect.top(), 1, interiorRect.height());
		
        painter.fillRect(selectionStartBar1, QtSLiMColorWithWhite(1.0, 0.15));
        //painter.fillRect(selectionEndBar1, QtSLiMColorWithWhite(1.0, 0.15));
		
		painter.fillRect(selectionStartBar2, Qt::black);
		painter.fillRect(selectionEndBar2, Qt::black);
		
        //painter.fillRect(selectionStartBar3, QtSLiMColorWithWhite(0.0, 0.30));
        painter.fillRect(selectionEndBar3, QtSLiMColorWithWhite(0.0, 0.30));
        
		// draw a ball at the end of each bar
        // FIXME this doesn't look quite as nice as SLiMgui, because QPainter doesn't antialias
        // also we can get clipped by one pixel at the edge of the view; subtle but imperfect
		QRect selectionStartBall = QRect(selectionRect.left() - selectionKnobSizeExtension, interiorRect.bottom() + (selectionKnobSize - 2), selectionKnobSize, selectionKnobSize);
		QRect selectionEndBall = QRect(selectionRect.left() + selectionRect.width() - (selectionKnobSizeExtension + 1), interiorRect.bottom() + (selectionKnobSize - 2), selectionKnobSize, selectionKnobSize);
		
        painter.save();
        painter.setPen(Qt::NoPen);
        
        painter.setBrush(Qt::black);	// outline
		painter.drawEllipse(selectionStartBall);
		painter.drawEllipse(selectionEndBall);
		
        painter.setBrush(QtSLiMColorWithWhite(0.3, 1.0));	// interior
		painter.drawEllipse(selectionStartBall.adjusted(1, 1, -1, -1));
		painter.drawEllipse(selectionEndBall.adjusted(1, 1, -1, -1));
		
		painter.setBrush(QtSLiMColorWithWhite(1.0, 0.5));	// highlight
		painter.drawEllipse(selectionStartBall.adjusted(1, 1, -2, -2));
		painter.drawEllipse(selectionEndBall.adjusted(1, 1, -2, -2));
        
        painter.restore();
    }
}

void QtSLiMChromosomeWidget::mousePressEvent(QMouseEvent *p_event)
{
    Species *displaySpecies = focalDisplaySpecies();
	bool ready = (selectable_ && isEnabled() && !controller_->invalidSimulation() && (displaySpecies != nullptr));
	
	// if the simulation is at tick 0, it is not ready
	if (ready)
		if (controller_->community->Tick() == 0)
			ready = false;
	
	if (ready)
	{
		QRect contentRect = getContentRect();
		QRect interiorRect = getInteriorRect();
		QtSLiMRange displayedRange = getDisplayedRange(displaySpecies);
        QPoint curPoint = p_event->pos();
		
		// Option-clicks just set the selection to the clicked genomic element, no questions asked
        if (p_event->modifiers() & Qt::AltModifier)
		{
            if (contentRect.contains(curPoint))
			{
				slim_position_t clickedBase = baseForPosition(curPoint.x(), interiorRect, displayedRange);
				QtSLiMRange selectionRange = QtSLiMRange(0, 0);
				Chromosome &chromosome = displaySpecies->TheChromosome();
				
				for (GenomicElement *genomicElement : chromosome.GenomicElements())
				{
					slim_position_t startPosition = genomicElement->start_position_;
					slim_position_t endPosition = genomicElement->end_position_;
					
					if ((clickedBase >= startPosition) && (clickedBase <= endPosition))
						selectionRange = QtSLiMRange(startPosition, endPosition - startPosition + 1);
				}
				
				setSelectedRange(selectionRange);
				return;
			}
		}
		
		// first check for a hit in one of our selection handles
		if (hasSelection_)
		{
			QRect selectionRect = rectEncompassingBaseToBase(selectionFirstBase_, selectionLastBase_, interiorRect, displayedRange);
			int leftEdge = selectionRect.left();
			int rightEdge = selectionRect.left() + selectionRect.width() - 1;	// -1 to be on the left edge of the right-edge pixel strip
			QRect leftSelectionBar = QRect(leftEdge - 2, selectionRect.top() - 1, 5, selectionRect.height() + 2);
			QRect leftSelectionKnob = QRect(leftEdge - (selectionKnobSizeExtension + 1), selectionRect.bottom() + (selectionKnobSize - 3), (selectionKnobSizeExtension + 1) * 2 + 1, selectionKnobSize + 2);
			QRect rightSelectionBar = QRect(rightEdge - 2, selectionRect.top() - 1, 5, selectionRect.height() + 2);
			QRect rightSelectionKnob = QRect(rightEdge - (selectionKnobSizeExtension + 1), selectionRect.bottom() + (selectionKnobSize - 3), (selectionKnobSizeExtension + 1) * 2 + 1, selectionKnobSize + 2);
			
			if (leftSelectionBar.contains(curPoint) || leftSelectionKnob.contains(curPoint))
			{
				isTracking_ = true;
				trackingXAdjust_ = (curPoint.x() - leftEdge) - 1;		// I'm not sure why the -1 is needed, but it is...
				trackingStartBase_ = selectionLastBase_;	// we're dragging the left knob, so the right knob is the tracking anchor
				trackingLastBase_ = baseForPosition(curPoint.x() - trackingXAdjust_, interiorRect, displayedRange);	// instead of selectionFirstBase, so the selection does not change at all if the mouse does not move
				
				mouseMoveEvent(p_event);	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
			else if (rightSelectionBar.contains(curPoint) || rightSelectionKnob.contains(curPoint))
			{
				isTracking_ = true;
				trackingXAdjust_ = (curPoint.x() - rightEdge);
				trackingStartBase_ = selectionFirstBase_;	// we're dragging the right knob, so the left knob is the tracking anchor
				trackingLastBase_ = baseForPosition(curPoint.x() - trackingXAdjust_, interiorRect, displayedRange);	// instead of selectionLastBase, so the selection does not change at all if the mouse does not move
				
				mouseMoveEvent(p_event);	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
		}
		
        if (contentRect.contains(curPoint))
        {
            isTracking_ = true;
            trackingStartBase_ = baseForPosition(curPoint.x(), interiorRect, displayedRange);
            trackingLastBase_ = trackingStartBase_;
            trackingXAdjust_ = 0;
            
            // We start off with no selection, and wait for the user to drag out a selection
			if (hasSelection_)
			{
				hasSelection_ = false;
				
				// Save the selection for restoring across recycles, etc.
				savedHasSelection_ = hasSelection_;
				
				update();
                emit selectedRangeChanged();
			}
        }
	}
}

// - (void)setUpMarker:(SLiMSelectionMarker **)marker atBase:(slim_position_t)selectionBase isLeft:(BOOL)isLeftMarker
// FIXME at present QtSLiM doesn't have the selection markers during tracking that SLiMgui has...

void QtSLiMChromosomeWidget::_mouseTrackEvent(QMouseEvent *p_event)
{
    QRect interiorRect = getInteriorRect();
    Species *displaySpecies = focalDisplaySpecies();
    QtSLiMRange displayedRange = getDisplayedRange(displaySpecies);
    QPoint curPoint = p_event->pos();
	
	QPoint correctedPoint = QPoint(curPoint.x() - trackingXAdjust_, curPoint.y());
	slim_position_t trackingNewBase = baseForPosition(correctedPoint.x(), interiorRect, displayedRange);
	bool selectionChanged = false;
	
	if (trackingNewBase != trackingLastBase_)
	{
		trackingLastBase_ = trackingNewBase;
		
		slim_position_t trackingLeftBase = trackingStartBase_, trackingRightBase = trackingLastBase_;
		
		if (trackingLeftBase > trackingRightBase)
		{
			trackingLeftBase = trackingLastBase_;
			trackingRightBase = trackingStartBase_;
		}
		
		if (trackingLeftBase <= static_cast<slim_position_t>(displayedRange.location))
			trackingLeftBase = static_cast<slim_position_t>(displayedRange.location);
		if (trackingRightBase > static_cast<slim_position_t>((displayedRange.location + displayedRange.length) - 1))
			trackingRightBase = static_cast<slim_position_t>((displayedRange.location + displayedRange.length) - 1);
		
		if (trackingRightBase <= trackingLeftBase + 100)
		{
			if (hasSelection_)
				selectionChanged = true;
			
			hasSelection_ = false;
			
			// Save the selection for restoring across recycles, etc.
			savedHasSelection_ = hasSelection_;
			
			//[self removeSelectionMarkers];
		}
		else
		{
			selectionChanged = true;
			hasSelection_ = true;
			selectionFirstBase_ = trackingLeftBase;
			selectionLastBase_ = trackingRightBase;
			
			// Save the selection for restoring across recycles, etc.
			savedSelectionFirstBase_ = selectionFirstBase_;
			savedSelectionLastBase_ = selectionLastBase_;
			savedHasSelection_ = hasSelection_;
			
			//[self setUpMarker:&startMarker atBase:selectionFirstBase isLeft:YES];
			//[self setUpMarker:&endMarker atBase:selectionLastBase isLeft:NO];
		}
		
		if (selectionChanged)
		{
			update();
            emit selectedRangeChanged();
		}
	}
}

void QtSLiMChromosomeWidget::mouseMoveEvent(QMouseEvent *p_event)
{
    if (selectable_ && isTracking_)
		_mouseTrackEvent(p_event);
}

void QtSLiMChromosomeWidget::mouseReleaseEvent(QMouseEvent *p_event)
{
    if (selectable_ && isTracking_)
	{
        _mouseTrackEvent(p_event);
		//[self removeSelectionMarkers];
	}
	
	isTracking_ = false;
}

void QtSLiMChromosomeWidget::contextMenuEvent(QContextMenuEvent * /* p_event */)
{
    // BCH 5/9/2022: I think now that we can have multiple chromosome views it might be best to make
    // people use the action button; a context menu running on a particular view looks view-specific,
    // but the multiple chromosome views share all their configuration state, so that would be odd.
    
    //if (!selectable_)
    //    controller_->runChromosomeContextMenuAtPoint(p_event->globalPos());
}




























