//
//  QtSLiMGraphView.cpp
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
#include <QLabel>
#include <QPainterPath>
#include <QDebug>

#include <map>
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>

#include "species.h"
#include "subpopulation.h"
#include "genome.h"
#include "mutation_run.h"


QFont QtSLiMGraphView::labelFontOfPointSize(double size)
{
    static QFont timesNewRoman("Times New Roman", 10);
    
    // Derive a font of the proper size, while leaving the original untouched
    QFont font(timesNewRoman);
#ifdef __linux__
    font.setPointSizeF(size * 0.75);
#else
    // font sizes are calibrated for macOS; on Linux they need to be a little smaller
    font.setPointSizeF(size);
#endif
    
    return font;
}

QtSLiMGraphView::QtSLiMGraphView(QWidget *p_parent, QtSLiMWindow *controller) : QWidget(p_parent)
{
    controller_ = controller;
    setFocalDisplaySpecies(controller_->focalDisplaySpecies());
    
    connect(controller, &QtSLiMWindow::controllerUpdatedAfterTick, this, &QtSLiMGraphView::updateAfterTick);
    connect(controller, &QtSLiMWindow::controllerChromosomeSelectionChanged, this, &QtSLiMGraphView::controllerChromosomeSelectionChanged);
    connect(controller, &QtSLiMWindow::controllerTickFinished, this, &QtSLiMGraphView::controllerTickFinished);
    connect(controller, &QtSLiMWindow::controllerRecycled, this, &QtSLiMGraphView::controllerRecycled);
    
    x0_ = 0.0;
    x1_ = 1.0;
    y0_ = 0.0;
    y1_ = 1.0;
    
    showXAxis_ = true;
    allowXAxisUserRescale_ = true;
    showXAxisTicks_ = true;
    
    showYAxis_ = true;
    allowYAxisUserRescale_ = true;
    showYAxisTicks_ = true;
    
    xAxisMin_ = x0_;
    xAxisMax_ = x1_;
    xAxisMajorTickInterval_ = 0.5;
    xAxisMinorTickInterval_ = 0.25;
    xAxisMajorTickModulus_ = 2;
    xAxisHistogramStyle_ = false;
    xAxisTickValuePrecision_ = 1;
    xAxisLabelsType_ = 1;           // default numeric labels

    yAxisMin_ = y0_;
    yAxisMax_ = y1_;
    yAxisMajorTickInterval_ = 0.5;
    yAxisMinorTickInterval_ = 0.25;
    yAxisMajorTickModulus_ = 2;
    yAxisTickValuePrecision_ = 1;
    yAxisHistogramStyle_ = false;
    yAxisLog_ = false;
    yAxisLabelsType_ = 1;           // default numeric labels
    
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
    // It would be nice if we could call these methods automatically for subclasses, but we cannot.  By the time
    // this destructor has been called, the subclass has already been destructed, and a virtual function call
    // here calls the QtSLiMGraphView implementation, not the subclass implementation.  Subclasses that use these
    // methods must call them themselves in their destructors.
    QtSLiMGraphView::invalidateDrawingCache();
    QtSLiMGraphView::invalidateCachedData();
    
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
    xAxisLabelsType_ = 1;
    yAxisLabelsType_ = 1;
    
    controller_ = nullptr;
}

void QtSLiMGraphView::setFocalDisplaySpecies(Species *species)
{
    if (species)
    {
        focalSpeciesName_ = species->name_;
        // focalSpeciesAvatar_ is set by updateSpeciesBadge()
    }
    else
    {
        focalSpeciesName_ = "";
        focalSpeciesAvatar_ = "";
    }
}

Species *QtSLiMGraphView::focalDisplaySpecies(void)
{
    // We look up our focal species object by name every time, since keeping a pointer to it would be unsafe
    // Before initialize() is done species have not been created, so we return nullptr in that case
    // Some graph types will have no focal species; in that case we always return nullptr
    if (focalSpeciesName_.length() == 0)
        return nullptr;
    
    if (controller_ && controller_->community && (controller_->community->Tick() >= 1))
        return controller_->community->SpeciesWithName(focalSpeciesName_);
    
    return nullptr;
}

bool QtSLiMGraphView::missingFocalDisplaySpecies(void)
{
    if (focalSpeciesName_.length() == 0)
        return false;
    return (focalDisplaySpecies() == nullptr);
}

void QtSLiMGraphView::updateSpeciesBadge(void)
{
    // graphs that do not have a focal species, such as QtSLiMGraphView_MultispeciesPopSizeOverTime, have no species badge
    if (focalSpeciesName_.length() == 0)
        return;
    
    // if we do not have a button layout, punt; in some cases we get called by updateAfterTick() before we have been placed in our window
    QHBoxLayout *enclosingLayout = buttonLayout();
    
    if (!enclosingLayout)
        return;
    
    int layoutCount = enclosingLayout->count();
    QLayoutItem *labelItem = (layoutCount > 0) ? enclosingLayout->itemAt(0) : nullptr;
    QWidget *labelWidget = labelItem ? labelItem->widget() : nullptr;
    QLabel *speciesLabel = qobject_cast<QLabel *>(labelWidget);
    
    if (!speciesLabel)
    {
        qDebug() << "No species label!  enclosingLayout ==" << enclosingLayout << ", layoutCount ==" << layoutCount << ", labelItem ==" << labelItem << ", labelWidget ==" << labelWidget;
        return;
    }
    
    Species *graphSpecies = focalDisplaySpecies();
    
    // Cache our graphSpecies avatar whenever we're in a valid state, because it could change,
    // and because we want to be able to display it even when the sim is in an invalid state
    if (graphSpecies)
    {
        if (graphSpecies->community_.all_species_.size() > 1)
            focalSpeciesAvatar_ = graphSpecies->avatar_;
        else
            focalSpeciesAvatar_ = "";
    }
    
    // Display our current avatar cache; if we have no avatar, hide the label
    if (focalSpeciesAvatar_.length())
    {
        speciesLabel->setText(QString::fromStdString(focalSpeciesAvatar_));
        speciesLabel->setHidden(false);
    }
    else
    {
        speciesLabel->setText("");
        speciesLabel->setHidden(true);
    }
}

QHBoxLayout *QtSLiMGraphView::buttonLayout(void)
{
    // Note this method makes assumptions about the layouts in the parent window
    // It needs to be kept parallel to QtSLiMWindow::graphWindowWithView()
    QVBoxLayout *topLayout = dynamic_cast<QVBoxLayout *>(window()->layout());
    
    if (topLayout && (topLayout->count() >= 2))
    {
        QLayoutItem *layoutItem = topLayout->itemAt(1);
        
        return dynamic_cast<QHBoxLayout *>(layoutItem);
    }
    
    return nullptr;
}

QPushButton *QtSLiMGraphView::actionButton(void)
{
    // Note this method makes assumptions about the layouts in the parent window
    // It needs to be kept parallel to QtSLiMWindow::graphWindowWithView()
    QHBoxLayout *enclosingLayout = buttonLayout();
    int layoutCount = enclosingLayout ? enclosingLayout->count() : 0;
    QLayoutItem *buttonItem = (layoutCount > 0) ? enclosingLayout->itemAt(layoutCount - 1) : nullptr;
    QWidget *buttonWidget = buttonItem ? buttonItem->widget() : nullptr;
    
    return qobject_cast<QPushButton *>(buttonWidget);
}

QComboBox *QtSLiMGraphView::newButtonInLayout(QHBoxLayout *p_layout)
{
    QComboBox *button = new QComboBox(this);
    button->setEditable(false);
    button->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    button->setMinimumContentsLength(2);
    p_layout->insertWidget(p_layout->count() - 2, button);  // left of the spacer and action button
    
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
	double fractionAlongSide = (plotx - x0_) / (x1_ - x0_);
	
    if (generatingPDF_)
    {
        // We go from the left edge of the first pixel to the right edge of the last pixel
        return (fractionAlongSide * interiorRect.width() + interiorRect.x());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel
        return (fractionAlongSide * (interiorRect.width() - 1.0) + interiorRect.x()) + 0.5;
    }
}

double QtSLiMGraphView::plotToDeviceY(double ploty, QRect interiorRect)
{
	double fractionAlongSide = (ploty - y0_) / (y1_ - y0_);
	
    if (generatingPDF_)
    {
        // We go from the bottom edge of the first pixel to the top edge of the last pixel
        return (fractionAlongSide * interiorRect.height() + interiorRect.y());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel
        return (fractionAlongSide * (interiorRect.height() - 1.0) + interiorRect.y()) + 0.5;
    }
}

