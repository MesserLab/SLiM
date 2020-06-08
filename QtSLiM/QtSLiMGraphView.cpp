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
#include <QtGlobal>
#include <QDebug>


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
    xAxisTickValuePrecision_ = 1;

    yAxisMin_ = 0.0;
    yAxisMax_ = 1.0;
    yAxisMajorTickInterval_ = 0.5;
    yAxisMinorTickInterval_ = 0.25;
    yAxisMajorTickModulus_ = 2;
    yAxisTickValuePrecision_ = 1;
    
    xAxisLabel_ = "This is the x-axis, yo";
    yAxisLabel_ = "This is the y-axis, yo";
    
    legendVisible_ = true;
    showHorizontalGridLines_ = false;
    showVerticalGridLines_ = false;
    showFullBox_ = false;
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

bool QtSLiMGraphView::needsButtonLayout(void)
{
    return false;
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
	
	// We go from the center of the first pixel to the center of the last pixel
	return (fractionAlongAxis * (interiorRect.width() - 1.0) + interiorRect.x()) + 0.5;
}

double QtSLiMGraphView::plotToDeviceY(double ploty, QRect interiorRect)
{
	double fractionAlongAxis = (ploty - yAxisMin_) / (yAxisMax_ - yAxisMin_);
	
	// We go from the center of the first pixel to the center of the last pixel
	return (fractionAlongAxis * (interiorRect.height() - 1.0) + interiorRect.y()) + 0.5;
}

double QtSLiMGraphView::roundPlotToDeviceX(double plotx, QRect interiorRect)
{
	double fractionAlongAxis = (plotx - xAxisMin_) / (xAxisMax_ - xAxisMin_);
	
	// We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
	return SLIM_SCREEN_ROUND(fractionAlongAxis * (interiorRect.width() - 1.0) + interiorRect.x()) + 0.5;
}

double QtSLiMGraphView::roundPlotToDeviceY(double ploty, QRect interiorRect)
{
	double fractionAlongAxis = (ploty - yAxisMin_) / (yAxisMax_ - yAxisMin_);
	
	// We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
	return SLIM_SCREEN_ROUND(fractionAlongAxis * (interiorRect.height() - 1.0) + interiorRect.y()) + 0.5;
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
	double minorTickInterval = xAxisMinorTickInterval_;
	int majorTickModulus = xAxisMajorTickModulus_;
	int tickValuePrecision = xAxisTickValuePrecision_;
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		bool isMajorTick = ((tickIndex % majorTickModulus) == 0);
		int xValueForTick = static_cast<int>(SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - axisMin) / (axisMax - axisMin))));    // left edge of pixel
		
		//qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", xValueForTick == " << xValueForTick;
		
        painter.fillRect(xValueForTick, interiorRect.y() - (isMajorTick ? 6 : 3), 1, isMajorTick ? 6 : 3, Qt::black);
        
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

void QtSLiMGraphView::drawXAxis(QPainter &painter, QRect interiorRect)
{
    int yAxisFudge = showYAxis_ ? 1 : 0;
	QRect axisRect(interiorRect.x() - yAxisFudge, interiorRect.y() - 1, interiorRect.width() + yAxisFudge, 1);
	
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
	double minorTickInterval = yAxisMinorTickInterval_;
	int majorTickModulus = yAxisMajorTickModulus_;
	int tickValuePrecision = yAxisTickValuePrecision_;
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		bool isMajorTick = ((tickIndex % majorTickModulus) == 0);
		int yValueForTick = static_cast<int>(SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - axisMin) / (axisMax - axisMin))));   // bottom edge of pixel
		
        //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", yValueForTick == " << yValueForTick;
		
        painter.fillRect(interiorRect.x() - (isMajorTick ? 6 : 3), yValueForTick, isMajorTick ? 6 : 3, 1, Qt::black);
		
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

void QtSLiMGraphView::drawYAxis(QPainter &painter, QRect interiorRect)
{
    int xAxisFudge = showXAxis_ ? 1 : 0;
	QRect axisRect(interiorRect.x() - 1, interiorRect.y() - xAxisFudge, 1, interiorRect.height() + xAxisFudge);
    
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
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		int xValueForTick = static_cast<int>(SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - axisMin) / static_cast<double>(axisMax - axisMin))));    // left edge of pixel
		
		if (xValueForTick == interiorRect.x())
			continue;
		if ((xValueForTick == (interiorRect.x() + interiorRect.width() - 1)) && showFullBox_)
			continue;
		
        painter.fillRect(xValueForTick, interiorRect.y(), 1, interiorRect.height(), gridColor);
	}
}

