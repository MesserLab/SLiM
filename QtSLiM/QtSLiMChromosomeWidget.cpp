//
//  QtSLiMChromosomeWidget.cpp
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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

#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QtDebug>


// OpenGL constants
static const int kMaxGLRects = 4000;				// 4000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each

// OpenGL macros
#define SLIM_GL_PREPARE()										\
	int displayListIndex = 0;									\
	float *vertices = glArrayVertices, *colors = glArrayColors;	\
																\
	glEnableClientState(GL_VERTEX_ARRAY);						\
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);			\
	glEnableClientState(GL_COLOR_ARRAY);						\
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);

#define SLIM_GL_DEFCOORDS(rect)                                 \
	float left = static_cast<float>(rect.left());               \
	float top = static_cast<float>(rect.top());                 \
	float right = left + static_cast<float>(rect.width());      \
	float bottom = top + static_cast<float>(rect.height());

#define SLIM_GL_PUSHRECT()										\
	*(vertices++) = left;										\
	*(vertices++) = top;										\
	*(vertices++) = left;										\
	*(vertices++) = bottom;										\
	*(vertices++) = right;										\
	*(vertices++) = bottom;										\
	*(vertices++) = right;										\
	*(vertices++) = top;

#define SLIM_GL_PUSHRECT_COLORS()								\
	for (int j = 0; j < 4; ++j)									\
	{															\
		*(colors++) = colorRed;									\
		*(colors++) = colorGreen;								\
		*(colors++) = colorBlue;								\
		*(colors++) = colorAlpha;								\
	}

#define SLIM_GL_CHECKBUFFERS()									\
	displayListIndex++;											\
																\
	if (displayListIndex == kMaxGLRects)						\
	{															\
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);		\
																\
		vertices = glArrayVertices;								\
		colors = glArrayColors;									\
		displayListIndex = 0;									\
	}

#define SLIM_GL_FINISH()										\
	if (displayListIndex)										\
	glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);			\
																\
	glDisableClientState(GL_VERTEX_ARRAY);						\
	glDisableClientState(GL_COLOR_ARRAY);


static const int numberOfTicksPlusOne = 4;
static const int tickLength = 5;
static const int heightForTicks = 16;
static const int selectionKnobSizeExtension = 2;	// a 5-pixel-width knob is 2: 2 + 1 + 2, an extension on each side plus the one pixel of the bar in the middle
static const int selectionKnobSize = selectionKnobSizeExtension + selectionKnobSizeExtension + 1;


QtSLiMChromosomeWidget::QtSLiMChromosomeWidget(QWidget *parent, Qt::WindowFlags f) : QOpenGLWidget(parent, f)
{
    //[self bind:@"enabled" toObject:[[self window] windowController] withKeyPath:@"invalidSimulation" options:@{NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName}];
    
    if (!glArrayVertices)
        glArrayVertices = static_cast<float *>(malloc(kMaxVertices * 2 * sizeof(float)));		// 2 floats per vertex, kMaxVertices vertices
    
    if (!glArrayColors)
        glArrayColors = static_cast<float *>(malloc(kMaxVertices * 4 * sizeof(float)));		// 4 floats per color, kMaxVertices colors
}

QtSLiMChromosomeWidget::~QtSLiMChromosomeWidget()
{
    setReferenceChromosomeView(nullptr);
	
	//[self unbind:@"enabled"];
	
	//[self removeSelectionMarkers];
	
	if (glArrayVertices)
	{
		free(glArrayVertices);
		glArrayVertices = nullptr;
	}
	
	if (glArrayColors)
	{
		free(glArrayColors);
		glArrayColors = nullptr;
	}
    
    if (haplotype_previous_bincounts)
        free(haplotype_previous_bincounts);
}

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

// This is a fast macro for when all we need is the offset of a base from the left edge of interiorRect; interiorRect.origin.x is not added here!
// This is based on the same math as rectEncompassingBase:toBase:interiorRect:displayedRange: below, and must be kept in synch with that method.
#define LEFT_OFFSET_OF_BASE(startBase, interiorRect, displayedRange) (static_cast<int>(floor(((startBase - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length)) * interiorRect.width())))

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
            connect(referenceChromosomeView_, &QtSLiMChromosomeWidget::selectedRangeChanged, this, [this]() { update(); });
	}
}

