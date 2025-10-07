//
//  QtSLiMGraphView_CustomPlot.cpp
//  SLiM
//
//  Created by Ben Haller on 1/19/2024.
//  Copyright (c) 2024-2025 Benjamin C. Haller.  All rights reserved.
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

#include "QtSLiMGraphView_CustomPlot.h"

#include <QPainterPath>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>

#include "QtSLiM_Plot.h"

#include <limits>
#include <algorithm>
#include <vector>


QtSLiMGraphView_CustomPlot::QtSLiMGraphView_CustomPlot(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    title_ = "Custom Plot";     // will be replaced
    xAxisLabel_ = "x";
    yAxisLabel_ = "y";
    
    // user-rescaling of the axes should work fine, but will switch to the "base plot" way of handling
    // the data range and the axis ticks, so some functionality will be disabled, such as auto-resizing
    // the data range and axes to fit newly added data; so it goes
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    setFocalDisplaySpecies(nullptr);
    
    QtSLiMGraphView_CustomPlot::updateAfterTick();
}

void QtSLiMGraphView_CustomPlot::freeData(void)
{
    // discard all plot data
    for (double *x1buffer : x1data_)
        free(x1buffer);
    
    for (double *y1buffer : y1data_)
        free(y1buffer);
    
    for (double *x2buffer : x2data_)
        free(x2buffer);
    
    for (double *y2buffer : y2data_)
        free(y2buffer);
    
    for (std::vector<QString> *labelsbuffer : labels_)
        delete labelsbuffer;
    
    for (std::vector<int> *symbolbuffer : symbol_)
        delete symbolbuffer;
    
    for (std::vector<QColor> *colorbuffer : color_)
        delete colorbuffer;
    
    for (std::vector<QColor> *borderbuffer : border_)
        delete borderbuffer;
    
    for (std::vector<double> *alphabuffer : alpha_)
        delete alphabuffer;
    
    for (std::vector<double> *lwdbuffer : line_width_)
        delete lwdbuffer;
    
    for (std::vector<double> *sizebuffer : size_)
        delete sizebuffer;
    
    for (std::vector<double> *anglebuffer : angle_)
        delete anglebuffer;
    
    plot_type_.clear();
    x1data_.clear();
    y1data_.clear();
    x2data_.clear();
    y2data_.clear();
    data_count_.clear();
    labels_.clear();
    symbol_.clear();
    color_.clear();
    border_.clear();
    alpha_.clear();
    line_width_.clear();
    size_.clear();
    angle_.clear();
    xadj_.clear();
    yadj_.clear();
    image_.clear();
    
    // reset the legend state
    legend_added_ = false;
    
    legend_position_ = QtSLiM_LegendPosition::kUnconfigured;
    legend_inset = -1;
    legend_labelSize = -1;
    legend_lineHeight = -1;
    legend_graphicsWidth = -1;
    legend_exteriorMargin = -1;
    legend_interiorMargin = -1;
    
    legend_entries_.clear();
}

QtSLiMGraphView_CustomPlot::~QtSLiMGraphView_CustomPlot()
{
    // We are responsible for our own destruction
    
    // We own our corresponding Eidos object of class Plot, and free it here.  It is not under retain/release,
    // and this should occur only at a "long-term boundary" since it is triggered by the plot window closing
    // in SLiMgui.  Note that sometimes eidos_plot_object_ is nullptr, such as when the custom plot was created
    // in SLiMgui's UI from LogFile data; this is fine, it just means the window is not controllable from script.
    if (eidos_plot_object_)
    {
        delete eidos_plot_object_;
        eidos_plot_object_ = nullptr;
    }
    
    freeData();
}

void QtSLiMGraphView_CustomPlot::setTitle(QString title)
{
    title_ = title;
    
    QWidget *graphWindow = window();
    
    if (graphWindow)
        graphWindow->setWindowTitle(title);
}

void QtSLiMGraphView_CustomPlot::setXLabel(QString x_label)
{
    xAxisLabel_ = x_label;
    update();
}

void QtSLiMGraphView_CustomPlot::setYLabel(QString y_label)
{
    yAxisLabel_ = y_label;
    update();
}

void QtSLiMGraphView_CustomPlot::setShowHorizontalGrid(bool showHorizontalGrid)
{
    showHorizontalGridLines_ = showHorizontalGrid;
    update();
}

void QtSLiMGraphView_CustomPlot::setShowVerticalGrid(bool showVerticalGrid)
{
    showVerticalGridLines_ = showVerticalGrid;
    update();
}

void QtSLiMGraphView_CustomPlot::setShowFullBox(bool showFullBox)
{
    showFullBox_ = showFullBox;
    update();
}

void QtSLiMGraphView_CustomPlot::setAxisLabelSize(double axisLabelSize)
{
    axisLabelSize_ = axisLabelSize;
    update();
}

void QtSLiMGraphView_CustomPlot::setTickLabelSize(double tickLabelSize)
{
    tickLabelSize_ = tickLabelSize;
    update();
}

void QtSLiMGraphView_CustomPlot::setLegendPosition(QtSLiM_LegendPosition position)
{
    legend_position_ = position;
    update();
}

