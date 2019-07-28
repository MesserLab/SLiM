#include "QtSLiMChromosomeWidget.h"
#include "QtSLiMWindow.h"
#include "QtSLiMExtras.h"
#include <QtDebug>
#include <QPainter>

// The Qt doc says we shouldn't need to do this, but we do seem to need to...
#if defined(__APPLE__)
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <GLKit/GLKMatrix4.h>
#else
# include <GL/gl.h>
# include <GL/glu.h>
# include <GL/glext.h>
#endif


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

#define SLIM_GL_DEFCOORDS(rect)									\
	float left = (float)rect.origin.x;							\
	float top = (float)rect.origin.y;							\
	float right = left + (float)rect.size.width;				\
	float bottom = top + (float)rect.size.height;

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
static QFont *tickFont = nullptr;


QtSLiMChromosomeWidget::QtSLiMChromosomeWidget(QWidget *parent, Qt::WindowFlags f) : QOpenGLWidget(parent, f)
{
    if (!tickFont)
    {
        tickFont = new QFont();
        tickFont->setPointSize(9);
    }
}

QtSLiMChromosomeWidget::~QtSLiMChromosomeWidget()
{
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
	GLKMatrix4 orthoMat = GLKMatrix4MakeOrtho(0.0, w, 0.0, h, -1.0f, 1.0f);
	glLoadMatrixf(orthoMat.m);
	glMatrixMode(GL_MODELVIEW);
}

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
	
	//[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
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
	
	//[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
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
        // erase the content area itself
        painter.fillRect(interiorRect, Qt::black);
		
		QtSLiMRange displayedRange = getDisplayedRange();
		
		bool splitHeight = (shouldDrawRateMaps_ && shouldDrawGenomicElements_);
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        topInteriorRect.setBottom(topInteriorRect.bottom() + remainingHeight);
		bottomInteriorRect.setHeight(remainingHeight);
        
		// draw ticks at bottom of content rect
        drawTicksInContentRect(contentRect, controller, displayedRange, painter);
		
        // draw our OpenGL content
        painter.beginNativePainting();
        glDrawRect();
        painter.endNativePainting();
        
        // frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
		QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(0.6, 1.0), painter);
        
		// overlay the selection last, since it bridges over the frame
//		if (hasSelection)
//			[self overlaySelectionInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
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
            tickLabel = QString::asprintf("%lld", static_cast<int64_t>(tickBase));
        
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
    
}

