QtSLiMRange QtSLiMChromosomeWidget::getSelectedRange(void)
{
    if (hasSelection_)
	{
		return QtSLiMRange(selectionFirstBase_, selectionLastBase_ - selectionFirstBase_ + 1);	// number of bases encompassed; a selection from x to x encompasses 1 base
	}
	else
	{
        QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
		Chromosome &chromosome = controller->sim->chromosome_;
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

const std::vector<slim_objectid_t> &QtSLiMChromosomeWidget::displayMuttypes(void)
{
    return display_muttypes_;
}

QtSLiMRange QtSLiMChromosomeWidget::getDisplayedRange(void)
{
	QtSLiMChromosomeWidget *reference = referenceChromosomeView_;
	
	if (reference)
		return reference->getSelectedRange();
	else
	{
        QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
		Chromosome &chromosome = controller->sim->chromosome_;
		slim_position_t chromosomeLastPosition = chromosome.last_position_;
		
		return QtSLiMRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
	}
}

void QtSLiMChromosomeWidget::paintGL()
{
    QPainter painter(this);
    
    painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    //painter.fillRect(rect(), Qt::red);
    
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    bool ready = isEnabled() && !controller->invalidSimulation();
    QRect contentRect = getContentRect();
	QRect interiorRect = getInteriorRect();
    
    // if the simulation is at generation 0, it is not ready
	if (ready)
		if (controller->sim->generation_ == 0)
			ready = false;
	
    if (ready)
    {
        // erase the content area itself; done in glDrawRect() now
        //painter.fillRect(interiorRect, Qt::black);
		
		QtSLiMRange displayedRange = getDisplayedRange();
        
		// draw ticks at bottom of content rect
        drawTicksInContentRect(contentRect, controller, displayedRange, painter);
		
        // draw our OpenGL content
        painter.beginNativePainting();
        glDrawRect();
        painter.endNativePainting();
        
        // frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
		QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(0.6, 1.0), painter);
        
		// overlay the selection last, since it bridges over the frame
		if (hasSelection_)
			overlaySelection(interiorRect, controller, displayedRange, painter);
    }
    else
    {
        // erase the content area itself
        painter.fillRect(interiorRect, QtSLiMColorWithWhite(0.88, 1.0));
        
        // frame
        QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(0.6, 1.0), painter);
    }
}

void QtSLiMChromosomeWidget::drawTicksInContentRect(QRect contentRect, __attribute__((__unused__)) QtSLiMWindow *controller, QtSLiMRange displayedRange, QPainter &painter)
{
	QRect interiorRect = getInteriorRect();
	int64_t lastTickIndex = numberOfTicksPlusOne;
	
	// Display fewer ticks when we are displaying a very small number of positions
	lastTickIndex = std::min(lastTickIndex, (displayedRange.length + 1) / 3);
	
	double tickIndexDivisor = ((lastTickIndex == 0) ? 1.0 : static_cast<double>(lastTickIndex));		// avoid a divide by zero when we are displaying a single site
	
    // BCH 9/23/2020: Note that this QFont usage causes a crash on quit in certain circumstances (which we now avoid).
    // See https://bugreports.qt.io/browse/QTBUG-86875 and the related bug QTBUG-86874.  Although this crash never
    // occurs now, I note it here in case the bug crops up in a different context.
    // BCH 9/24/2020: Note that QTBUG-86875 is fixed in 5.15.1, but we don't want to require that.
    static QFont *tickFont = nullptr;
    
    if (!tickFont)
    {
        tickFont = new QFont();
#ifdef __APPLE__
        tickFont->setPointSize(9);
#else
        tickFont->setPointSize(7);
#endif
    }
    painter.setFont(*tickFont);
    
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
		
        painter.fillRect(tickRect, QColor(127, 127, 127, 255));
		
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
}

void QtSLiMChromosomeWidget::glDrawRect(void)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    bool ready = isEnabled() && !controller->invalidSimulation();
	QRect interiorRect = getInteriorRect();
    
    // if the simulation is at generation 0, it is not ready
	if (ready)
		if (controller->sim->generation_ == 0)
			ready = false;
	
    if (ready)
    {
        // erase the content area itself
        glColor3f(0.0f, 0.0f, 0.0f);
		glRecti(interiorRect.left(), interiorRect.top(), interiorRect.left() + interiorRect.width(), interiorRect.top() + interiorRect.height());
		QtSLiMRange displayedRange = getDisplayedRange();
		
		bool splitHeight = (shouldDrawRateMaps_ && shouldDrawGenomicElements_);
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
        
        // draw recombination intervals in interior
		if (shouldDrawRateMaps_)
			glDrawRateMaps(splitHeight ? topInteriorRect : interiorRect, controller, displayedRange);
		
		// draw genomic elements in interior
		if (shouldDrawGenomicElements_)
			glDrawGenomicElements(splitHeight ? bottomInteriorRect : interiorRect, controller, displayedRange);
		
		// figure out which mutation types we're displaying
		if (shouldDrawFixedSubstitutions_ || shouldDrawMutations_)
			updateDisplayedMutationTypes();
		
		// draw fixed substitutions in interior
		if (shouldDrawFixedSubstitutions_)
			glDrawFixedSubstitutions(interiorRect, controller, displayedRange);
		
		// draw mutations in interior
		if (shouldDrawMutations_)
		{
			if (display_haplotypes_)
			{
				// display mutations as a haplotype plot, courtesy of SLiMHaplotypeManager; we use kSLiMHaplotypeClusterNearestNeighbor and
				// kSLiMHaplotypeClusterNoOptimization because they're fast, and NN might also provide a bit more run-to-run continuity
				size_t interiorHeight = static_cast<size_t>(interiorRect.height());	// one sample per available pixel line, for simplicity and speed; 47, in the current UI layout
				QtSLiMHaplotypeManager haplotypeManager(nullptr, QtSLiMHaplotypeManager::ClusterNearestNeighbor, QtSLiMHaplotypeManager::ClusterNoOptimization, controller, interiorHeight, false);
				
				haplotypeManager.glDrawHaplotypes(interiorRect, false, false, false, &haplotype_previous_bincounts);
				
				// it's a little bit odd to throw away haplotypeManager here; if the user drag-resizes the window, we do a new display each
				// time, with a new sample, and so the haplotype display changes with every pixel resize change; we could keep this...?
			}
			else
			{
				// display mutations as a frequency plot; this is the standard display mode
                glDrawMutations(interiorRect, controller, displayedRange);
			}
		}
    }
    else
    {
        // erase the content area itself
		glColor3f(0.88f, 0.88f, 0.88f);
        glRecti(0, 0, interiorRect.width(), interiorRect.height());
    }
}

