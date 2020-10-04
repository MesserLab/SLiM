//
//  QtSLiMGraphView.cpp
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

#include "QtSLiMGraphView.h"

#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFileDialog>
#include <QStandardPaths>
#include <QPdfWriter>
#include <QBuffer>
#include <QComboBox>
#include <QtGlobal>
#include <QMessageBox>
#include <QDebug>

#include "subpopulation.h"
#include "genome.h"
#include "mutation_run.h"


QFont QtSLiMGraphView::labelFontOfPointSize(double size)
{
    static QFont timesNewRoman("Times New Roman", 10);
    
    // Derive a font of the proper size, while leaving the original untouched
    QFont font(timesNewRoman);
#ifdef __APPLE__
    font.setPointSizeF(size);
#else
    // font sizes are calibrated for macOS; on Linux they need to be a little smaller
    font.setPointSizeF(size * 0.75);
#endif
    
    return font;
}

QtSLiMGraphView::QtSLiMGraphView(QWidget *parent, QtSLiMWindow *controller) : QWidget(parent)
{
    controller_ = controller;
    
    connect(controller, &QtSLiMWindow::controllerUpdatedAfterTick, this, &QtSLiMGraphView::updateAfterTick);
    connect(controller, &QtSLiMWindow::controllerSelectionChanged, this, &QtSLiMGraphView::controllerSelectionChanged);
    connect(controller, &QtSLiMWindow::controllerGenerationFinished, this, &QtSLiMGraphView::controllerGenerationFinished);
    connect(controller, &QtSLiMWindow::controllerRecycled, this, &QtSLiMGraphView::controllerRecycled);
    
    showXAxis_ = true;
    allowXAxisUserRescale_ = true;
    showXAxisTicks_ = true;
    
    showYAxis_ = true;
    allowYAxisUserRescale_ = true;
    showYAxisTicks_ = true;
    
    xAxisMin_ = 0.0;
    xAxisMax_ = 1.0;
    xAxisMajorTickInterval_ = 0.5;
    xAxisMinorTickInterval_ = 0.25;
    xAxisMajorTickModulus_ = 2;
    xAxisHistogramStyle_ = false;
    xAxisTickValuePrecision_ = 1;

    yAxisMin_ = 0.0;
    yAxisMax_ = 1.0;
    yAxisMajorTickInterval_ = 0.5;
    yAxisMinorTickInterval_ = 0.25;
    yAxisMajorTickModulus_ = 2;
    yAxisTickValuePrecision_ = 1;
    yAxisHistogramStyle_ = false;
    yAxisLog_ = false;
    
    xAxisLabel_ = "This is the x-axis, yo";
    yAxisLabel_ = "This is the y-axis, yo";
    
    legendVisible_ = true;
    showHorizontalGridLines_ = false;
    showVerticalGridLines_ = false;
    showGridLinesMajorOnly_ = false;
    showFullBox_ = false;
    allowHorizontalGridChange_ = true;
    allowVerticalGridChange_ = true;
    allowFullBoxChange_ = true;
}

void QtSLiMGraphView::addedToWindow(void)
{
}

QtSLiMGraphView::~QtSLiMGraphView()
{
    cleanup();
    
    controller_ = nullptr;
}

void QtSLiMGraphView::cleanup()
{
    invalidateDrawingCache();
}

QHBoxLayout *QtSLiMGraphView::buttonLayout(void)
{
    // Note this method makes assumptions about the layouts in the parent window
    // It needs to be kept parallel to QtSLiMWindow::graphWindowWithView()
    QVBoxLayout *topLayout = dynamic_cast<QVBoxLayout *>(window()->layout());
    
    if (topLayout && (topLayout->count() >= 2))
    {
        QLayoutItem *layoutItem = topLayout->itemAt(1);
        QHBoxLayout *buttonLayout = dynamic_cast<QHBoxLayout *>(layoutItem);
        
        return buttonLayout;
    }
    
    return nullptr;
}

QPushButton *QtSLiMGraphView::actionButton(void)
{
    // Note this method makes assumptions about the layouts in the parent window
    // It needs to be kept parallel to QtSLiMWindow::graphWindowWithView()
    QHBoxLayout *layout = buttonLayout();
    int layoutCount = layout ? layout->count() : 0;
    QLayoutItem *buttonItem = (layoutCount > 0) ? layout->itemAt(layoutCount - 1) : nullptr;
    QWidget *buttonWidget = buttonItem ? buttonItem->widget() : nullptr;
    
    return qobject_cast<QPushButton *>(buttonWidget);
}

QComboBox *QtSLiMGraphView::newButtonInLayout(QHBoxLayout *layout)
{
    QComboBox *button = new QComboBox(this);
    button->setEditable(false);
    button->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    button->setMinimumContentsLength(2);
    layout->insertWidget(layout->count() - 2, button);  // left of the spacer and action button
    
    return button;
}

QRect QtSLiMGraphView::interiorRectForBounds(QRect bounds)
{
    QRect interiorRect = bounds;
	
	// For now, 10 pixels margin on a side if there is no axis, 40 pixels margin if there is an axis
	
	if (showXAxis_)
        interiorRect.adjust(50, 0, -10, 0);
	else
        interiorRect.adjust(10, 0, -10, 0);
	
	if (showYAxis_)
        interiorRect.adjust(0, 50, 0, -10);
	else
        interiorRect.adjust(0, 10, 0, -10);
	
	return interiorRect;
}

double QtSLiMGraphView::plotToDeviceX(double plotx, QRect interiorRect)
{
	double fractionAlongAxis = (plotx - xAxisMin_) / (xAxisMax_ - xAxisMin_);
	
    if (generatingPDF_)
    {
        // We go from the left edge of the first pixel to the right edge of the last pixel
        return (fractionAlongAxis * interiorRect.width() + interiorRect.x());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel
        return (fractionAlongAxis * (interiorRect.width() - 1.0) + interiorRect.x()) + 0.5;
    }
}

double QtSLiMGraphView::plotToDeviceY(double ploty, QRect interiorRect)
{
	double fractionAlongAxis = (ploty - yAxisMin_) / (yAxisMax_ - yAxisMin_);
	
    if (generatingPDF_)
    {
        // We go from the bottom edge of the first pixel to the top edge of the last pixel
        return (fractionAlongAxis * interiorRect.height() + interiorRect.y());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel
        return (fractionAlongAxis * (interiorRect.height() - 1.0) + interiorRect.y()) + 0.5;
    }
}

double QtSLiMGraphView::roundPlotToDeviceX(double plotx, QRect interiorRect)
{
	double fractionAlongAxis = (plotx - xAxisMin_) / (xAxisMax_ - xAxisMin_);
	
    if (generatingPDF_)
    {
        // We go from the left edge of the first pixel to the right edge of the last pixel
        return (fractionAlongAxis * interiorRect.width() + interiorRect.x());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
        return SLIM_SCREEN_ROUND(fractionAlongAxis * (interiorRect.width() - 1.0) + interiorRect.x()) + 0.5;
    }
}

double QtSLiMGraphView::roundPlotToDeviceY(double ploty, QRect interiorRect)
{
	double fractionAlongAxis = (ploty - yAxisMin_) / (yAxisMax_ - yAxisMin_);
	
    if (generatingPDF_)
    {
        // We go from the bottom edge of the first pixel to the top edge of the last pixel
        return (fractionAlongAxis * interiorRect.height() + interiorRect.y());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
        return SLIM_SCREEN_ROUND(fractionAlongAxis * (interiorRect.height() - 1.0) + interiorRect.y()) + 0.5;
    }
}

void QtSLiMGraphView::willDraw(QPainter & /* painter */, QRect /* interiorRect */)
{
}

