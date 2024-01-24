//
//  QtSLiM_SLiMgui.cpp
//  SLiM
//
//  Created by Ben Haller on 12/7/2019.
//  Copyright (c) 2019-2024 Philipp Messer.  All rights reserved.
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


#include "QtSLiM_SLiMgui.h"
#include "QtSLiMWindow.h"

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <unistd.h>

#include <string>
#include <algorithm>
#include <vector>


#pragma mark -
#pragma mark SLiMGUI
#pragma mark -

SLiMgui::SLiMgui(Community &p_community, QtSLiMWindow *p_controller) :
	community_(p_community),
	controller_(p_controller),
	self_symbol_(gID_slimgui, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_SLiMgui_Class)))
{
}

SLiMgui::~SLiMgui(void)
{
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *SLiMgui::Class(void) const
{
	return gSLiM_SLiMgui_Class;
}

void SLiMgui::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP SLiMgui::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		// constants
		case gID_pid:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(getpid()));
		}
		
		// variables
		
		// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void SLiMgui::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
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

EidosValue_SP SLiMgui::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_openDocument:				return ExecuteMethod_openDocument(p_method_id, p_arguments, p_interpreter);
		case gID_pauseExecution:			return ExecuteMethod_pauseExecution(p_method_id, p_arguments, p_interpreter);
		case gID_plotCreate:				return ExecuteMethod_plotCreate(p_method_id, p_arguments, p_interpreter);
		case gID_plotLines:					return ExecuteMethod_plotLines(p_method_id, p_arguments, p_interpreter);
		case gID_plotPoints:				return ExecuteMethod_plotPoints(p_method_id, p_arguments, p_interpreter);
		case gID_plotText:					return ExecuteMethod_plotText(p_method_id, p_arguments, p_interpreter);
		default:							return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)openDocument(string$ path)
//
EidosValue_SP SLiMgui::ExecuteMethod_openDocument(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex_NOCAST(0, nullptr)));
	QString filePath = QString::fromStdString(file_path);
	
	controller_->eidos_openDocument(filePath);
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)pauseExecution(void)
//
EidosValue_SP SLiMgui::ExecuteMethod_pauseExecution(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
    controller_->eidos_pauseExecution();
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)plotCreate(string$ title, [Nif xrange = NULL], [Nif yrange = NULL],[string$ xlab = "x"], [string$ ylab = "y"], [Nif$ width = NULL], [Nif$ height = NULL])
//
EidosValue_SP SLiMgui::ExecuteMethod_plotCreate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *title_value = p_arguments[0].get();
    EidosValue *xrange_value = p_arguments[1].get();
    EidosValue *yrange_value = p_arguments[2].get();
    EidosValue *xlab_value = p_arguments[3].get();
    EidosValue *ylab_value = p_arguments[4].get();
    EidosValue *width_value = p_arguments[5].get();
    EidosValue *height_value = p_arguments[6].get();
    
    QString title = QString::fromStdString(title_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (title.length() == 0)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires a non-empty plot title." << EidosTerminate();
    
    double *x_range = nullptr;
    double x_range_array[2];
    
    if (xrange_value->Type() != EidosValueType::kValueNULL)
    {
        if (xrange_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires xrange to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        x_range_array[0] = xrange_value->NumericAtIndex_NOCAST(0, nullptr);
        x_range_array[1] = xrange_value->NumericAtIndex_NOCAST(1, nullptr);
        x_range = x_range_array;
        
        if (x_range[0] >= x_range[1])
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires xrange[0] < xrange[1], when a range is specified (non-NULL)." << EidosTerminate();
    }
    
    double *y_range = nullptr;
    double y_range_array[2];
    
    if (yrange_value->Type() != EidosValueType::kValueNULL)
    {
        if (yrange_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires yrange to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        y_range_array[0] = yrange_value->NumericAtIndex_NOCAST(0, nullptr);
        y_range_array[1] = yrange_value->NumericAtIndex_NOCAST(1, nullptr);
        y_range = y_range_array;
        
        if (y_range[0] >= y_range[1])
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires yrange[0] < yrange[1], when a range is specified (non-NULL)." << EidosTerminate();
    }
    
    QString xlab = QString::fromStdString(xlab_value->StringAtIndex_NOCAST(0, nullptr));
    QString ylab = QString::fromStdString(ylab_value->StringAtIndex_NOCAST(0, nullptr));
    
    double width = 0.0;
    
    if (width_value->Type() != EidosValueType::kValueNULL)
    {
        width = width_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(width) || (width <= 0.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires width to be > 0.0, or NULL." << EidosTerminate();
    }
    
    double height = 0.0;
    
    if (height_value->Type() != EidosValueType::kValueNULL)
    {
        height = height_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(height) || (height <= 0.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotCreate): plotCreate() requires height to be > 0.0, or NULL." << EidosTerminate();
    }
    
    controller_->eidos_plotCreate(title, x_range, y_range, xlab, ylab, width, height);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)plotLines(string$ title, numeric x, numeric y, [string$ color = "red"], [numeric$ lwd = 1.0])
//
EidosValue_SP SLiMgui::ExecuteMethod_plotLines(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *title_value = p_arguments[0].get();
    EidosValue *x_value = p_arguments[1].get();
    EidosValue *y_value = p_arguments[2].get();
    EidosValue *color_value = p_arguments[3].get();
    EidosValue *lwd_value = p_arguments[4].get();
    
    // title
    QString title = QString::fromStdString(title_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (title.length() == 0)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotLines): plotLines() requires a non-empty plot title." << EidosTerminate();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    
    if (xcount != ycount)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotLines): plotLines() requires x and y to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotLines): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
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
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotLines): plotLines() requires the line width lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    std::vector<double> *lineWidths = new std::vector<double>;  // we only take a singleton width, but the API expects a buffer
    
    lineWidths->push_back(lwd);
    
    controller_->eidos_plotLines(title, x, y, xcount, colors, lineWidths);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)plotPoints(string$ title, numeric x, numeric y, [integer symbol = 0], [string color = "red"], [string border = "black"], [numeric lwd = 1.0], [numeric size = 1.0])
