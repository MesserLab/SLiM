//
//  QtSLiM_Plot.cpp
//  SLiM
//
//  Created by Ben Haller on 1/30/2024.
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


#include "QtSLiM_Plot.h"
#include "QtSLiMGraphView_CustomPlot.h"

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <unistd.h>

#include <string>
#include <algorithm>
#include <vector>


#pragma mark -
#pragma mark Plot
#pragma mark -

Plot::Plot(const std::string &title, QtSLiMGraphView_CustomPlot *p_plotview) :
    title_(title), plotview_(p_plotview)
{
}

Plot::~Plot(void)
{
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Plot::Class(void) const
{
	return gSLiM_Plot_Class;
}

void Plot::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP Plot::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		// constants
        case gID_title:
        {
            return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(title_));
        }
        
        // variables
		
		// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void Plot::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Plot::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
        case gID_abline:                return ExecuteMethod_abline(p_method_id, p_arguments, p_interpreter);
        case gID_addLegend:             return ExecuteMethod_addLegend(p_method_id, p_arguments, p_interpreter);
        case gID_axis:                  return ExecuteMethod_axis(p_method_id, p_arguments, p_interpreter);
		case gID_legendLineEntry:       return ExecuteMethod_legendLineEntry(p_method_id, p_arguments, p_interpreter);
		case gID_legendPointEntry:      return ExecuteMethod_legendPointEntry(p_method_id, p_arguments, p_interpreter);
		case gID_legendSwatchEntry:     return ExecuteMethod_legendSwatchEntry(p_method_id, p_arguments, p_interpreter);
		case gID_lines:					return ExecuteMethod_lines(p_method_id, p_arguments, p_interpreter);
		case gID_points:				return ExecuteMethod_points(p_method_id, p_arguments, p_interpreter);
		case gID_text:					return ExecuteMethod_text(p_method_id, p_arguments, p_interpreter);
		case gEidosID_write:			return ExecuteMethod_write(p_method_id, p_arguments, p_interpreter);
		default:                        return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)abline([Nif a = NULL], [Nif b = NULL], [Nif h = NULL], [Nif v = NULL], [string color = "red"], [numeric lwd = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_abline(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *a_value = p_arguments[0].get();
    EidosValue *b_value = p_arguments[1].get();
    EidosValue *h_value = p_arguments[2].get();
    EidosValue *v_value = p_arguments[3].get();
    EidosValue *color_value = p_arguments[4].get();
    EidosValue *lwd_value = p_arguments[5].get();
    double *a = nullptr, *b = nullptr, *h = nullptr, *v = nullptr;
    int line_count;
    
    if ((a_value->Type() != EidosValueType::kValueNULL) && (b_value->Type() != EidosValueType::kValueNULL) && (h_value->Type() == EidosValueType::kValueNULL) && (v_value->Type() == EidosValueType::kValueNULL))
    {
        // a and b
        int acount = a_value->Count();
        int bcount = b_value->Count();
        
        if (acount == bcount)   line_count = acount;
        else if (acount == 1)   line_count = bcount;
        else if (bcount == 1)   line_count = acount;
        else
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires a and b to be the same length, or one of them to be singleton." << EidosTerminate();
        
        if (line_count == 0)
            return gStaticEidosValueVOID;
        
        a = (double *)malloc(line_count * sizeof(double));
        b = (double *)malloc(line_count * sizeof(double));
        
        if (!a || !b)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
        
        for (int index = 0; index < line_count; ++index)
        {
            a[index] = a_value->NumericAtIndex_NOCAST(index % acount, nullptr);
            b[index] = b_value->NumericAtIndex_NOCAST(index % bcount, nullptr);
        }
    }
    else if ((a_value->Type() == EidosValueType::kValueNULL) && (b_value->Type() == EidosValueType::kValueNULL) && (h_value->Type() != EidosValueType::kValueNULL) && (v_value->Type() == EidosValueType::kValueNULL))
    {
        // h
        line_count = h_value->Count();
        
        if (line_count == 0)
            return gStaticEidosValueVOID;
        
        h = (double *)malloc(line_count * sizeof(double));
        
        if (!h)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
        
        for (int index = 0; index < line_count; ++index)
            h[index] = h_value->NumericAtIndex_NOCAST(index, nullptr);
    }
    else if ((a_value->Type() == EidosValueType::kValueNULL) && (b_value->Type() == EidosValueType::kValueNULL) && (h_value->Type() == EidosValueType::kValueNULL) && (v_value->Type() != EidosValueType::kValueNULL))
    {
        // v
        line_count = v_value->Count();
        
        if (line_count == 0)
            return gStaticEidosValueVOID;
        
        v = (double *)malloc(line_count * sizeof(double));
        
        if (!v)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
        
        for (int index = 0; index < line_count; ++index)
            v[index] = v_value->NumericAtIndex_NOCAST(index, nullptr);
    }
    else
    {
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires one of three usage modes: (1) a and b are non-NULL while h and v are NULL; (2) a, b, and v are NULL while h is non-NULL; or (3) a, b, and h are NULL while v is non-NULL." << EidosTerminate(nullptr);
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != line_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires color to match the number of lines, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // lwd
    std::vector<double> *lwds = new std::vector<double>;
    int lwd_count = lwd_value->Count();
    
    if ((lwd_count != 1) && (lwd_count != line_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires lwd to match the number of lines, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    plotview_->addABLineData(a, b, h, v, line_count, colors, lwds);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)addLegend([Ns$ position = NULL], [Ni$ inset = NULL], [Nif$ labelSize = NULL], [Nif$ lineHeight = NULL], [Nif$ graphicsWidth = NULL], [Nif$ exteriorMargin = NULL], [Nif$ interiorMargin = NULL])
//
EidosValue_SP Plot::ExecuteMethod_addLegend(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *position_value = p_arguments[0].get();
    EidosValue *inset_value = p_arguments[1].get();
    EidosValue *labelSize_value = p_arguments[2].get();
    EidosValue *lineHeight_value = p_arguments[3].get();
    EidosValue *graphicsWidth_value = p_arguments[4].get();
    EidosValue *exteriorMargin_value = p_arguments[5].get();
    EidosValue *interiorMargin_value = p_arguments[6].get();
    
    // position
    QtSLiM_LegendPosition position;
    
    if (position_value->Type() == EidosValueType::kValueNULL)
    {
        position = QtSLiM_LegendPosition::kUnconfigured;
    }
    else
    {
        QString positionString  = QString::fromStdString(position_value->StringAtIndex_NOCAST(0, nullptr));
        
        if (positionString == "topLeft")
            position = QtSLiM_LegendPosition::kTopLeft;
        else if (positionString == "topRight")
            position = QtSLiM_LegendPosition::kTopRight;
        else if (positionString == "bottomLeft")
            position = QtSLiM_LegendPosition::kBottomLeft;
        else if (positionString == "bottomRight")
            position = QtSLiM_LegendPosition::kBottomRight;
        else
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires position to be 'topLeft', 'topRight', 'bottomLeft', or 'bottomRight' (or NULL)." << EidosTerminate(nullptr);
    }
    
    // inset
    int inset;
    
    if (inset_value->Type() == EidosValueType::kValueNULL)
        inset = -1;
    else
    {
        inset = inset_value->IntAtIndex_NOCAST(0, nullptr);
        
        if ((inset < 0) || (inset > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires inset to be in [0, 50]." << EidosTerminate(nullptr);
    }
    
    // labelSize
    double labelSize;
    
    if (labelSize_value->Type() == EidosValueType::kValueNULL)
        labelSize = -1;
    else
    {
        labelSize = labelSize_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(labelSize) || (labelSize < 5) || (labelSize > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires labelSize to be in [5, 50]." << EidosTerminate(nullptr);
    }
    
    // lineHeight
    double lineHeight;
    
    if (lineHeight_value->Type() == EidosValueType::kValueNULL)
        lineHeight = -1;
    else
    {
        lineHeight = lineHeight_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(lineHeight) || (lineHeight < 5) || (lineHeight > 100))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires lineHeight to be in [5, 100]." << EidosTerminate(nullptr);
    }
    
    // graphicsWidth
    double graphicsWidth;
    
    if (graphicsWidth_value->Type() == EidosValueType::kValueNULL)
        graphicsWidth = -1;
    else
    {
        graphicsWidth = graphicsWidth_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(graphicsWidth) || (graphicsWidth < 5) || (graphicsWidth > 100))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires graphicsWidth to be in [5, 100]." << EidosTerminate(nullptr);
    }
    
    // exteriorMargin
    double exteriorMargin;
    
    if (exteriorMargin_value->Type() == EidosValueType::kValueNULL)
        exteriorMargin = -1;
    else
    {
        exteriorMargin = exteriorMargin_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(exteriorMargin) || (exteriorMargin < 0) || (exteriorMargin > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires exteriorMargin to be in [0, 50]." << EidosTerminate(nullptr);
    }
    
    // interiorMargin
    double interiorMargin;
    
    if (interiorMargin_value->Type() == EidosValueType::kValueNULL)
        interiorMargin = -1;
    else
    {
        interiorMargin = interiorMargin_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(interiorMargin) || (interiorMargin < 0) || (interiorMargin > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires interiorMargin to be in [0, 50]." << EidosTerminate(nullptr);
    }
    
    if (plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() has already been called for this plot, and should only be called once." << EidosTerminate(nullptr);
    
    plotview_->addLegend(position, inset, labelSize, lineHeight, graphicsWidth, exteriorMargin, interiorMargin);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)axis(integer$ side, [Nif at = NULL], [ls labels = T])
//
EidosValue_SP Plot::ExecuteMethod_axis(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *side_value = p_arguments[0].get();
    EidosValue *at_value = p_arguments[1].get();
    EidosValue *labels_value = p_arguments[2].get();
    
    // side
    int side = (int)side_value->IntAtIndex_NOCAST(0, nullptr);
    
    if ((side != 1) && (side != 2))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires side to be 1 (for the x axis) or 2 (for the y axis)." << EidosTerminate(nullptr);
    
    // at
    std::vector<double> *at = nullptr;
    int at_length = 0;
    double last_at = -std::numeric_limits<double>::infinity();
    
    if (at_value->Type() != EidosValueType::kValueNULL)
    {
        at_length = at_value->Count();
        at = new std::vector<double>;
        
        for (int index = 0; index < at_length; ++index)
        {
            double pos = at_value->NumericAtIndex_NOCAST(index, nullptr);
            
            if (!std::isfinite(pos))
                EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires the elements of at to be finite." << EidosTerminate(nullptr);
            
            if (pos <= last_at)
                EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires the elements of at to be in sorted (increasing) order." << EidosTerminate(nullptr);
            last_at = pos;
            
            at->push_back(pos);
        }
    }
    
    // labels
    std::vector<QString> *labels;
    int labels_type;
    
    if (labels_value->Type() == EidosValueType::kValueLogical)
    {
        // labels can be T, F, or a vector of type string; we need a separate flag to differentiate those cases
        // T is 1, F is 0, and the string vector case is 2
        labels = nullptr;
        labels_type = (int)labels_value->LogicalAtIndex_NOCAST(0, nullptr);
    }
    else
    {
        if (at_value->Type() == EidosValueType::kValueNULL)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires that when at is NULL, labels be T or F; a vector of labels cannot be supplied without corresponding positions." << EidosTerminate(nullptr);
        
        const std::string *string_data = labels_value->StringData();
        int labels_length = labels_value->Count();
        
        if (labels_length != at_length)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires that labels be the same length as at (if labels is not T or F), to supply a label for each corresponding position." << EidosTerminate(nullptr);
        
        labels = new std::vector<QString>;
        labels_type = 2;
        
        for (int index = 0; index < labels_length; ++index)
            labels->emplace_back(QString::fromStdString(string_data[index]));
    }
    
    plotview_->setAxisConfiguration(side, at, labels_type, labels);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendLineEntry(string$ label, [string$ color = "red"], [numeric$ lwd = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_legendLineEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    EidosValue *color_value = p_arguments[1].get();
    EidosValue *lwd_value = p_arguments[2].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (label.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): legendLineEntry() requires a non-empty legend label." << EidosTerminate();
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    QColor color(colorR, colorG, colorB, 255);
    
    // lwd
    double lwd = lwd_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((lwd < 0.0) || (lwd > 100.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): legendLineEntry() requires the line width lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendLineEntry(label, color, lwd);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendPointEntry(string$ label, [integer$ symbol = 0], [string$ color = "red"], [string$ border = "black"], [numeric$ lwd = 1.0], [numeric$ size = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_legendPointEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    EidosValue *symbol_value = p_arguments[1].get();
    EidosValue *color_value = p_arguments[2].get();
    EidosValue *border_value = p_arguments[3].get();
    EidosValue *lwd_value = p_arguments[4].get();
    EidosValue *size_value = p_arguments[5].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (label.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires a non-empty legend label." << EidosTerminate();
    
    // symbol
    int symbol = symbol_value->IntAtIndex_NOCAST(0, nullptr);
    
    if (symbol < 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires the elements of symbol to be >= 0." << EidosTerminate();
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    QColor color(colorR, colorG, colorB, 255);
    
    // border
    std::string border_string = border_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t borderR, borderG, borderB;
    
    Eidos_GetColorComponents(border_string, &borderR, &borderG, &borderB);
    
    QColor border(borderR, borderG, borderB, 255);
    
    // lwd
    double lwd = lwd_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((lwd < 0.0) || (lwd > 100.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    // size
    double size = size_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((size <= 0.0) || (size > 1000.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendPointEntry(label, symbol, color, border, lwd, size);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendSwatchEntry(string$ label, [string$ color = "red"])
//
EidosValue_SP Plot::ExecuteMethod_legendSwatchEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    EidosValue *color_value = p_arguments[1].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (label.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendSwatchEntry): legendSwatchEntry() requires a non-empty legend label." << EidosTerminate();
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    QColor color(colorR, colorG, colorB, 255);
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendSwatchEntry(label, color);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)lines(numeric x, numeric y, [string$ color = "red"], [numeric$ lwd = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_lines(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *x_value = p_arguments[0].get();
    EidosValue *y_value = p_arguments[1].get();
    EidosValue *color_value = p_arguments[2].get();
    EidosValue *lwd_value = p_arguments[3].get();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    
    if (xcount != ycount)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): lines() requires x and y to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x, x_value->FloatData(), xcount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x_value->IntData();
        
        for (int index = 0; index < xcount; ++index)
            x[index] = int_data[index];
    }
    
    if (y_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y, y_value->FloatData(), ycount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y_value->IntData();
        
        for (int index = 0; index < ycount; ++index)
            y[index] = int_data[index];
    }
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    std::vector<QColor> *colors = new std::vector<QColor>;
    
    colors->emplace_back(colorR, colorG, colorB, 255);
    
    // lwd
    double lwd = lwd_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((lwd < 0.0) || (lwd > 100.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): lines() requires the line width lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    std::vector<double> *lineWidths = new std::vector<double>;  // we only take a singleton width, but the API expects a buffer
    
    lineWidths->push_back(lwd);
    
    plotview_->addLineData(x, y, xcount, colors, lineWidths);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)points(numeric x, numeric y, [integer symbol = 0], [string color = "red"], [string border = "black"], [numeric lwd = 1.0], [numeric size = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_points(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *x_value = p_arguments[0].get();
    EidosValue *y_value = p_arguments[1].get();
    EidosValue *symbol_value = p_arguments[2].get();
    EidosValue *color_value = p_arguments[3].get();
    EidosValue *border_value = p_arguments[4].get();
    EidosValue *lwd_value = p_arguments[5].get();
    EidosValue *size_value = p_arguments[6].get();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    
    if (xcount != ycount)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires x and y to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x, x_value->FloatData(), xcount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x_value->IntData();
        
        for (int index = 0; index < xcount; ++index)
            x[index] = int_data[index];
    }
    
    if (y_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y, y_value->FloatData(), ycount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y_value->IntData();
        
        for (int index = 0; index < ycount; ++index)
            y[index] = int_data[index];
    }
    
    // symbol
    std::vector<int> *symbols = new std::vector<int>;
    int symbol_count = symbol_value->Count();
    
    if ((symbol_count != 1) && (symbol_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires symbol to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < symbol_count; ++index)
    {
        int symbol = symbol_value->IntAtIndex_NOCAST(index, nullptr);
        
        if (symbol < 0)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of symbol to be >= 0." << EidosTerminate();
        
        symbols->push_back(symbol);
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires color to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // border
    std::vector<QColor> *borders = new std::vector<QColor>;
    int border_count = border_value->Count();
    
    if ((border_count != 1) && (border_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires border to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < border_count; ++index)
    {
        std::string border_string = border_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t borderR, borderG, borderB;
        
        Eidos_GetColorComponents(border_string, &borderR, &borderG, &borderB);
        
        borders->emplace_back(borderR, borderG, borderB, 255);
    }
    
    // lwd
    std::vector<double> *lwds = new std::vector<double>;
    int lwd_count = lwd_value->Count();
    
    if ((lwd_count != 1) && (lwd_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires lwd to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    // size
    std::vector<double> *sizes = new std::vector<double>;
    int size_count = size_value->Count();
    
    if ((size_count != 1) && (size_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires size to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < size_count; ++index)
    {
        double size = size_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((size <= 0.0) || (size > 1000.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
        
        sizes->push_back(size);
    }
    
    plotview_->addPointData(x, y, xcount, symbols, colors, borders, lwds, sizes);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)text(numeric x, numeric y, string labels, [string color = "black"], [numeric size = 10.0], [Nif adj = NULL])
//
EidosValue_SP Plot::ExecuteMethod_text(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *x_value = p_arguments[0].get();
    EidosValue *y_value = p_arguments[1].get();
    EidosValue *labels_value = p_arguments[2].get();
    EidosValue *color_value = p_arguments[3].get();
    EidosValue *size_value = p_arguments[4].get();
    EidosValue *adj_value = p_arguments[5].get();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    int labelscount = labels_value->Count();
    
    if ((xcount != ycount) || (xcount != labelscount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires x, y, and labels to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x, x_value->FloatData(), xcount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x_value->IntData();
        
        for (int index = 0; index < xcount; ++index)
            x[index] = int_data[index];
    }
    
    if (y_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y, y_value->FloatData(), ycount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y_value->IntData();
        
        for (int index = 0; index < ycount; ++index)
            y[index] = int_data[index];
    }
    
    // labels
    std::vector<QString> *labels = new std::vector<QString>;
    const std::string *string_data = labels_value->StringData();
    
    for (int index = 0; index < labelscount; ++index)
        labels->emplace_back(QString::fromStdString(string_data[index]));
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires color to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // size
    std::vector<double> *sizes = new std::vector<double>;
    int size_count = size_value->Count();
    
    if ((size_count != 1) && (size_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires size to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < size_count; ++index)
    {
        double size = size_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((size <= 0.0) || (size > 1000.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
        
        sizes->push_back(size);
    }
    
    // adj
    double adj[2] = {0.5, 0.5};
    
    if (adj_value->Type() != EidosValueType::kValueNULL)
    {
        if (adj_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires adj to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        adj[0] = adj_value->NumericAtIndex_NOCAST(0, nullptr);
        adj[1] = adj_value->NumericAtIndex_NOCAST(1, nullptr);
    }
    
    plotview_->addTextData(x, y, labels, xcount, colors, sizes, adj);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)write(string$ filePath)
//
EidosValue_SP Plot::ExecuteMethod_write(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *filePath_value = p_arguments[0].get();
    
    std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (outfile_path.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_write): write() requires a non-empty path." << EidosTerminate();
    
    QString qpath = QString::fromStdString(outfile_path);
    bool success = plotview_->writeToFile(qpath);
    
    if (!success)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_write): write() could not write to " << outfile_path << "; check the permissions of the enclosing directory." << EidosTerminate();
    
    return gStaticEidosValueVOID;
}


//
//	Plot_Class
//
#pragma mark -
#pragma mark Plot_Class
#pragma mark -

EidosClass *gSLiM_Plot_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Plot_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
        
        properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_title, true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
        
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Plot_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
        
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_abline, kEidosValueMaskVOID))
                                  ->AddNumeric_ON("a", gStaticEidosValueNULL)->AddNumeric_ON("b", gStaticEidosValueNULL)
                                  ->AddNumeric_ON("h", gStaticEidosValueNULL)->AddNumeric_ON("v", gStaticEidosValueNULL)
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_addLegend, kEidosValueMaskVOID))
                                  ->AddString_OSN("position", gStaticEidosValueNULL)->AddInt_OSN("inset", gStaticEidosValueNULL)
                                  ->AddNumeric_OSN("labelSize", gStaticEidosValueNULL)->AddNumeric_OSN("lineHeight", gStaticEidosValueNULL)
                                  ->AddNumeric_OSN("graphicsWidth", gStaticEidosValueNULL)->AddNumeric_OSN("exteriorMargin", gStaticEidosValueNULL)
                                  ->AddNumeric_OSN("interiorMargin", gStaticEidosValueNULL)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_axis, kEidosValueMaskVOID))
                                  ->AddInt_S("side")->AddNumeric_ON("at", gStaticEidosValueNULL)
                                  ->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional, "labels", nullptr, gStaticEidosValue_LogicalT)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendLineEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendPointEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")->AddInt_OS("symbol", gStaticEidosValue_Integer0)
                                  ->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddString_OS("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)->AddNumeric_OS("size", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendSwatchEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_lines, kEidosValueMaskVOID))
                                  ->AddNumeric("x")->AddNumeric("y")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_points, kEidosValueMaskVOID))
                                  ->AddNumeric("x")->AddNumeric("y")->AddInt_O("symbol", gStaticEidosValue_Integer0)
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddString_O("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddNumeric_O("size", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_text, kEidosValueMaskVOID))
                                  ->AddNumeric("x")->AddNumeric("y")->AddString("labels")
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("size", EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(10)))
                                  ->AddNumeric_ON("adj", gStaticEidosValueNULL)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gEidosStr_write, kEidosValueMaskVOID))
                                  ->AddString_S(gEidosStr_filePath)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
