void QtSLiMGraphView::drawXAxisTicks(QPainter &painter, QRect interiorRect)
{
    painter.setFont(QtSLiMGraphView::fontForTickLabels());
    painter.setBrush(Qt::black);
    
    double axisMin = xAxisMin_;
    double axisMax = xAxisMax_;
    int tickValuePrecision = xAxisTickValuePrecision_;
    double tickValue;
    
    if (!xAxisHistogramStyle_)
    {
        double minorTickInterval = xAxisMinorTickInterval_;
        int majorTickModulus = xAxisMajorTickModulus_;
        int tickIndex;
        
        for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
        {
            bool isMajorTick = ((tickIndex % majorTickModulus) == 0);
            double tickLength = (isMajorTick ? 6 : 3);
            double xValueForTick;
            
            if (generatingPDF_)
                xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - axisMin) / (axisMax - axisMin)) - 0.5);    // left edge of pixel
            else
                xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - axisMin) / (axisMax - axisMin)));    // left edge of pixel
            
            //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", xValueForTick == " << xValueForTick;
            
            painter.fillRect(QRectF(xValueForTick, interiorRect.y() - tickLength, 1, tickLength - (generatingPDF_ ? 0.5 : 0.0)), Qt::black);
            
            if (isMajorTick)
            {
                double labelValueForTick;
                
                labelValueForTick = tickValue;
                
                QString labelText = QString("%1").arg(labelValueForTick, 0, 'f', tickValuePrecision);
                QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
                double labelWidth = labelBoundingRect.width(), labelHeight = labelBoundingRect.height();
                double labelX = xValueForTick - SLIM_SCREEN_ROUND(labelWidth / 2.0);
                double labelY = interiorRect.y() - (labelHeight + 4);
                
                labelY = painter.transform().map(QPointF(labelX, labelY)).y();
                
                if (tweakXAxisTickLabelAlignment_)
                {
                    if (fabs(tickValue - axisMin) < 0.000001)
                        labelX = xValueForTick - 2.0;
                    else if (fabs(tickValue - axisMax) < 0.000001)
                        labelX = xValueForTick - SLIM_SCREEN_ROUND(labelWidth) + 2.0;
                }
                
                painter.setWorldMatrixEnabled(false);
                painter.drawText(QPointF(labelX, labelY), labelText);
                painter.setWorldMatrixEnabled(true);
            }
        }
    }
    else
    {
        // Histogram-style ticks are centered under each bar position, at the 0.5 positions on the axis
        // So a histogram-style axis declared with min/max of 0/10 actually spans 1..10, with ticks at 0.5..9.5 labeled 1..10
        double axisStart = axisMin + 1;
        
        for (tickValue = axisStart; tickValue <= axisMax; tickValue++)
        {
            bool isMajorTick = (tickValue == axisStart) || (tickValue == axisMax);
            double tickLength = (isMajorTick ? 6 : 3);
            double xValueForTick;
            
            if (generatingPDF_)
                xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - 0.5 - axisMin) / (axisMax - axisMin)) - 0.5);    // left edge of pixel
            else
                xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - 0.5 - axisMin) / (axisMax - axisMin)));    // left edge of pixel
            
            //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", xValueForTick == " << xValueForTick;
            
            painter.fillRect(QRectF(xValueForTick, interiorRect.y() - tickLength, 1, tickLength - (generatingPDF_ ? 0.5 : 0.0)), Qt::black);
            
            if (isMajorTick)
            {
                double labelValueForTick;
                
                labelValueForTick = tickValue;
                
                QString labelText = QString("%1").arg(labelValueForTick, 0, 'f', tickValuePrecision);
                QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
                double labelWidth = labelBoundingRect.width(), labelHeight = labelBoundingRect.height();
                double labelX = xValueForTick - SLIM_SCREEN_ROUND(labelWidth / 2.0);
                double labelY = interiorRect.y() - (labelHeight + 4);
                
                labelY = painter.transform().map(QPointF(labelX, labelY)).y();
                
                if (tweakXAxisTickLabelAlignment_)
                {
                    if (fabs(tickValue - axisStart) < 0.000001)
                        labelX = xValueForTick - 2.0;
                    else if (fabs(tickValue - axisMax) < 0.000001)
                        labelX = xValueForTick - SLIM_SCREEN_ROUND(labelWidth) + 2.0;
                }
                
                painter.setWorldMatrixEnabled(false);
                painter.drawText(QPointF(labelX, labelY), labelText);
                painter.setWorldMatrixEnabled(true);
            }
        }
    }
}

void QtSLiMGraphView::drawXAxis(QPainter &painter, QRect interiorRect)
{
    double yAxisFudge = showYAxis_ ? 1 : (generatingPDF_ ? 0.5 : 0);
	QRectF axisRect(interiorRect.x() - yAxisFudge, interiorRect.y() - 1, interiorRect.width() + yAxisFudge + (generatingPDF_ ? 0.5 : 0), 1);
	
    painter.fillRect(axisRect, Qt::black);
	
	// show label
    painter.setFont(QtSLiMGraphView::fontForAxisLabels());
    painter.setBrush(Qt::black);
    
    QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, xAxisLabel_);
    QPoint drawPoint(interiorRect.x() + (interiorRect.width() - labelBoundingRect.width()) / 2, interiorRect.y() - 42);
    
    drawPoint = painter.transform().map(drawPoint);
    painter.setWorldMatrixEnabled(false);
    painter.drawText(drawPoint, xAxisLabel_);
    painter.setWorldMatrixEnabled(true);
}