void QtSLiMGraphView_CustomPlot::setDataRanges(double *x_range, double *y_range)
{
    // this is called by QtSLiMWindow::eidos_createPlot(), to set up for the user's specified ranges
    // nullptr for an axis indicates that we want that axis to be controlled by the range of the data
    // otherwise, we set up the min and max values for the axis from the given (two-valued) range buffer
    if (x_range)
    {
        original_x0_ = x_range[0];
        original_x1_ = x_range[1];
        
        x0_ = original_x0_;
        x1_ = original_x1_;
        
        configureAxisForRange(x0_, x1_, xAxisMin_, xAxisMax_,
                              xAxisMajorTickInterval_, xAxisMinorTickInterval_,
                              xAxisMajorTickModulus_, xAxisTickValuePrecision_);
        xAxisIsUserRescaled_ = true;
        xAxisIsUIRescaled_ = false;
    }
    else
    {
        // allow any user configuration in the UI to persist through a recycle
        // if a range was set by createPlot(), though, we want to reset that
        if (!xAxisIsUIRescaled_)
            xAxisIsUserRescaled_ = false;
    }
    
    if (y_range)
    {
        original_y0_ = y_range[0];
        original_y1_ = y_range[1];
        
        y0_ = original_y0_;
        y1_ = original_y1_;
        
        configureAxisForRange(y0_, y1_, yAxisMin_, yAxisMax_,
                              yAxisMajorTickInterval_, yAxisMinorTickInterval_,
                              yAxisMajorTickModulus_, yAxisTickValuePrecision_);
        yAxisIsUserRescaled_ = true;
        yAxisIsUIRescaled_ = false;
    }
    else
    {
        // allow any user configuration in the UI to persist through a recycle
        // if a range was set by createPlot(), though, we want to reset that
        if (!yAxisIsUIRescaled_)
            yAxisIsUserRescaled_ = false;
    }
}

void QtSLiMGraphView_CustomPlot::setAxisConfiguration(int side, std::vector<double> *at, int labels_type, std::vector<QString> *labels)
{
    // This method is called by the Eidos method Plot::axis() to customize axis display.
    // Note that Plot::ExecuteMethod_axis() does a bunch of bounds-checking and such for us.
    if (side == 1)
    {
        // x-axis configuration
        if (xAxisAt_)
        {
            delete xAxisAt_;
            xAxisAt_ = nullptr;
        }
        if (xAxisLabels_)
        {
            delete xAxisLabels_;
            xAxisLabels_ = nullptr;
        }
        
        if (at)
        {
            xAxisAt_ = at;
            allowXAxisUserRescale_ = false;
        }
        else
        {
            allowXAxisUserRescale_ = true;
        }
        
        xAxisLabelsType_ = labels_type;
        xAxisLabels_ = labels;
    }
    else if (side == 2)
    {
        // y-axis configuration
        if (yAxisAt_)
        {
            delete yAxisAt_;
            yAxisAt_ = nullptr;
        }
        if (yAxisLabels_)
        {
            delete yAxisLabels_;
            yAxisLabels_ = nullptr;
        }
        
        if (at)
        {
            yAxisAt_ = at;
            allowYAxisUserRescale_ = false;
        }
        else
        {
            allowYAxisUserRescale_ = true;
        }
        
        yAxisLabelsType_ = labels_type;
        yAxisLabels_ = labels;
    }
}

void QtSLiMGraphView_CustomPlot::dataRange(std::vector<double *> &data_vector, double *p_min, double *p_max)
{
    // This method accumulates the min/max for the range of our data, in either x or y
    // It excludes NAN and INF values from the range; such values are not plotted
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();
    
    for (int data_index = 0; data_index < (int)data_vector.size(); ++data_index)
    {
        // lines from abline() are not included in the data range; they are decoration
        if ((plot_type_[data_index] == QtSLiM_CustomPlotType::kABLines) ||
            (plot_type_[data_index] == QtSLiM_CustomPlotType::kHLines) ||
            (plot_type_[data_index] == QtSLiM_CustomPlotType::kVLines))
            continue;
        
        double *point_data = data_vector[data_index];
        
        if (point_data)
        {
            int point_count = data_count_[data_index];
            
            for (int point_index = 0; point_index < point_count; ++point_index)
            {
                double point_value = point_data[point_index];
                
                if (std::isfinite(point_value))
                {
                    min = std::min(min, point_value);
                    max = std::max(max, point_value);
                }
            }
        }
    }
    
    *p_min = min;
    *p_max = max;
}

void QtSLiMGraphView_CustomPlot::rescaleAxesForDataRange(void)
{
    // this is called when new data is added to a plot, to rescale the axes as needed
    // set up axes based on the data range; we try to apply a little intelligence, but if the user
    // wants really intelligent axis ranges, they can set them up themselves...
    double x1min, x1max, y1min, y1max, x2min, x2max, y2min, y2max;
    
    dataRange(x1data_, &x1min, &x1max);
    dataRange(y1data_, &y1min, &y1max);
    dataRange(x2data_, &x2min, &x2max);
    dataRange(y2data_, &y2min, &y2max);
    
    double xmin = std::min(x1min, x2min);
    double xmax = std::max(x1max, x2max);
    double ymin = std::min(y1min, y2min);
    double ymax = std::max(y1max, y2max);
    
    //std::cout << "-----" << std::endl;
    //std::cout << "   xmin == " << xmin << ", x1min == " << x1min << ", x2min << " << x2min << std::endl;
    //std::cout << "   xmax == " << xmax << ", x1max == " << x1max << ", x2max << " << x2max << std::endl;
    //std::cout << "   ymin == " << ymin << ", y1min == " << y1min << ", y2min << " << y2min << std::endl;
    //std::cout << "   ymax == " << ymax << ", y1max == " << y1max << ", y2max << " << y2max << std::endl;
    
    has_finite_data_ = false;
    
    if (std::isfinite(xmin) && std::isfinite(xmax) && std::isfinite(ymin) && std::isfinite(ymax))
    {
        if (!xAxisIsUserRescaled_)
        {
            original_x0_ = xmin;
            original_x1_ = xmax;
            
            x0_ = original_x0_;
            x1_ = original_x1_;
            
            configureAxisForRange(x0_, x1_, xAxisMin_, xAxisMax_, xAxisMajorTickInterval_, xAxisMinorTickInterval_,
                                  xAxisMajorTickModulus_, xAxisTickValuePrecision_);
        }
        
        if (!yAxisIsUserRescaled_)
        {
            original_y0_ = ymin;
            original_y1_ = ymax;
            
            y0_ = original_y0_;
            y1_ = original_y1_;
            
            configureAxisForRange(y0_, y1_, yAxisMin_, yAxisMax_, yAxisMajorTickInterval_, yAxisMinorTickInterval_,
                                  yAxisMajorTickModulus_, yAxisTickValuePrecision_);
        }
        
        has_finite_data_ = true;
    }
}