double QtSLiMGraphView::roundPlotToDeviceX(double plotx, QRect interiorRect)
{
	double fractionAlongSide = (plotx - x0_) / (x1_ - x0_);
	
    if (generatingPDF_)
    {
        // We go from the left edge of the first pixel to the right edge of the last pixel
        return (fractionAlongSide * interiorRect.width() + interiorRect.x());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
        return SLIM_SCREEN_ROUND(fractionAlongSide * (interiorRect.width() - 1.0) + interiorRect.x()) + 0.5;
    }
}

double QtSLiMGraphView::roundPlotToDeviceY(double ploty, QRect interiorRect)
{
	double fractionAlongSide = (ploty - y0_) / (y1_ - y0_);
	
    if (generatingPDF_)
    {
        // We go from the bottom edge of the first pixel to the top edge of the last pixel
        return (fractionAlongSide * interiorRect.height() + interiorRect.y());
    }
    else
    {
        // We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
        return SLIM_SCREEN_ROUND(fractionAlongSide * (interiorRect.height() - 1.0) + interiorRect.y()) + 0.5;
    }
}

void QtSLiMGraphView::willDraw(QPainter & /* painter */, QRect /* interiorRect */)
{
}

QString QtSLiMGraphView::labelTextForTick(double tickValue, int tickValuePrecision, double minorTickInterval)
{
    // This utility method handles negative tickValuePrecision values, which request
    // output mode 'g' instead of 'f', and also make sure that values extremely close
    // to zero are output as zero.  (The need for the latter correction is because
    // we use a double value as a for loop index in the plotting code, which is not
    // really a good idea.)
    if (std::fabs(tickValue) < std::fabs(minorTickInterval) / 1e6)
        tickValue = 0.0;
    
    if (tickValuePrecision < 0)
        return QString("%1").arg(tickValue, 0, 'g', -tickValuePrecision);
    else
        return QString("%1").arg(tickValue, 0, 'f', tickValuePrecision);
}

void QtSLiMGraphView::drawAxisTickLabel(QPainter &painter, QString labelText, double xValueForTick, double axisLength,
                                        bool isFirstTick, bool isLastTick)
{
    // Draws a tick label.  This method thinks of the axis as being the x axis, and assumes that the coordinate system
    // of the painter has been rotated as needed for that assumption to make sense.  The coordinate system should be
    // shifted so that the axis starts at x==0, and drawing the text with a baseline at y==0 is correct.
    QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
    double labelWidth = labelBoundingRect.width();
    double labelX = xValueForTick - SLIM_SCREEN_ROUND(labelWidth / 2.0);
    
    if (tweakXAxisTickLabelAlignment_)
    {
        if (isFirstTick && (labelX < 0))
            labelX = xValueForTick - 2.0;
        else if (isLastTick && (labelX + labelWidth > axisLength))
            labelX = xValueForTick - SLIM_SCREEN_ROUND(labelWidth) + 2.0;
    }
    
    // draw a debugging line that is positioned where we intend the baseline of the tick label to go
    //painter.fillRect(QRectF(0, 0, axisLength, 1), Qt::red);
    
    painter.drawText(QPointF(labelX, 0), labelText);
}

void QtSLiMGraphView::drawXAxisTicks(QPainter &painter, QRect interiorRect)
{
    QFont font = QtSLiMGraphView::fontForTickLabels();
    QFontMetricsF fontMetrics(font);
    double capHeight = std::ceil(fontMetrics.capHeight());
    
    painter.setFont(font);
    painter.setBrush(Qt::black);
    
    if (xAxisAt_)
    {
        // user-specified tick positions, which may or may not have corresponding label strings
        int tickCount = (int)xAxisAt_->size();
        
        for (int tickIndex = 0; tickIndex < tickCount; ++tickIndex)
        {
            double tickValue = (*xAxisAt_)[tickIndex];
            bool isFirstTick = (tickIndex == 0);
            bool isLastTick = (tickIndex == tickCount - 1);
            QString labelText;
            
            if (xAxisLabelsType_ == 1)
                labelText = labelTextForTick(tickValue, -8, 1e-100);
            else if (xAxisLabelsType_ == 2)
                labelText = (*xAxisLabels_)[tickIndex];
            else
                labelText = " ";    // force a major tick mark when labels are turned off
            
            bool isMajorTick = labelText.length();
            double tickLength = (isMajorTick ? 6 : 3);
            double xValueForTick;
            
            if (generatingPDF_)
                xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - x0_) / (x1_ - x0_)) - 0.5);    // left edge of pixel
            else
                xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - x0_) / (x1_ - x0_)));    // left edge of pixel
            
            //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", xValueForTick == " << xValueForTick;
            
            painter.fillRect(QRectF(xValueForTick, interiorRect.y() - tickLength, 1, tickLength - (generatingPDF_ ? 0.5 : 0.0)), Qt::black);
            
            if (isMajorTick && (xAxisLabelsType_ != 0))
            {
                painter.save();
                painter.translate(interiorRect.x(), 41 - capHeight);
                painter.scale(1.0, -1.0);
                drawAxisTickLabel(painter, labelText, xValueForTick - interiorRect.x(), interiorRect.width(), isFirstTick, isLastTick);
                painter.restore();
            }
        }
    }
    else
    {
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
                bool isFirstTick = (tickIndex == 0);
                bool isLastTick = (tickValue + minorTickInterval > (axisMax + minorTickInterval / 10.0));
                bool isMajorTick = ((tickIndex % majorTickModulus) == 0);
                double tickLength = (isMajorTick ? 6 : 3);
                double xValueForTick;
                
                if (generatingPDF_)
                    xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - x0_) / (x1_ - x0_)) - 0.5);    // left edge of pixel
                else
                    xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - x0_) / (x1_ - x0_)));    // left edge of pixel
                
                //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", xValueForTick == " << xValueForTick;
                
                painter.fillRect(QRectF(xValueForTick, interiorRect.y() - tickLength, 1, tickLength - (generatingPDF_ ? 0.5 : 0.0)), Qt::black);
                
                if (isMajorTick && (xAxisLabelsType_ != 0))
                {
                    QString labelText = labelTextForTick(tickValue, tickValuePrecision, minorTickInterval);
                    
                    painter.save();
                    painter.translate(interiorRect.x(), 41 - capHeight);
                    painter.scale(1.0, -1.0);
                    drawAxisTickLabel(painter, labelText, xValueForTick - interiorRect.x(), interiorRect.width(), isFirstTick, isLastTick);
                    painter.restore();
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
                bool isFirstTick = (tickValue == axisStart);
                bool isLastTick = (tickValue == axisMax);
                bool isMajorTick = isFirstTick || isLastTick;
                double tickLength = (isMajorTick ? 6 : 3);
                double xValueForTick;
                
                if (generatingPDF_)
                    xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - 0.5 - x0_) / (x1_ - x0_)) - 0.5);    // left edge of pixel
                else
                    xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - 0.5 - x0_) / (x1_ - x0_)));    // left edge of pixel
                
                //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", xValueForTick == " << xValueForTick;
                
                painter.fillRect(QRectF(xValueForTick, interiorRect.y() - tickLength, 1, tickLength - (generatingPDF_ ? 0.5 : 0.0)), Qt::black);
                
                if (isMajorTick && (xAxisLabelsType_ != 0))
                {
                    QString labelText = labelTextForTick(tickValue, tickValuePrecision, 1.0);
                    
                    painter.save();
                    painter.translate(interiorRect.x(), 41 - capHeight);
                    painter.scale(1.0, -1.0);
                    drawAxisTickLabel(painter, labelText, xValueForTick - interiorRect.x(), interiorRect.width(), isFirstTick, isLastTick);
                    painter.restore();
                }
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
    QFont font = QtSLiMGraphView::fontForAxisLabels();
    QFontMetricsF fontMetrics(font);
    double capHeight = fontMetrics.capHeight();
    
    painter.setFont(font);
    painter.setBrush(Qt::black);
    
    QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, xAxisLabel_);
    QPoint drawPoint(interiorRect.x() + (interiorRect.width() - labelBoundingRect.width()) / 2, 0);
    
    painter.save();
    painter.translate(0, 14 - std::ceil(capHeight / 2.0));
    painter.scale(1.0, -1.0);
    
    // draw debugging lines that are positioned where we intend the axis label to go
    //painter.fillRect(QRectF(interiorRect.x(), 0, interiorRect.width(), 1), Qt::blue);
    //painter.fillRect(QRectF(drawPoint.x(), -capHeight * 0.5, labelBoundingRect.width(), 1), Qt::red);
    
    painter.drawText(drawPoint, xAxisLabel_);
    
    painter.restore();
}