void QtSLiMGraphView::drawYAxisTicks(QPainter &painter, QRect interiorRect)
{
    painter.setFont(QtSLiMGraphView::fontForTickLabels());
    painter.setBrush(Qt::black);
    
	double axisMin = yAxisMin_;
	double axisMax = yAxisMax_;
	int tickValuePrecision = yAxisTickValuePrecision_;
	double tickValue;
	
    if (!yAxisHistogramStyle_)
    {
        double axisStart = (yAxisLog_ ? round(yAxisMin_) : yAxisMin_);  // with a log scale, we leave a little room at the bottom
        double minorTickInterval = yAxisMinorTickInterval_;
        int majorTickModulus = yAxisMajorTickModulus_;
        int tickIndex;
        
        for (tickValue = axisStart, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
        {
            bool isMajorTick = ((tickIndex % majorTickModulus) == 0);
            double tickLength = (isMajorTick ? 6 : 3);
            double transformedTickValue = tickValue;
            
            if (yAxisLog_ && !isMajorTick)
            {
                // with a log scale, adjust the tick positions so they are non-linear; this is hackish
                double intPart = std::floor(tickValue);
                double fractPart = tickValue - intPart;
                double minorTickIndex = fractPart * 9.0;
                double minorTickOffset = std::log10(minorTickIndex + 1.0);
                transformedTickValue = intPart + minorTickOffset;
                //std::cout << intPart << " , " << fractPart << "\n";
            }
            
            double yValueForTick;
            
            if (generatingPDF_)
                yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((transformedTickValue - axisMin) / (axisMax - axisMin)) - 0.5);   // bottom edge of pixel
            else
                yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((transformedTickValue - axisMin) / (axisMax - axisMin)));   // bottom edge of pixel
            
            //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", yValueForTick == " << yValueForTick;
            
            painter.fillRect(QRectF(interiorRect.x() - tickLength, yValueForTick, tickLength - (generatingPDF_ ? 0.5 : 0.0), 1), Qt::black);
            
            if (isMajorTick)
            {
                double labelValueForTick;
                
                labelValueForTick = tickValue;
                
                QString labelText = QString("%1").arg(labelValueForTick, 0, 'f', tickValuePrecision);
                
                if (yAxisLog_)
                {
                    if (std::abs(tickValue - 0.0) < 0.0000001)          labelText = "1";
                    else if (std::abs(tickValue - 1.0) < 0.0000001)     labelText = "10";
                    else if (std::abs(tickValue - 2.0) < 0.0000001)     labelText = "100";
                    else                                                labelText = QString("10^%1").arg((int)round(tickValue));
                }
                
                QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
                double labelWidth = labelBoundingRect.width(), labelHeight = labelBoundingRect.height();
                double labelY = yValueForTick - SLIM_SCREEN_ROUND(labelHeight / 2.0) + 3;
                double labelX = interiorRect.x() - (labelWidth + 8);
                
                labelY = painter.transform().map(QPointF(labelX, labelY)).y();
                
                painter.setWorldMatrixEnabled(false);
                painter.drawText(QPointF(labelX, labelY), labelText);
                painter.setWorldMatrixEnabled(true);
            }
        }
    }
    else
    {
        // Histogram-style ticks are centered to the left of each bar position, at the 0.5 positions on the axis
        // So a histogram-style axis declared with min/max of 0/10 actually spans 1..10, with ticks at 0.5..9.5 labeled 1..10
        double axisStart = axisMin + 1;
        
        for (tickValue = axisStart; tickValue <= axisMax; tickValue++)
        {
            bool isMajorTick = (tickValue == axisStart) || (tickValue == axisMax);
            double tickLength = (isMajorTick ? 6 : 3);
            double yValueForTick;
            
            if (generatingPDF_)
                yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((tickValue - 0.5 - axisMin) / (axisMax - axisMin)) - 0.5);   // bottom edge of pixel
            else
                yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - 0.5 - axisMin) / (axisMax - axisMin)));   // bottom edge of pixel
            
            //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", yValueForTick == " << yValueForTick;
            
            painter.fillRect(QRectF(interiorRect.x() - tickLength, yValueForTick, tickLength - (generatingPDF_ ? 0.5 : 0.0), 1), Qt::black);
            
            if (isMajorTick)
            {
                double labelValueForTick;
                
                labelValueForTick = tickValue;
                
                QString labelText = QString("%1").arg(labelValueForTick, 0, 'f', tickValuePrecision);
                QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
                double labelWidth = labelBoundingRect.width(), labelHeight = labelBoundingRect.height();
                double labelY = yValueForTick - SLIM_SCREEN_ROUND(labelHeight / 2.0) + 3;
                double labelX = interiorRect.x() - (labelWidth + 8);
                
                labelY = painter.transform().map(QPointF(labelX, labelY)).y();
                
                painter.setWorldMatrixEnabled(false);
                painter.drawText(QPointF(labelX, labelY), labelText);
                painter.setWorldMatrixEnabled(true);
            }
        }
    }
}

void QtSLiMGraphView::drawYAxis(QPainter &painter, QRect interiorRect)
{
    double xAxisFudge = showXAxis_ ? 1 : (generatingPDF_ ? 0.5 : 0);
	QRectF axisRect(interiorRect.x() - 1, interiorRect.y() - xAxisFudge, 1, interiorRect.height() + xAxisFudge + (generatingPDF_ ? 0.5 : 0));
    
    painter.fillRect(axisRect, Qt::black);
    
    // show label, rotated
    painter.setFont(QtSLiMGraphView::fontForAxisLabels());
    painter.setBrush(Qt::black);
    
    QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, yAxisLabel_);
    
    painter.save();
    painter.translate(interiorRect.x() - 35, interiorRect.y() + SLIM_SCREEN_ROUND(interiorRect.height() / 2.0));
    painter.rotate(90);
    painter.scale(1.0, -1.0);
    
    painter.drawText(QPointF(SLIM_SCREEN_ROUND(-labelBoundingRect.width() / 2.0), 0), yAxisLabel_);
    
    painter.restore();
}

void QtSLiMGraphView::drawFullBox(QPainter &painter, QRect interiorRect)
{
	QRect axisRect;
	
	// upper x axis
	int yAxisFudge = showYAxis_ ? 1 : 0;
	
	axisRect = QRect(interiorRect.x() - yAxisFudge, interiorRect.y() + interiorRect.height(), interiorRect.width() + yAxisFudge + 1, 1);
	
    painter.fillRect(axisRect, Qt::black);
	
	// right-hand y axis
	int xAxisFudge = showXAxis_ ? 1 : 0;
	
	axisRect = QRect(interiorRect.x() + interiorRect.width(), interiorRect.y() - xAxisFudge, 1, interiorRect.height() + xAxisFudge + 1);
	
    painter.fillRect(axisRect, Qt::black);
}

void QtSLiMGraphView::drawVerticalGridLines(QPainter &painter, QRect interiorRect)
{
    QColor gridColor = QtSLiMGraphView::gridLineColor();
	double axisMin = xAxisMin_;
	double axisMax = xAxisMax_;
	double minorTickInterval = xAxisMinorTickInterval_;
	double tickValue;
	
	for (tickValue = axisMin; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval)
	{
		double xValueForTick;
        
        if (generatingPDF_)
            xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - axisMin) / (axisMax - axisMin)) - 0.5);    // left edge of pixel
        else
            xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - axisMin) / (axisMax - axisMin)));    // left edge of pixel
		
		if (std::abs(xValueForTick - interiorRect.x()) < 1.25)
			continue;
		if ((std::abs(xValueForTick - (interiorRect.x() + interiorRect.width() - 1)) < 1.25) && showFullBox_)
			continue;
		
        painter.fillRect(QRectF(xValueForTick, interiorRect.y(), 1, interiorRect.height()), gridColor);
	}
}

void QtSLiMGraphView::drawHorizontalGridLines(QPainter &painter, QRect interiorRect)
{
    QColor gridColor = QtSLiMGraphView::gridLineColor();
	double axisMin = yAxisMin_;
	double axisMax = yAxisMax_;
	double minorTickInterval = yAxisMinorTickInterval_;
	double tickValue;
    double axisStart = (yAxisLog_ ? round(yAxisMin_) : yAxisMin_);  // with a log scale, we leave a little room at the bottom
    double tickValueIncrement = (showGridLinesMajorOnly_ ? yAxisMajorTickInterval_ : minorTickInterval);
	
	for (tickValue = axisStart; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += tickValueIncrement)
	{
		double yValueForTick;
        
        if (generatingPDF_)
            yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((tickValue - axisMin) / (axisMax - axisMin)) - 0.5);   // bottom edge of pixel
        else
            yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - axisMin) / (axisMax - axisMin)));   // bottom edge of pixel
		
		if (std::abs(yValueForTick - interiorRect.y()) < 1.25)
			continue;
		if ((std::abs(yValueForTick - (interiorRect.y() + interiorRect.height() - 1)) < 1.25) && showFullBox_)
			continue;
		
        painter.fillRect(QRectF(interiorRect.x(), yValueForTick, interiorRect.width(), 1), gridColor);
	}
}

void QtSLiMGraphView::drawMessage(QPainter &painter, QString messageString, QRect rect)
{
    painter.setFont(QtSLiMGraphView::labelFontOfPointSize(16));
    painter.setBrush(QtSLiMColorWithWhite(0.4, 1.0));
    
    painter.drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, messageString);
}

void QtSLiMGraphView::drawGraph(QPainter &painter, QRect interiorRect)
{
    painter.fillRect(interiorRect, QtSLiMColorWithHSV(0.15, 0.15, 1.0, 1.0));
}

QtSLiMLegendSpec QtSLiMGraphView::legendKey(void)
{
    return QtSLiMLegendSpec();
}

