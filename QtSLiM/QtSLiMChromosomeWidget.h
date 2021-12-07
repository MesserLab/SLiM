//
//  QtSLiMChromosomeWidget.h
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2021 Philipp Messer.  All rights reserved.
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
    
    bool selectable_ = false;
    QtSLiMChromosomeWidget *referenceChromosomeView_ = nullptr;
    
    bool shouldDrawMutations_ = true;
    bool shouldDrawFixedSubstitutions_ = false;
    bool shouldDrawGenomicElements_ = false;
    bool shouldDrawRateMaps_ = false;
    
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
    
    // Display options
	bool display_haplotypes_ = false;                   // if false, displaying frequencies; if true, displaying haplotypes
    int64_t *haplotype_previous_bincounts = nullptr;    // used by QtSLiMHaplotypeManager to keep the sort order stable
    QtSLiMHaplotypeManager *haplotype_mgr_ = nullptr;   // the haplotype manager constructed for the current display; cached
	std::vector<slim_objectid_t> display_muttypes_;     // if empty, display all mutation types; otherwise, display only the muttypes chosen
    
public:
    explicit QtSLiMChromosomeWidget(QWidget *p_parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMChromosomeWidget() override;
    
    inline void setSelectable(bool p_flag) { selectable_ = p_flag; }
    void setReferenceChromosomeView(QtSLiMChromosomeWidget *p_ref_widget);
    
    inline void setShouldDrawGenomicElements(bool p_flag) { shouldDrawGenomicElements_ = p_flag; }
    inline void setShouldDrawRateMaps(bool p_flag) { shouldDrawRateMaps_ = p_flag; }
    inline void setShouldDrawMutations(bool p_flag) { shouldDrawMutations_ = p_flag; }
    inline void setShouldDrawFixedSubstitutions(bool p_flag) { shouldDrawFixedSubstitutions_ = p_flag; }
    
    bool hasSelection(void) { return hasSelection_; }
    QtSLiMRange getSelectedRange(void);
    void setSelectedRange(QtSLiMRange p_selectionRange);
    void restoreLastSelection(void);
    const std::vector<slim_objectid_t> &displayMuttypes(void);
    
    QtSLiMRange getDisplayedRange(void);
    
    void stateChanged(void);    // update when the SLiM model state changes; tosses any cached display info
    
    void runContextMenuAtPoint(QPoint p_globalPoint);
    
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
    
    void drawTicksInContentRect(QRect contentRect, QtSLiMWindow *controller, QtSLiMRange displayedRange, QPainter &painter);
    void overlaySelection(QRect interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange, QPainter &painter);
    void glDrawRect(void);
    
    void glDrawGenomicElements(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange);
    void updateDisplayedMutationTypes(void);
    void glDrawFixedSubstitutions(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange);
    void glDrawMutations(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange);
    
    void _glDrawRateMapIntervals(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue);
    void glDrawRecombinationIntervals(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange);
    void glDrawMutationIntervals(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange);
    void glDrawRateMaps(QRect &interiorRect, QtSLiMWindow *controller, QtSLiMRange displayedRange);
    
    virtual void mousePressEvent(QMouseEvent *p_event) override;
    void _mouseTrackEvent(QMouseEvent *p_event);
    virtual void mouseMoveEvent(QMouseEvent *p_event) override;
    virtual void mouseReleaseEvent(QMouseEvent *p_event) override;
    virtual void contextMenuEvent(QContextMenuEvent *p_event) override;
};

#endif // QTSLIMCHROMOSOMEWIDGET_H
