void QtSLiMGraphView::drawYAxisTicks(QPainter &painter, QRect interiorRect)
{
    painter.setFont(QtSLiMGraphView::fontForTickLabels());
    painter.setBrush(Qt::black);
    
    if (yAxisAt_)
    {
        // user-specified tick positions, which may or may not have corresponding label strings
        int tickCount = (int)yAxisAt_->size();
        
        for (int tickIndex = 0; tickIndex < tickCount; ++tickIndex)
        {
            double tickValue = (*yAxisAt_)[tickIndex];
            bool isFirstTick = (tickIndex == 0);
            bool isLastTick = (tickIndex == tickCount - 1);
            QString labelText;
            
            if (yAxisLabelsType_ == 1)
                labelText = labelTextForTick(tickValue, -8, 1e-100);
            else if (yAxisLabelsType_ == 2)
                labelText = (*yAxisLabels_)[tickIndex];
            else
                labelText = " ";    // force a major tick mark when labels are turned off
            
            bool isMajorTick = labelText.length();
            double tickLength = (isMajorTick ? 6 : 3);
            double yValueForTick;
            
            if (generatingPDF_)
                yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((tickValue - y0_) / (y1_ - y0_)) - 0.5);   // bottom edge of pixel
            else
                yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - y0_) / (y1_ - y0_)));   // bottom edge of pixel
            
            //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", yValueForTick == " << yValueForTick;
            
            painter.fillRect(QRectF(interiorRect.x() - tickLength, yValueForTick, tickLength - (generatingPDF_ ? 0.5 : 0.0), 1), Qt::black);
            
            if (isMajorTick)
            {
                painter.save();
                painter.translate(41, interiorRect.y());
                painter.rotate(90);
                painter.scale(1.0, -1.0);
                drawAxisTickLabel(painter, labelText, yValueForTick - interiorRect.y(), interiorRect.height(), isFirstTick, isLastTick);
                painter.restore();
            }
        }
    }
    else
    {
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
                bool isFirstTick = (tickIndex == 0);
                bool isLastTick = (tickValue + minorTickInterval > (axisMax + minorTickInterval / 10.0));
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
                    yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((transformedTickValue - y0_) / (y1_ - y0_)) - 0.5);   // bottom edge of pixel
                else
                    yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((transformedTickValue - y0_) / (y1_ - y0_)));   // bottom edge of pixel
                
                //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", yValueForTick == " << yValueForTick;
                
                painter.fillRect(QRectF(interiorRect.x() - tickLength, yValueForTick, tickLength - (generatingPDF_ ? 0.5 : 0.0), 1), Qt::black);
                
                if (isMajorTick)
                {
                    QString labelText = labelTextForTick(tickValue, tickValuePrecision, minorTickInterval);
                    
                    if (yAxisLog_)
                    {
                        if (std::abs(tickValue - 0.0) < 0.0000001)          labelText = "1";
                        else if (std::abs(tickValue - 1.0) < 0.0000001)     labelText = "10";
                        else if (std::abs(tickValue - 2.0) < 0.0000001)     labelText = "100";
                        else                                                labelText = QString("10^%1").arg((int)round(tickValue));
                    }
                    
                    painter.save();
                    painter.translate(41, interiorRect.y());
                    painter.rotate(90);
                    painter.scale(1.0, -1.0);
                    drawAxisTickLabel(painter, labelText, yValueForTick - interiorRect.y(), interiorRect.height(), isFirstTick, isLastTick);
                    painter.restore();
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
                bool isFirstTick = (tickValue == axisStart);
                bool isLastTick = (tickValue == axisMax);
                bool isMajorTick = isFirstTick || isLastTick;
                double tickLength = (isMajorTick ? 6 : 3);
                double yValueForTick;
                
                if (generatingPDF_)
                    yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((tickValue - 0.5 - y0_) / (y1_ - y0_)) - 0.5);   // bottom edge of pixel
                else
                    yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - 0.5 - y0_) / (y1_ - y0_)));   // bottom edge of pixel
                
                //qDebug() << "tickValue == " << tickValue << ", isMajorTick == " << (isMajorTick ? "YES" : "NO") << ", yValueForTick == " << yValueForTick;
                
                painter.fillRect(QRectF(interiorRect.x() - tickLength, yValueForTick, tickLength - (generatingPDF_ ? 0.5 : 0.0), 1), Qt::black);
                
                if (isMajorTick)
                {
                    QString labelText = labelTextForTick(tickValue, tickValuePrecision, 1.0);
                    
                    painter.save();
                    painter.translate(41, interiorRect.y());
                    painter.rotate(90);
                    painter.scale(1.0, -1.0);
                    drawAxisTickLabel(painter, labelText, yValueForTick - interiorRect.y(), interiorRect.height(), isFirstTick, isLastTick);
                    painter.restore();
                }
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
    QFont font = QtSLiMGraphView::fontForAxisLabels();
    QFontMetricsF fontMetrics(font);
    double capHeight = fontMetrics.capHeight();
    
    painter.setFont(font);
    painter.setBrush(Qt::black);
    
    QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, yAxisLabel_);
    QPoint drawPoint(interiorRect.y() + (interiorRect.height() - labelBoundingRect.width()) / 2, 0);
    
    painter.save();
    painter.translate(11 + std::ceil(capHeight / 2.0), 0.0);
    painter.rotate(90);
    painter.scale(1.0, -1.0);
    
    // draw debugging lines that are positioned where we intend the axis label to go
    //painter.fillRect(QRectF(interiorRect.y(), 0, interiorRect.height(), 1), Qt::blue);
    //painter.fillRect(QRectF(drawPoint.x(), -capHeight * 0.5, labelBoundingRect.width(), 1), Qt::red);
    
    painter.drawText(drawPoint, yAxisLabel_);
    
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
    // we assume that no grid lines fall outside of the axis range
    QColor gridColor = QtSLiMGraphView::gridLineColor();
	double axisMin = xAxisMin_;
	double axisMax = xAxisMax_;
	double minorTickInterval = xAxisMinorTickInterval_;
	double tickValue;
	
	for (tickValue = axisMin; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval)
	{
		double xValueForTick;
        
        if (generatingPDF_)
            xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + interiorRect.width() * ((tickValue - x0_) / (x1_ - x0_)) - 0.5);    // left edge of pixel
        else
            xValueForTick = SLIM_SCREEN_ROUND(interiorRect.x() + (interiorRect.width() - 1) * ((tickValue - x0_) / (x1_ - x0_)));    // left edge of pixel
		
		if (std::abs(xValueForTick - interiorRect.x()) < 1.25)
			continue;
		if ((std::abs(xValueForTick - (interiorRect.x() + interiorRect.width() - 1)) < 1.25) && showFullBox_)
			continue;
		
        painter.fillRect(QRectF(xValueForTick, interiorRect.y(), 1, interiorRect.height()), gridColor);
	}
}

void QtSLiMGraphView::drawHorizontalGridLines(QPainter &painter, QRect interiorRect)
{
    // we assume that no grid lines fall outside of the axis range
    QColor gridColor = QtSLiMGraphView::gridLineColor();
	double axisMin = yAxisMin_;
	double axisMax = yAxisMax_;
	double minorTickInterval = yAxisMinorTickInterval_;
	double tickValue;
    double axisStart = (yAxisLog_ ? round(axisMin) : axisMin);  // with a log scale, we leave a little room at the bottom
    double tickValueIncrement = (showGridLinesMajorOnly_ ? yAxisMajorTickInterval_ : minorTickInterval);
	
	for (tickValue = axisStart; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += tickValueIncrement)
	{
		double yValueForTick;
        
        if (generatingPDF_)
            yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + interiorRect.height() * ((tickValue - y0_) / (y1_ - y0_)) - 0.5);   // bottom edge of pixel
        else
            yValueForTick = SLIM_SCREEN_ROUND(interiorRect.y() + (interiorRect.height() - 1) * ((tickValue - y0_) / (y1_ - y0_)));   // bottom edge of pixel
		
		if (std::abs(yValueForTick - interiorRect.y()) < 1.25)
			continue;
		if ((std::abs(yValueForTick - (interiorRect.y() + interiorRect.height() - 1)) < 1.25) && showFullBox_)
			continue;
		
        painter.fillRect(QRectF(interiorRect.x(), yValueForTick, interiorRect.width(), 1), gridColor);
	}
}