QSize QtSLiMGraphView::legendSize(QPainter &painter)
{
    QtSLiMLegendSpec legend = legendKey();
	int legendEntryCount = static_cast<int>(legend.size());
    
	if (legendEntryCount == 0)
		return QSize();
	
	const int legendRowHeight = 15;
	QSize legendSize = QSize(0, legendRowHeight * legendEntryCount - 6);
	
    for (const QtSLiMLegendEntry &legendEntry : legend)
    {
        QString labelString = legendEntry.first;
        QRect labelBoundingBox = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelString);
        
        legendSize.setWidth(std::max(legendSize.width(), 0 + (legendRowHeight - 6) + 5 + labelBoundingBox.width()));
    }
    
	return legendSize;
}

void QtSLiMGraphView::drawLegend(QPainter &painter, QRect legendRect)
{
    QtSLiMLegendSpec legend = legendKey();
    int legendEntryCount = static_cast<int>(legend.size());
	const int legendRowHeight = 15;
	int idx = 0;
    
    for (const QtSLiMLegendEntry &legendEntry : legend)
    {
        QRect swatchRect(legendRect.x(), legendRect.y() + ((legendEntryCount - 1) * legendRowHeight - 3) - idx * legendRowHeight + 3, legendRowHeight - 6, legendRowHeight - 6);
        QString labelString = legendEntry.first;
        QColor entryColor = legendEntry.second;
        
        painter.fillRect(swatchRect, entryColor);
        QtSLiMFrameRect(swatchRect, Qt::black, painter);
        
        double labelX = swatchRect.x() + swatchRect.width() + 5;
        double labelY = swatchRect.y() + 1;
        
        labelY = painter.transform().map(QPointF(labelX, labelY)).y();
        
        painter.setWorldMatrixEnabled(false);
        painter.drawText(QPointF(labelX, labelY), labelString);
        painter.setWorldMatrixEnabled(true);
        
        idx++;
    }
}

void QtSLiMGraphView::drawLegendInInteriorRect(QPainter &painter, QRect interiorRect)
{
    painter.setFont(QtSLiMGraphView::fontForLegendLabels());
    
    QSize size = legendSize(painter);
	int legendWidth = static_cast<int>(ceil(size.width()));
	int legendHeight = static_cast<int>(ceil(size.height()));
	
	if ((legendWidth > 0) && (legendHeight > 0))
	{
		const int legendMargin = 10;
		QRect legendRect(0, 0, legendWidth + legendMargin + legendMargin, legendHeight + legendMargin + legendMargin);
		
		// Position the legend in the upper right, for now; this should be configurable
		legendRect.moveLeft(interiorRect.x() + interiorRect.width() - (legendRect.width() + 2));
		legendRect.moveTop(interiorRect.y() + interiorRect.height() - (legendRect.height() + 2));
		
		// Frame the legend and erase it with a slightly translucent wash
        painter.fillRect(legendRect, QtSLiMColorWithWhite(0.95, 0.9));
        QtSLiMFrameRect(legendRect, QtSLiMColorWithWhite(0.3, 1.0), painter);
		
		// Inset and draw the legend content
		legendRect.adjust(legendMargin, legendMargin, -legendMargin, -legendMargin);
		drawLegend(painter, legendRect);
	}
}

void QtSLiMGraphView::drawContents(QPainter &painter)
{
    // Erase background
    QRect bounds = rect();
    
	if (!generatingPDF_)
		painter.fillRect(bounds, Qt::white);
	
	// Get our controller and test for validity, so subclasses don't have to worry about this
    if (!controller_ || controller_->invalidSimulation())
    {
        drawMessage(painter, "invalid\nsimulation", bounds);
    }
    else if (controller_->sim->generation_ == 0)
    {
        drawMessage(painter, "no\ndata", bounds);
    }
    else if (disableMessage().length() > 0)
    {
        drawMessage(painter, disableMessage(), bounds);
    }
    else
	{
		QRect interiorRect = interiorRectForBounds(bounds);
        
        // flip the coordinate system so (0, 0) is at lower left
        // see https://stackoverflow.com/questions/4413570/use-window-viewport-to-flip-qpainter-y-axis
        painter.save();
        painter.translate(0, height());
        painter.scale(1.0, -1.0);
		
		willDraw(painter, interiorRect);
		
		// Draw grid lines, if requested, and if tick marks are turned on for the corresponding axis
		if (showHorizontalGridLines_ && showYAxis_ && showYAxisTicks_)
			drawHorizontalGridLines(painter, interiorRect);
		
		if (showVerticalGridLines_ && showXAxis_ && showXAxisTicks_)
			drawVerticalGridLines(painter, interiorRect);
		
		// Draw the interior of the graph; this will be overridden by the subclass
		// We clip the interior drawing to the interior rect, so outliers get clipped out
        painter.save();
        painter.setClipRect(interiorRect, Qt::IntersectClip);
		
		drawGraph(painter, interiorRect);
		
        painter.restore();
		
		// If we're caching, skip all overdrawing, since it cannot be cached (it would then appear under new drawing that supplements the cache)
		if (!cachingNow_)
		{
			// Overdraw axes, ticks, and axis labels, if requested
            if (showXAxis_)
				drawXAxis(painter, interiorRect);
			
			if (showYAxis_)
				drawYAxis(painter, interiorRect);
			
            if (showFullBox_)
				drawFullBox(painter,interiorRect);
			
			if (showXAxis_ && showXAxisTicks_)
				drawXAxisTicks(painter, interiorRect);
			
			if (showYAxis_ && showYAxisTicks_)
				drawYAxisTicks(painter, interiorRect);
			
			// Overdraw the legend
			if (legendVisible_)
				drawLegendInInteriorRect(painter, interiorRect);
		}
        
        // unflip
        painter.restore();
	}
}

void QtSLiMGraphView::paintEvent(QPaintEvent * /* event */)
{
	QPainter painter(this);
    
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawContents(painter);
}

void QtSLiMGraphView::invalidateDrawingCache(void)
{
	// GraphView has no drawing cache, but it supports the idea of one
}

void QtSLiMGraphView::graphWindowResized(void)
{
	invalidateDrawingCache();
}

void QtSLiMGraphView::resizeEvent(QResizeEvent *event)
{
    // this override is private; subclassers should override graphWindowResized()
    graphWindowResized();
    QWidget::resizeEvent(event);
}

void QtSLiMGraphView::controllerRecycled(void)
{
	invalidateDrawingCache();
    update();
    
    QPushButton *action = actionButton();
    if (action) action->setEnabled(!controller_->invalidSimulation() && (controller_->sim->generation_ > 0));
}

void QtSLiMGraphView::controllerSelectionChanged(void)
{
}

void QtSLiMGraphView::controllerGenerationFinished(void)
{
}

void QtSLiMGraphView::updateAfterTick(void)
{
	update();
    
    QPushButton *action = actionButton();
    if (action) action->setEnabled(!controller_->invalidSimulation() && (controller_->sim->generation_ > 0));
}

bool QtSLiMGraphView::providesStringForData(void)
{
    // subclassers that override stringForData() should also override this to return true
    // this is an annoying substitute for Obj-C's respondsToSelector:
    return false;
}

QString QtSLiMGraphView::stringForData(void)
{
    QString string("# Graph data: ");
	
    string.append(graphTitle());
    string.append("\n");
    string.append(dateline());
    string.append("\n\n");
    
    appendStringForData(string);
    
    // Get rid of extra commas, as a service to subclasses
    string.replace(", \n", "\n");
    
    return string;
}

QString QtSLiMGraphView::dateline(void)
{
    QDateTime dateTime = QDateTime::currentDateTime();
    QString dateTimeString = dateTime.toString("M/d/yy, h:mm:ss AP");        // format: 3/28/20, 8:03:09 PM
    
	return QString("# %1").arg(dateTimeString);
}

