//
//  QtSLiMGraphView.h
//  SLiM
//
//  Created by Ben Haller on 3/27/2020.
//  Copyright (c) 2020-2024 Philipp Messer.  All rights reserved.
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
class QComboBox;
class QPushButton;


// A quick and dirty macro to enable rounding of coordinates to the nearest pixel only when we are not generating PDF
// FIXME this ought to be revisited in the light of Retina displays, printing, etc.
#define SLIM_SCREEN_ROUND(x)		(generatingPDF_ ? (x) : round(x))


// Legend support
enum class QtSLiM_LegendEntryType : int {
    kUninitialized = -1,
    kSwatch = 0,
    kLine,
    kPoint
};

class QtSLiMLegendEntry {
public: 
    // the text label for the legend entry
    QString label;
    
    // the type of legend entry
    QtSLiM_LegendEntryType entry_type = QtSLiM_LegendEntryType::kUninitialized;
    
    // attributes for a swatch component (QtSLiM_LegendEntryType::kSwatch)
    QColor swatch_color;
    
    // attributes for a line component (QtSLiM_LegendEntryType::kLine); same as for plotLines()
    double line_lwd;
    QColor line_color;
    
    // attributes for a point component (QtSLiM_LegendEntryType::kPoint); same as for plotPoints()
    int point_symbol;
    QColor point_color;
    QColor point_border;
    double point_lwd;
    double point_size;
    
    // constructor for an entry that shows a color swatch
    QtSLiMLegendEntry(QString p_label, QColor p_swatch_color) :
        label(p_label), entry_type(QtSLiM_LegendEntryType::kSwatch), swatch_color(p_swatch_color) {};
    
    // constructor for an entry that shows a line segment
    QtSLiMLegendEntry(QString p_label, double p_line_lwd, QColor p_line_color) :
        label(p_label), entry_type(QtSLiM_LegendEntryType::kLine), line_lwd(p_line_lwd), line_color(p_line_color) {};
    
    // constructor for an entry that shows a point symbol
    QtSLiMLegendEntry(QString p_label, int p_point_symbol, QColor p_point_color, QColor p_point_border, double p_point_lwd, double p_point_size) :
        label(p_label), entry_type(QtSLiM_LegendEntryType::kPoint), point_symbol(p_point_symbol), point_color(p_point_color), point_border(p_point_border), point_lwd(p_point_lwd), point_size(p_point_size) {};
};

typedef std::vector<QtSLiMLegendEntry> QtSLiMLegendSpec;


class QtSLiMGraphView : public QWidget
{
    Q_OBJECT
    
public:
    static QFont labelFontOfPointSize(double size);
    static inline QColor gridLineColor(void) { return QtSLiMColorWithWhite(0.85, 1.0); }
    
