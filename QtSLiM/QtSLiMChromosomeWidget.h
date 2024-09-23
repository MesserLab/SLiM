//
//  QtSLiMChromosomeWidget.h
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

#ifndef QTSLIMCHROMOSOMEWIDGET_H
#define QTSLIMCHROMOSOMEWIDGET_H

// Silence deprecated OpenGL warnings on macOS
#define GL_SILENCE_DEPRECATION

#include <QWidget>

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


struct QtSLiMRange
{
    int64_t location, length;
    
    explicit QtSLiMRange(int64_t p_location, int64_t p_length) : location(p_location), length(p_length) {}
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
    
    QtSLiMWindow *controller_ = nullptr;
    std::string focalSpeciesName_;                                  // we keep the name of our focal species, since a pointer would be unsafe
    std::string focalChromosomeSymbol_;                             // we keep the name of our focal species, since a pointer would be unsafe
    
    bool isOverview_ = true;
    QtSLiMChromosomeWidget *referenceChromosomeView_ = nullptr;
    
    // Selection
	bool hasSelection_ = false;
	slim_position_t selectionFirstBase_ = 0, selectionLastBase_ = 0;
	
	// Selection memory – saved and restored across events like recycles
	bool savedHasSelection_ = false;
	slim_position_t savedSelectionFirstBase_ = 0, savedSelectionLastBase_ = 0;
    
    // Mouse-over display change (only in the overview)
    bool mouseInside_ = false;
    int mouseInsideCounter_ = 0;
    bool showChromosomeNumbers_ = false;    // set true after a delay when the mouse is inside
    
    // Tracking
    bool isTracking_ = false;
    QRect contentRectForTrackedChromosome_;
    slim_position_t trackingStartBase_ = 0, trackingLastBase_ = 0;
	int trackingXAdjust_ = 0;	// to keep the cursor stuck on a knob that is click-dragged
	//SLiMSelectionMarker *startMarker, *endMarker;
    
    // Haplotype display
    int64_t *haplotype_previous_bincounts = nullptr;    // used by QtSLiMHaplotypeManager to keep the sort order stable
    QtSLiMHaplotypeManager *haplotype_mgr_ = nullptr;   // the haplotype manager constructed for the current display; cached
    
public:
    explicit QtSLiMChromosomeWidget(QWidget *p_parent = nullptr, QtSLiMWindow *controller = nullptr, Species *displaySpecies = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMChromosomeWidget() override;
    
    void setController(QtSLiMWindow *controller);
    void setFocalDisplaySpecies(Species *displaySpecies);
    Species *focalDisplaySpecies(void);
    void setFocalChromosome(Chromosome *chromosome);
    Chromosome *focalChromosome(void);
    
    void setReferenceChromosomeView(QtSLiMChromosomeWidget *p_ref_widget);
    
    bool hasSelection(void) { return hasSelection_; }
    QtSLiMRange getSelectedRange(Chromosome *chromosome);
    void setSelectedRange(QtSLiMRange p_selectionRange);
    void restoreLastSelection(void);
    
    QtSLiMRange getDisplayedRange(Chromosome *chromosome);
    
    void stateChanged(void);    // update when the SLiM model state changes; tosses any cached display info
    
signals:
    void selectedRangeChanged(void);
    
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
    QRect getInteriorRect(void);
    
    void drawOverview(Species *displaySpecies, QPainter &painter);
    void drawTicksInContentRect(QRect contentRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void overlaySelection(QRect interiorRect, QtSLiMRange displayedRange, QPainter &painter);
    void updateDisplayedMutationTypes(Species *displaySpecies);
    
    // OpenGL drawing; this is the primary drawing code
#ifndef SLIM_NO_OPENGL
    void glDrawRect(Species *displaySpecies);
    void glDrawGenomicElements(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange);
    void glDrawFixedSubstitutions(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void glDrawMutations(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void _glDrawRateMapIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue);
    void glDrawRecombinationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void glDrawMutationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void glDrawRateMaps(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
#endif
    
    // Qt-based drawing, provided as a backup if OpenGL has problems on a given platform
    void qtDrawRect(Species *displaySpecies, QPainter &painter);
    void qtDrawGenomicElements(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawFixedSubstitutions(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawMutations(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void _qtDrawRateMapIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue, QPainter &painter);
    void qtDrawRecombinationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawMutationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void qtDrawRateMaps(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    
    Chromosome *_setFocalChromosomeForTracking(QMouseEvent *p_event);
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
    inline bool shouldDrawMutations(void) const { return isOverview_ ? false : controller_->chromosome_shouldDrawMutations_; }
    inline bool shouldDrawFixedSubstitutions(void) const { return isOverview_ ? false : controller_->chromosome_shouldDrawFixedSubstitutions_; }
    inline bool shouldDrawGenomicElements(void) const { return isOverview_ ? true : controller_->chromosome_shouldDrawGenomicElements_; }
    inline bool shouldDrawRateMaps(void) const { return isOverview_ ? false : controller_->chromosome_shouldDrawRateMaps_; }
    inline bool displayHaplotypes(void) const { return isOverview_ ? false : controller_->chromosome_display_haplotypes_; }
    inline std::vector<slim_objectid_t> &displayMuttypes(void) const { return controller_->chromosome_display_muttypes_; }
};

#endif // QTSLIMCHROMOSOMEWIDGET_H
















