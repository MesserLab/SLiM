//
//  QtSLiMGraphView.h
//  SLiM
//
//  Created by Ben Haller on 3/27/2020.
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

#ifndef QTSLIMGRAPHVIEW_H
#define QTSLIMGRAPHVIEW_H

#include <QWidget>
#include <QFont>
#include <QColor>

#include <utility>
#include <vector>

#include "QtSLiMExtras.h"
#include "QtSLiMWindow.h"

class QHBoxLayout;


// A quick and dirty macro to enable rounding of coordinates to the nearest pixel only when we are not generating PDF
// FIXME this ought to be revisited in the light of Retina displays, printing, etc.
#define SLIM_SCREEN_ROUND(x)		(generatingPDF_ ? (x) : round(x))


// Legend support
typedef std::pair<QString, QColor> QtSLiMLegendEntry;
typedef std::vector<QtSLiMLegendEntry> QtSLiMLegendSpec;


class QtSLiMGraphView : public QWidget
{
    Q_OBJECT
    
public:
    static QFont labelFontOfPointSize(double size);
    static inline QFont fontForAxisLabels(void) { return labelFontOfPointSize(15); }
    static inline QFont fontForTickLabels(void) { return labelFontOfPointSize(10); }
    static inline QFont fontForLegendLabels(void) { return labelFontOfPointSize(10); }
    static inline QColor gridLineColor(void) { return QtSLiMColorWithWhite(0.85, 1.0); }
    
    QtSLiMGraphView(QWidget *parent, QtSLiMWindow *controller);
    ~QtSLiMGraphView() override;
    
    virtual QString graphTitle(void) = 0;
    virtual bool needsButtonLayout(void);
    virtual void drawGraph(QPainter &painter, QRect interiorRect);
    
public slots:
    void dispatch(QtSLiMWindow::DynamicDispatchID dispatchID);  // called by QtSLiMWindow::sendAllLinkedViewsSelector()
    virtual void addedToWindow(void);
    virtual void invalidateDrawingCache(void);
    virtual void graphWindowResized(void);
    virtual void controllerRecycled(void);
    virtual void controllerSelectionChanged(void);
    virtual void controllerGenerationFinished(void);
    virtual void updateAfterTick(void);
    
protected:
    QtSLiMWindow *controller_ = nullptr;
    
    // Base graphing functionality
    QRect interiorRectForBounds(QRect bounds);
    
    double plotToDeviceX(double plotx, QRect interiorRect);
    double plotToDeviceY(double ploty, QRect interiorRect);
    double roundPlotToDeviceX(double plotx, QRect interiorRect);	// rounded off to the nearest midpixel
    double roundPlotToDeviceY(double ploty, QRect interiorRect);	// rounded off to the nearest midpixel
    
    void drawXAxisTicks(QPainter &painter, QRect interiorRect);
    void drawXAxis(QPainter &painter, QRect interiorRect);
    void drawYAxisTicks(QPainter &painter, QRect interiorRect);
    void drawYAxis(QPainter &painter, QRect interiorRect);
    void drawFullBox(QPainter &painter, QRect interiorRect);
    void drawVerticalGridLines(QPainter &painter, QRect interiorRect);
    void drawHorizontalGridLines(QPainter &painter, QRect interiorRect);
    void drawMessage(QPainter &painter, QString messageString, QRect rect);
    void drawLegendInInteriorRect(QPainter &painter, QRect interiorRect);
    
    // Optional subclass overrides
    virtual void cleanup();
    virtual void willDraw(QPainter &painter, QRect interiorRect);
    virtual bool providesStringForData(void);
    virtual QString stringForData(void);    
    virtual QtSLiMLegendSpec legendKey(void);
    virtual QSize legendSize(QPainter &painter);
    virtual void drawLegend(QPainter &painter, QRect legendRect);
    virtual void subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent *event);
    
    // Adding new widgets at the bottom of the window
    QHBoxLayout *buttonLayout(void);
    
    // Prefab additions
    QString dateline(void);
    void setXAxisRangeFromGeneration(void);
    QtSLiMLegendSpec mutationTypeLegendKey(void);
    void drawGroupedBarplot(QPainter &painter, QRect interiorRect, double *buffer, int subBinCount, int mainBinCount, double firstBinValue, double mainBinWidth);
    void drawBarplot(QPainter &painter, QRect interiorRect, double *buffer, int binCount, double firstBinValue, double binWidth);
    
    // Properties; initialzed in the constructor, these defaults are just zero-fill
    bool showXAxis_ = false;
    bool allowXAxisUserRescale_ = false;
    bool xAxisIsUserRescaled_ = false;
    bool showXAxisTicks_ = false;
    double xAxisMin_ = 0.0, xAxisMax_ = 0.0;
    double xAxisMajorTickInterval_ = 0.0, xAxisMinorTickInterval_ = 0.0;
    int xAxisMajorTickModulus_ = 0, xAxisTickValuePrecision_ = 0;
    QString xAxisLabel_;
    
    bool showYAxis_ = false;
    bool allowYAxisUserRescale_ = false;
    bool yAxisIsUserRescaled_ = false;
    bool showYAxisTicks_ = false;
    double yAxisMin_ = 0.0, yAxisMax_ = 0.0;
    double yAxisMajorTickInterval_ = 0.0, yAxisMinorTickInterval_ = 0.0;
    int yAxisMajorTickModulus_ = 0, yAxisTickValuePrecision_ = 0;
    QString yAxisLabel_;
    
    bool legendVisible_ = false;
    bool showHorizontalGridLines_ = false;
    bool showVerticalGridLines_ = false;
    bool showFullBox_ = false;
    
    bool tweakXAxisTickLabelAlignment_ = false;
    
    // Prefab additions properties
    int histogramBinCount_ = 0;
    bool allowXAxisBinRescale_ = false;
    
    // set to YES during a copy: operation, to allow customization
	bool generatingPDF_ = false;
	
	// caching for drawing speed is up to subclasses, if they want to do it, but we provide minimal support
    // in GraphView to make it work smoothly this flag is to prevent recursion in the drawing code, and to
    // disable drawing of things that don't belong in a cache, such as the legend
	bool cachingNow_ = false;
    
private:
    void paintEvent(QPaintEvent *event) override;
    void drawContents(QPainter &painter);
    
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};


#endif // QTSLIMGRAPHVIEW_H

