void QtSLiMGraphView_CustomPlot::addABLineData(double *a_values, double *b_values,
                                               double *h_values, double *v_values, int data_count,
                                               std::vector<QColor> *color, std::vector<double> *alpha,
                                               std::vector<double> *lwd)
{
    if (a_values)
    {
        plot_type_.push_back(QtSLiM_CustomPlotType::kABLines);
        x1data_.push_back(a_values);
        y1data_.push_back(b_values);
    }
    else if (h_values)
    {
        plot_type_.push_back(QtSLiM_CustomPlotType::kHLines);
        x1data_.push_back(h_values);
        y1data_.push_back(nullptr);
    }
    else if (v_values)
    {
        plot_type_.push_back(QtSLiM_CustomPlotType::kVLines);
        x1data_.push_back(v_values);
        y1data_.push_back(nullptr);
    }
    
    x2data_.push_back(nullptr);             // unused for abline
    y2data_.push_back(nullptr);             // unused for abline
    labels_.push_back(nullptr);             // unused for abline
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for abline
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for abline
    alpha_.push_back(alpha);
    line_width_.push_back(lwd);
    size_.push_back(nullptr);               // unused for abline
    angle_.push_back(nullptr);              // unused for abline
    xadj_.push_back(-1);                    // unused for abline
    yadj_.push_back(-1);                    // unused for abline
    image_.push_back(QImage());             // unused for abline
    
    //rescaleAxesForDataRange();            // not needed for abline
    update();
}