void QtSLiMGraphView::drawMessage(QPainter &painter, QString messageString, QRect p_rect)
{
    painter.setFont(QtSLiMGraphView::labelFontOfPointSize(16));
    painter.setBrush(QtSLiMColorWithWhite(0.4, 1.0));
    
    painter.drawText(p_rect, Qt::AlignHCenter | Qt::AlignVCenter, messageString);
}

void QtSLiMGraphView::drawGraph(QPainter &painter, QRect interiorRect)
{
    painter.fillRect(interiorRect, QtSLiMColorWithHSV(0.15, 0.15, 1.0, 1.0));
}

QtSLiMLegendSpec QtSLiMGraphView::legendKey(void)
{
    return QtSLiMLegendSpec();
}

int QtSLiMGraphView::lineCountForLegend(QtSLiMLegendSpec &legend)
{
    // check for duplicate labels, which get uniqued into a single line
    // displayedLabels maps from label to index, but we don't use the index here; parallel with drawLegend()
    QMap<QString, int> displayedLabels;
    int lineCount = 0;
    
    for (const QtSLiMLegendEntry &legendEntry : legend)
    {
        QString labelString = legendEntry.label;
        auto existingEntry = displayedLabels.find(labelString);
        
        if (existingEntry == displayedLabels.end())
        {
            // not a duplicate
            displayedLabels.insert(labelString, 0);
            lineCount++;
        }
    }
    
    return lineCount;
}

double QtSLiMGraphView::graphicsWidthForLegend(QtSLiMLegendSpec &legend, double legendLineHeight)
{
    // with a specified width, use that width
    if (legend_graphicsWidth != -1)
        return legend_graphicsWidth;
    
    // otherwise, the default graphics width depends upon whether there are any duplicate
    // entries and lines; we want to make the area a bit wider if we have points on top of lines
    const double legendGraphicsWidth_default = legendLineHeight;
    
    int entryCount = static_cast<int>(legend.size());
    int lineCount = lineCountForLegend(legend);    // remove duplicate lines from the count
    
    if (entryCount != lineCount)
    {
        for (const QtSLiMLegendEntry &legendEntry : legend)
        {
            if (legendEntry.entry_type == QtSLiM_LegendEntryType::kLine)
            {
                // duplicates entries, and some entries are lines; expand
                return legendGraphicsWidth_default * 2.0;
            }
        }
    }
    
    return legendGraphicsWidth_default;
}

QSizeF QtSLiMGraphView::legendSize(QPainter &painter)
{
    // This method must be synchronized with QtSLiMGraphView::drawLegend()
    QtSLiMLegendSpec legend = legendKey();
    int entryCount = static_cast<int>(legend.size());
    
	if (entryCount == 0)
		return QSize();
    
    const double legendLabelPointSize = ((legend_labelSize == -1) ? 10 : legend_labelSize);
    const double legendLineHeight = ((legend_lineHeight == -1) ? legendLabelPointSize : legend_lineHeight);
    const double legendInteriorMargin = ((legend_interiorMargin == -1) ?  5 : legend_interiorMargin);
    const double legendGraphicsWidth = graphicsWidthForLegend(legend, legendLineHeight);
    
    int lineCount = lineCountForLegend(legend);    // remove duplicate lines from the count
    QSizeF legend_size = QSize(0, legendLineHeight * lineCount + legendInteriorMargin * (lineCount - 1));
	
    for (const QtSLiMLegendEntry &legendEntry : legend)
    {
        // we don't bother removing duplicate lines here, we just measure them twice; no harm
        QString labelString = legendEntry.label;
        
        // incorporate the width of the label into the width of the legend
        QRectF labelBoundingBox = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelString);
        double labelWidth = legendGraphicsWidth + legendInteriorMargin + labelBoundingBox.width();
        
        labelWidth = SLIM_SCREEN_ROUND(labelWidth);
        
        legend_size.setWidth(std::max(legend_size.width(), labelWidth));
    }
    
	return legend_size;
}

void QtSLiMGraphView::drawLegend(QPainter &painter, QRectF legendRect)
{
    // This method must be synchronized with QtSLiMGraphView::legendSize()
    // drawLegendInInteriorRect() has already done the frame/fill, including margins, for us
    QtSLiMLegendSpec legend = legendKey();
    int entryCount = static_cast<int>(legend.size());
    
    if (entryCount == 0)
        return;
    
    const double legendLabelPointSize = ((legend_labelSize == -1) ? 10 : legend_labelSize);
    const double legendLineHeight = ((legend_lineHeight == -1) ? legendLabelPointSize : legend_lineHeight);
    const double legendInteriorMargin = ((legend_interiorMargin == -1) ?  5 : legend_interiorMargin);
    const double legendGraphicsWidth = graphicsWidthForLegend(legend, legendLineHeight);
    
    QFont legendFont = labelFontOfPointSize(legendLabelPointSize);
    QFontMetricsF legendFontMetrics(legendFont);
    double capHeight = legendFontMetrics.capHeight();
    const double labelVerticalAdjust = (legendLineHeight - capHeight) / 2.0;
    double swatchSize = capHeight * 1.5;
    int lineCount = lineCountForLegend(legend);             // remove duplicate lines from the count
    QMap<QString, int> displayedLabels;       // maps from label to index
    
#if 0
    // show the legend layout, for debugging
    for (int index = 0; index < lineCount; ++index)
    {
        int positionIndex = (lineCount - 1) - index;                   // top to bottom
        QRectF entryBox(legendRect.x(), legendRect.y() + positionIndex * (legendLineHeight + legendInteriorMargin), legendRect.width(), legendLineHeight);
        QRectF graphicsBox(entryBox);
        QRectF labelBox(entryBox);
        
        graphicsBox.setWidth(legendGraphicsWidth);
        labelBox.adjust(legendGraphicsWidth + legendInteriorMargin, 0, 0, 0);
        
        painter.fillRect(entryBox, Qt::white);
        painter.fillRect(graphicsBox, QtSLiMColorWithWhite(0.85, 1.0));
        painter.fillRect(labelBox, QtSLiMColorWithWhite(0.85, 1.0));
        QtSLiMFrameRect(entryBox, Qt::black, painter, 1.0);
    }
#endif
    
    int nextLinePosition = lineCount - 1;      // top to bottom
    
    for (int index = 0; index < entryCount; ++index)
    {
        const QtSLiMLegendEntry &legendEntry = legend[index];
        QString labelString = legendEntry.label;
        
        // check for duplicate labels, which get uniqued into a single line
        int positionIndex;
        auto existingEntry = displayedLabels.find(labelString);
        
        if (existingEntry == displayedLabels.end())
        {
            // not a duplicate
            positionIndex = (nextLinePosition--);
            displayedLabels.insert(labelString, positionIndex);
        }
        else
        {
            // duplicate; use the previously determined position
            positionIndex = existingEntry.value();
        }
        
        QRectF entryBox(legendRect.x(), legendRect.y() + positionIndex * (legendLineHeight + legendInteriorMargin), legendRect.width(), legendLineHeight);
        QRectF graphicsBox(entryBox);
        QRectF labelBox(entryBox);
        
        graphicsBox.setWidth(legendGraphicsWidth);
        labelBox.adjust(legendGraphicsWidth + legendInteriorMargin, 0, 0, 0);
        
        // draw the graphics in graphicsBox
        switch (legendEntry.entry_type)
        {
        case QtSLiM_LegendEntryType::kSwatch:
        {
            QRectF swatchBox = graphicsBox;
            
            // make the width and height be, at most, swatchSize (a scaled factor of the capHeight)
            {
                double widthAdj = (swatchBox.width() > swatchSize) ? (swatchBox.width() - swatchSize) / 2 : 0;
                double heightAdj = (swatchBox.height() > swatchSize) ? (swatchBox.height() - swatchSize) / 2 : 0;
                swatchBox.adjust(widthAdj, heightAdj, -widthAdj, -heightAdj);
            }
            
            // make sure the swatch is square, by shrinking it
            if (swatchBox.width() != swatchBox.height())
            {
                double widthAdj = (swatchBox.width() > swatchBox.height()) ? (swatchBox.width() - swatchBox.height()) / 2 : 0;
                double heightAdj = (swatchBox.height() > swatchBox.width()) ? (swatchBox.height() - swatchBox.width()) / 2 : 0;
                swatchBox.adjust(widthAdj, heightAdj, -widthAdj, -heightAdj);
            }
            
            QColor swatchColor = legendEntry.swatch_color;
            
            painter.fillRect(swatchBox, swatchColor);
            QtSLiMFrameRect(swatchBox, Qt::black, painter, 1.0);
            break;
        }
        case QtSLiM_LegendEntryType::kLine:
        {
            double lineWidth = legendEntry.line_lwd;
            QColor lineColor = legendEntry.line_color;
            QPainterPath linePath;
            QPen linePen(lineColor, lineWidth);
            double y = SLIM_SCREEN_ROUND(graphicsBox.center().y()) + 0.5;
            
            linePen.setCapStyle(Qt::FlatCap);
            
            linePath.moveTo(graphicsBox.left(), y);
            linePath.lineTo(graphicsBox.right(), y);
            painter.strokePath(linePath, linePen);
            break;
        }
        case QtSLiM_LegendEntryType::kPoint:
        {
            drawPointSymbol(painter, graphicsBox.center().x(), graphicsBox.center().y(),
                            legendEntry.point_symbol, legendEntry.point_color, legendEntry.point_border,
                            legendEntry.point_lwd, legendEntry.point_size);
            break;
        }
        default: break;
        }
        
        // if the entry is not a duplicate, draw the text label
        if (existingEntry == displayedLabels.end())
        {
            double labelX = labelBox.x();
            double labelY = labelBox.y() + labelVerticalAdjust;
            
            labelY = painter.transform().map(QPointF(labelX, labelY)).y();
            
            painter.setWorldMatrixEnabled(false);
            painter.drawText(QPointF(labelX, labelY), labelString);
            painter.setWorldMatrixEnabled(true);
        }
    }
}

