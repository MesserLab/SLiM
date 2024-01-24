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


QtSLiMGraphView_CustomPlot::QtSLiMGraphView_CustomPlot(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    title_ = "Custom Plot";     // will be replaced
    xAxisLabel_ = "x";
    yAxisLabel_ = "y";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    setFocalDisplaySpecies(nullptr);
    
    QtSLiMGraphView_CustomPlot::updateAfterTick();
}

void QtSLiMGraphView_CustomPlot::freeData(void)
{
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
    labels_.clear();
    data_count_.clear();
    symbol_.clear();
    color_.clear();
    border_.clear();
    line_width_.clear();
    size_.clear();
    xadj_.clear();
    yadj_.clear();
}

QtSLiMGraphView_CustomPlot::~QtSLiMGraphView_CustomPlot()
{
    // We are responsible for our own destruction
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

void QtSLiMGraphView_CustomPlot::setAxisRanges(double *x_range, double *y_range)
{
    // nullptr for an axis indicates that we want that axis to be controlled by the range of the data
    // otherwise, we set up the min and max values for the axis from the given (two-valued) range buffer
    if (x_range)
    {
        configureAxisForRange(x_range[0], x_range[1], xAxisMin_, xAxisMax_, xAxisMajorTickInterval_, xAxisMinorTickInterval_,
                              xAxisMajorTickModulus_, xAxisTickValuePrecision_);
        xAxisIsUserRescaled_ = true;
    }
    else
    {
        // leave the axis as it was, so that any user configuration persists through a recycle
    }
    
    if (y_range)
    {
        configureAxisForRange(y_range[0], y_range[1], yAxisMin_, yAxisMax_, yAxisMajorTickInterval_, yAxisMinorTickInterval_,
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
    // set up axes based on the data range; we try to apply a little intelligence, but if the user
    // wants really intelligent axis ranges, they can set them up themselves...
    double xmin, xmax, ymin, ymax;
    
    dataRange(xdata_, &xmin, &xmax);
    dataRange(ydata_, &ymin, &ymax);
    
    has_finite_data_ = false;
    
    if (std::isfinite(xmin) && std::isfinite(xmax) && std::isfinite(ymin) && std::isfinite(ymax))
    {
        if (!xAxisIsUserRescaled_)
            configureAxisForRange(xmin, xmax, xAxisMin_, xAxisMax_, xAxisMajorTickInterval_, xAxisMinorTickInterval_,
                                  xAxisMajorTickModulus_, xAxisTickValuePrecision_);
        
        if (!yAxisIsUserRescaled_)
            configureAxisForRange(ymin, ymax, yAxisMin_, yAxisMax_, yAxisMajorTickInterval_, yAxisMinorTickInterval_,
                                  yAxisMajorTickModulus_, yAxisTickValuePrecision_);
        
        has_finite_data_ = true;
    }
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

QString QtSLiMGraphView_CustomPlot::graphTitle(void)
{
    return title_;
}

QString QtSLiMGraphView_CustomPlot::aboutString(void)
{
    return "The Custom Plot graph type displays user-provided data that is supplied "
           "in script with calls to plotLines(), plotPoints(), and plotText().";
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
        }
    }
}

void QtSLiMGraphView_CustomPlot::appendStringForData(QString & /* string */)
{
    // No data string
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
        
        size = size * 3.5;       // this scales what size=1 looks like
        
        // given that the line width, color, etc. can change with each symbol, we just plot each symbol individually
        if (std::isfinite(user_x) && std::isfinite(user_y))
        {
            double x = plotToDeviceX(user_x, interiorRect);
            double y = plotToDeviceY(user_y, interiorRect);
            
            switch (symbol)
            {
            case 0:     // square outline
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addRect(x - size, y - size, size * 2, size * 2);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 1:     // circle outline
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addEllipse(x - size * 1.13, y - size * 1.13, size * 2 * 1.13, size * 2 * 1.13);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 2:     // triangle outline pointing up
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x, y + size * 1.4);
                symbolStrokePath.lineTo(x + 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolStrokePath.lineTo(x - 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolStrokePath.closeSubpath();
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 3:     // orthogonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x, y + size);
                symbolStrokePath.lineTo(x, y - size);
                symbolStrokePath.moveTo(x + size, y);
                symbolStrokePath.lineTo(x - size, y);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 4:     // diagonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x + size * 0.7071, y + size * 0.7071);
                symbolStrokePath.lineTo(x - size * 0.7071, y - size * 0.7071);
                symbolStrokePath.moveTo(x + size * 0.7071, y - size * 0.7071);
                symbolStrokePath.lineTo(x - size * 0.7071, y + size * 0.7071);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 5:     // diamond outline
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x + size * 1.3, y);
                symbolStrokePath.lineTo(x, y - size * 1.3);
                symbolStrokePath.lineTo(x - size * 1.3, y);
                symbolStrokePath.lineTo(x, y + size * 1.3);
                symbolStrokePath.closeSubpath();
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 6:     // triangle outline pointing down
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x, y - size * 1.4);
                symbolStrokePath.lineTo(x + 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolStrokePath.lineTo(x - 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolStrokePath.closeSubpath();
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 7:     // square outline with diagonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addRect(x - size, y - size, size * 2, size * 2);
                symbolStrokePath.moveTo(x + size * 0.93, y + size * 0.93);
                symbolStrokePath.lineTo(x - size * 0.93, y - size * 0.93);
                symbolStrokePath.moveTo(x + size * 0.93, y - size * 0.93);
                symbolStrokePath.lineTo(x - size * 0.93, y + size * 0.93);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 8:     // 8-pointed asterisk
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x, y + size);
                symbolStrokePath.lineTo(x, y - size);
                symbolStrokePath.moveTo(x + size, y);
                symbolStrokePath.lineTo(x - size, y);
                symbolStrokePath.moveTo(x + size * 0.7071, y + size * 0.7071);
                symbolStrokePath.lineTo(x - size * 0.7071, y - size * 0.7071);
                symbolStrokePath.moveTo(x + size * 0.7071, y - size * 0.7071);
                symbolStrokePath.lineTo(x - size * 0.7071, y + size * 0.7071);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 9:     // diamond with orthogonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x + size * 1.3, y);
                symbolStrokePath.lineTo(x, y - size * 1.3);
                symbolStrokePath.lineTo(x - size * 1.3, y);
                symbolStrokePath.lineTo(x, y + size * 1.3);
                symbolStrokePath.closeSubpath();
                symbolStrokePath.moveTo(x, y + size * 1.2);
                symbolStrokePath.lineTo(x, y - size * 1.2);
                symbolStrokePath.moveTo(x + size * 1.2, y);
                symbolStrokePath.lineTo(x - size * 1.2, y);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 10:     // circle outline with orthogonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addEllipse(x - size * 1.13, y - size * 1.13, size * 2 * 1.13, size * 2 * 1.13);
                symbolStrokePath.moveTo(x, y + size * 1.05);
                symbolStrokePath.lineTo(x, y - size * 1.05);
                symbolStrokePath.moveTo(x + size * 1.05, y);
                symbolStrokePath.lineTo(x - size * 1.05, y);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 11:     // six-pointed star outline (overlapping triangles)
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.moveTo(x, y + size * 1.4);
                symbolStrokePath.lineTo(x + 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolStrokePath.lineTo(x - 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolStrokePath.closeSubpath();
                symbolStrokePath.moveTo(x, y - size * 1.4);
                symbolStrokePath.lineTo(x + 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolStrokePath.lineTo(x - 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolStrokePath.closeSubpath();
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 12:     // square outline with orthogonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addRect(x - size, y - size, size * 2, size * 2);
                symbolStrokePath.moveTo(x, y + size * 0.9);
                symbolStrokePath.lineTo(x, y - size * 0.9);
                symbolStrokePath.moveTo(x + size * 0.9, y);
                symbolStrokePath.lineTo(x - size * 0.9, y);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 13:     // circle outline with diagonal cross
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addEllipse(x - size * 1.13, y - size * 1.13, size * 2 * 1.13, size * 2 * 1.13);
                symbolStrokePath.moveTo(x + size * 0.75, y + size * 0.75);
                symbolStrokePath.lineTo(x - size * 0.75, y - size * 0.75);
                symbolStrokePath.moveTo(x + size * 0.75, y - size * 0.75);
                symbolStrokePath.lineTo(x - size * 0.75, y + size * 0.75);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 14:     // square with embedded triangle
            {
                QPainterPath symbolStrokePath;
                symbolStrokePath.addRect(x - size, y - size, size * 2, size * 2);
                symbolStrokePath.moveTo(x - size, y - size);
                symbolStrokePath.lineTo(x, y + size);
                symbolStrokePath.lineTo(x + size, y - size);
                painter.strokePath(symbolStrokePath, QPen(symbolColor, lineWidth));
                break;
            }
            case 15:     // square filled
            {
                QPainterPath symbolFillPath;
                symbolFillPath.addRect(x - size, y - size, size * 2, size * 2);
                painter.fillPath(symbolFillPath, symbolColor);
                break;
            }
            case 16:     // circle filled
            {
                QPainterPath symbolFillPath;
                symbolFillPath.addEllipse(x - size * 1.13, y - size * 1.13, size * 2 * 1.13, size * 2 * 1.13);
                painter.fillPath(symbolFillPath, symbolColor);
                break;
            }
            case 17:     // triangle filled pointing up
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x, y + size * 1.4);
                symbolFillPath.lineTo(x + 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolFillPath.lineTo(x - 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolFillPath.closeSubpath();
                painter.fillPath(symbolFillPath, symbolColor);
                break;
            }
            case 18:     // diamond filled
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x + size * 1.3, y);
                symbolFillPath.lineTo(x, y - size * 1.3);
                symbolFillPath.lineTo(x - size * 1.3, y);
                symbolFillPath.lineTo(x, y + size * 1.3);
                symbolFillPath.closeSubpath();
                painter.fillPath(symbolFillPath, symbolColor);
                break;
            }
            case 19:     // triangle filled pointing down
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x, y - size * 1.4);
                symbolFillPath.lineTo(x + 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolFillPath.lineTo(x - 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolFillPath.closeSubpath();
                painter.fillPath(symbolFillPath, symbolColor);
                break;
            }
            case 20:     // six-pointed star filled (overlapping triangles)
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x, y + size * 1.4);
                symbolFillPath.lineTo(x + 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolFillPath.lineTo(x - 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolFillPath.closeSubpath();
                symbolFillPath.moveTo(x, y - size * 1.4);
                symbolFillPath.lineTo(x - 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolFillPath.lineTo(x + 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolFillPath.closeSubpath();
                symbolFillPath.setFillRule(Qt::WindingFill);
                painter.fillPath(symbolFillPath, symbolColor);
                break;
            }
            case 21:     // circle filled and stroked
            {
                QPainterPath symbolFillPath;
                symbolFillPath.addEllipse(x - size * 1.13, y - size * 1.13, size * 2 * 1.13, size * 2 * 1.13);
                painter.fillPath(symbolFillPath, symbolColor);
                painter.strokePath(symbolFillPath, QPen(borderColor, lineWidth));
                break;
            }
            case 22:     // square filled and stroked
            {
                QPainterPath symbolFillPath;
                symbolFillPath.addRect(x - size, y - size, size * 2, size * 2);
                painter.fillPath(symbolFillPath, symbolColor);
                painter.strokePath(symbolFillPath, QPen(borderColor, lineWidth));
                break;
            }
            case 23:     // diamond filled and stroked
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x + size * 1.3, y);
                symbolFillPath.lineTo(x, y - size * 1.3);
                symbolFillPath.lineTo(x - size * 1.3, y);
                symbolFillPath.lineTo(x, y + size * 1.3);
                symbolFillPath.closeSubpath();
                painter.fillPath(symbolFillPath, symbolColor);
                painter.strokePath(symbolFillPath, QPen(borderColor, lineWidth));
                break;
            }
            case 24:     // triangle filled and stroked pointing up
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x, y + size * 1.4);
                symbolFillPath.lineTo(x + 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolFillPath.lineTo(x - 0.8660 * size * 1.4, y - 0.5 * size * 1.4);
                symbolFillPath.closeSubpath();
                painter.fillPath(symbolFillPath, symbolColor);
                painter.strokePath(symbolFillPath, QPen(borderColor, lineWidth));
                break;
            }
            case 25:     // triangle filled and stroked pointing down
            {
                QPainterPath symbolFillPath;
                symbolFillPath.moveTo(x, y - size * 1.4);
                symbolFillPath.lineTo(x + 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolFillPath.lineTo(x - 0.8660 * size * 1.4, y + 0.5 * size * 1.4);
                symbolFillPath.closeSubpath();
                painter.fillPath(symbolFillPath, symbolColor);
                painter.strokePath(symbolFillPath, QPen(borderColor, lineWidth));
                break;
            }
            default:        // other symbols draw nothing
                break;
            }
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















