void QtSLiMGraphView::drawHorizontalGridLines(QPainter &painter, QRect interiorRect)
{
    QColor gridColor = QtSLiMGraphView::gridLineColor();
	double axisMin = yAxisMin_;
	double axisMax = yAxisMax_;
	double minorTickInterval = yAxisMinorTickInterval_;
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		int yValueForTick = static_cast<int>(SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - axisMin) / static_cast<double>(axisMax - axisMin))));   // bottom edge of pixel
		
		if (yValueForTick == interiorRect.y())
			continue;
		if ((yValueForTick == (interiorRect.y() + interiorRect.height() - 1)) && showFullBox_)
			continue;
		
        painter.fillRect(interiorRect.x(), yValueForTick, interiorRect.width(), 1, gridColor);
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
		legendRect.moveLeft(interiorRect.x() + interiorRect.width() - legendRect.width());
		legendRect.moveTop(interiorRect.y() + interiorRect.height() - legendRect.height());
		
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
	if (!controller_->invalidSimulation() && (controller_->sim->generation_ > 0))
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
			if (showXAxis_ && showXAxisTicks_)
				drawXAxisTicks(painter, interiorRect);
			
			if (showYAxis_ && showYAxisTicks_)
				drawYAxisTicks(painter, interiorRect);
			
			if (showXAxis_)
				drawXAxis(painter, interiorRect);
			
			if (showYAxis_)
				drawYAxis(painter, interiorRect);
			
			if (showFullBox_)
				drawFullBox(painter,interiorRect);
			
			// Overdraw the legend
			if (legendVisible_)
				drawLegendInInteriorRect(painter, interiorRect);
		}
        
        // unflip
        painter.restore();
	}
	else
	{
		// The controller is invalid, so just draw a generic message
        drawMessage(painter, "invalid\nsimulation", bounds);
	}
}

void QtSLiMGraphView::paintEvent(QPaintEvent * /* event */)
{
	QPainter painter(this);
    
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawContents(painter);
}

void QtSLiMGraphView::dispatch(QtSLiMWindow::DynamicDispatchID dispatchID)
{
    // poor man's dynamic dispatch; dispatchID is essentially the selector we want to send
    switch (dispatchID)
    {
    case QtSLiMWindow::DynamicDispatchID::updateAfterTick: updateAfterTick(); break;
    case QtSLiMWindow::DynamicDispatchID::controllerSelectionChanged: controllerSelectionChanged(); break;
    case QtSLiMWindow::DynamicDispatchID::controllerGenerationFinished: controllerGenerationFinished(); break;
    case QtSLiMWindow::DynamicDispatchID::controllerRecycled: controllerRecycled(); break;
    }
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
}

bool QtSLiMGraphView::providesStringForData(void)
{
    // subclassers that override stringForData() should also override this to return true
    // this is an annoying substitute for Obj-C's respondsToSelector:
    return false;
}

QString QtSLiMGraphView::stringForData(void)
{
    return QString();
}

QString QtSLiMGraphView::dateline(void)
{
    QDateTime dateTime = QDateTime::currentDateTime();
    QString dateTimeString = dateTime.toString("M/d/yy, h:mm:ss AP");        // format: 3/28/20, 8:03:09 PM
    
	return QString("# %1").arg(dateTimeString);
}

void QtSLiMGraphView::subclassAddItemsToMenu(QMenu & /* contextMenu */, QContextMenuEvent * /* event */)
{
}

