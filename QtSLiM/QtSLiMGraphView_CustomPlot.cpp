//
//  QtSLiMGraphView_CustomPlot.cpp
//  SLiM
//
//  Created by Ben Haller on 1/19/2024.
//  Copyright (c) 2024 Philipp Messer.  All rights reserved.
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
    for (double *xbuffer : xdata_)
        free(xbuffer);
    
    for (double *ybuffer : ydata_)
        free(ybuffer);
    
    for (std::vector<QString> *labelsbuffer : labels_)
        delete labelsbuffer;
    
    for (std::vector<int> *symbolbuffer : symbol_)
        delete symbolbuffer;
    
    for (std::vector<QColor> *colorbuffer : color_)
        delete colorbuffer;
    
    for (std::vector<QColor> *borderbuffer : border_)
        delete borderbuffer;
    
    for (std::vector<double> *lwdbuffer : line_width_)
        delete lwdbuffer;
    
    for (std::vector<double> *sizebuffer : size_)
        delete sizebuffer;
    
    plot_type_.clear();
    xdata_.clear();
    ydata_.clear();
    data_count_.clear();
    labels_.clear();
    symbol_.clear();
    color_.clear();
    border_.clear();
    line_width_.clear();
    size_.clear();
    xadj_.clear();
    yadj_.clear();
    
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

void QtSLiMGraphView_CustomPlot::setLegendPosition(QtSLiM_LegendPosition position)
{
    legend_position_ = position;
    update();
}

void QtSLiMGraphView_CustomPlot::setAxisRanges(double *x_range, double *y_range)
{
    // this is called by QtSLiMWindow::eidos_createPlot(), to set up for the user's specified ranges
    // nullptr for an axis indicates that we want that axis to be controlled by the range of the data
    // otherwise, we set up the min and max values for the axis from the given (two-valued) range buffer
    if (x_range)
    {
        x0_ = x_range[0];
        x1_ = x_range[1];
        
        configureAxisForRange(x0_, x1_, xAxisMin_, xAxisMax_,
                              xAxisMajorTickInterval_, xAxisMinorTickInterval_,
                              xAxisMajorTickModulus_, xAxisTickValuePrecision_);
        xAxisIsUserRescaled_ = true;
    }
    else
    {
        // leave the axis as it was, so that any user configuration persists through a recycle
    }
    
    if (y_range)
    {
        y0_ = y_range[0];
        y1_ = y_range[1];
        
        configureAxisForRange(y0_, y1_, yAxisMin_, yAxisMax_,
                              yAxisMajorTickInterval_, yAxisMinorTickInterval_,
                              yAxisMajorTickModulus_, yAxisTickValuePrecision_);
        yAxisIsUserRescaled_ = true;
    }
    else
    {
        // leave the axis as it was, so that any user configuration persists through a recycle
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
    
    *p_min = min;
    *p_max = max;
}

void QtSLiMGraphView_CustomPlot::rescaleAxesForDataRange(void)
{
    // this is called when new data is added to a plot, to rescale the axes as needed
    // set up axes based on the data range; we try to apply a little intelligence, but if the user
    // wants really intelligent axis ranges, they can set them up themselves...
    double xmin, xmax, ymin, ymax;
    
    dataRange(xdata_, &xmin, &xmax);
    dataRange(ydata_, &ymin, &ymax);
    
    has_finite_data_ = false;
    
    if (std::isfinite(xmin) && std::isfinite(xmax) && std::isfinite(ymin) && std::isfinite(ymax))
    {
        if (!xAxisIsUserRescaled_)
        {
            x0_ = xmin;
            x1_ = xmax;
            
            configureAxisForRange(x0_, x1_, xAxisMin_, xAxisMax_, xAxisMajorTickInterval_, xAxisMinorTickInterval_,
                                  xAxisMajorTickModulus_, xAxisTickValuePrecision_);
        }
        
        if (!yAxisIsUserRescaled_)
        {
            y0_ = ymin;
            y1_ = ymax;
            
            configureAxisForRange(y0_, y1_, yAxisMin_, yAxisMax_, yAxisMajorTickInterval_, yAxisMinorTickInterval_,
                                  yAxisMajorTickModulus_, yAxisTickValuePrecision_);
        }
        
        has_finite_data_ = true;
    }
}

void QtSLiMGraphView_CustomPlot::addABLineData(double *a_values, double *b_values,
                                               double *h_values, double *v_values, int data_count,
                                               std::vector<QColor> *color, std::vector<double> *lwd)
{
    if (a_values)
    {
        plot_type_.push_back(QtSLiM_CustomPlotType::kABLines);
        xdata_.push_back(a_values);
        ydata_.push_back(b_values);
    }
    else if (h_values)
    {
        plot_type_.push_back(QtSLiM_CustomPlotType::kHLines);
        xdata_.push_back(h_values);
        ydata_.push_back(nullptr);
    }
    else if (v_values)
    {
        plot_type_.push_back(QtSLiM_CustomPlotType::kVLines);
        xdata_.push_back(v_values);
        ydata_.push_back(nullptr);
    }
    
    labels_.push_back(nullptr);             // unused for abline
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for abline
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for abline
    line_width_.push_back(lwd);
    size_.push_back(nullptr);               // unused for abline
    xadj_.push_back(-1);                    // unused for abline
    yadj_.push_back(-1);                    // unused for abline
    
    //rescaleAxesForDataRange();            // not needed for abline
    update();
}

void QtSLiMGraphView_CustomPlot::addLineData(double *x_values, double *y_values, int data_count,
                                             std::vector<QColor> *color, std::vector<double> *lwd)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kLines);
    xdata_.push_back(x_values);
    ydata_.push_back(y_values);
    labels_.push_back(nullptr);             // unused for lines
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for lines
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for lines
    line_width_.push_back(lwd);
    size_.push_back(nullptr);               // unused for lines
    xadj_.push_back(-1);                    // unused for lines
    yadj_.push_back(-1);                    // unused for lines
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addPointData(double *x_values, double *y_values, int data_count,
                                              std::vector<int> *symbol, std::vector<QColor> *color, std::vector<QColor> *border,
                                              std::vector<double> *lwd, std::vector<double> *size)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kPoints);
    xdata_.push_back(x_values);
    ydata_.push_back(y_values);
    labels_.push_back(nullptr);             // unused for lines
    data_count_.push_back(data_count);
    symbol_.push_back(symbol);
    color_.push_back(color);
    border_.push_back(border);
    line_width_.push_back(lwd);
    size_.push_back(size);
    xadj_.push_back(-1);                    // unused for points
    yadj_.push_back(-1);                    // unused for points
    
    rescaleAxesForDataRange();
    update();
}