void QtSLiMGraphView::drawLegendInInteriorRect(QPainter &painter, QRect interiorRect)
{
    // set the legend label font for the methods we call, which will rely on it
    const double legendLabelPointSize = ((legend_labelSize == -1) ? 10 : legend_labelSize);
    QFont legendFont = labelFontOfPointSize(legendLabelPointSize);
    
    painter.setFont(legendFont);
    
    // assess the size of the legend, given all configuration preferences
    QSizeF legend_size = legendSize(painter);
	int legendWidth = static_cast<int>(ceil(legend_size.width()));
	int legendHeight = static_cast<int>(ceil(legend_size.height()));
	
	if ((legendWidth > 0) && (legendHeight > 0))
	{
        // legend_entryMargin provides the margin between and around each entry, within the legend's box; +1 for the width of the legend's frame
        const double legendExteriorMargin = ((legend_exteriorMargin == -1) ?  9 : legend_exteriorMargin) + 1;
        
		QRect legendRect(0, 0, legendWidth + legendExteriorMargin + legendExteriorMargin, legendHeight + legendExteriorMargin + legendExteriorMargin);
        
        // positional inset from the edge, outside the legend's box; -1 so an inset of zero matches the "full box"
        int legendInset = ((legend_inset == -1) ?  3 : legend_inset) - 1;
        
		// position the legend in the chosen corner with the chosen inset
        QtSLiM_LegendPosition position = legend_position_;
        
        if (position == QtSLiM_LegendPosition::kUnconfigured)
            position = QtSLiM_LegendPosition::kTopRight;
        
        if ((position == QtSLiM_LegendPosition::kTopLeft) || (position == QtSLiM_LegendPosition::kTopRight))
            legendRect.moveTop(interiorRect.y() + interiorRect.height() - (legendRect.height() + legendInset));
        else if ((position == QtSLiM_LegendPosition::kBottomLeft) || (position == QtSLiM_LegendPosition::kBottomRight))
            legendRect.moveTop(interiorRect.y() + legendInset);
        
        if ((position == QtSLiM_LegendPosition::kTopRight) || (position == QtSLiM_LegendPosition::kBottomRight))
            legendRect.moveLeft(interiorRect.x() + interiorRect.width() - (legendRect.width() + legendInset));
        else if ((position == QtSLiM_LegendPosition::kTopLeft) || (position == QtSLiM_LegendPosition::kBottomLeft))
            legendRect.moveLeft(interiorRect.x() + legendInset);
        
		// frame the legend and erase it with a slightly gray wash
        painter.fillRect(legendRect, QtSLiMColorWithWhite(0.95, 1.0));
        QtSLiMFrameRect(legendRect, QtSLiMColorWithWhite(0.3, 1.0), painter);
        
		// inset and draw the legend content
		legendRect.adjust(legendExteriorMargin, legendExteriorMargin, -legendExteriorMargin, -legendExteriorMargin);
		drawLegend(painter, legendRect);
	}
}