    QtSLiMGraphView(QWidget *p_parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView() override;
    
    virtual QString graphTitle(void) = 0;
    virtual QString aboutString(void) = 0;
    virtual void drawGraph(QPainter &painter, QRect interiorRect);
    
    bool writeToFile(QString fileName);
    
public slots:
    virtual void addedToWindow(void);
    virtual void invalidateCachedData(void);        // subclasses must call this themselves in their destructor - super cannot do it!
    virtual void invalidateDrawingCache(void);      // subclasses must call this themselves in their destructor - super cannot do it!
    virtual void graphWindowResized(void);
    virtual void controllerRecycled(void);          // subclasses must call super: QtSLiMGraphView::controllerRecycled()
    virtual void controllerChromosomeSelectionChanged(void);
    virtual void controllerTickFinished(void);
    virtual void updateAfterTick(void);             // subclasses must call super: QtSLiMGraphView::updateAfterTick()
    void actionButtonRunMenu(QtSLiMPushButton *actionButton);
    
protected:
    QtSLiMWindow *controller_ = nullptr;
    std::string focalSpeciesName_;                                  // we keep the name of our focal species, since a pointer would be unsafe
    std::string focalSpeciesAvatar_;                                // cached so we can display it even when the simulation is invalid
    
    void setFocalDisplaySpecies(Species *species);
    Species *focalDisplaySpecies(void);
    bool missingFocalDisplaySpecies(void);                          // true if the graph has a focal display species but can't find it
    void updateSpeciesBadge(void);
    
    // Base graphing functionality
    QRect interiorRectForBounds(QRect bounds);
    
    double plotToDeviceX(double plotx, QRect interiorRect);
    double plotToDeviceY(double ploty, QRect interiorRect);
    double roundPlotToDeviceX(double plotx, QRect interiorRect);	// rounded off to the nearest midpixel
    double roundPlotToDeviceY(double ploty, QRect interiorRect);	// rounded off to the nearest midpixel
    
    inline QFont fontForAxisLabels(void) { return labelFontOfPointSize(axisLabelSize_); }
    inline QFont fontForTickLabels(void) { return labelFontOfPointSize(tickLabelSize_); }
    
    QString labelTextForTick(double tickValue, int tickValuePrecision, double minorTickInterval);
    void drawAxisTickLabel(QPainter &painter, QString labelText, double xValueForTick, double axisLength,
                           bool isFirstTick, bool isLastTick);
    
    void drawXAxisTicks(QPainter &painter, QRect interiorRect);
    void drawXAxis(QPainter &painter, QRect interiorRect);
    void drawYAxisTicks(QPainter &painter, QRect interiorRect);
    void drawYAxis(QPainter &painter, QRect interiorRect);
    void drawFullBox(QPainter &painter, QRect interiorRect);
    void drawVerticalGridLines(QPainter &painter, QRect interiorRect);
    void drawHorizontalGridLines(QPainter &painter, QRect interiorRect);
    void drawMessage(QPainter &painter, QString messageString, QRect rect);
    void drawLegendInInteriorRect(QPainter &painter, QRect interiorRect);
    
    // Mandatory subclass overrides
    virtual void appendStringForData(QString &string) = 0;
    
    // Optional subclass overrides
    virtual void willDraw(QPainter &painter, QRect interiorRect);
    virtual bool providesStringForData(void);
    virtual QtSLiMLegendSpec legendKey(void);
    virtual QSizeF legendSize(QPainter &painter);
    virtual void drawLegend(QPainter &painter, QRectF legendRect);
    virtual void subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent *p_event);
    virtual QString disableMessage(void);
    
    // Adding new widgets at the bottom of the window
    QHBoxLayout *buttonLayout(void);
    QPushButton *actionButton(void);
    QComboBox *newButtonInLayout(QHBoxLayout *layout);
    
    // Prefab additions
    void setXAxisRangeFromTick(void);
    void configureAxisForRange(double &dim0, double &dim1, double &axisMin, double &axisMax,
                               double &majorTickInterval, double &minorTickInterval,
                               int &majorTickModulus, int &tickValuePrecision);
    QtSLiMLegendSpec subpopulationLegendKey(std::vector<slim_objectid_t> &subpopsToDisplay, bool drawSubpopsGray);
    QtSLiMLegendSpec mutationTypeLegendKey(void);
    void drawGroupedBarplot(QPainter &painter, QRect interiorRect, double *buffer, int subBinCount, int mainBinCount, double firstBinValue, double mainBinWidth);
    void drawBarplot(QPainter &painter, QRect interiorRect, double *buffer, int binCount, double firstBinValue, double binWidth);
    void drawHeatmap(QPainter &painter, QRect interiorRect, double *buffer, int xBinCount, int yBinCount);
    bool addSubpopulationsToMenu(QComboBox *subpopButton, slim_objectid_t selectedSubpopID, slim_objectid_t avoidSubpopID = -1);
    bool addMutationTypesToMenu(QComboBox *mutTypeButton, int selectedMutIDIndex);
    size_t tallyGUIMutationReferences(slim_objectid_t subpop_id, int muttype_index);
    size_t tallyGUIMutationReferences(const std::vector<Genome *> &genomes, int muttype_index);
    
    // Properties; initialized in the constructor, these defaults are just zero-fill
    // Note that the bounds in user coordinates (x0_/x1_/y0_/y1_) are now separate from
    // the axis limits (xAxisMin_, ...), but at present they are always the same except
    // for custom plots.  That may change, going forward, to improve axis behavior.
    double x0_ = 0.0, x1_ = 0.0, y0_ = 0.0, y1_ = 0.0;  // coordinate space bounds
    
    double axisLabelSize_ = 15;
    double tickLabelSize_ = 10;
    