void QtSLiMGraphView_CustomPlot::addTextData(double *x_values, double *y_values, std::vector<QString> *labels, int data_count,
                                             std::vector<QColor> *color, std::vector<double> *size, double *adj)
{
    plot_type_.push_back(QtSLiM_CustomPlotType::kText);
    xdata_.push_back(x_values);
    ydata_.push_back(y_values);
    labels_.push_back(labels);
    data_count_.push_back(data_count);
    symbol_.push_back(nullptr);             // unused for text
    color_.push_back(color);
    border_.push_back(nullptr);             // unused for text
    line_width_.push_back(nullptr);         // unused for text
    size_.push_back(size);
    xadj_.push_back(adj[0]);
    yadj_.push_back(adj[1]);
    
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
    double *adata = xdata_[dataIndex];
    double *bdata = ydata_[dataIndex];
    int lineCount = data_count_[dataIndex];
    std::vector<QColor> &lineColors = *color_[dataIndex];           // might be one value of N values
    std::vector<double> &lineWidths = *line_width_[dataIndex];      // might be one value of N values
    
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
            double lineWidth = lineWidths[lineIndex % lineWidths.size()];
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            
            painter.strokePath(linePath, QPen(lineColor, lineWidth));
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawHLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *hdata = xdata_[dataIndex];
    int lineCount = data_count_[dataIndex];
    std::vector<QColor> &lineColors = *color_[dataIndex];           // might be one value of N values
    std::vector<double> &lineWidths = *line_width_[dataIndex];      // might be one value of N values
    
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
            double lineWidth = lineWidths[lineIndex % lineWidths.size()];
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            
            painter.strokePath(linePath, QPen(lineColor, lineWidth));
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawVLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *vdata = xdata_[dataIndex];
    int lineCount = data_count_[dataIndex];
    std::vector<QColor> &lineColors = *color_[dataIndex];           // might be one value of N values
    std::vector<double> &lineWidths = *line_width_[dataIndex];      // might be one value of N values
    
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
            double lineWidth = lineWidths[lineIndex % lineWidths.size()];
            
            linePath.moveTo(devicePoint1);
            linePath.lineTo(devicePoint2);
            
            painter.strokePath(linePath, QPen(lineColor, lineWidth));
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawLines(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = xdata_[dataIndex];
    double *ydata = ydata_[dataIndex];
    int pointCount = data_count_[dataIndex];
    QColor lineColor = (*color_[dataIndex])[0];         // guaranteed to be only one value, for plotLines()
    double lineWidth = (*line_width_[dataIndex])[0];    // guaranteed to be only one value, for plotLines()
    
    QPainterPath linePath;
    bool startedLine = false;
    
    for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)
    {
        double user_x = xdata[pointIndex];
        double user_y = ydata[pointIndex];
        
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
    
    painter.strokePath(linePath, QPen(lineColor, lineWidth));
}

void QtSLiMGraphView_CustomPlot::drawPoints(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = xdata_[dataIndex];
    double *ydata = ydata_[dataIndex];
    int pointCount = data_count_[dataIndex];
    std::vector<int> &symbols = *symbol_[dataIndex];
    std::vector<QColor> &symbolColors = *color_[dataIndex];
    std::vector<QColor> &borderColors = *border_[dataIndex];
    std::vector<double> &lineWidths = *line_width_[dataIndex];
    std::vector<double> &sizes = *size_[dataIndex];
    
    for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)
    {
        double user_x = xdata[pointIndex];
        double user_y = ydata[pointIndex];
        int symbol = symbols[pointIndex % symbols.size()];
        QColor symbolColor = symbolColors[pointIndex % symbolColors.size()];
        QColor borderColor = borderColors[pointIndex % borderColors.size()];
        double lineWidth = lineWidths[pointIndex % lineWidths.size()];
        double size = sizes[pointIndex % sizes.size()];
        
        // given that the line width, color, etc. can change with each symbol, we just plot each symbol individually
        if (std::isfinite(user_x) && std::isfinite(user_y))
        {
            double x = plotToDeviceX(user_x, interiorRect);
            double y = plotToDeviceY(user_y, interiorRect);
            
            drawPointSymbol(painter, x, y, symbol, symbolColor, borderColor, lineWidth, size);
        }
    }
}