void QtSLiMChromosomeWidget::glDrawGenomicElements(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange)
{
    Chromosome &chromosome = controller->sim->chromosome_;
	int previousIntervalLeftEdge = -10000;
	
	SLIM_GL_PREPARE();
	
	for (GenomicElement *genomicElement : chromosome.GenomicElements())
	{
		slim_position_t startPosition = genomicElement->start_position_;
		slim_position_t endPosition = genomicElement->end_position_;
		QRect elementRect = rectEncompassingBaseToBase(startPosition, endPosition, interiorRect, displayedRange);
		bool widthOne = (elementRect.width() == 1);
		
		// We want to avoid overdrawing width-one intervals, which are important but small, so if the previous interval was width-one,
		// and we are not, and we are about to overdraw it, then we scoot our left edge over one pixel to leave it alone.
		if (!widthOne && (elementRect.left() == previousIntervalLeftEdge))
            elementRect.adjust(1, 0, 0, 0);
		
		// draw only the visible part, if any
        elementRect = elementRect.intersected(interiorRect);
		
		if (!elementRect.isEmpty())
		{
			GenomicElementType *geType = genomicElement->genomic_element_type_ptr_;
			float colorRed, colorGreen, colorBlue, colorAlpha;
			
			if (!geType->color_.empty())
			{
				colorRed = geType->color_red_;
				colorGreen = geType->color_green_;
				colorBlue = geType->color_blue_;
				colorAlpha = 1.0;
			}
			else
			{
				slim_objectid_t elementTypeID = geType->genomic_element_type_id_;
                
				controller->colorForGenomicElementType(geType, elementTypeID, &colorRed, &colorGreen, &colorBlue, &colorAlpha);
			}
			
			SLIM_GL_DEFCOORDS(elementRect);
			SLIM_GL_PUSHRECT();
			SLIM_GL_PUSHRECT_COLORS();
			SLIM_GL_CHECKBUFFERS();
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = elementRect.left();
			else
				previousIntervalLeftEdge = -10000;
		}
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::updateDisplayedMutationTypes(void)
{
	// We use a flag in MutationType to indicate whether we're drawing that type or not; we update those flags here,
	// before every drawing of mutations, from the vector of mutation type IDs that we keep internally
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	
	if (controller)
	{
		SLiMSim *sim = controller->sim;
		
		if (sim)
		{
			std::map<slim_objectid_t,MutationType*> &muttypes = sim->mutation_types_;
			
			for (auto muttype_iter : muttypes)
			{
				MutationType *muttype = muttype_iter.second;
				
				if (display_muttypes_.size())
				{
					slim_objectid_t muttype_id = muttype->mutation_type_id_;
					
					muttype->mutation_type_displayed_ = (std::find(display_muttypes_.begin(), display_muttypes_.end(), muttype_id) != display_muttypes_.end());
				}
				else
				{
					muttype->mutation_type_displayed_ = true;
				}
			}
		}
	}
}

void QtSLiMChromosomeWidget::glDrawMutations(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange)
{
	double scalingFactor = 0.8; // controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.gui_total_genome_count_;				// this includes only genomes in the selected subpopulations
    int registry_size;
    const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// Set up to draw rects
	float colorRed = 0.0f, colorGreen = 0.0f, colorBlue = 0.0f, colorAlpha = 1.0;
	
	SLIM_GL_PREPARE();
	
	if ((registry_size < 1000) || (displayedRange.length < interiorRect.width()))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			const Mutation *mutation = mut_block_ptr + registry[registry_index];
			const MutationType *mutType = mutation->mutation_type_ptr_;
			
			if (mutType->mutation_type_displayed_)
			{
				slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
				slim_position_t mutationPosition = mutation->position_;
				QRect mutationTickRect = rectEncompassingBaseToBase(mutationPosition, mutationPosition, interiorRect, displayedRange);
				
				if (!mutType->color_.empty())
				{
					colorRed = mutType->color_red_;
					colorGreen = mutType->color_green_;
					colorBlue = mutType->color_blue_;
				}
				else
				{
					RGBForSelectionCoeff(static_cast<double>(mutation->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
				}
				
                int height_adjust = mutationTickRect.height() - static_cast<int>(ceil((mutationRefCount / totalGenomeCount) * interiorRect.height()));
                mutationTickRect.setTop(mutationTickRect.top() + height_adjust);
                
				SLIM_GL_DEFCOORDS(mutationTickRect);
				SLIM_GL_PUSHRECT();
				SLIM_GL_PUSHRECT_COLORS();
				SLIM_GL_CHECKBUFFERS();
			}
		}
	}
	else
	{
		// We have a lot of mutations, so let's try to be smarter.  It's hard to be smarter.  The overhead from allocating the NSColors and such
		// is pretty negligible; practially all the time is spent in NSRectFill().  Unfortunately, NSRectFillListWithColors() provides basically
		// no speedup; Apple doesn't appear to have optimized it.  So, here's what I came up with.  For each mutation type that uses a fixed DFE,
		// and thus a fixed color, we can do a radix sort of mutations into bins corresponding to each pixel in our displayed image.  Then we
		// can draw each bin just once, making one bar for the highest bar in that bin.  Mutations from non-fixed DFEs, and mutations which have
		// had their selection coefficient changed, will be drawn at the end in the usual (slow) way.
		int displayPixelWidth = interiorRect.width();
		int16_t *heightBuffer = static_cast<int16_t *>(malloc(static_cast<size_t>(displayPixelWidth) * sizeof(int16_t)));
		bool *mutationsPlotted = static_cast<bool *>(calloc(static_cast<size_t>(registry_size), sizeof(bool)));	// faster than using gui_scratch_reference_count_ because of cache locality
		int64_t remainingMutations = registry_size;
		
		// First zero out the scratch refcount, which we use to track which mutations we have drawn already
		//for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
		//	mutations[mutIndex]->gui_scratch_reference_count_ = 0;
		
		// Then loop through the declared mutation types
		std::map<slim_objectid_t,MutationType*> &mut_types = controller->sim->mutation_types_;
		bool draw_muttypes_sequentially = (mut_types.size() <= 20);	// with a lot of mutation types, the algorithm below becomes very inefficient
		
		for (auto mutationTypeIter : mut_types)
		{
			MutationType *mut_type = mutationTypeIter.second;
			
			if (mut_type->mutation_type_displayed_)
			{
				if (draw_muttypes_sequentially)
				{
					bool mut_type_fixed_color = !mut_type->color_.empty();
					
					// We optimize fixed-DFE mutation types only, and those using a fixed color set by the user
					if ((mut_type->dfe_type_ == DFEType::kFixed) || mut_type_fixed_color)
					{
						slim_selcoeff_t mut_type_selcoeff = (mut_type_fixed_color ? 0.0 : static_cast<slim_selcoeff_t>(mut_type->dfe_parameters_[0]));
						
						EIDOS_BZERO(heightBuffer, static_cast<size_t>(displayPixelWidth) * sizeof(int16_t));
						
						// Scan through the mutation list for mutations of this type with the right selcoeff
						for (int registry_index = 0; registry_index < registry_size; ++registry_index)
						{
							const Mutation *mutation = mut_block_ptr + registry[registry_index];
							
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                            // We do want to do an exact floating-point equality compare here; we want to see whether the mutation's selcoeff is unmodified from the fixed DFE
							if ((mutation->mutation_type_ptr_ == mut_type) && (mut_type_fixed_color || (mutation->selection_coeff_ == mut_type_selcoeff)))
#pragma clang diagnostic pop
#pragma GCC diagnostic pop
							{
								slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// includes only refs from the selected subpopulations
								slim_position_t mutationPosition = mutation->position_;
								//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
								//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
								int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
								int16_t height = static_cast<int16_t>(ceil((mutationRefCount / totalGenomeCount) * interiorRect.height()));
								
								if ((xPos >= 0) && (xPos < displayPixelWidth))
									if (height > heightBuffer[xPos])
										heightBuffer[xPos] = height;
								
								// tally this mutation as handled
								//mutation->gui_scratch_reference_count_ = 1;
								mutationsPlotted[registry_index] = true;
								--remainingMutations;
							}
						}
						
						// Now draw all of the mutations we found, by looping through our radix bins
						if (mut_type_fixed_color)
						{
							colorRed = mut_type->color_red_;
							colorGreen = mut_type->color_green_;
							colorBlue = mut_type->color_blue_;
						}
						else
						{
							RGBForSelectionCoeff(static_cast<double>(mut_type_selcoeff), &colorRed, &colorGreen, &colorBlue, scalingFactor);
						}
						
						for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
						{
							int height = heightBuffer[binIndex];
							
							if (height)
							{
								QRect mutationTickRect(interiorRect.x() + binIndex, interiorRect.y(), 1, interiorRect.height());
                                mutationTickRect.setTop(mutationTickRect.top() + interiorRect.height() - height);
								
								SLIM_GL_DEFCOORDS(mutationTickRect);
								SLIM_GL_PUSHRECT();
								SLIM_GL_PUSHRECT_COLORS();
								SLIM_GL_CHECKBUFFERS();
							}
						}
					}
				}
			}
			else
			{
				// We're not displaying this mutation type, so we need to mark off all the mutations belonging to it as handled
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					const Mutation *mutation = mut_block_ptr + registry[registry_index];
					
					if (mutation->mutation_type_ptr_ == mut_type)
					{
						// tally this mutation as handled
						//mutation->gui_scratch_reference_count_ = 1;
						mutationsPlotted[registry_index] = true;
						--remainingMutations;
					}
				}
			}
		}
		
		// Draw any undrawn mutations on top; these are guaranteed not to use a fixed color set by the user, since those are all handled above
		if (remainingMutations)
		{
			if (remainingMutations < 1000)
			{
				// Plot the remainder by brute force, since there are not that many
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						const Mutation *mutation = mut_block_ptr + registry[registry_index];
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
                        QRect mutationTickRect = rectEncompassingBaseToBase(mutationPosition, mutationPosition, interiorRect, displayedRange);
                        int height_adjust = mutationTickRect.height() - static_cast<int>(ceil((mutationRefCount / totalGenomeCount) * interiorRect.height()));
						
                        mutationTickRect.setTop(mutationTickRect.top() + height_adjust);
						RGBForSelectionCoeff(static_cast<double>(mutation->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						SLIM_GL_DEFCOORDS(mutationTickRect);
						SLIM_GL_PUSHRECT();
						SLIM_GL_PUSHRECT_COLORS();
						SLIM_GL_CHECKBUFFERS();
					}
				}
			}
			else
			{
				// OK, we have a lot of mutations left to draw.  Here we will again use the radix sort trick, to keep track of only the tallest bar in each column
				MutationIndex *mutationBuffer = static_cast<MutationIndex *>(calloc(static_cast<size_t>(displayPixelWidth),  sizeof(MutationIndex)));
				
				EIDOS_BZERO(heightBuffer, static_cast<size_t>(displayPixelWidth) * sizeof(int16_t));
				
				// Find the tallest bar in each column
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						MutationIndex mutationBlockIndex = registry[registry_index];
						const Mutation *mutation = mut_block_ptr + mutationBlockIndex;
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
						int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
						int16_t height = static_cast<int16_t>(ceil((mutationRefCount / totalGenomeCount) * interiorRect.height()));
						
						if ((xPos >= 0) && (xPos < displayPixelWidth))
						{
							if (height > heightBuffer[xPos])
							{
								heightBuffer[xPos] = height;
								mutationBuffer[xPos] = mutationBlockIndex;
							}
						}
					}
				}
				
				// Now plot the bars
				for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
				{
					int height = heightBuffer[binIndex];
					
					if (height)
					{
                        QRect mutationTickRect(interiorRect.x() + binIndex, interiorRect.y(), 1, interiorRect.height());
                        mutationTickRect.setTop(mutationTickRect.top() + interiorRect.height() - height);
                        
						const Mutation *mutation = mut_block_ptr + mutationBuffer[binIndex];
						
						RGBForSelectionCoeff(static_cast<double>(mutation->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						SLIM_GL_DEFCOORDS(mutationTickRect);
						SLIM_GL_PUSHRECT();
						SLIM_GL_PUSHRECT_COLORS();
						SLIM_GL_CHECKBUFFERS();
					}
				}
				
				free(mutationBuffer);
			}
		}
		
		free(heightBuffer);
		free(mutationsPlotted);
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::glDrawFixedSubstitutions(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange)
{
    double scalingFactor = 0.8; // controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
    Chromosome &chromosome = sim->chromosome_;
	bool chromosomeHasDefaultColor = !chromosome.color_sub_.empty();
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	// Set up to draw rects
	float colorRed = 0.2f, colorGreen = 0.2f, colorBlue = 1.0f, colorAlpha = 1.0;
	
	if (chromosomeHasDefaultColor)
	{
		colorRed = chromosome.color_sub_red_;
		colorGreen = chromosome.color_sub_green_;
		colorBlue = chromosome.color_sub_blue_;
	}
	
	SLIM_GL_PREPARE();
	
	if ((substitutions.size() < 1000) || (displayedRange.length < interiorRect.width()))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				QRect substitutionTickRect = rectEncompassingBaseToBase(substitutionPosition, substitutionPosition, interiorRect, displayedRange);
				
				if (!shouldDrawMutations_ || !chromosomeHasDefaultColor)
				{
					// If we're drawing mutations as well, then substitutions just get colored blue (set above), to contrast
					// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						colorRed = mutType->color_sub_red_;
						colorGreen = mutType->color_sub_green_;
						colorBlue = mutType->color_sub_blue_;
					}
					else
					{
						RGBForSelectionCoeff(static_cast<double>(substitution->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
				}
				
				SLIM_GL_DEFCOORDS(substitutionTickRect);
				SLIM_GL_PUSHRECT();
				SLIM_GL_PUSHRECT_COLORS();
				SLIM_GL_CHECKBUFFERS();
			}
		}
	}
	else
	{
		// We have a lot of substitutions, so do a radix sort, as we do in drawMutationsInInteriorRect: below.
		int displayPixelWidth = interiorRect.width();
		const Substitution **subBuffer = static_cast<const Substitution **>(calloc(static_cast<size_t>(displayPixelWidth), sizeof(Substitution *)));
		
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				double startFraction = (substitutionPosition - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length);
				int xPos = static_cast<int>(floor(startFraction * interiorRect.width()));
				
				if ((xPos >= 0) && (xPos < displayPixelWidth))
					subBuffer[xPos] = substitution;
			}
		}
		
		if (shouldDrawMutations_ && chromosomeHasDefaultColor)
		{
			// If we're drawing mutations as well, then substitutions just get colored blue, to contrast
			QRect mutationTickRect = interiorRect;
            
			for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
			{
				const Substitution *substitution = subBuffer[binIndex];
				
				if (substitution)
				{
                    mutationTickRect.setX(interiorRect.x() + binIndex);
                    mutationTickRect.setWidth(1);
                    
					// consolidate adjacent lines together, since they are all the same color
					while ((binIndex + 1 < displayPixelWidth) && subBuffer[binIndex + 1])
					{
                        mutationTickRect.setWidth(mutationTickRect.width() + 1);
						binIndex++;
					}
					
					SLIM_GL_DEFCOORDS(mutationTickRect);
					SLIM_GL_PUSHRECT();
					SLIM_GL_PUSHRECT_COLORS();
					SLIM_GL_CHECKBUFFERS();
				}
			}
		}
		else
		{
			// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
            QRect mutationTickRect = interiorRect;
			
			for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
			{
				const Substitution *substitution = subBuffer[binIndex];
				
				if (substitution)
				{
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						colorRed = mutType->color_sub_red_;
						colorGreen = mutType->color_sub_green_;
						colorBlue = mutType->color_sub_blue_;
					}
					else
					{
						RGBForSelectionCoeff(static_cast<double>(substitution->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
					
                    mutationTickRect.setX(interiorRect.x() + binIndex);
                    mutationTickRect.setWidth(1);
					SLIM_GL_DEFCOORDS(mutationTickRect);
					SLIM_GL_PUSHRECT();
					SLIM_GL_PUSHRECT_COLORS();
					SLIM_GL_CHECKBUFFERS();
				}
			}
		}
		
		free(subBuffer);
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::_glDrawRateMapIntervals(QRect &interiorRect, __attribute__((__unused__)) QtSLiMWindow *controller, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue)
{
	size_t recombinationIntervalCount = ends.size();
	slim_position_t intervalStartPosition = 0;
	int previousIntervalLeftEdge = -10000;
	
	SLIM_GL_PREPARE();
	
	for (size_t interval = 0; interval < recombinationIntervalCount; ++interval)
	{
		slim_position_t intervalEndPosition = ends[interval];
		double intervalRate = rates[interval];
		QRect intervalRect = rectEncompassingBaseToBase(intervalStartPosition, intervalEndPosition, interiorRect, displayedRange);
		bool widthOne = (intervalRect.width() == 1);
		
		// We want to avoid overdrawing width-one intervals, which are important but small, so if the previous interval was width-one,
		// and we are not, and we are about to overdraw it, then we scoot our left edge over one pixel to leave it alone.
		if (!widthOne && (intervalRect.left() == previousIntervalLeftEdge))
            intervalRect.adjust(1, 0, 0, 0);
		
		// draw only the visible part, if any
        intervalRect = intervalRect.intersected(interiorRect);
		
        if (!intervalRect.isEmpty())
		{
			// color according to how "hot" the region is
			double r, g, b, a;
			
			if (intervalRate == 0.0)
			{
				// a recombination or mutation rate of 0.0 comes out as black, whereas the lowest brightness below is 0.5; we want to distinguish this
				r = g = b = 0.0;
				a = 1.0;
			}
			else
			{
				// the formula below scales 1e-6 to 1.0 and 1e-9 to 0.0; values outside that range clip, but we want there to be
				// reasonable contrast within the range of values commonly used, so we don't want to make the range too wide
				double lightness, brightness = 1.0, saturation = 1.0;
				
				lightness = (log10(intervalRate) + 9.0) / 3.0;
				lightness = std::max(lightness, 0.0);
				lightness = std::min(lightness, 1.0);
				
				if (lightness >= 0.5)	saturation = 1.0 - ((lightness - 0.5) * 2.0);	// goes from 1.0 at lightness 0.5 to 0.0 at lightness 1.0
				else					brightness = 0.5 + lightness;					// goes from 1.0 at lightness 0.5 to 0.5 at lightness 0.0
				
                QColor intervalColor = QtSLiMColorWithHSV(hue, saturation, brightness, 1.0);
				intervalColor.getRgbF(&r, &g, &b, &a);
			}
			
			float colorRed = static_cast<float>(r), colorGreen = static_cast<float>(g), colorBlue = static_cast<float>(b), colorAlpha = static_cast<float>(a);
			
			SLIM_GL_DEFCOORDS(intervalRect);
			SLIM_GL_PUSHRECT();
			SLIM_GL_PUSHRECT_COLORS();
			SLIM_GL_CHECKBUFFERS();
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = intervalRect.left();
			else
				previousIntervalLeftEdge = -10000;
		}
		
		// the next interval starts at the next base after this one ended
		intervalStartPosition = intervalEndPosition + 1;
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::glDrawRecombinationIntervals(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange)
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	if (chromosome.single_recombination_map_)
	{
		_glDrawRateMapIntervals(interiorRect, controller, displayedRange, chromosome.recombination_end_positions_H_, chromosome.recombination_rates_H_, 0.65);
	}
	else
	{
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
		
		_glDrawRateMapIntervals(topInteriorRect, controller, displayedRange, chromosome.recombination_end_positions_M_, chromosome.recombination_rates_M_, 0.65);
		_glDrawRateMapIntervals(bottomInteriorRect, controller, displayedRange, chromosome.recombination_end_positions_F_, chromosome.recombination_rates_F_, 0.65);
	}
}

void QtSLiMChromosomeWidget::glDrawMutationIntervals(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange)
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	if (chromosome.single_mutation_map_)
	{
		_glDrawRateMapIntervals(interiorRect, controller, displayedRange, chromosome.mutation_end_positions_H_, chromosome.mutation_rates_H_, 0.75);
	}
	else
	{
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
		
		_glDrawRateMapIntervals(topInteriorRect, controller, displayedRange, chromosome.mutation_end_positions_M_, chromosome.mutation_rates_M_, 0.75);
		_glDrawRateMapIntervals(bottomInteriorRect, controller, displayedRange, chromosome.mutation_end_positions_F_, chromosome.mutation_rates_F_, 0.75);
	}
}

void QtSLiMChromosomeWidget::glDrawRateMaps(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange)
{
	Chromosome &chromosome = controller->sim->chromosome_;
	bool recombinationWorthShowing = false;
	bool mutationWorthShowing = false;
	
	if (chromosome.single_mutation_map_)
		mutationWorthShowing = (chromosome.mutation_end_positions_H_.size() > 1);
	else
		mutationWorthShowing = ((chromosome.mutation_end_positions_M_.size() > 1) || (chromosome.mutation_end_positions_F_.size() > 1));
	
	if (chromosome.single_recombination_map_)
		recombinationWorthShowing = (chromosome.recombination_end_positions_H_.size() > 1);
	else
		recombinationWorthShowing = ((chromosome.recombination_end_positions_M_.size() > 1) || (chromosome.recombination_end_positions_F_.size() > 1));
	
	// If neither map is worth showing, we show just the recombination map, to mirror the behavior of 2.4 and earlier
	if ((!mutationWorthShowing && !recombinationWorthShowing) || (!mutationWorthShowing && recombinationWorthShowing))
	{
		glDrawRecombinationIntervals(interiorRect, controller, displayedRange);
	}
	else if (mutationWorthShowing && !recombinationWorthShowing)
	{
		glDrawMutationIntervals(interiorRect, controller, displayedRange);
	}
	else	// mutationWorthShowing && recombinationWorthShowing
	{
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
        
		glDrawRecombinationIntervals(topInteriorRect, controller, displayedRange);
		glDrawMutationIntervals(bottomInteriorRect, controller, displayedRange);
	}
}

void QtSLiMChromosomeWidget::overlaySelection(QRect interiorRect, QtSLiMWindow * /*controller*/, QtSLiMRange displayedRange, QPainter &painter)
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

void QtSLiMChromosomeWidget::mousePressEvent(QMouseEvent *event)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	bool ready = (selectable_ && isEnabled() && !controller->invalidSimulation());
	
	// if the simulation is at generation 0, it is not ready
	if (ready)
		if (controller->sim->generation_ == 0)
			ready = false;
	
	if (ready)
	{
		QRect contentRect = getContentRect();
		QRect interiorRect = getInteriorRect();
		QtSLiMRange displayedRange = getDisplayedRange();
        QPoint curPoint = event->pos();
		
		// Option-clicks just set the selection to the clicked genomic element, no questions asked
        if (event->modifiers() & Qt::AltModifier)
		{
            if (contentRect.contains(curPoint))
			{
				slim_position_t clickedBase = baseForPosition(curPoint.x(), interiorRect, displayedRange);
				QtSLiMRange selectionRange = QtSLiMRange(0, 0);
				Chromosome &chromosome = controller->sim->chromosome_;
				
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
				
				mouseMoveEvent(event);	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
			else if (rightSelectionBar.contains(curPoint) || rightSelectionKnob.contains(curPoint))
			{
				isTracking_ = true;
				trackingXAdjust_ = (curPoint.x() - rightEdge);
				trackingStartBase_ = selectionFirstBase_;	// we're dragging the right knob, so the left knob is the tracking anchor
				trackingLastBase_ = baseForPosition(curPoint.x() - trackingXAdjust_, interiorRect, displayedRange);	// instead of selectionLastBase, so the selection does not change at all if the mouse does not move
				
				mouseMoveEvent(event);	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
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

void QtSLiMChromosomeWidget::_mouseTrackEvent(QMouseEvent *event)
{
    QRect interiorRect = getInteriorRect();
    QtSLiMRange displayedRange = getDisplayedRange();
    QPoint curPoint = event->pos();
	
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

void QtSLiMChromosomeWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (selectable_ && isTracking_)
		_mouseTrackEvent(event);
}

void QtSLiMChromosomeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (selectable_ && isTracking_)
	{
        _mouseTrackEvent(event);
		//[self removeSelectionMarkers];
	}
	
	isTracking_ = false;
}

void QtSLiMChromosomeWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    
    if (!controller->invalidSimulation())// && !isSelectable() && enabled())
	{
		SLiMSim *sim = controller->sim;
		
		if (sim)
		{
			std::map<slim_objectid_t,MutationType*> &muttypes = sim->mutation_types_;
			
			if (muttypes.size() > 0)
			{
                QMenu contextMenu("chromosome_menu", this);
                
                QAction *displayFrequencies = contextMenu.addAction("Display Frequencies");
                displayFrequencies->setCheckable(true);
                displayFrequencies->setChecked(!display_haplotypes_);
                
                QAction *displayHaplotypes = contextMenu.addAction("Display Haplotypes");
                displayHaplotypes->setCheckable(true);
                displayHaplotypes->setChecked(display_haplotypes_);
                
                QActionGroup *displayGroup = new QActionGroup(this);    // On Linux this provides a radio-button-group appearance
                displayGroup->addAction(displayFrequencies);
                displayGroup->addAction(displayHaplotypes);
                
                contextMenu.addSeparator();
                
                QAction *displayAllMutations = contextMenu.addAction("Display All Mutations");
                displayAllMutations->setCheckable(true);
                displayAllMutations->setChecked(display_muttypes_.size() == 0);
                
                // Make a sorted list of all mutation types we know – those that exist, and those that used to exist that we are displaying
				std::vector<slim_objectid_t> all_muttypes;
				
				for (auto muttype_iter : muttypes)
				{
					MutationType *muttype = muttype_iter.second;
					slim_objectid_t muttype_id = muttype->mutation_type_id_;
					
					all_muttypes.push_back(muttype_id);
				}
				
				all_muttypes.insert(all_muttypes.end(), display_muttypes_.begin(), display_muttypes_.end());
                
                // Avoid building a huge menu, which will hang the app
				if (all_muttypes.size() <= 500)
				{
					std::sort(all_muttypes.begin(), all_muttypes.end());
					all_muttypes.resize(static_cast<size_t>(std::distance(all_muttypes.begin(), std::unique(all_muttypes.begin(), all_muttypes.end()))));
					
					// Then add menu items for each of those muttypes
					for (slim_objectid_t muttype_id : all_muttypes)
					{
                        QString menuItemTitle = QString("Display m%1").arg(muttype_id);
                        QAction *mutationAction = contextMenu.addAction(menuItemTitle);
                        
                        mutationAction->setData(muttype_id);
                        mutationAction->setCheckable(true);
                        
						if (std::find(display_muttypes_.begin(), display_muttypes_.end(), muttype_id) != display_muttypes_.end())
							mutationAction->setChecked(true);
					}
				}
                
                contextMenu.addSeparator();
                
                QAction *selectNonneutralMutations = contextMenu.addAction("Select Non-Neutral MutationTypes");
                
                // Run the context menu synchronously
                QAction *action = contextMenu.exec(event->globalPos());
                
                // Act upon the chosen action; we just do it right here instead of dealing with slots
                if (action)
                {
                    if (action == displayFrequencies)
                        display_haplotypes_ = false;
                    else if (action == displayHaplotypes)
                        display_haplotypes_ = true;
                    else if (action == displayAllMutations)
                        display_muttypes_.clear();
                    else if (action == selectNonneutralMutations)
                    {
                        // - (IBAction)filterNonNeutral:(id)sender
                        display_muttypes_.clear();
                        
                        for (auto muttype_iter : muttypes)
                        {
                            MutationType *muttype = muttype_iter.second;
                            slim_objectid_t muttype_id = muttype->mutation_type_id_;
                            
                            if ((muttype->dfe_type_ != DFEType::kFixed) || (muttype->dfe_parameters_[0] != 0.0))
                                display_muttypes_.push_back(muttype_id);
                        }
                    }
                    else
                    {
                        // - (IBAction)filterMutations:(id)sender
                        slim_objectid_t muttype_id = action->data().toInt();
                        auto present_iter = std::find(display_muttypes_.begin(), display_muttypes_.end(), muttype_id);
                        
                        if (present_iter == display_muttypes_.end())
                        {
                            // this mut-type is not being displayed, so add it to our display list
                            display_muttypes_.push_back(muttype_id);
                        }
                        else
                        {
                            // this mut-type is being displayed, so remove it from our display list
                            display_muttypes_.erase(present_iter);
                        }
                    }
                    
                    update();
                }
            }
        }
    }
}




