void QtSLiMGraphView::actionButtonRunMenu(QPushButton *actionButton)
{
    contextMenuEvent(nullptr);
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    actionButton->setIcon(QIcon(":/buttons/action.png"));
}

void QtSLiMGraphView::subclassAddItemsToMenu(QMenu & /* contextMenu */, QContextMenuEvent * /* event */)
{
}

QString QtSLiMGraphView::disableMessage(void)
{
    return "";
}

void QtSLiMGraphView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!controller_->invalidSimulation() && (controller_->sim->generation_ > 0)) // && ![[controller window] attachedSheet])
	{
		bool addedItems = false;
        QMenu contextMenu("graph_menu", this);
        
        QAction *aboutGraph = nullptr;
        QAction *legendToggle = nullptr;
        QAction *gridHToggle = nullptr;
        QAction *gridVToggle = nullptr;
        QAction *boxToggle = nullptr;
        QAction *changeBinCount = nullptr;
        QAction *changeHeatmapMargins = nullptr;
        QAction *changeXAxisScale = nullptr;
        QAction *changeYAxisScale = nullptr;
        QAction *copyGraph = nullptr;
        QAction *exportGraph = nullptr;
        QAction *copyData = nullptr;
        QAction *exportData = nullptr;
        
        // Show a description of the graph
        aboutGraph = contextMenu.addAction("About This Graph...");
        contextMenu.addSeparator();
        
		// Toggle legend visibility
		if (legendKey().size() > 0)
		{
            legendToggle = contextMenu.addAction(legendVisible_ ? "Hide Legend" : "Show Legend");
			addedItems = true;
		}
		
		// Toggle horizontal grid line visibility
		if (allowHorizontalGridChange_ && showYAxis_ && showYAxisTicks_)
		{
			gridHToggle = contextMenu.addAction(showHorizontalGridLines_ ? "Hide Horizontal Grid" : "Show Horizontal Grid");
			addedItems = true;
		}
		
		// Toggle vertical grid line visibility
		if (allowVerticalGridChange_ && showXAxis_ && showXAxisTicks_)
		{
			gridVToggle = contextMenu.addAction(showVerticalGridLines_ ? "Hide Vertical Grid" : "Show Vertical Grid");
			addedItems = true;
		}
		
		// Toggle box visibility
		if (allowFullBoxChange_ && showXAxis_ && showYAxis_)
		{
			boxToggle = contextMenu.addAction(showFullBox_ ? "Hide Full Box" : "Show Full Box");
			addedItems = true;
		}
		
		// Add a separator if we had any visibility-toggle menu items above
        if (addedItems)
            contextMenu.addSeparator();
		addedItems = false;
		
		// Rescale axes
        if (histogramBinCount_ && allowBinCountRescale_)
		{
            changeBinCount = contextMenu.addAction("Change Bin Count...");
			addedItems = true;
		}
        if (allowHeatmapMarginsChange_)
        {
            changeHeatmapMargins = contextMenu.addAction(heatmapMargins_ ? "Remove Patch Margins" : "Add Patch Margins");
            addedItems = true;
        }
		if (showXAxis_ && showXAxisTicks_ && allowXAxisUserRescale_)
		{
            changeXAxisScale = contextMenu.addAction("Change X Axis Scale...");
			addedItems = true;
		}
		if (showYAxis_ && showYAxisTicks_ && allowYAxisUserRescale_)
		{
            changeYAxisScale = contextMenu.addAction("Change Y Axis Scale...");
			addedItems = true;
		}
        
		// Add a separator if we had any visibility-toggle menu items above
		if (addedItems)
            contextMenu.addSeparator();
		addedItems = false;
		(void)addedItems;	// dead store above is deliberate
		
		// Allow a subclass to introduce menu items here, above the copy/export menu items, which belong at the bottom;
        // we are responsible for adding a separator afterwards if needed
		int preSubclassItemCount = contextMenu.actions().count();
		
		subclassAddItemsToMenu(contextMenu, event);
		
		if (preSubclassItemCount != contextMenu.actions().count())
            contextMenu.addSeparator();
        
		// Copy/export the graph image
		{
            // BCH 4/21/2020: FIXME the "as ..." names here are temporary, until the bug below is fixed and we can copy PDF...
            copyGraph = contextMenu.addAction("Copy Graph as Bitmap");
            exportGraph = contextMenu.addAction("Export Graph as PDF...");
		}
        
		// Copy/export the data to the clipboard
		if (providesStringForData())
		{
            contextMenu.addSeparator();
            copyData = contextMenu.addAction("Copy Data");
            exportData = contextMenu.addAction("Export Data...");
		}
        
        // Run the context menu synchronously
        QPoint menuPos = (event ? event->globalPos() : QCursor::pos());
        QAction *action = contextMenu.exec(menuPos);
        
        // Act upon the chosen action; we just do it right here instead of dealing with slots
        if (action)
        {
            if (action == aboutGraph)
            {
                QString title = graphTitle();
                QString about = aboutString();
                
                QMessageBox messageBox(this);
                messageBox.setText(title);
                messageBox.setInformativeText(about);
                messageBox.setIcon(QMessageBox::Information);
                messageBox.setWindowModality(Qt::WindowModal);
                messageBox.exec();
            }
            if (action == legendToggle)
            {
                legendVisible_ = !legendVisible_;
                update();
            }
            if (action == gridHToggle)
            {
                showHorizontalGridLines_ = !showHorizontalGridLines_;
                update();
            }
            if (action == gridVToggle)
            {
                showVerticalGridLines_ = !showVerticalGridLines_;
                update();
            }
            if (action == boxToggle)
            {
                showFullBox_ = !showFullBox_;
                update();
            }
            if (action == changeBinCount)
            {
                QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a bin count:", QStringList("Bin count:"), QStringList(QString::number(histogramBinCount_)));
                
                if (choices.length() == 1)
                {
                    int newBinCount = choices[0].toInt();
                    
                    if ((newBinCount > 1) && (newBinCount <= 500))
                    {
                        histogramBinCount_ = newBinCount;
                        invalidateDrawingCache();
                        update();
                    }
                    else qApp->beep();
                }
            }
            if (action == changeHeatmapMargins)
            {
                heatmapMargins_ = 1- heatmapMargins_;   // toggle
                update();
            }
            if (action == changeXAxisScale)
            {
                QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a configuration for the axis:",
                                                                   QStringList{"Minimum value:", "Maximum value:", "Major tick interval:", "Minor tick divisions:", "Tick label precision:"},
                                                                   QStringList{QString::number(xAxisMin_), QString::number(xAxisMax_), QString::number(xAxisMajorTickInterval_), QString::number(xAxisMajorTickModulus_),QString::number(xAxisTickValuePrecision_)});
                
                if (choices.length() == 5)
                {
                    xAxisMin_ = choices[0].toDouble();
                    xAxisMax_ = choices[1].toDouble();
                    xAxisMajorTickInterval_ = choices[2].toDouble();
                    xAxisMajorTickModulus_ = choices[3].toInt();
                    xAxisTickValuePrecision_ = choices[4].toInt();
                    xAxisMinorTickInterval_ = xAxisMajorTickInterval_ / xAxisMajorTickModulus_;
                    xAxisIsUserRescaled_ = true;
                    
                    invalidateDrawingCache();
                    update();
                }
            }
            if (action == changeYAxisScale)
            {
                if (!yAxisLog_)
                {
                    QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a configuration for the axis:",
                                                                       QStringList{"Minimum value:", "Maximum value:", "Major tick interval:", "Minor tick divisions:", "Tick label precision:"},
                                                                       QStringList{QString::number(yAxisMin_), QString::number(yAxisMax_), QString::number(yAxisMajorTickInterval_), QString::number(yAxisMajorTickModulus_),QString::number(yAxisTickValuePrecision_)});
                    
                    if (choices.length() == 5)
                    {
                        yAxisMin_ = choices[0].toDouble();
                        yAxisMax_ = choices[1].toDouble();
                        yAxisMajorTickInterval_ = choices[2].toDouble();
                        yAxisMajorTickModulus_ = choices[3].toInt();
                        yAxisTickValuePrecision_ = choices[4].toInt();
                        yAxisMinorTickInterval_ = yAxisMajorTickInterval_ / yAxisMajorTickModulus_;
                        yAxisIsUserRescaled_ = true;
                        
                        invalidateDrawingCache();
                        update();
                    }
                }
                else
                {
                    QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a maximum log-scale power:",
                                                                       QStringList{"Maximum value (10^x):"},
                                                                       QStringList{QString::number(yAxisMax_)});
                    
                    if (choices.length() == 1)
                    {
                        int newPower = choices[0].toInt();
                        
                        if ((newPower >= 1) && (newPower <= 10))
                        {
                            yAxisMax_ = (double)newPower;
                            
                            invalidateDrawingCache();
                            update();
                        }
                        else qApp->beep();
                    }
                }
            }
            if (action == copyGraph)
            {
#if 0
                // FIXME this clipboard data is not usable on macOS, apparently because the MIME tag doesn't
                // come through properly; see https://bugreports.qt.io/browse/QTBUG-83164
                // I can't find a workaround so I'll wait for them to respond
                QBuffer buffer;
                
                buffer.open(QIODevice::WriteOnly);
                {
                    QSize graphSize = size();
                    QPdfWriter pdfwriter(&buffer);
                    QPageSize pageSize = QPageSize(graphSize, QString(), QPageSize::ExactMatch);
                    QMarginsF margins(0, 0, 0, 0);
                    
                    pdfwriter.setCreator("SLiMgui");
                    pdfwriter.setResolution(72);    // match the screen?
                    pdfwriter.setPageSize(pageSize);
                    pdfwriter.setPageMargins(margins);
                    
                    QPainter painter(&pdfwriter);
                    generatingPDF_ = true;
                    drawContents(painter);
                    generatingPDF_ = false;
                }
                buffer.close();
                
                QClipboard *clipboard = QGuiApplication::clipboard();
                QMimeData mimeData;
                mimeData.setData("application/pdf", buffer.data());
                clipboard->setMimeData(&mimeData);
#else
                // Until the above bug gets fixed, we'll copy raster data to the clipboard instead
                QPixmap pixmap(size());
                render(&pixmap);
                QImage image = pixmap.toImage();
                QClipboard *clipboard = QGuiApplication::clipboard();
                clipboard->setImage(image);
#endif
            }
            if (action == exportGraph)
            {
                // FIXME maybe this should use QtSLiMDefaultSaveDirectory?  see QtSLiMWindow::saveAs()
                QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                QFileInfo fileInfo(QDir(desktopPath), "graph.pdf");
                QString path = fileInfo.absoluteFilePath();
                QString fileName = QFileDialog::getSaveFileName(this, "Export Graph", path);
                
                if (!fileName.isEmpty())
                {
                    QSize graphSize = size();
                    QPdfWriter pdfwriter(fileName);
                    QPageSize pageSize = QPageSize(graphSize, QString(), QPageSize::ExactMatch);
                    QMarginsF margins(0, 0, 0, 0);
                    
                    pdfwriter.setCreator("SLiMgui");
                    pdfwriter.setResolution(72);    // match the screen?
                    pdfwriter.setPageSize(pageSize);
                    pdfwriter.setPageMargins(margins);
                    
                    QPainter painter(&pdfwriter);
                    generatingPDF_ = true;
                    drawContents(painter);
                    generatingPDF_ = false;
                }
            }
            if (action == copyData)
            {
                QClipboard *clipboard = QGuiApplication::clipboard();
                clipboard->setText(stringForData());
            }
            if (action == exportData)
            {
                // FIXME maybe this should use QtSLiMDefaultSaveDirectory?  see QtSLiMWindow::saveAs()
                QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                QFileInfo fileInfo(QDir(desktopPath), "data.txt");
                QString path = fileInfo.absoluteFilePath();
                QString fileName = QFileDialog::getSaveFileName(this, "Export Data", path);
                
                if (!fileName.isEmpty())
                {
                    QFile file(fileName);
                    
                    if (file.open(QFile::WriteOnly | QFile::Text))
                        file.write(stringForData().toUtf8());
                    else
                        qApp->beep();
                }
            }
        }
	}
}

