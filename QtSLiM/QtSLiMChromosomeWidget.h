//
//  QtSLiMChromosomeWidget.h
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
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
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

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

class QtSLiMChromosomeWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT    
    
    QtSLiMWindow *controller_ = nullptr;
    std::string focalSpeciesName_;                                  // we keep the name of our focal species, since a pointer would be unsafe
    
    bool selectable_ = false;
    QtSLiMChromosomeWidget *referenceChromosomeView_ = nullptr;
    
    // Selection
	bool hasSelection_ = false;
	slim_position_t selectionFirstBase_ = 0, selectionLastBase_ = 0;
	
	// Selection memory – saved and restored across events like recycles
	bool savedHasSelection_ = false;
	slim_position_t savedSelectionFirstBase_ = 0, savedSelectionLastBase_ = 0;
	
    // Tracking
	bool isTracking_ = false;
	slim_position_t trackingStartBase_ = 0, trackingLastBase_ = 0;
	int trackingXAdjust_ = 0;	// to keep the cursor stuck on a knob that is click-dragged
	//SLiMSelectionMarker *startMarker, *endMarker;
    
    // OpenGL buffers
	float *glArrayVertices = nullptr;
	float *glArrayColors = nullptr;
    
    // Haplotype display
    int64_t *haplotype_previous_bincounts = nullptr;    // used by QtSLiMHaplotypeManager to keep the sort order stable
    QtSLiMHaplotypeManager *haplotype_mgr_ = nullptr;   // the haplotype manager constructed for the current display; cached
    
public:
    explicit QtSLiMChromosomeWidget(QWidget *p_parent = nullptr, QtSLiMWindow *controller = nullptr, Species *displaySpecies = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMChromosomeWidget() override;
    
    void setController(QtSLiMWindow *controller);
    void setFocalDisplaySpecies(Species *displaySpecies);
    Species *focalDisplaySpecies(void);
    
    inline void setSelectable(bool p_flag) { selectable_ = p_flag; }
    void setReferenceChromosomeView(QtSLiMChromosomeWidget *p_ref_widget);
    
    bool hasSelection(void) { return hasSelection_; }
    QtSLiMRange getSelectedRange(Species *displaySpecies);
    void setSelectedRange(QtSLiMRange p_selectionRange);
    void restoreLastSelection(void);
    
    QtSLiMRange getDisplayedRange(Species *displaySpecies);
    
    void stateChanged(void);    // update when the SLiM model state changes; tosses any cached display info
    
signals:
    void selectedRangeChanged(void);
    
protected:
    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;
    
    QRect rectEncompassingBaseToBase(slim_position_t startBase, slim_position_t endBase, QRect interiorRect, QtSLiMRange displayedRange);
    slim_position_t baseForPosition(double position, QRect interiorRect, QtSLiMRange displayedRange);
    QRect getContentRect(void);
    QRect getInteriorRect(void);
    
    void drawTicksInContentRect(QRect contentRect, Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter);
    void overlaySelection(QRect interiorRect, QtSLiMRange displayedRange, QPainter &painter);
    void glDrawRect(Species *displaySpecies);
    
    void glDrawGenomicElements(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void updateDisplayedMutationTypes(Species *displaySpecies);
    void glDrawFixedSubstitutions(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void glDrawMutations(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    
    void _glDrawRateMapIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue);
    void glDrawRecombinationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void glDrawMutationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    void glDrawRateMaps(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange);
    
    virtual void mousePressEvent(QMouseEvent *p_event) override;
    void _mouseTrackEvent(QMouseEvent *p_event);
    virtual void mouseMoveEvent(QMouseEvent *p_event) override;
    virtual void mouseReleaseEvent(QMouseEvent *p_event) override;
    virtual void contextMenuEvent(QContextMenuEvent *p_event) override;
    
    // Our configuration is kept by the controller, since it is shared by all chromosome views for multispecies models
    // However, "overview" chromosome views are always configured the same, hard-coded here
    inline bool shouldDrawMutations(void) const { return referenceChromosomeView_ ? controller_->chromosome_shouldDrawMutations_ : false; }
    inline bool shouldDrawFixedSubstitutions(void) const { return referenceChromosomeView_ ? controller_->chromosome_shouldDrawFixedSubstitutions_ : false; }
    inline bool shouldDrawGenomicElements(void) const { return referenceChromosomeView_ ? controller_->chromosome_shouldDrawGenomicElements_ : true; }
    inline bool shouldDrawRateMaps(void) const { return referenceChromosomeView_ ? controller_->chromosome_shouldDrawRateMaps_ : false; }
    inline bool displayHaplotypes(void) const { return referenceChromosomeView_ ? controller_->chromosome_display_haplotypes_ : false; }
    inline std::vector<slim_objectid_t> &displayMuttypes(void) const { return controller_->chromosome_display_muttypes_; }
};

#endif // QTSLIMCHROMOSOMEWIDGET_H
















