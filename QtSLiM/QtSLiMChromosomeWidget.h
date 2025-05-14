//
//  QtSLiMChromosomeWidget.h
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2025 Benjamin C. Haller.  All rights reserved.
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

#ifndef QTSLIMCHROMOSOMEWIDGET_H
#define QTSLIMCHROMOSOMEWIDGET_H

// Silence deprecated OpenGL warnings on macOS
#define GL_SILENCE_DEPRECATION

#include <QWidget>
#include <QPointer>

#ifndef SLIM_NO_OPENGL
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#endif

#include "slim_globals.h"

#include "QtSLiMWindow.h"


class QtSLiMWindow;
class QtSLiMHaplotypeManager;
class QPainter;
class QContextMenuEvent;
class QButton;


struct QtSLiMRange
{
    int64_t location, length;
    
    explicit QtSLiMRange() : location(0), length(0) {}
    explicit QtSLiMRange(int64_t p_location, int64_t p_length) : location(p_location), length(p_length) {}
};


// This is a little controller class that governs a chromosome view or views, and an associated action button
// It is used by QtSLiMWindow for the main chromosome views (overview and zoomed), and by the chromosome display
class QtSLiMChromosomeWidgetController : public QObject
{
    Q_OBJECT
    
    QtSLiMWindow *slimWindow_ = nullptr;
    
    // state used only in the chromosome display case
    QPointer<QWidget> displayWindow_ = nullptr;
    std::string focalSpeciesName_;                  // we keep the name of our focal species, since a pointer would be unsafe
    std::string focalSpeciesAvatar_;                // cached so we can display it even when the simulation is invalid
    std::string chromosomeSymbol_;                  // a chromosome symbol, or "" for "all chromosomes"
    bool needsRebuild_ = false;                     // true immediately after recycling
    
public:
    bool useScaledWidths_ = true;                   // used only by the chromosome display
    bool shouldDrawMutations_ = true;
    bool shouldDrawFixedSubstitutions_ = false;
    bool shouldDrawGenomicElements_ = false;
    bool shouldDrawRateMaps_ = false;
    
    bool displayHaplotypes_ = false;                // if false, displaying frequencies; if true, displaying haplotypes
    std::vector<slim_objectid_t> displayMuttypes_;  // if empty, display all mutation types; otherwise, display only the muttypes chosen
    
    QtSLiMChromosomeWidgetController(QtSLiMWindow *slimWindow, QWidget *displayWindow, Species *focalSpecies, std::string chromosomeSymbol);
    
    void buildChromosomeDisplay(bool resetWindowSize);
    void updateFromController(void);
    
    void runChromosomeContextMenuAtPoint(QPoint p_globalPoint);
    void actionButtonRunMenu(QtSLiMPushButton *p_actionButton);
    
    // forwards from slimWindow_; this is everything QtSLiMChromosomeWidget needs from the outside world
    bool invalidSimulation(void) { return slimWindow_->invalidSimulation(); }
    Community *community(void) { return slimWindow_->community; }
    Species *focalDisplaySpecies(void);
    void colorForGenomicElementType(GenomicElementType *elementType, slim_objectid_t elementTypeID, float *p_red, float *p_green, float *p_blue, float *p_alpha)
        { slimWindow_->colorForGenomicElementType(elementType, elementTypeID, p_red, p_green, p_blue, p_alpha); }
    QtSLiMWindow *slimWindow(void) { return slimWindow_; }
        
signals:
    void needsRedisplay(void);
};


// This is a fast macro for when all we need is the offset of a base from the left edge of interiorRect; interiorRect.origin.x is not added here!
// This is based on the same math as rectEncompassingBase:toBase:interiorRect:displayedRange:, and must be kept in synch with that method.
#define LEFT_OFFSET_OF_BASE(startBase, interiorRect, displayedRange) (static_cast<int>(floor(((startBase - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length)) * interiorRect.width())))