void QtSLiMGraphView_CustomPlot::drawText(QPainter &painter, QRect interiorRect, int dataIndex)
{
    double *xdata = xdata_[dataIndex];
    double *ydata = ydata_[dataIndex];
    std::vector<QString> &labels = *labels_[dataIndex];
    int pointCount = data_count_[dataIndex];
    std::vector<QColor> &textColors = *color_[dataIndex];
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
            double x = plotToDeviceX(user_x, interiorRect);
            double y = plotToDeviceY(user_y, interiorRect);
            double pointSize = pointSizes[pointIndex % pointSizes.size()];
            
            if (pointSize != lastPointSize)
            {
                labelFont = QtSLiMGraphView::labelFontOfPointSize(pointSize);
                capHeight = QFontMetricsF(labelFont).capHeight();
                painter.setFont(labelFont);
                
                lastPointSize = pointSize;
            }
            
            QColor textColor = textColors[pointIndex % textColors.size()];
            
            painter.setPen(textColor);
            
            QString &labelText = labels[pointIndex];
            QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
            
            // labelBoundingRect is useful for its width, which seems to be calculated correctly; its height, however, is oddly large, and
            // is not useful, so we use the capHeight from the font metrics instead.  This means that vertically centered (yadj == 0.5) is
            // the midpoint between the baseline and the capHeight, which I think is probably the best behavior.
            double labelWidth = labelBoundingRect.width();
            double labelHeight = capHeight;
            double labelX = x - SLIM_SCREEN_ROUND(labelWidth * xadj);
            double labelY = y - SLIM_SCREEN_ROUND(labelHeight * yadj);
            
            //qDebug() << "labelText ==" << labelText << ", user_x ==" << user_x << ", user_y ==" << user_y << ", x ==" << x << ", y ==" << y;
            //qDebug() << "   labelBoundingRect ==" << labelBoundingRect << ", labelWidth ==" << labelWidth << ", labelHeight ==" << labelHeight << ", capHeight ==" << capHeight;
            //qDebug() << "   labelX ==" << labelX << ", labelY ==" << labelY;
            
            // we need to correct for the fact that the coordinate system is flipped; text would draw upside-down
            // we do that by transforming labelY and then turning off the world matrix, to turn off the flipping
            // note that labelX transformed is unchanged, since the device coordinate origin is 0 anyway
            QPointF transformedPoint = painter.transform().map(QPointF(labelX, labelY));
            
            labelY = transformedPoint.y();
            
            //qDebug() << "   transformedPoint ==" << transformedPoint << ", labelY ==" << labelY;
            
            painter.setWorldMatrixEnabled(false);
            painter.drawText(QPointF(labelX, labelY), labelText);
            painter.setWorldMatrixEnabled(true);
        }
        else
        {
            // a NAN or INF value for x or y is not plotted
        }
    }
}















