void QtSLiMGraphView::setXAxisRangeFromGeneration(void)
{
	SLiMSim *sim = controller_->sim;
	slim_generation_t lastGen = sim->EstimatedLastGeneration();
	
	// The last generation could be just about anything, so we need some smart axis setup code here – a problem we neglect elsewhere
	// since we use hard-coded axis setups in other places.  The goal is to (1) have the axis max be >= last_gen, (2) have the axis
	// max be == last_gen if last_gen is a reasonably round number (a single-digit multiple of a power of 10, say), (3) have just a few
	// other major tick intervals drawn, so labels don't collide or look crowded, and (4) have a few minor tick intervals in between
	// the majors.  Labels that are single-digit multiples of powers of 10 are to be strongly preferred.
	double lower10power = pow(10.0, floor(log10(lastGen)));		// 8000 gives 1000, 1000 gives 1000, 10000 gives 10000
	double lower5mult = lower10power / 2.0;						// 8000 gives 500, 1000 gives 500, 10000 gives 5000
	double axisMax = ceil(lastGen / lower5mult) * lower5mult;	// 8000 gives 8000, 7500 gives 7500, 1100 gives 1500
	double contained5mults = axisMax / lower5mult;				// 8000 gives 16, 7500 gives 15, 1100 gives 3, 1000 gives 2
	
	if (contained5mults <= 3)
	{
		// We have a max like 1500 that divides into 5mults well, so do that
		xAxisMax_ = axisMax;
		xAxisMajorTickInterval_ = lower5mult;
		xAxisMinorTickInterval_ = lower5mult / 5;
		xAxisMajorTickModulus_ = 5;
		xAxisTickValuePrecision_ = 0;
	}
	else
	{
		// We have a max like 7000 that does not divide into 5mults well; for simplicity, we just always divide these in two
		xAxisMax_ = axisMax;
		xAxisMajorTickInterval_ = axisMax / 2;
		xAxisMinorTickInterval_ = axisMax / 4;
		xAxisMajorTickModulus_ = 2;
		xAxisTickValuePrecision_ = 0;
	}
}

QtSLiMLegendSpec QtSLiMGraphView::subpopulationLegendKey(std::vector<slim_objectid_t> &subpopsToDisplay, bool drawSubpopsGray)
{
    QtSLiMLegendSpec legendKey;
    
    // put "All" first, if it occurs in subpopsToDisplay
    if (std::find(subpopsToDisplay.begin(), subpopsToDisplay.end(), -1) != subpopsToDisplay.end())
        legendKey.push_back(QtSLiMLegendEntry("All", Qt::black));

    if (drawSubpopsGray)
    {
        legendKey.push_back(QtSLiMLegendEntry("pX", QtSLiMColorWithWhite(0.5, 1.0)));
    }
    else
    {
        for (auto subpop_id : subpopsToDisplay)
        {
            if (subpop_id != -1)
            {
                QString labelString = QString("p%1").arg(subpop_id);

                legendKey.push_back(QtSLiMLegendEntry(labelString, controller_->whiteContrastingColorForIndex(subpop_id)));
            }
        }
    }

    return legendKey;
}