#ifndef SLIM_NO_OPENGL
class QtSLiMChromosomeWidget : public QOpenGLWidget, protected QOpenGLFunctions
#else
class QtSLiMChromosomeWidget : public QWidget
#endif
{
    Q_OBJECT
    
    QtSLiMChromosomeWidgetController *controller_ = nullptr;
    std::string focalSpeciesName_;                                  // we keep the name of our focal species, since a pointer would be unsafe
    std::string focalChromosomeSymbol_;                             // we keep the symbol of our focal chromosome, since a pointer would be unsafe
    
    bool isOverview_ = false;
    QtSLiMChromosomeWidget *dependentChromosomeView_ = nullptr;
    
    bool showsTicks_ = true;
    
    // Displayed range (only in a regular chromosome view)
    QtSLiMRange displayedRange_;
    
    // Selection (only in the overview)
	bool hasSelection_ = false;
	slim_position_t selectionFirstBase_ = 0, selectionLastBase_ = 0;
	
	// Selection memory – saved and restored across events like recycles
	bool savedHasSelection_ = false;
	slim_position_t savedSelectionFirstBase_ = 0, savedSelectionLastBase_ = 0;
    
    // Mouse-over display change (only in the overview)
    bool mouseInside_ = false;
    int mouseInsideCounter_ = 0;
    bool showChromosomeNumbers_ = false;            // set true after a delay when the mouse is inside
    
    // Tracking (only in the overview)
    bool isTracking_ = false;
    bool movedSufficiently_ = false;                // a movement threshold is required before dragging begins
    int initialMouseX = 0;                          // the initial x for the movement threshold
    
    QRect contentRectForTrackedChromosome_;
    slim_position_t trackingStartBase_ = 0, trackingLastBase_ = 0;
	int trackingXAdjust_ = 0;                       // to keep the cursor stuck on a knob that is click-dragged
    bool simpleClickInFocalChromosome_ = false;     // used to keep track of whether we could deselect on mouse-up
	//SLiMSelectionMarker *startMarker, *endMarker;
    
public:
    explicit QtSLiMChromosomeWidget(QWidget *p_parent = nullptr, QtSLiMChromosomeWidgetController *controller = nullptr, Species *displaySpecies = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMChromosomeWidget() override;
    
    void setController(QtSLiMChromosomeWidgetController *controller);
    void setFocalDisplaySpecies(Species *displaySpecies);
    Species *focalDisplaySpecies(void);
    void setFocalChromosome(Chromosome *chromosome);
    Chromosome *focalChromosome(void);
    
    void setDependentChromosomeView(QtSLiMChromosomeWidget *p_dependent_widget);
    
    bool hasSelection(void) { return hasSelection_; }
    QtSLiMRange getSelectedRange(Chromosome *chromosome);
    void setSelectedRange(QtSLiMRange p_selectionRange);
    void restoreLastSelection(void);
    void updateDependentView(void);
    
    QtSLiMRange getDisplayedRange(Chromosome *chromosome);
    void setDisplayedRange(QtSLiMRange p_displayedRange);
    
    bool showsTicks(void) { return showsTicks_; }
    void setShowsTicks(bool p_showTicks);
    
    void stateChanged(void);    // update when the SLiM model state changes; tosses any cached display info
    void updateAfterTick(void);
    
protected:
#ifndef SLIM_NO_OPENGL
    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;
#else
    virtual void paintEvent(QPaintEvent *event) override;
#endif
    
    QRect rectEncompassingBaseToBase(slim_position_t startBase, slim_position_t endBase, QRect interiorRect, QtSLiMRange displayedRange);
    slim_position_t baseForPosition(double position, QRect interiorRect, QtSLiMRange displayedRange);
    QRect getContentRect(void);
    
    void drawOverview(Species *displaySpecies, QPainter &painter);
    void drawFullGenome(Species *displaySpecies, QPainter &painter);
    
    void drawTicksInContentRect(QRect contentRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void overlaySelection(QRect interiorRect, QtSLiMRange displayedRange, bool drawInteriorShadow, QPainter &painter);
    void updateDisplayedMutationTypes(Species *displaySpecies);
    
    // OpenGL drawing; this is the primary drawing code
#ifndef SLIM_NO_OPENGL
    void glDrawRect(QRect contentRect, Species *displaySpecies, Chromosome *chromosome);
    void glDrawGenomicElements(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
    void glDrawFixedSubstitutions(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
    void glDrawMutations(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
    void _glDrawRateMapIntervals(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue);
    void glDrawRecombinationIntervals(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
    void glDrawMutationIntervals(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
    void glDrawRateMaps(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
#endif
    
    // Qt-based drawing, provided as a backup if OpenGL has problems on a given platform
    void qtDrawRect(QRect contentRect, Species *displaySpecies, Chromosome *chromosome, QPainter &painter);
    void qtDrawGenomicElements(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawFixedSubstitutions(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawMutations(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    void _qtDrawRateMapIntervals(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue, QPainter &painter);
    void qtDrawRecombinationIntervals(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawMutationIntervals(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawRateMaps(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    
    Chromosome *_findFocalChromosomeForTracking(QMouseEvent *p_event);
    virtual void mousePressEvent(QMouseEvent *p_event) override;
    void _mouseTrackEvent(QMouseEvent *p_event);
    virtual void mouseMoveEvent(QMouseEvent *p_event) override;
    virtual void mouseReleaseEvent(QMouseEvent *p_event) override;
    virtual void contextMenuEvent(QContextMenuEvent *p_event) override;
    
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#define QTSLIM_ENTER_EVENT  QEvent
#else
#define QTSLIM_ENTER_EVENT  QEnterEvent
#endif
    virtual void enterEvent(QTSLIM_ENTER_EVENT *event) override;
    virtual void leaveEvent(QEvent *event) override;
    
    // Our configuration is kept by the controller, since it is shared by all chromosome views for multispecies models
    // However, "overview" chromosome views are always configured the same, hard-coded here
    inline bool shouldDrawMutations(void) const { return isOverview_ ? false : controller_->shouldDrawMutations_; }
    inline bool shouldDrawFixedSubstitutions(void) const { return isOverview_ ? false : controller_->shouldDrawFixedSubstitutions_; }
    inline bool shouldDrawGenomicElements(void) const { return isOverview_ ? true : controller_->shouldDrawGenomicElements_; }
    inline bool shouldDrawRateMaps(void) const { return isOverview_ ? false : controller_->shouldDrawRateMaps_; }
    inline bool displayHaplotypes(void) const { return isOverview_ ? false : controller_->displayHaplotypes_; }
    inline std::vector<slim_objectid_t> &displayMuttypes(void) const { return controller_->displayMuttypes_; }
};

#endif // QTSLIMCHROMOSOMEWIDGET_H
