void QtSLiMGraphView::drawContents(QPainter &painter)
{
    // Set to a default color of black; I thought Qt did this for me, but apparently not
    painter.setPen(Qt::black);
    
    // Erase background
    QRect bounds = rect();
    
	if (!generatingPDF_)
		painter.fillRect(bounds, Qt::white);
	
	// Get our controller and test for validity, so subclasses don't have to worry about this
    if (!controller_ || controller_->invalidSimulation())
    {
        drawMessage(painter, "invalid\nsimulation", bounds);
    }
    else if (controller_->community->Tick() == 0)
    {
        drawMessage(painter, "no\ndata", bounds);
    }
    else if (missingFocalDisplaySpecies())
    {
        // The species name no longer refers to a species in the community
        drawMessage(painter, "missing\nspecies", bounds);
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

void QtSLiMGraphView::paintEvent(QPaintEvent * /* p_paintEvent */)
{
	QPainter painter(this);
    
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawContents(painter);
}

void QtSLiMGraphView::invalidateCachedData()
{
    // GraphView has no cached data, but it supports the idea of it
    // If anything ever gets added here, calls to super will need to be added in subclasses
}

void QtSLiMGraphView::invalidateDrawingCache(void)
{
    // GraphView has no drawing cache, but it supports the idea of one
    // If anything ever gets added here, calls to super will need to be added in subclasses
}

void QtSLiMGraphView::graphWindowResized(void)
{
	invalidateDrawingCache();
}

void QtSLiMGraphView::resizeEvent(QResizeEvent *p_event)
{
    // this override is private; subclassers should override graphWindowResized()
    graphWindowResized();
    QWidget::resizeEvent(p_event);
}

void QtSLiMGraphView::controllerRecycled(void)
{
    updateSpeciesBadge();
    
	invalidateDrawingCache();
    invalidateCachedData();
    
    // recycling reverts custom axis settings from axis() back to the default
    // the design of what reverts on a recycle and what doesn't is kind of ad hoc...
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
    xAxisLabelsType_ = 1;
    yAxisLabelsType_ = 1;
    
    update();
    
    QPushButton *action = actionButton();
    if (action) action->setEnabled(!controller_->invalidSimulation() && !missingFocalDisplaySpecies());
}

void QtSLiMGraphView::controllerChromosomeSelectionChanged(void)
{
}

void QtSLiMGraphView::controllerTickFinished(void)
{
}

void QtSLiMGraphView::updateAfterTick(void)
{
    updateSpeciesBadge();
    
    update();
    
    QPushButton *action = actionButton();
    if (action) action->setEnabled(!controller_->invalidSimulation() && !missingFocalDisplaySpecies());
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
    string.append(slimDateline());
    string.append("\n\n");
    
    appendStringForData(string);
    
    // Get rid of extra commas, as a service to subclasses
    string.replace(", \n", "\n");
    
    return string;
}

void QtSLiMGraphView::actionButtonRunMenu(QtSLiMPushButton *p_actionButton)
{
    contextMenuEvent(nullptr);
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    p_actionButton->qtslimSetHighlight(false);
}

void QtSLiMGraphView::subclassAddItemsToMenu(QMenu & /* contextMenu */, QContextMenuEvent * /* event */)
{
}

QString QtSLiMGraphView::disableMessage(void)
{
    return "";
}

bool QtSLiMGraphView::writeToFile(QString fileName)
{
    QSize graphSize = size();
    QPdfWriter pdfwriter(fileName);
    QPageSize pageSize = QPageSize(graphSize, QString(), QPageSize::ExactMatch);
    QMarginsF margins(0, 0, 0, 0);
    
    pdfwriter.setCreator("SLiMgui");
    pdfwriter.setResolution(72);    // match the screen?
    pdfwriter.setPageSize(pageSize);
    pdfwriter.setPageMargins(margins);
    
    QPainter painter;
    
    if (painter.begin(&pdfwriter))
    {
        generatingPDF_ = true;
        drawContents(painter);
        generatingPDF_ = false;
        painter.end();
        
        return true;
    }
    else
    {
        return false;
    }
}

void QtSLiMGraphView::contextMenuEvent(QContextMenuEvent *p_event)
{
    if (!controller_->invalidSimulation() && !missingFocalDisplaySpecies()) // && ![[controller window] attachedSheet])
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
		
		subclassAddItemsToMenu(contextMenu, p_event);
		
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
        QPoint menuPos = (p_event ? p_event->globalPos() : QCursor::pos());
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
                        invalidateCachedData();
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
                                                                   QStringList{"Minimum value:", "Maximum value:", "Interval between major ticks:", "Minor tick divisions per major tick interval:", "Tick label precision:"},
                                                                   QStringList{QString::number(xAxisMin_), QString::number(xAxisMax_), QString::number(xAxisMajorTickInterval_), QString::number(xAxisMajorTickModulus_),QString::number(xAxisTickValuePrecision_)});
                
                if (choices.length() == 5)
                {
                    xAxisMin_ = choices[0].toDouble();
                    xAxisMax_ = choices[1].toDouble();
                    xAxisMajorTickInterval_ = choices[2].toDouble();
                    xAxisMajorTickModulus_ = std::max(choices[3].toInt(), 1);       // zero causes a crash; better would be to validate that it is an integer value, etc.
                    xAxisTickValuePrecision_ = choices[4].toInt();
                    xAxisMinorTickInterval_ = xAxisMajorTickInterval_ / xAxisMajorTickModulus_;
                    xAxisIsUserRescaled_ = true;
                    
                    // for now, these are the same, except in custom plots
                    x0_ = xAxisMin_;
                    x1_ = xAxisMax_;
                    
                    invalidateDrawingCache();
                    update();
                }
            }
            if (action == changeYAxisScale)
            {
                if (!yAxisLog_)
                {
                    QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a configuration for the axis:",
                                                                       QStringList{"Minimum value:", "Maximum value:", "Interval between major ticks:", "Minor tick divisions per major tick interval:", "Tick label precision:"},
                                                                       QStringList{QString::number(yAxisMin_), QString::number(yAxisMax_), QString::number(yAxisMajorTickInterval_), QString::number(yAxisMajorTickModulus_),QString::number(yAxisTickValuePrecision_)});
                    
                    if (choices.length() == 5)
                    {
                        yAxisMin_ = choices[0].toDouble();
                        yAxisMax_ = choices[1].toDouble();
                        yAxisMajorTickInterval_ = choices[2].toDouble();
                        yAxisMajorTickModulus_ = std::max(choices[3].toInt(), 1);       // zero causes a crash; better would be to validate that it is an integer value, etc.
                        yAxisTickValuePrecision_ = choices[4].toInt();
                        yAxisMinorTickInterval_ = yAxisMajorTickInterval_ / yAxisMajorTickModulus_;
                        yAxisIsUserRescaled_ = true;
                        
                        // for now, these are the same, except in custom plots
                        y0_ = yAxisMin_;
                        y1_ = yAxisMax_;
                        
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
                            
                            // for now, these are the same, except in custom plots
                            y1_ = yAxisMax_;
                            
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
                    bool success = writeToFile(fileName);
                    
                    if (!success)
                        qApp->beep();
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

void QtSLiMGraphView::setXAxisRangeFromTick(void)
{
	Community *community = controller_->community;
	slim_tick_t lastTick = community->EstimatedLastTick();
	
	// The last tick could be just about anything, so we need some smart axis setup code here – a problem we neglect elsewhere
	// since we use hard-coded axis setups in other places.  The goal is to (1) have the axis max be >= lastTick, (2) have the axis
	// max be == lastTick if lastTick is a reasonably round number (a single-digit multiple of a power of 10, say), (3) have just a few
	// other major tick intervals drawn, so labels don't collide or look crowded, and (4) have a few minor tick intervals in between
	// the majors.  Labels that are single-digit multiples of powers of 10 are to be strongly preferred.
	double lower10power = pow(10.0, floor(log10(lastTick)));    // 8000 gives 1000, 1000 gives 1000, 10000 gives 10000
	double lower5mult = lower10power / 2.0;						// 8000 gives 500, 1000 gives 500, 10000 gives 5000
	double axisMax = ceil(lastTick / lower5mult) * lower5mult;	// 8000 gives 8000, 7500 gives 7500, 1100 gives 1500
	double contained5mults = axisMax / lower5mult;				// 8000 gives 16, 7500 gives 15, 1100 gives 3, 1000 gives 2
	
	if (contained5mults <= 3)
	{
		// We have a max like 1500 that divides into 5mults well, so do that
		xAxisMax_ = axisMax;
		xAxisMajorTickInterval_ = lower5mult;
		xAxisMinorTickInterval_ = lower5mult / 5;
		xAxisMajorTickModulus_ = 5;
		xAxisTickValuePrecision_ = 0;
        
        // for now, these are the same, except in custom plots
        x1_ = xAxisMax_;
	}
	else
	{
		// We have a max like 7000 that does not divide into 5mults well; for simplicity, we just always divide these in two
		xAxisMax_ = axisMax;
		xAxisMajorTickInterval_ = axisMax / 2;
		xAxisMinorTickInterval_ = axisMax / 4;
		xAxisMajorTickModulus_ = 2;
		xAxisTickValuePrecision_ = 0;
        
        // for now, these are the same, except in custom plots
        x1_ = xAxisMax_;
	}
}

void QtSLiMGraphView::configureAxisForRange(double &dim0, double &dim1, double &axisMin, double &axisMax,
                                            double &majorTickInterval, double &minorTickInterval,
                                            int &majorTickModulus, int &tickValuePrecision)
{
    // We call down to our R-inspired axis calculation methods to figure out a good axis layout.
    // The call here, to _GScale(), is parallel to the point in R's plot.window() function where
    // it calls down to GScale() for each of the two axes.
    {
        int nDivisions;
        
        //qDebug() << "configureAxisForRange() : original dim0 ==" << dim0 << ", dim1 ==" << dim1;
        
        _GScale(dim0, dim1, axisMin, axisMax, nDivisions);
        
        //qDebug() << "    after axisGScale() : dim0 ==" << dim0 << ", dim1 ==" << dim1;
        //qDebug() << "    after axisGScale() : axisMin ==" << axisMin << ", axisMax ==" << axisMax << ", nDivisions ==" << nDivisions;
        
        // We go beyond R a little, designating some ticks as "major" (getting a label, and a longer tick
        // mark) and others "minor" (just a short tick mark with no label).  We do that after the R-based
        // tick calculations are done, just assigned roles based on the number of divisions; this could
        // probably be improved.  It's a good idea primarily because we tend to display plots at a much
        // smaller default size than R, and so there just isn't room for every tick mark to get a label.
        switch (nDivisions)
        {
        case 2:
        case 4:
        case 6:
        case 8:
        case 10:
            majorTickInterval = (axisMax - axisMin) / 2.0;
            minorTickInterval = (axisMax - axisMin) / nDivisions;
            majorTickModulus = nDivisions / 2;
            break;
        case 3:
        case 9:
            majorTickInterval = (axisMax - axisMin) / 3.0;
            minorTickInterval = (axisMax - axisMin) / nDivisions;
            majorTickModulus = nDivisions / 3;
            break;
        default:
            majorTickInterval = axisMax - axisMin;
            minorTickInterval = majorTickInterval;
            majorTickModulus = 1;
            break;
        }
        
        //qDebug() << "    majorTickInterval ==" << majorTickInterval << ", minorTickInterval ==" << minorTickInterval << ", majorTickModulus ==" << majorTickModulus;
        
        // We now use a negative tick precision to ask the tick-plotting code to use output mode 'g'
        // instead of 'f', with the tick precision meaning the number of significant digits, not
        // the number of digits after the decimal point.  This is used only by this method; old-
        // style QtSLiM plots still use mode 'f'.  The precision value chosen here is arbitrary,
        // but note that trailing zeros are removed by mode 'g', so this precision will only be
        // used if it is needed; and mode 'g' also switches to scientific notation if it is more
        // concise.
        tickValuePrecision = -8;
        
        //qDebug() << "    tickValuePrecision ==" << tickValuePrecision;
    }
}

QtSLiMLegendSpec QtSLiMGraphView::subpopulationLegendKey(std::vector<slim_objectid_t> &subpopsToDisplay, bool drawSubpopsGray)
{
    QtSLiMLegendSpec legend_key;
    
    // put "All" first, if it occurs in subpopsToDisplay
    if (std::find(subpopsToDisplay.begin(), subpopsToDisplay.end(), -1) != subpopsToDisplay.end())
        legend_key.emplace_back("All", Qt::black);

    if (drawSubpopsGray)
    {
        legend_key.emplace_back("pX", QtSLiMColorWithWhite(0.5, 1.0));
    }
    else
    {
        for (auto subpop_id : subpopsToDisplay)
        {
            if (subpop_id != -1)
            {
                QString labelString = QString("p%1").arg(subpop_id);

                legend_key.emplace_back(labelString, controller_->whiteContrastingColorForIndex(subpop_id));
            }
        }
    }

    return legend_key;
}

QtSLiMLegendSpec QtSLiMGraphView::mutationTypeLegendKey(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies)
        return QtSLiMLegendSpec();
    
    int mutationTypeCount = static_cast<int>(graphSpecies->mutation_types_.size());
    
    // if we only have one mutation type, do not show a legend
    if (mutationTypeCount < 2)
        return QtSLiMLegendSpec();
    
    QtSLiMLegendSpec legend_key;
    
    // first we put in placeholders, of swatch type
    std::map<slim_objectid_t,MutationType*> &mutTypes = graphSpecies->mutation_types_;
    
    for (size_t index = 0; index < mutTypes.size(); ++index)
        legend_key.emplace_back("placeholder", QColor());
    
    // then we replace the placeholders with lines, but we do it out of order, according to mutation_type_index_ values
    for (auto mutationTypeIter : mutTypes)
    {
        MutationType *mutationType = mutationTypeIter.second;
        int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
        QString labelString = QString("m%1").arg(mutationType->mutation_type_id_);
        QtSLiMLegendEntry &entry = legend_key[static_cast<size_t>(mutationTypeIndex)];
        
        entry.label = labelString;
        entry.swatch_color = controller_->blackContrastingColorForIndex(mutationTypeIndex);
    }
    
    return legend_key;
}

void QtSLiMGraphView::drawPointSymbol(QPainter &painter, double x, double y, int symbol, QColor symbolColor, QColor borderColor, double lineWidth, double size)
{
    size = size * 3.5;       // this scales what size=1 looks like
    
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
    
    for (int xc = 0; xc < xBinCount; ++xc)
    {
        for (int yc = 0; yc < yBinCount; ++yc)
        {
            double value = buffer[xc + yc * xBinCount];
            double patchX1 = SLIM_SCREEN_ROUND(interiorRect.left() + patchWidth * xc) + intHeatMapMargins;
            double patchX2 = SLIM_SCREEN_ROUND(interiorRect.left() + patchWidth * (xc + 1));
            double patchY1 = SLIM_SCREEN_ROUND(interiorRect.top() + patchHeight * yc) + intHeatMapMargins;
            double patchY2 = SLIM_SCREEN_ROUND(interiorRect.top() + patchHeight * (yc + 1));
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
    Species *graphSpecies = focalDisplaySpecies();
	slim_objectid_t firstTag = -1;
    
    // QComboBox::currentIndexChanged signals will be sent during rebuilding; this flag
    // allows client code to avoid (over-)reacting to those signals.
    rebuildingMenu_ = true;
	
	// Depopulate and populate the menu
	subpopButton->clear();

	if (graphSpecies)
	{
		Population &population = graphSpecies->population_;
		
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
    Species *graphSpecies = focalDisplaySpecies();
	int firstTag = -1;
	
    // QComboBox::currentIndexChanged signals will be sent during rebuilding; this flag
    // allows client code to avoid (over-)reacting to those signals.
    rebuildingMenu_ = true;
	
	// Depopulate and populate the menu
	mutTypeButton->clear();
	
	if (graphSpecies)
	{
		std::map<slim_objectid_t,MutationType*> &mutationTypes = graphSpecies->mutation_types_;
		
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
	// BCH 4/21/2023: This could use mutrun use counts to run faster...
	//
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies)
        return 0;
    
    Population &population = graphSpecies->population_;
    size_t subpop_total_genome_count = 0;
    
    Mutation *mut_block_ptr = gSLiM_Mutation_Block;
    
    {
        int registry_size;
        const MutationIndex *registry = population.MutationRegistry(&registry_size);
        const MutationIndex *registry_iter_end = registry + registry_size;
        
        for (const MutationIndex *registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
            (mut_block_ptr + *registry_iter)->gui_scratch_reference_count_ = 0;
    }
    
    Subpopulation *subpop = graphSpecies->SubpopulationWithID(subpop_id);
    
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
                    const MutationRun *mutrun = genome.mutruns_[run_index];
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
	// BCH 4/21/2023: This could use mutrun use counts to run faster...
	//
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies)
        return 0;
    
    Population &population = graphSpecies->population_;
	
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
                const MutationRun *mutrun = genome->mutruns_[run_index];
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


//
//  Axis tick calculations
//
//  This code is based upon the code in R 4.3.2.  R is open source under the GPL, so
//  we are free to use it in Eidos/SLiM which is also GPL.  The GPL license is already
//  incorporated in this distribution.  Thanks to all the contributors to this code in
//  R, which provides a nice algorithm.
//
//  In QtSLiM's adapted code, I have removed a bunch of debugging code, removed the
//  log-axis case, removed support for axis min > max, removed all the par()-based
//  graphics-parameter stuff, removed various errors and warnings, etc.  These changes
//  simplified the code, at the cost of making it less general and robust.  QtSLiM
//  doesn't really want to be reporting random internal warnings and errors to the
//  user, though; if we hit one of the edge cases that R used to handle, then que sera,
//  sera.
//

// modified from R-4.3.2/src/library/graphics/src/graphics.c : void GScale(double min, double max, int axis, pGEDevDesc dd)
void QtSLiMGraphView::_GScale(double &minValue, double &maxValue, double &axisMin, double &axisMax, int &nDivisions)
{
#define EPS_FAC_1  16
    
    // number of divisions; this comes from lab[0] in R, but for us we just always use the default of 5
    nDivisions = 5;
    
    double temp = std::max(std::fabs(maxValue), std::fabs(minValue));
    
    if (temp == 0)      /* min = max = 0 */
    {
        minValue = -1;
        maxValue = 1;
    }
    else
    {
        // careful to avoid overflow (and underflow) here:
        double tf = (temp > 1) ? (temp * DBL_EPSILON) * EPS_FAC_1 : (temp * EPS_FAC_1  ) * DBL_EPSILON;
        
        if (tf == 0)
            tf = DBL_MIN;
        
        if (std::fabs(maxValue - minValue) < tf)
        {
            temp *= 1e-2;
            minValue -= temp;
            maxValue += temp;
        }
    }
    
    if (true)
    {
        // R axis style 'r': (regular) first extends the data range by 4 percent at each end
        // and then finds an axis with pretty labels that fits within the extended range.
        temp = (temp > 100
                    ? 0.04 * maxValue - 0.04 * minValue   // not to overflow
                    : 0.04 * (maxValue - minValue));      // is negative iff max < min
        
        // careful now to not get to +/- Inf :
        double d;
        
        d = minValue - temp;
        if (std::isfinite(d))
            minValue = d;
        else
            minValue = (d < 0) ? -DBL_MAX : DBL_MAX;
        
        d = maxValue + temp;
        if (std::isfinite(d))
            maxValue = d;
        else
            maxValue = (d < 0) ? -DBL_MAX : DBL_MAX;
    }
    else
    {
        // R axis style 'i': (internal) just finds an axis with pretty labels that fits
        // within the original data range.  Presently inaccessible in QtSLiM.
    }
    
    // Computation of [xy]axp[0:2] == (min,max,n) :
    axisMin = minValue;
    axisMax = maxValue;
    
    _GAxisPars(&axisMin, &axisMax, &nDivisions);

#undef EPS_FAC_1
}

// modified from R-4.3.2/src/main/graphics.c : void GAxisPars(double *min, double *max, int *n, Rboolean log, int axis)
void QtSLiMGraphView::_GAxisPars(double *minValue, double *maxValue, int *nDivisions)
{
#define EPS_FAC_2 16
    
    /* save only for the extreme case (EPS_FAC_2): */
    double min_o = *minValue, max_o = *maxValue;
    
    _GEPretty(minValue, maxValue, nDivisions);
    
    double t_ = std::max(fabs(*maxValue), fabs(*minValue)),
        tf = // careful to avoid overflow (and underflow) here:
        (t_ > 1)
            ? (t_ * DBL_EPSILON) * EPS_FAC_2
            : (t_ * EPS_FAC_2  ) * DBL_EPSILON;
    if (tf == 0) tf = DBL_MIN;
    
    if (fabs(*maxValue - *minValue) <= tf) {
        /* Treat this case somewhat similar to the (min ~= max) case above */
        /* Too much accuracy here just shows machine differences */
        
        /* No pretty()ing anymore */
        *minValue = min_o;
        *maxValue = max_o;
        double eps = .005 * (*maxValue - *minValue);/* .005: not to go to DBL_MIN/MAX */
        *minValue += eps;
        *maxValue -= eps;
        *nDivisions = 1;
    }

#undef EPS_FAC_2
}

// modified R-4.3.2/src/main/graphics.c : static void GLPretty(double *ul, double *uh, int *n)
void QtSLiMGraphView::_GEPretty(double *lo, double *up, int *nDivisions)
{
    /*	Set scale and ticks for linear scales.
 *
 *	Pre:	    x1 == lo < up == x2      ;  nDivisions >= 1
 *	Post: x1 <= y1 := lo < up =: y2 <= x2;	nDivisions >= 1
 */
    if (*nDivisions <= 0)
        return;
    
    if (!std::isfinite(*lo) || !std::isfinite(*up)) // also catch NA etc
        return;
    
    // For *finite* boundaries, now allow (*up - *lo) = +/- inf  as R_pretty() now does
    double ns = *lo, nu = *up;
    double unit, high_u_fact[3] = { .8, 1.7, 1.125 };
        // =   (h, h5 , f_min) = (high.u.bias, u5.bias, f_min)
    
    unit = _R_pretty(&ns, &nu, nDivisions, /* min_n = */ 1,
                    /* shrink_sml = */ 0.25,
                    high_u_fact,
                    2 /* do eps_correction in any case */);
    
    // The following is ugly since it kind of happens already in R_pretty(..):
#define rounding_eps 1e-10 /* <- compatible to seq*(); was 1e-7 till 2017-08-14 */
    
    if (nu >= ns + 1) {
        int mod = 0;
        if (               ns * unit < *lo - rounding_eps * unit) { ns++; mod++; }
        if (nu > ns + 1 && nu * unit > *up + rounding_eps * unit) { nu--; mod++; }
        if (mod) *nDivisions = (int)(nu - ns);
    }
    
    *lo = ns * unit;
    *up = nu * unit;
}

// modified from R-4.3.2/src/appl/pretty.c : double R_pretty(double *lo, double *up, int *ndiv, int min_n, double shrink_sml, const double high_u_fact[], int eps_correction, int return_bounds)
double QtSLiMGraphView::_R_pretty(double *lo, double *up, int *nDivisions, int min_n,
                double shrink_sml,
                const double high_u_fact[], // = (h, h5, f_min) below 
                int eps_correction)
{
/* From version 0.65 on, we had rounding_eps := 1e-5, before, r..eps = 0
 * then, 1e-7 was consistent with seq.default() and seq.int() till 2010-02-03,
 * where it was changed to 1e-10 for seq*(), and in 2017-08-14 for pretty(): */
#define rounding_eps 1e-10

// (h, h5, f_min) = c(high.u.bias, u5.bias, f.min) in base::pretty.default():
#define h     high_u_fact[0]
#define h5    high_u_fact[1]
#define f_min high_u_fact[2]
    
    double // save input boundaries
        lo_ = *lo,
        up_ = *up,
        dx = up_ - lo_,
        cell, U;
    bool i_small;
    /* cell := "scale"	here */
    if (dx == 0 && up_ == 0) { /*  up == lo == 0	 */
        cell = 1;
        i_small = true;
    } else {
        cell = std::max(fabs(lo_),fabs(up_));
        /* U = upper bound on cell/unit */
        U = 1 + ((h5 >= 1.5*h+.5) ? 1/(1+h) : 1.5/(1+h5));
        U *= std::max(1,*nDivisions) * DBL_EPSILON; // avoid overflow for large nDivisions
        /* added times 3, as several calculations here */
        i_small = dx < cell * U * 3;
    }

    /*OLD: cell = FLT_EPSILON+ dx / *nDivisions; FLT_EPSILON = 1.192e-07 */
    if (i_small) {
        if(cell > 10)
            cell = 9 + cell/10;
        cell *= shrink_sml;
        if(min_n > 1) cell /= min_n;
    } else {
        cell = dx;
        if (std::isfinite(dx)) {
            if (*nDivisions > 1) cell /= *nDivisions;
        } else { // up - lo = +Inf (overflow; both are finite)
            if (*nDivisions >= 2) {
                cell = up_ / (*nDivisions) - lo_ / (*nDivisions);
            }
        }
    }

// f_min: arg, default = 2^-20, was 20.  till R 4.1.0 (2021-05)
#define MAX_F 1.25 //		was 10.   "   "   "
    
    double subsmall = f_min*DBL_MIN;
    if (subsmall == 0.) // subnormals underflowing to zero (not yet seen!)
        subsmall = DBL_MIN;
    if (cell < subsmall) { // possibly subnormal
        cell = subsmall;
    } else if (cell > DBL_MAX/MAX_F) {
        cell = DBL_MAX/MAX_F;
    }

#undef MAX_F
    
    /* NB: the power can be negative and this relies on exact
       calculation, which glibc's exp10 does not achieve */
    double base = pow(10.0, floor(log10(cell))); /* base <= cell < 10*base */
    
    /* unit : from { 1,2,5,10 } * base
     *	 such that |u - cell| is small,
     * favoring larger (if h > 1, else smaller)  u  values;
     * favor '5' more than '2'  if h5 > h  (default h5 = .5 + 1.5 h) */
    double unit = base;
    if ((U = 2*base)-cell <  h*(cell-unit)) { unit = U;
        if ((U = 5*base)-cell < h5*(cell-unit)) { unit = U;
            if ((U =10*base)-cell <  h*(cell-unit))   unit = U; }}
    /* Result (c := cell,  b := base,  u := unit):
     *	c in [	1,	       (2+ h)/ (1+h) ] b ==> u=  b
     *	c in ( (2+ h) /(1+h),  (5+2h5)/(1+h5)] b ==> u= 2b
     *	c in ( (5+2h5)/(1+h5), (10+5h)/(1+h) ] b ==> u= 5b
     *	c in ((10+5h) /(1+h),	     10      ) b ==> u=10b
     *
     *	===>	2/5 *(2+h)/(1+h)  <=  c/u  <=  (2+h)/(1+h)	*/
    
    double ns = floor(lo_/unit+rounding_eps);
    double nu = ceil (up_/unit-rounding_eps);
    
    if (eps_correction && (eps_correction > 1 || !i_small)) {
        // FIXME?: assumes 0 <= lo <= up  (what if lo <= up < 0 ?)
        if (lo_ != 0.) *lo *= (1- DBL_EPSILON); else *lo = -DBL_MIN;
        if (up_ != 0.) *up *= (1+ DBL_EPSILON); else *up = +DBL_MIN;
    }

    while (ns*unit > *lo + rounding_eps*unit) ns--;
    while (!std::isfinite(ns*unit)) ns++;

    while (nu*unit < *up - rounding_eps*unit) nu++;
    while (!std::isfinite(nu*unit)) nu--;
    
    int k = (int)(0.5 + nu - ns);
    if (k < min_n) {
        /* ensure that	nu - ns	 == min_n */
        k = min_n - k;
        if (lo_ == 0. && ns == 0. && up_ != 0.) {
            nu += k;
        } else if (up_ == 0. && nu == 0. && lo_ != 0.) {
            ns -= k;
        } else if (ns >= 0.) {
            nu += k/2;
            ns -= k/2 + k%2;/* ==> nu-ns = old(nu-ns) + min_n -k = min_n */
        } else {
            ns -= k/2;
            nu += k/2 + k%2;
        }
        *nDivisions = min_n;
    }
    else {
        *nDivisions = k;
    }
    
    *lo = ns;
    *up = nu;

    return unit;
#undef h
#undef h5
}












