void QtSLiMGraphView_CustomPlot::addLineData(double *x_values, double *y_values, int data_count,
                                             std::vector<QColor> *color, std::vector<double> *alpha,
                                             std::vector<double> *lwd)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kLines);
    x1data_.push_back(x_values);
    y1data_.push_back(y_values);
    x2data_.push_back(nullptr);             // unused for lines
    y2data_.push_back(nullptr);             // unused for lines
    labels_.push_back(nullptr);             // unused for lines
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for lines
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for lines
    alpha_.push_back(alpha);
    line_width_.push_back(lwd);
    size_.push_back(nullptr);               // unused for lines
    angle_.push_back(nullptr);              // unused for lines
    xadj_.push_back(-1);                    // unused for lines
    yadj_.push_back(-1);                    // unused for lines
    image_.push_back(QImage());             // unused for lines
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addRectData(double *x1_values, double *y1_values, double *x2_values, double *y2_values,
                                             int data_count, std::vector<QColor> *color, std::vector<QColor> *border,
                                             std::vector<double> *alpha, std::vector<double> *lwd)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kRects);
    x1data_.push_back(x1_values);
    y1data_.push_back(y1_values);
    x2data_.push_back(x2_values);
    y2data_.push_back(y2_values);
    labels_.push_back(nullptr);             // unused for rects
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for rects
    color_.push_back(color);
    border_.push_back(border);
    alpha_.push_back(alpha);
    line_width_.push_back(lwd);
    size_.push_back(nullptr);               // unused for rects
    angle_.push_back(nullptr);              // unused for rects
    xadj_.push_back(-1);                    // unused for rects
    yadj_.push_back(-1);                    // unused for rects
    image_.push_back(QImage());             // unused for rects
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addSegmentData(double *x1_values, double *y1_values, double *x2_values, double *y2_values,
                                                int data_count, std::vector<QColor> *color,
                                                std::vector<double> *alpha, std::vector<double> *lwd)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kSegments);
    x1data_.push_back(x1_values);
    y1data_.push_back(y1_values);
    x2data_.push_back(x2_values);
    y2data_.push_back(y2_values);
    labels_.push_back(nullptr);             // unused for segments
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for segments
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for segments
    alpha_.push_back(alpha);
    line_width_.push_back(lwd);
    size_.push_back(nullptr);               // unused for segments
    angle_.push_back(nullptr);              // unused for segments
    xadj_.push_back(-1);                    // unused for segments
    yadj_.push_back(-1);                    // unused for segments
    image_.push_back(QImage());             // unused for segments
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addMarginTextData(double *x_values, double *y_values, std::vector<QString> *labels, int data_count,
                                                   std::vector<QColor> *color, std::vector<double> *alpha,
                                                   std::vector<double> *size, double *adj, std::vector<double> *angle)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kMarginText);
    x1data_.push_back(x_values);
    y1data_.push_back(y_values);
    x2data_.push_back(nullptr);             // unused for text
    y2data_.push_back(nullptr);             // unused for text
    labels_.push_back(labels);
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for text
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for text
    alpha_.push_back(alpha);
    line_width_.push_back(nullptr);         // unused for text
    size_.push_back(size);
    angle_.push_back(angle);                // unused for text
    xadj_.push_back(adj[0]);
    yadj_.push_back(adj[1]);
    image_.push_back(QImage());             // unused for text
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addPointData(double *x_values, double *y_values, int data_count,
                                              std::vector<int> *symbol, std::vector<QColor> *color, std::vector<QColor> *border,
                                              std::vector<double> *alpha, std::vector<double> *lwd, std::vector<double> *size)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kPoints);
    x1data_.push_back(x_values);
    y1data_.push_back(y_values);
    x2data_.push_back(nullptr);             // unused for points
    y2data_.push_back(nullptr);             // unused for points
    labels_.push_back(nullptr);             // unused for points
    data_count_.push_back(data_count);
    symbol_.push_back(symbol);
    color_.push_back(color);
    border_.push_back(border);
    alpha_.push_back(alpha);
    line_width_.push_back(lwd);
    size_.push_back(size);
    angle_.push_back(nullptr);              // unused for points
    xadj_.push_back(-1);                    // unused for points
    yadj_.push_back(-1);                    // unused for points
    image_.push_back(QImage());             // unused for points
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addTextData(double *x_values, double *y_values, std::vector<QString> *labels, int data_count,
                                             std::vector<QColor> *color, std::vector<double> *alpha,
                                             std::vector<double> *size, double *adj, std::vector<double> *angle)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kText);
    x1data_.push_back(x_values);
    y1data_.push_back(y_values);
    x2data_.push_back(nullptr);             // unused for text
    y2data_.push_back(nullptr);             // unused for text
    labels_.push_back(labels);
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for text
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for text
    alpha_.push_back(alpha);
    line_width_.push_back(nullptr);         // unused for text
    size_.push_back(size);
    angle_.push_back(angle);                // unused for text
    xadj_.push_back(adj[0]);
    yadj_.push_back(adj[1]);
    image_.push_back(QImage());             // unused for text
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addImageData(double *x_values, double *y_values, int data_count,
                  QImage image, std::vector<double> *alpha)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kImage);
    x1data_.push_back(x_values);
    y1data_.push_back(y_values);
    x2data_.push_back(nullptr);             // unused for image
    y2data_.push_back(nullptr);             // unused for image
    labels_.push_back(nullptr);             // unused for image
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for image
    color_.push_back(nullptr);              // unused for image
    border_.push_back(nullptr);             // unused for image
    alpha_.push_back(alpha);
    line_width_.push_back(nullptr);         // unused for image
    size_.push_back(nullptr);               // unused for image
    angle_.push_back(nullptr);              // unused for image
    xadj_.push_back(-1);                    // unused for image
    yadj_.push_back(-1);                    // unused for image
    image_.push_back(image);
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addLegend(QtSLiM_LegendPosition position, int inset, double labelSize, double lineHeight,
                                           double graphicsWidth, double exteriorMargin, double interiorMargin)
{
    legend_added_ = true;
    
    legend_position_ = position;
    legend_inset = inset;
    legend_labelSize = labelSize;
    legend_lineHeight = lineHeight;
    legend_graphicsWidth = graphicsWidth;
    legend_exteriorMargin = exteriorMargin;
    legend_interiorMargin = interiorMargin;
    update();
}

void QtSLiMGraphView_CustomPlot::addLegendLineEntry(QString label, QColor color, double lwd)
{
    legend_entries_.emplace_back(label, lwd, color);
    update();
}

void QtSLiMGraphView_CustomPlot::addLegendPointEntry(QString label, int symbol, QColor color, QColor border, double lwd, double size)
{
    legend_entries_.emplace_back(label, symbol, color, border, lwd, size);
    update();
}

void QtSLiMGraphView_CustomPlot::addLegendSwatchEntry(QString label, QColor color)
{
    legend_entries_.emplace_back(label, color);
    update();
}

void QtSLiMGraphView_CustomPlot::addLegendTitleEntry(QString label)
{
    legend_entries_.emplace_back(label);
    update();
}

QString QtSLiMGraphView_CustomPlot::graphTitle(void)
{
    return title_;
}

QString QtSLiMGraphView_CustomPlot::aboutString(void)
{
    return "The Custom Plot graph type displays user-provided data that is supplied "
           "in script with createPlot() and subsequent calls.";
}