QtSLiMLegendSpec QtSLiMGraphView::mutationTypeLegendKey(void)
{
	SLiMSim *sim = controller_->sim;
	int mutationTypeCount = static_cast<int>(sim->mutation_types_.size());
	
	// if we only have one mutation type, do not show a legend
	if (mutationTypeCount < 2)
		return QtSLiMLegendSpec();
	
	QtSLiMLegendSpec legendKey;
	
	// first we put in placeholders
    legendKey.resize(sim->mutation_types_.size());
	
	// then we replace the placeholders with lines, but we do it out of order, according to mutation_type_index_ values
	for (auto mutationTypeIter : sim->mutation_types_)
	{
		MutationType *mutationType = mutationTypeIter.second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
        QString labelString = QString("m%1").arg(mutationType->mutation_type_id_);
		QtSLiMLegendEntry &entry = legendKey[static_cast<size_t>(mutationTypeIndex)];
        
        entry.first = labelString;
        entry.second = controller_->blackContrastingColorForIndex(mutationTypeIndex);
	}
	
	return legendKey;
}

void QtSLiMGraphView::drawGroupedBarplot(QPainter &painter, QRect interiorRect, double *buffer, int subBinCount, int mainBinCount, double firstBinValue, double mainBinWidth)
{
    // Decide on a display style; if we have lots of width, then we draw bars with a fill color, spaced out nicely,
	// but if we are cramped for space then we draw solid black bars.  Note the latter style does not really
	// work with sub-bins; not much we can do about that, since we don't have the room to draw...
	int interiorWidth = interiorRect.width();
	int totalBarCount = subBinCount * mainBinCount;
	int drawStyle = 0;
	
	if (totalBarCount * 7 + 1 <= interiorWidth)				// room for space, space, space, frame, fill, fill, frame...
		drawStyle = 0;
	else if (totalBarCount * 5 + 1 <= interiorWidth)		// room for space, frame, fill, fill, frame...
		drawStyle = 1;
	else if (totalBarCount * 2 + 1 <= interiorWidth)		// room for frame, fill, [frame]...
		drawStyle = 2;
	else
		drawStyle = 3;
	
    if (generatingPDF_ && (drawStyle == 3))
        drawStyle = 2;
    
    for (int mainBinIndex = 0; mainBinIndex < mainBinCount; ++mainBinIndex)
    {
		double binMinValue = mainBinIndex * mainBinWidth + firstBinValue;
		double binMaxValue = (mainBinIndex + 1) * mainBinWidth + firstBinValue;
		double barLeft = roundPlotToDeviceX(binMinValue, interiorRect);
		double barRight = roundPlotToDeviceX(binMaxValue, interiorRect);
        double lineWidth = generatingPDF_ ? 0.3 : 1.0;
        double halfLineWidth = lineWidth / 2.0;
		
		switch (drawStyle)
		{
			case 0: barLeft += (1.0 + halfLineWidth); barRight -= (1.0 + halfLineWidth); break;
			case 1: barLeft += halfLineWidth; barRight -= halfLineWidth; break;
			case 2: barLeft -= halfLineWidth; barRight += halfLineWidth; break;
			case 3: barLeft -= halfLineWidth; barRight += halfLineWidth; break;
		}
		
		for (int subBinIndex = 0; subBinIndex < subBinCount; ++subBinIndex)
		{
			int actualBinIndex = subBinIndex + mainBinIndex * subBinCount;
			double binValue = buffer[actualBinIndex];
			double barBottom = interiorRect.y() - (generatingPDF_ ? 0.5 : 1.0);
			double barTop;
			QRectF barRect;
            
            if (fabs(binValue - 0) < 0.00000001)
                continue;
            
            if (generatingPDF_)
            {
                barTop =  plotToDeviceY(binValue, interiorRect);
                barRect = QRectF(barLeft, barBottom, barRight - barLeft, barTop - barBottom);
            }
            else
            {
                barTop = roundPlotToDeviceY(binValue, interiorRect) + 0.5;
                barRect = QRectF(qRound(barLeft), qRound(barBottom), qRound(barRight - barLeft), qRound(barTop - barBottom));
			}
            
			if (barRect.height() > 0.0)
			{
				// subdivide into sub-bars for each mutation type, if there is more than one
				if (subBinCount > 1)
				{
					double barWidth = barRect.width();
					double subBarWidth = (barWidth - lineWidth) / subBinCount;
					double subbarLeft = SLIM_SCREEN_ROUND(barRect.x() + subBinIndex * subBarWidth);
					double subbarRight = SLIM_SCREEN_ROUND(barRect.x() + (subBinIndex + 1) * subBarWidth) + lineWidth;
					
                    if (generatingPDF_)
                    {
                        barRect.setX(subbarLeft);
                        barRect.setWidth(subbarRight - subbarLeft);
                    }
                    else
                    {
                        barRect.setX(qRound(subbarLeft));
                        barRect.setWidth(qRound(subbarRight - subbarLeft));
                    }
				}
                
				// fill and frame
				if (drawStyle == 3)
				{
                    painter.fillRect(barRect, Qt::black);
				}
				else
				{
                    painter.fillRect(barRect, controller_->blackContrastingColorForIndex(subBinIndex));
                    QtSLiMFrameRect(barRect, Qt::black, painter, lineWidth);
				}
			}
		}
	}
}

void QtSLiMGraphView::drawBarplot(QPainter &painter, QRect interiorRect, double *buffer, int binCount, double firstBinValue, double binWidth)
{
	drawGroupedBarplot(painter, interiorRect, buffer, 1, binCount, firstBinValue, binWidth);
}

void QtSLiMGraphView::drawHeatmap(QPainter &painter, QRect interiorRect, double *buffer, int xBinCount, int yBinCount)
{
    int intHeatMapMargins = (generatingPDF_ ? 0 : heatmapMargins_);     // when generating a PDF we use an inset for accuracy
    double patchWidth = (interiorRect.width() - intHeatMapMargins) / (double)xBinCount;
    double patchHeight = (interiorRect.height() - intHeatMapMargins) / (double)yBinCount;
    
    for (int x = 0; x < xBinCount; ++x)
    {
        for (int y = 0; y < yBinCount; ++y)
        {
            double value = buffer[x + y * xBinCount];
            double patchX1 = SLIM_SCREEN_ROUND(interiorRect.left() + patchWidth * x) + intHeatMapMargins;
            double patchX2 = SLIM_SCREEN_ROUND(interiorRect.left() + patchWidth * (x + 1));
            double patchY1 = SLIM_SCREEN_ROUND(interiorRect.top() + patchHeight * y) + intHeatMapMargins;
            double patchY2 = SLIM_SCREEN_ROUND(interiorRect.top() + patchHeight * (y + 1));
            QRectF patchRect(patchX1, patchY1, patchX2 - patchX1, patchY2 - patchY1);
            
            if (generatingPDF_)
                patchRect.adjust(0.5 * heatmapMargins_, 0.5 * heatmapMargins_, -0.5 * heatmapMargins_, -0.5 * heatmapMargins_);
            
            double r, g, b;
            
            if (value == -10000)
            {
                r = 0.25; g = 0.25; b = 1.0;  // a special "no value" color for the 2D SFS plot
            }
            else
                Eidos_ColorPaletteLookup(1.0 - value, EidosColorPalette::kPalette_hot, r, g, b);
            
            painter.fillRect(patchRect, QtSLiMColorWithRGB(r, g, b, 1.0));
        }
    }
}