void QtSLiMGraphView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!controller_->invalidSimulation()) // && ![[controller window] attachedSheet])
	{
		bool addedItems = false;
        QMenu contextMenu("graph_menu", this);
        
        QAction *legendToggle = nullptr;
        QAction *gridHToggle = nullptr;
        QAction *gridVToggle = nullptr;
        QAction *boxToggle = nullptr;
        QAction *changeXAxisScale = nullptr;
        QAction *changeXBarCount = nullptr;
        QAction *changeYAxisScale = nullptr;
        QAction *copyGraph = nullptr;
        QAction *exportGraph = nullptr;
        QAction *copyData = nullptr;
        QAction *exportData = nullptr;
        
		// Toggle legend visibility
		if (legendKey().size() > 0)
		{
            legendToggle = contextMenu.addAction(legendVisible_ ? "Hide Legend" : "Show Legend");
			addedItems = true;
		}
		
		// Toggle horizontal grid line visibility
		if (showYAxis_ && showYAxisTicks_)
		{
			gridHToggle = contextMenu.addAction(showHorizontalGridLines_ ? "Hide Horizontal Grid" : "Show Horizontal Grid");
			addedItems = true;
		}
		
		// Toggle vertical grid line visibility
		if (showXAxis_ && showXAxisTicks_)
		{
			gridVToggle = contextMenu.addAction(showVerticalGridLines_ ? "Hide Vertical Grid" : "Show Vertical Grid");
			addedItems = true;
		}
		
		// Toggle box visibility
		if (showXAxis_ && showYAxis_)
		{
			boxToggle = contextMenu.addAction(showFullBox_ ? "Hide Full Box" : "Show Full Box");
			addedItems = true;
		}
		
		// Add a separator if we had any visibility-toggle menu items above
        if (addedItems)
            contextMenu.addSeparator();
		addedItems = false;
		
		// Rescale axes
		if (showXAxis_ && showXAxisTicks_ && allowXAxisUserRescale_)
		{
            changeXAxisScale = contextMenu.addAction("Change X Axis Scale...");
			addedItems = true;
		}
		if (histogramBinCount_ && allowXAxisBinRescale_)
		{
            changeXBarCount = contextMenu.addAction("Change X Bar Count...");
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
        QAction *action = contextMenu.exec(event->globalPos());
        
        // Act upon the chosen action; we just do it right here instead of dealing with slots
        if (action)
        {
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
            if (action == changeXBarCount)
            {
                QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a bar count:", QStringList("Bar count:"), QStringList(QString::number(histogramBinCount_)));
                
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
            if (action == changeYAxisScale)
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
                    
                    pdfwriter.setCreator("QtSLiM");
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
                    
                    pdfwriter.setCreator("QtSLiM");
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
                QString data = stringForData();
                QClipboard *clipboard = QGuiApplication::clipboard();
                clipboard->setText(data);
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
	for (auto mutationTypeIter = sim->mutation_types_.begin(); mutationTypeIter != sim->mutation_types_.end(); ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
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
	
	for (int mainBinIndex = 0; mainBinIndex < mainBinCount; ++mainBinIndex)
	{
		double binMinValue = mainBinIndex * mainBinWidth + firstBinValue;
		double binMaxValue = (mainBinIndex + 1) * mainBinWidth + firstBinValue;
		double barLeft = roundPlotToDeviceX(binMinValue, interiorRect);
		double barRight = roundPlotToDeviceX(binMaxValue, interiorRect);
		
		switch (drawStyle)
		{
			case 0: barLeft += 1.5; barRight -= 1.5; break;
			case 1: barLeft += 0.5; barRight -= 0.5; break;
			case 2: barLeft -= 0.5; barRight += 0.5; break;
			case 3: barLeft -= 0.5; barRight += 0.5; break;
		}
		
		for (int subBinIndex = 0; subBinIndex < subBinCount; ++subBinIndex)
		{
			int actualBinIndex = subBinIndex + mainBinIndex * subBinCount;
			double binValue = buffer[actualBinIndex];
			double barBottom = interiorRect.y() - 1;
			double barTop = (fabs(binValue - 0) < 0.00000001 ? interiorRect.y() - 1 : roundPlotToDeviceY(binValue, interiorRect) + 0.5);
			QRect barRect(qRound(barLeft), qRound(barBottom), qRound(barRight - barLeft), qRound(barTop - barBottom));
			
			if (barRect.height() > 0.0)
			{
				// subdivide into sub-bars for each mutation type, if there is more than one
				if (subBinCount > 1)
				{
					double barWidth = barRect.width();
					double subBarWidth = (barWidth - 1) / subBinCount;
					double subbarLeft = SLIM_SCREEN_ROUND(barRect.x() + subBinIndex * subBarWidth);
					double subbarRight = SLIM_SCREEN_ROUND(barRect.x() + (subBinIndex + 1) * subBarWidth) + 1;
					
					barRect.setX(qRound(subbarLeft));
					barRect.setWidth(qRound(subbarRight - subbarLeft));
				}
                
				// fill and frame
				if (drawStyle == 3)
				{
                    painter.fillRect(barRect, Qt::black);
				}
				else
				{
                    painter.fillRect(barRect, controller_->blackContrastingColorForIndex(subBinIndex));
                    QtSLiMFrameRect(barRect, Qt::black, painter);
				}
			}
		}
	}
}

void QtSLiMGraphView::drawBarplot(QPainter &painter, QRect interiorRect, double *buffer, int binCount, double firstBinValue, double binWidth)
{
	drawGroupedBarplot(painter, interiorRect, buffer, 1, binCount, firstBinValue, binWidth);
}




