    bool showXAxis_ = false;
    bool allowXAxisUserRescale_ = false;
    bool xAxisIsUserRescaled_ = false;
    bool showXAxisTicks_ = false;
    double xAxisMin_ = 0.0, xAxisMax_ = 0.0;
    double xAxisMajorTickInterval_ = 0.0, xAxisMinorTickInterval_ = 0.0;
    int xAxisMajorTickModulus_ = 0;
    int xAxisTickValuePrecision_ = 0;   // negative values request output mode 'g' instead of 'f'
    bool xAxisHistogramStyle_ = false;
    QString xAxisLabel_;
    std::vector<double> *xAxisAt_ = nullptr;
    int xAxisLabelsType_ = 0;           // 0 == F (don't show labels), 1 == T (numeric position labels), 2 == use xAxisLabels_
    std::vector<QString> *xAxisLabels_ = nullptr;
    
    bool showYAxis_ = false;
    bool allowYAxisUserRescale_ = false;
    bool yAxisIsUserRescaled_ = false;
    bool showYAxisTicks_ = false;
    double yAxisMin_ = 0.0, yAxisMax_ = 0.0;
    double yAxisMajorTickInterval_ = 0.0, yAxisMinorTickInterval_ = 0.0;
    int yAxisMajorTickModulus_ = 0;
    int yAxisTickValuePrecision_ = 0;   // negative values request output mode 'g' instead of 'f'
    bool yAxisHistogramStyle_ = false;
    bool yAxisLog_ = false;
    QString yAxisLabel_;
    std::vector<double> *yAxisAt_ = nullptr;
    int yAxisLabelsType_ = 0;           // 0 == F (don't show labels), 1 == T (numeric position labels), 2 == use yAxisLabels_
    std::vector<QString> *yAxisLabels_ = nullptr;
    
    bool legendVisible_ = false;
    QtSLiM_LegendPosition legend_position_ = QtSLiM_LegendPosition::kUnconfigured;
    int legend_inset = -1;
    double legend_labelSize = -1;
    double legend_lineHeight = -1;
    double legend_graphicsWidth = -1;
    double legend_exteriorMargin = -1;
    double legend_interiorMargin = -1;
    
    bool showHorizontalGridLines_ = false;
    bool showVerticalGridLines_ = false;
    bool showGridLinesMajorOnly_ = false;
    bool showFullBox_ = false;
    bool allowHorizontalGridChange_ = false;
    bool allowVerticalGridChange_ = false;
    bool allowFullBoxChange_ = false;
    
    bool tweakXAxisTickLabelAlignment_ = false;
    
    // Prefab additions properties
    int histogramBinCount_ = 0;
    bool allowBinCountRescale_ = false;
    int heatmapMargins_ = 0;
    bool allowHeatmapMarginsChange_ = false;
    bool rebuildingMenu_ = false;           // set to true during addSubpopulationsToMenu() / addMutationTypesToMenu()
    
    // set to YES during a copy: operation, to allow customization
	bool generatingPDF_ = false;
	
	// caching for drawing speed is up to subclasses, if they want to do it, but we provide minimal support
    // in GraphView to make it work smoothly; this flag is to prevent recursion in the drawing code, and to
    // disable drawing of things that don't belong in a cache, such as the legend
	bool cachingNow_ = false;
    
protected:
    int lineCountForLegend(QtSLiMLegendSpec &legend);
    double graphicsWidthForLegend(QtSLiMLegendSpec &legend, double legendLineHeight);
    void drawPointSymbol(QPainter &painter, double x, double y, int symbol, QColor symbolColor, QColor borderColor, double lineWidth, double size);
    
private:
    virtual void paintEvent(QPaintEvent *p_paintEvent) override;
    void drawContents(QPainter &painter);
    
    virtual void resizeEvent(QResizeEvent *p_event) override;
    virtual void contextMenuEvent(QContextMenuEvent *p_event) override;
    
    QString stringForData(void);
    
    // Internal axis layout code, based on R; presently used only by custom graphs
    void _GScale(double &minValue, double &maxValue, double &axisMin, double &axisMax, int &nDivisions);
    void _GAxisPars(double *minValue, double *maxValue, int *nDivisions);
    void _GEPretty(double *lo, double *up, int *nDivisions);
    double _R_pretty(double *lo, double *up, int *nDivisions, int min_n, double shrink_sml,
                        const double high_u_fact[], int eps_correction);
};


#endif // QTSLIMGRAPHVIEW_H

