void QtSLiMGraphView_CustomPlot::drawGraph(QPainter &painter, QRect interiorRect)
{
    for (int i = 0; i < (int)plot_type_.size(); ++i)
    {
        QtSLiM_CustomPlotType plot_type = plot_type_[i];
        
        switch (plot_type)
        {
        case QtSLiM_CustomPlotType::kLines:
            drawLines(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kSegments:
            drawSegments(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kRects:
            drawRects(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kMarginText:
            drawMarginText(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kPoints:
            drawPoints(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kText:
            drawText(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kABLines:
            drawABLines(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kHLines:
            drawHLines(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kVLines:
            drawVLines(painter, interiorRect, i);
            break;
        case QtSLiM_CustomPlotType::kImage:
            drawImage(painter, interiorRect, i);
            break;
        }
    }
}

void QtSLiMGraphView_CustomPlot::appendStringForData(QString & /* string */)
{
    // No data string
}

QtSLiMLegendSpec QtSLiMGraphView_CustomPlot::legendKey(void)
{
    return legend_entries_;
}

void QtSLiMGraphView_CustomPlot::controllerRecycled(void)
{
    freeData();
    update();
    
    QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_CustomPlot::disableMessage(void)
{
    if ((plot_type_.size() == 0) || !has_finite_data_)
        return "no\ndata";
    
    return "";
}

void QtSLiMGraphView_CustomPlot::drawABLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *adata = x1data_[dataIndex];
    double *bdata = y1data_[dataIndex];
    int lineCount = data_count_[dataIndex];
    std::vector<QColor> &lineColors = *color_[dataIndex];           // might be one value or N values
    std::vector<double> &lineAlphas = *alpha_[dataIndex];           // might be one value or N values
    std::vector<double> &lineWidths = *line_width_[dataIndex];      // might be one value or N values
    
    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
    {
        QPainterPath linePath;
        double user_a = adata[lineIndex];
        double user_b = bdata[lineIndex];
        
        if (std::isfinite(user_a) && std::isfinite(user_b))
        {
            // slope-intercept: y = a + bx
            double user_x1 = x0_ - 100000.0;
            double user_x2 = x1_ + 100000.0;
            double user_y1 = user_a + user_b * user_x1;
            double user_y2 = user_a + user_b * user_x2;
            QPointF devicePoint1(plotToDeviceX(user_x1, interiorRect), plotToDeviceY(user_y1, interiorRect));
            QPointF devicePoint2(plotToDeviceX(user_x2, interiorRect), plotToDeviceY(user_y2, interiorRect));
            QColor lineColor = lineColors[lineIndex % lineColors.size()];
            double lineAlpha = lineAlphas[lineIndex % lineAlphas.size()];
            double lineWidth = lineWidths[lineIndex % lineWidths.size()];
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            
            if (lineAlpha != 1.0)
                lineColor.setAlphaF(lineAlpha);
            
            painter.strokePath(linePath, QPen(lineColor, lineWidth));
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawHLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *hdata = x1data_[dataIndex];
    int lineCount = data_count_[dataIndex];
    std::vector<QColor> &lineColors = *color_[dataIndex];           // might be one value or N values
    std::vector<double> &lineAlphas = *alpha_[dataIndex];           // might be one value or N values
    std::vector<double> &lineWidths = *line_width_[dataIndex];      // might be one value or N values
    
    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
    {
        QPainterPath linePath;
        double user_h = hdata[lineIndex];
        
        if (std::isfinite(user_h))
        {
            // round the y-coordinate for display to make the line look nicer, especially for lwd 1.0
            QPointF devicePoint1(plotToDeviceX(x0_ - 100000.0, interiorRect), roundPlotToDeviceY(user_h, interiorRect));
            QPointF devicePoint2(plotToDeviceX(x1_ + 100000.0, interiorRect), roundPlotToDeviceY(user_h, interiorRect));
            QColor lineColor = lineColors[lineIndex % lineColors.size()];
            double lineAlpha = lineAlphas[lineIndex % lineAlphas.size()];
            double lineWidth = lineWidths[lineIndex % lineWidths.size()];
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            
            if (lineAlpha != 1.0)
                lineColor.setAlphaF(lineAlpha);
            
            painter.strokePath(linePath, QPen(lineColor, lineWidth));
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawVLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *vdata = x1data_[dataIndex];
    int lineCount = data_count_[dataIndex];
    std::vector<QColor> &lineColors = *color_[dataIndex];           // might be one value or N values
    std::vector<double> &lineAlphas = *alpha_[dataIndex];           // might be one value or N values
    std::vector<double> &lineWidths = *line_width_[dataIndex];      // might be one value or N values
    
    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
    {
        QPainterPath linePath;
        double user_v = vdata[lineIndex];
        
        if (std::isfinite(user_v))
        {
            // round the x-coordinate for display to make the line look nicer, especially for lwd 1.0
            QPointF devicePoint1(roundPlotToDeviceX(user_v, interiorRect), plotToDeviceY(y0_ - 100000.0, interiorRect));
            QPointF devicePoint2(roundPlotToDeviceX(user_v, interiorRect), plotToDeviceY(y1_ + 100000.0, interiorRect));
            QColor lineColor = lineColors[lineIndex % lineColors.size()];
            double lineAlpha = lineAlphas[lineIndex % lineAlphas.size()];
            double lineWidth = lineWidths[lineIndex % lineWidths.size()];
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            
            if (lineAlpha != 1.0)
                lineColor.setAlphaF(lineAlpha);
            
            painter.strokePath(linePath, QPen(lineColor, lineWidth));
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = x1data_[dataIndex];
    double *ydata = y1data_[dataIndex];
    int vertexCount = data_count_[dataIndex];
    QColor lineColor = (*color_[dataIndex])[0];                 // guaranteed to be only one value, for lines()
    double lineAlpha = (*alpha_[dataIndex])[0];                 // guaranteed to be only one value, for lines()
    double lineWidth = (*line_width_[dataIndex])[0];            // guaranteed to be only one value, for lines()
    
    if (lineAlpha != 1.0)
    {
        // This strokes each line segment as a separate path, so that when successive line segments cross, the alpha
        // value affects their area of overlap.  This is arguably more likely to be what the user expects.  However,
        // it doesn't draw the line joins nicely, with bevels and such, so the line path as a whole might less pretty.
        // We therefore use this drawing method only when alpha is not 1.0.
        lineColor.setAlphaF(lineAlpha);
        
        for (int vertexIndex = 0; vertexIndex < vertexCount - 1; ++vertexIndex)
        {
            QPainterPath linePath;
            double user_x1 = xdata[vertexIndex];
            double user_y1 = ydata[vertexIndex];
            double user_x2 = xdata[vertexIndex + 1];
            double user_y2 = ydata[vertexIndex + 1];
            
            if (!std::isnan(user_x1) && !std::isnan(user_y1) && !std::isnan(user_x2) && !std::isnan(user_y2))
            {
                QPointF devicePoint1(plotToDeviceX(user_x1, interiorRect), plotToDeviceY(user_y1, interiorRect));
                QPointF devicePoint2(plotToDeviceX(user_x2, interiorRect), plotToDeviceY(user_y2, interiorRect));
                
                linePath.moveTo(devicePoint1);
                linePath.lineTo(devicePoint2);
                painter.strokePath(linePath, QPen(lineColor, lineWidth));
            }
        }
    }
    else
    {
        QPainterPath linePath;
        bool startedLine = false;
        
        for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            double user_x = xdata[vertexIndex];
            double user_y = ydata[vertexIndex];
            
            if (!std::isnan(user_x) && !std::isnan(user_y))
            {
                QPointF devicePoint(plotToDeviceX(user_x, interiorRect), plotToDeviceY(user_y, interiorRect));
                
                if (startedLine)    linePath.lineTo(devicePoint);
                else                linePath.moveTo(devicePoint);
                
                startedLine = true;
            }
            else
            {
                // a NAN value for x or y interrupts the line being plotted; INF values are plotted, but don't affect axis ranges
                startedLine = false;
            }
        }
        
        if (lineAlpha != 1.0)
            lineColor.setAlphaF(lineAlpha);
        
        painter.strokePath(linePath, QPen(lineColor, lineWidth));
    }
}

void QtSLiMGraphView_CustomPlot::drawMarginText(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = x1data_[dataIndex];
    double *ydata = y1data_[dataIndex];
    std::vector<QString> &labels = *labels_[dataIndex];
    int pointCount = data_count_[dataIndex];
    std::vector<QColor> &textColors = *color_[dataIndex];
    std::vector<double> &textAlphas = *alpha_[dataIndex];
    std::vector<double> &textAngles = *angle_[dataIndex];
    std::vector<double> &pointSizes = *size_[dataIndex];
    double xadj = xadj_[dataIndex];
    double yadj = yadj_[dataIndex];
    
    // move clipping area outward to encompass our entire parent widget
    QRect bounds = rect();
    
    painter.save();
    painter.setClipRect(bounds, Qt::ReplaceClip);
    
    //QtSLiMFrameRect(interiorRect, Qt::green, painter);
    
    // set up to get the font and font info
    double lastPointSize = -1;
    QFont labelFont;
    double capHeight = 0;
    
    for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)
    {
        double user_x = xdata[pointIndex];
        double user_y = ydata[pointIndex];
        
        if (std::isfinite(user_x) && std::isfinite(user_y))
        {
            // for mtext(), coordinates inside the plot area are in [0,1]
            QString &labelText = labels[pointIndex];
            double x = user_x * interiorRect.width() + interiorRect.x();
            double y = user_y * interiorRect.height() + interiorRect.y();
            
            //qDebug() << "labelText ==" << labelText << ", user_x ==" << user_x << ", user_y ==" << user_y << ", x ==" << x << ", y ==" << y;
            
            // translate the painter so (x, y) is at (0, 0)
            painter.save();
            painter.translate(x, y);
            x = 0;
            y = 0;
            
            double pointSize = pointSizes[pointIndex % pointSizes.size()];
            
            if (pointSize != lastPointSize)
            {
                labelFont = QtSLiMGraphView::labelFontOfPointSize(pointSize);
                capHeight = QFontMetricsF(labelFont).capHeight();
                painter.setFont(labelFont);
                
                lastPointSize = pointSize;
            }
            
            QColor textColor = textColors[pointIndex % textColors.size()];
            double alpha = textAlphas[pointIndex % textAlphas.size()];
            
            if (alpha != 1.0)
                textColor.setAlphaF(alpha);
            
            painter.setPen(textColor);
            
            QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
            
            // labelBoundingRect is useful for its width, which seems to be calculated correctly; its height, however, is oddly large, and
            // is not useful, so we use the capHeight from the font metrics instead.  This means that vertically centered (yadj == 0.5) is
            // the midpoint between the baseline and the capHeight, which I think is probably the best behavior.
            double labelWidth = labelBoundingRect.width();
            double labelHeight = capHeight;
            double labelX = x - SLIM_SCREEN_ROUND(labelWidth * xadj);
            double labelY = y - SLIM_SCREEN_ROUND(labelHeight * yadj);
            
            //qDebug() << "   labelBoundingRect ==" << labelBoundingRect << ", labelWidth ==" << labelWidth << ", labelHeight ==" << labelHeight << ", capHeight ==" << capHeight;
            //qDebug() << "   labelX ==" << labelX << ", labelY ==" << labelY;
            
            // rotate the coordinate system around (x, y); for example, -10.0 is 10 degrees clockwise
            double textAngle = textAngles[pointIndex];
            
            if (textAngle != 0.0)
                painter.rotate(-textAngles[pointIndex]);
            
#if 0
            // draw the axes for the text around the pivot point
            QPainterPath linePath;
            
            linePath.moveTo(-50, 0);
            linePath.lineTo(50, 0);
            linePath.moveTo(0, -50);
            linePath.lineTo(0, 50);
            painter.strokePath(linePath, QPen(Qt::green, 1.0));
#endif
            
            // flip vertically so the text is upright, and then use -labelY since we're flipped
            painter.scale(1.0, -1.0);
            
            painter.drawText(QPointF(labelX, -labelY), labelText);
            
            painter.restore();
        }
        else
        {
            // a NAN or INF value for x or y is not plotted
        }
    }
    
    painter.restore();
}

void QtSLiMGraphView_CustomPlot::drawRects(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *x1data = x1data_[dataIndex];
    double *y1data = y1data_[dataIndex];
    double *x2data = x2data_[dataIndex];
    double *y2data = y2data_[dataIndex];
    int segmentCount = data_count_[dataIndex];
    std::vector<QColor> &colors = *color_[dataIndex];
    std::vector<QColor> &borderColors = *border_[dataIndex];
    std::vector<double> &alphas = *alpha_[dataIndex];
    std::vector<double> &lineWidths = *line_width_[dataIndex];
    
    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
    {
        double user_x1 = x1data[segmentIndex];
        double user_y1 = y1data[segmentIndex];
        double user_x2 = x2data[segmentIndex];
        double user_y2 = y2data[segmentIndex];
        
        if (!std::isnan(user_x1) && !std::isnan(user_y1) && !std::isnan(user_x2) && !std::isnan(user_y2))
        {
            double device_x1 = plotToDeviceX(user_x1, interiorRect);
            double device_y1 = plotToDeviceY(user_y1, interiorRect);
            double device_x2 = plotToDeviceX(user_x2, interiorRect);
            double device_y2 = plotToDeviceY(user_y2, interiorRect);
            QColor color = colors[segmentIndex % colors.size()];
            QColor borderColor = borderColors[segmentIndex % borderColors.size()];
            double alpha = alphas[segmentIndex % alphas.size()];
            double lineWidth = lineWidths[segmentIndex % lineWidths.size()];
            
            if (color.alphaF() != 0.0f)
            {
                // fill the rect
                if (alpha != 1.0)
                    color.setAlphaF(alpha);
                
                QRectF rect(device_x1, device_y1, device_x2 - device_x1, device_y2 - device_y1);
                
                painter.fillRect(rect, color);
            }
            
            if (borderColor.alphaF() != 0.0f)
            {
                // frame the rect
                if (alpha != 1.0)
                    borderColor.setAlphaF(alpha);
                
                QPainterPath linePath;
                linePath.moveTo(device_x1, device_y1);
                linePath.lineTo(device_x2, device_y1);
                linePath.lineTo(device_x2, device_y2);
                linePath.lineTo(device_x1, device_y2);
                linePath.closeSubpath();
                painter.strokePath(linePath, QPen(borderColor, lineWidth));
            }
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawSegments(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *x1data = x1data_[dataIndex];
    double *y1data = y1data_[dataIndex];
    double *x2data = x2data_[dataIndex];
    double *y2data = y2data_[dataIndex];
    int segmentCount = data_count_[dataIndex];
    std::vector<QColor> &colors = *color_[dataIndex];
    std::vector<double> &alphas = *alpha_[dataIndex];
    std::vector<double> &lineWidths = *line_width_[dataIndex];
    
    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
    {
        QPainterPath linePath;
        double user_x1 = x1data[segmentIndex];
        double user_y1 = y1data[segmentIndex];
        double user_x2 = x2data[segmentIndex];
        double user_y2 = y2data[segmentIndex];
        
        if (!std::isnan(user_x1) && !std::isnan(user_y1) && !std::isnan(user_x2) && !std::isnan(user_y2))
        {
            QPointF devicePoint1(plotToDeviceX(user_x1, interiorRect), plotToDeviceY(user_y1, interiorRect));
            QPointF devicePoint2(plotToDeviceX(user_x2, interiorRect), plotToDeviceY(user_y2, interiorRect));
            QColor color = colors[segmentIndex % colors.size()];
            double alpha = alphas[segmentIndex % alphas.size()];
            double lineWidth = lineWidths[segmentIndex % lineWidths.size()];
            
            if (alpha != 1.0)
                color.setAlphaF(alpha);
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            painter.strokePath(linePath, QPen(color, lineWidth));
        }
    }
}


void QtSLiMGraphView_CustomPlot::drawPoints(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = x1data_[dataIndex];
    double *ydata = y1data_[dataIndex];
    int pointCount = data_count_[dataIndex];
    std::vector<int> &symbols = *symbol_[dataIndex];
    std::vector<QColor> &symbolColors = *color_[dataIndex];
    std::vector<QColor> &borderColors = *border_[dataIndex];
    std::vector<double> &alphas = *alpha_[dataIndex];
    std::vector<double> &lineWidths = *line_width_[dataIndex];
    std::vector<double> &sizes = *size_[dataIndex];
    
    for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)
    {
        double user_x = xdata[pointIndex];
        double user_y = ydata[pointIndex];
        
        // given that the line width, color, etc. can change with each symbol, we just plot each symbol individually
        if (std::isfinite(user_x) && std::isfinite(user_y))
        {
            double x = plotToDeviceX(user_x, interiorRect);
            double y = plotToDeviceY(user_y, interiorRect);
            int symbol = symbols[pointIndex % symbols.size()];
            QColor symbolColor = symbolColors[pointIndex % symbolColors.size()];
            QColor borderColor = borderColors[pointIndex % borderColors.size()];
            double alpha = alphas[pointIndex % alphas.size()];
            double lineWidth = lineWidths[pointIndex % lineWidths.size()];
            double size = sizes[pointIndex % sizes.size()];
            
            drawPointSymbol(painter, x, y, symbol, symbolColor, borderColor, alpha, lineWidth, size);
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawText(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = x1data_[dataIndex];
    double *ydata = y1data_[dataIndex];
    std::vector<QString> &labels = *labels_[dataIndex];
    int pointCount = data_count_[dataIndex];
    std::vector<QColor> &textColors = *color_[dataIndex];
    std::vector<double> &textAlphas = *alpha_[dataIndex];
    std::vector<double> &textAngles = *angle_[dataIndex];
    std::vector<double> &pointSizes = *size_[dataIndex];
    double xadj = xadj_[dataIndex];
    double yadj = yadj_[dataIndex];
    
    // set up to get the font and font info
    double lastPointSize = -1;
    QFont labelFont;
    double capHeight = 0;
    
    for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)
    {
        double user_x = xdata[pointIndex];
        double user_y = ydata[pointIndex];
        
        if (std::isfinite(user_x) && std::isfinite(user_y))
        {
            QString &labelText = labels[pointIndex];
            double x = plotToDeviceX(user_x, interiorRect);
            double y = plotToDeviceY(user_y, interiorRect);
            
            //qDebug() << "labelText ==" << labelText << ", user_x ==" << user_x << ", user_y ==" << user_y << ", x ==" << x << ", y ==" << y;
            
            // translate the painter so (x, y) is at (0, 0)
            painter.save();
            painter.translate(x, y);
            x = 0;
            y = 0;
            
            double pointSize = pointSizes[pointIndex % pointSizes.size()];
            
            if (pointSize != lastPointSize)
            {
                labelFont = QtSLiMGraphView::labelFontOfPointSize(pointSize);
                capHeight = QFontMetricsF(labelFont).capHeight();
                painter.setFont(labelFont);
                
                lastPointSize = pointSize;
            }
            
            QColor textColor = textColors[pointIndex % textColors.size()];
            double alpha = textAlphas[pointIndex % textAlphas.size()];
            
            if (alpha != 1.0)
                textColor.setAlphaF(alpha);
            
            painter.setPen(textColor);
            
            QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
            
            // labelBoundingRect is useful for its width, which seems to be calculated correctly; its height, however, is oddly large, and
            // is not useful, so we use the capHeight from the font metrics instead.  This means that vertically centered (yadj == 0.5) is
            // the midpoint between the baseline and the capHeight, which I think is probably the best behavior.
            double labelWidth = labelBoundingRect.width();
            double labelHeight = capHeight;
            double labelX = x - SLIM_SCREEN_ROUND(labelWidth * xadj);
            double labelY = y - SLIM_SCREEN_ROUND(labelHeight * yadj);
            
            //qDebug() << "   labelBoundingRect ==" << labelBoundingRect << ", labelWidth ==" << labelWidth << ", labelHeight ==" << labelHeight << ", capHeight ==" << capHeight;
            //qDebug() << "   labelX ==" << labelX << ", labelY ==" << labelY;
            
            // rotate the coordinate system around (x, y); for example, -10.0 is 10 degrees clockwise
            double textAngle = textAngles[pointIndex];
            
            if (textAngle != 0.0)
                painter.rotate(-textAngles[pointIndex]);
                
#if 0
            // draw the axes for the text around the pivot point
            QPainterPath linePath;
            
            linePath.moveTo(-50, 0);
            linePath.lineTo(50, 0);
            linePath.moveTo(0, -50);
            linePath.lineTo(0, 50);
            painter.strokePath(linePath, QPen(Qt::green, 1.0));
#endif
            
            // flip vertically so the text is upright, and then use -labelY since we're flipped
            painter.scale(1.0, -1.0);
            
            painter.drawText(QPointF(labelX, -labelY), labelText);
            
            painter.restore();
        }
        else
        {
            // a NAN or INF value for x or y is not plotted
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawImage(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = x1data_[dataIndex];
    double *ydata = y1data_[dataIndex];
    double alpha = (*alpha_[dataIndex])[0];
    
    double user_x1 = xdata[0], user_y1 = ydata[0];
    double user_x2 = xdata[1], user_y2 = ydata[1];
    
    // for image() we always want to use pixel edges, as in PDF, not pixel centers, so that the plotted image
    // uses up the full pixels at the edges of the plot area if the image fills the whole plot area
    bool old_generatingPDF_ = generatingPDF_;
    generatingPDF_ = true;
    
    double x1 = plotToDeviceX(user_x1, interiorRect);
    double y1 = plotToDeviceY(user_y1, interiorRect);
    double x2 = plotToDeviceX(user_x2, interiorRect);
    double y2 = plotToDeviceY(user_y2, interiorRect);
    
    generatingPDF_ = old_generatingPDF_;
    
    // the coordinates are absolute, but Qt wants them as width/height
    double target_width = x2 - x1;
    double target_height = y2 - y1;
    
    // get the image data
    const QImage &image = image_[dataIndex];
    
    QRectF target(x1, y1, target_width, target_height);
    
    if (alpha != 1.0)
        painter.setOpacity(alpha);
    
    // we do not want antialiasing of images drawn here; unfortunately that is difficult, because Qt ignores its render hints
    // in some cases and still gives us an interpolated image, so we also have to scale the image itself sometimes
    bool old_antialiasing = painter.testRenderHint(QPainter::Antialiasing);
    bool old_smoothpixmap = painter.testRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    
    if (generatingPDF_)
    {
        const QImage scaledImage = image.scaled(target_width, target_height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        
        painter.drawImage(target, scaledImage);
    }
    else
    {
        // for on-screen display I have not seen it smooth the rescale, and I don't want the overhead of making a new image every time...
        painter.drawImage(target, image);
    }
    
    painter.setRenderHint(QPainter::Antialiasing, old_antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, old_smoothpixmap);
    
    if (alpha != 1.0)
        painter.setOpacity(1.0);
}















