//
EidosValue_SP SLiMgui::ExecuteMethod_plotPoints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *title_value = p_arguments[0].get();
    EidosValue *x_value = p_arguments[1].get();
    EidosValue *y_value = p_arguments[2].get();
    EidosValue *symbol_value = p_arguments[3].get();
    EidosValue *color_value = p_arguments[4].get();
    EidosValue *border_value = p_arguments[5].get();
    EidosValue *lwd_value = p_arguments[6].get();
    EidosValue *size_value = p_arguments[7].get();
    
    // title
    QString title = QString::fromStdString(title_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (title.length() == 0)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires a non-empty plot title." << EidosTerminate();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    
    if (xcount != ycount)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires x and y to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
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
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires symbol to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < symbol_count; ++index)
    {
        int symbol = symbol_value->IntAtIndex_NOCAST(index, nullptr);
        
        if (symbol < 0)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires the elements of symbol to be >= 0." << EidosTerminate();
        
        symbols->push_back(symbol);
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != xcount))
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires color to match the length of x and y, or be singleton." << EidosTerminate();
    
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
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires border to match the length of x and y, or be singleton." << EidosTerminate();
    
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
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires lwd to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    // size
    std::vector<double> *sizes = new std::vector<double>;
    int size_count = size_value->Count();
    
    if ((size_count != 1) && (size_count != xcount))
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires size to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < size_count; ++index)
    {
        double size = size_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((size <= 0.0) || (size > 1000.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
        
        sizes->push_back(size);
    }
    
    controller_->eidos_plotPoints(title, x, y, xcount, symbols, colors, borders, lwds, sizes);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)plotText(string$ title, numeric x, numeric y, string labels, [string color = "black"], [numeric size = 10.0], [Nif adj = NULL])
//
EidosValue_SP SLiMgui::ExecuteMethod_plotText(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *title_value = p_arguments[0].get();
    EidosValue *x_value = p_arguments[1].get();
    EidosValue *y_value = p_arguments[2].get();
    EidosValue *labels_value = p_arguments[3].get();
    EidosValue *color_value = p_arguments[4].get();
    EidosValue *size_value = p_arguments[5].get();
    EidosValue *adj_value = p_arguments[6].get();
    
    // title
    QString title = QString::fromStdString(title_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (title.length() == 0)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotText): plotText() requires a non-empty plot title." << EidosTerminate();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    int labelscount = labels_value->Count();
    
    if ((xcount != ycount) || (xcount != labelscount))
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotText): plotText() requires x, y, and labels to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotText): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
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
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotText): plotText() requires color to match the length of x and y, or be singleton." << EidosTerminate();
    
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
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotPoints): plotPoints() requires size to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < size_count; ++index)
    {
        double size = size_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((size <= 0.0) || (size > 1000.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotText): plotText() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
        
        sizes->push_back(size);
    }
    
    // adj
    double adj[2] = {0.5, 0.5};
    
    if (adj_value->Type() != EidosValueType::kValueNULL)
    {
        if (adj_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotText): plotText() requires adj to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        adj[0] = adj_value->NumericAtIndex_NOCAST(0, nullptr);
        adj[1] = adj_value->NumericAtIndex_NOCAST(1, nullptr);
    }
    
    controller_->eidos_plotText(title, x, y, labels, xcount, colors, sizes, adj);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}


//
//	SLiMgui_Class
//
#pragma mark -
#pragma mark SLiMgui_Class
#pragma mark -

EidosClass *gSLiM_SLiMgui_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *SLiMgui_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back(static_cast<EidosPropertySignature *>((new EidosPropertySignature(gStr_pid, true,	kEidosValueMaskInt | kEidosValueMaskSingleton))));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *SLiMgui_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_openDocument, kEidosValueMaskVOID))->AddString_S("filePath")));
		methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_pauseExecution, kEidosValueMaskVOID))));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_plotCreate, kEidosValueMaskVOID))->AddString_S("title")
                                  ->AddNumeric_ON("xrange", gStaticEidosValueNULL)->AddNumeric_ON("yrange", gStaticEidosValueNULL)
                                  ->AddString_OS("xlab", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("x")))
                                  ->AddString_OS("ylab", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("y")))
                                  ->AddNumeric_OSN("width", gStaticEidosValueNULL)->AddNumeric_OSN("height", gStaticEidosValueNULL)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_plotLines, kEidosValueMaskVOID))->AddString_S("title")
                                  ->AddNumeric("x")->AddNumeric("y")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_plotPoints, kEidosValueMaskVOID))->AddString_S("title")
                                  ->AddNumeric("x")->AddNumeric("y")->AddInt_O("symbol", gStaticEidosValue_Integer0)
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddString_O("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddNumeric_O("size", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_plotText, kEidosValueMaskVOID))->AddString_S("title")
                                  ->AddNumeric("x")->AddNumeric("y")->AddString("labels")
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("size", EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(10)))
                                  ->AddNumeric_ON("adj", gStaticEidosValueNULL)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
