bool QtSLiMGraphView::addSubpopulationsToMenu(QComboBox *subpopButton, slim_objectid_t selectedSubpopID, slim_objectid_t avoidSubpopID)
{
	slim_objectid_t firstTag = -1;
    
    // QComboBox::currentIndexChanged signals will be sent during rebuilding; this flag
    // allows client code to avoid (over-)reacting to those signals.
    rebuildingMenu_ = true;
	
	// Depopulate and populate the menu
	subpopButton->clear();

	if (!controller_->invalidSimulation())
	{
		Population &population = controller_->sim->population_;
		
		for (auto popIter : population.subpops_)
		{
			slim_objectid_t subpopID = popIter.first;
			QString subpopString = QString("p%1").arg(subpopID);
			
			subpopButton->addItem(subpopString, subpopID);
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
            // If we have a tag to avoid, avoid it, preferring the second item if necessary
			if (firstTag == -1)
				firstTag = subpopID;
            if (firstTag == avoidSubpopID)
                firstTag = subpopID;
		}
	}
	
	//[subpopulationButton slimSortMenuItemsByTag];
	
	// If it is empty, disable it
	bool hasItems = (subpopButton->count() >= 1);
	
    subpopButton->setEnabled(hasItems);
	
    // Done rebuilding the menu, resume change messages
    rebuildingMenu_ = false;
    
	// Fix the selection and then select the chosen subpopulation
	if (hasItems)
	{
		int indexOfTag = subpopButton->findData(selectedSubpopID);
		
		if (indexOfTag == -1)
            selectedSubpopID = -1;
		if (selectedSubpopID == -1)
            selectedSubpopID = firstTag;
        if (selectedSubpopID == avoidSubpopID)
            selectedSubpopID = firstTag;
		
        subpopButton->setCurrentIndex(subpopButton->findData(selectedSubpopID));
        
        // this signal, emitted after rebuildingMenu_ is set to false, is the one that sticks
        emit subpopButton->QComboBox::currentIndexChanged(subpopButton->currentIndex());
	}
	
	return hasItems;	// true if we found at least one subpop to add to the menu, false otherwise
}

bool QtSLiMGraphView::addMutationTypesToMenu(QComboBox *mutTypeButton, int selectedMutIDIndex)
{
	int firstTag = -1;
	
    // QComboBox::currentIndexChanged signals will be sent during rebuilding; this flag
    // allows client code to avoid (over-)reacting to those signals.
    rebuildingMenu_ = true;
	
	// Depopulate and populate the menu
	mutTypeButton->clear();
	
	if (!controller_-> invalidSimulation())
	{
		std::map<slim_objectid_t,MutationType*> &mutationTypes = controller_->sim->mutation_types_;
		
		for (auto mutTypeIter : mutationTypes)
		{
			MutationType *mutationType = mutTypeIter.second;
			slim_objectid_t mutationTypeID = mutationType->mutation_type_id_;
			int mutationTypeIndex = mutationType->mutation_type_index_;
			QString mutationTypeString = QString("m%1").arg(mutationTypeID);
			
			mutTypeButton->addItem(mutationTypeString, mutationTypeIndex);
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = mutationTypeIndex;
		}
	}
	
	//[mutationTypeButton slimSortMenuItemsByTag];
	
	// If it is empty, disable it
	bool hasItems = (mutTypeButton->count() >= 1);
	
	mutTypeButton->setEnabled(hasItems);
	
    // Done rebuilding the menu, resume change messages
    rebuildingMenu_ = false;
    
	// Fix the selection and then select the chosen mutation type
	if (hasItems)
	{
		int indexOfTag = mutTypeButton->findData(selectedMutIDIndex);
		
		if (indexOfTag == -1)
            selectedMutIDIndex = -1;
		if (selectedMutIDIndex == -1)
            selectedMutIDIndex = firstTag;
		
		mutTypeButton->setCurrentIndex(mutTypeButton->findData(selectedMutIDIndex));
        
        // this signal, emitted after rebuildingMenu_ is set to false, is the one that sticks
        emit mutTypeButton->QComboBox::currentIndexChanged(mutTypeButton->currentIndex());
	}
	
	return hasItems;	// true if we found at least one muttype to add to the menu, false otherwise
}

size_t QtSLiMGraphView::tallyGUIMutationReferences(slim_objectid_t subpop_id, int muttype_index)
{
    //
	// this code is a slightly modified clone of the code in Population::TallyMutationReferences; here we scan only the
	// subpopulation that is being displayed in this graph, and tally into gui_scratch_reference_count only
	//
    SLiMSim *sim = controller_->sim;
	Population &population = sim->population_;
	size_t subpop_total_genome_count = 0;
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
    
    {
        int registry_size;
        const MutationIndex *registry = population.MutationRegistry(&registry_size);
        const MutationIndex *registry_iter_end = registry + registry_size;
        
        for (const MutationIndex *registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
            (mut_block_ptr + *registry_iter)->gui_scratch_reference_count_ = 0;
    }
    
    Subpopulation *subpop = sim->SubpopulationWithID(subpop_id);
    
    if (subpop)	// tally only within our chosen subpop
    {
        slim_popsize_t subpop_genome_count = 2 * subpop->parent_subpop_size_;
        std::vector<Genome *> &subpop_genomes = subpop->parent_genomes_;
        
        for (int i = 0; i < subpop_genome_count; i++)
        {
            Genome &genome = *subpop_genomes[static_cast<size_t>(i)];
            
            if (!genome.IsNull())
            {
                int mutrun_count = genome.mutrun_count_;
                
                for (int run_index = 0; run_index < mutrun_count; ++run_index)
                {
                    MutationRun *mutrun = genome.mutruns_[run_index].get();
                    const MutationIndex *genome_iter = mutrun->begin_pointer_const();
                    const MutationIndex *genome_end_iter = mutrun->end_pointer_const();
                    
                    for (; genome_iter != genome_end_iter; ++genome_iter)
                    {
                        const Mutation *mutation = mut_block_ptr + *genome_iter;
                        
                        if (mutation->mutation_type_ptr_->mutation_type_index_ == muttype_index)
                            (mutation->gui_scratch_reference_count_)++;
                    }
                }
                
                subpop_total_genome_count++;
            }
        }
    }
    
    return subpop_total_genome_count;
}

size_t QtSLiMGraphView::tallyGUIMutationReferences(const std::vector<Genome *> &genomes, int muttype_index)
{
    //
	// this code is a slightly modified clone of the code in Population::TallyMutationReferences; here we scan only the
	// subpopulation that is being displayed in this graph, and tally into gui_scratch_reference_count only
	//
    SLiMSim *sim = controller_->sim;
	Population &population = sim->population_;
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
    
    {
        int registry_size;
        const MutationIndex *registry = population.MutationRegistry(&registry_size);
        const MutationIndex *registry_iter_end = registry + registry_size;
        
        for (const MutationIndex *registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
            (mut_block_ptr + *registry_iter)->gui_scratch_reference_count_ = 0;
    }
    
	for (const Genome *genome : genomes)
	{
        if (!genome->IsNull())
        {
            int mutrun_count = genome->mutrun_count_;
            
            for (int run_index = 0; run_index < mutrun_count; ++run_index)
            {
                MutationRun *mutrun = genome->mutruns_[run_index].get();
                const MutationIndex *genome_iter = mutrun->begin_pointer_const();
                const MutationIndex *genome_end_iter = mutrun->end_pointer_const();
                
                for (; genome_iter != genome_end_iter; ++genome_iter)
                {
                    const Mutation *mutation = mut_block_ptr + *genome_iter;
                    
                    if (mutation->mutation_type_ptr_->mutation_type_index_ == muttype_index)
                        (mutation->gui_scratch_reference_count_)++;
                }
            }
        }
    }
    
    return genomes.size();
}




























