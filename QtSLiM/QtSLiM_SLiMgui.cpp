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
#include "QtSLiMGraphView_CustomPlot.h"
#include "QtSLiM_Plot.h"

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "log_file.h"

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
        case gID_createPlot:				return ExecuteMethod_createPlot(p_method_id, p_arguments, p_interpreter);
        case gID_logFileData:				return ExecuteMethod_logFileData(p_method_id, p_arguments, p_interpreter);
        case gID_openDocument:				return ExecuteMethod_openDocument(p_method_id, p_arguments, p_interpreter);
        case gID_pauseExecution:			return ExecuteMethod_pauseExecution(p_method_id, p_arguments, p_interpreter);
        case gID_plotWithTitle:				return ExecuteMethod_plotWithTitle(p_method_id, p_arguments, p_interpreter);
        default:							return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (No<Plot>$)createPlot(string$ title, [Nif xrange = NULL], [Nif yrange = NULL], [string$ xlab = "x"], [string$ ylab = "y"], [Nif$ width = NULL], [Nif$ height = NULL]
//                                                  [Nl$ showHorizontalGrid = NULL], [Nl$ showVerticalGrid = NULL], [Nl$ showFullBox = NULL])
//
EidosValue_SP SLiMgui::ExecuteMethod_createPlot(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *title_value = p_arguments[0].get();
    EidosValue *xrange_value = p_arguments[1].get();
    EidosValue *yrange_value = p_arguments[2].get();
    EidosValue *xlab_value = p_arguments[3].get();
    EidosValue *ylab_value = p_arguments[4].get();
    EidosValue *width_value = p_arguments[5].get();
    EidosValue *height_value = p_arguments[6].get();
    EidosValue *showHorizontalGrid_value = p_arguments[7].get();
    EidosValue *showVerticalGrid_value = p_arguments[8].get();
    EidosValue *showFullBox_value = p_arguments[9].get();
    
    QString title = QString::fromStdString(title_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (title.length() == 0)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires a non-empty plot title." << EidosTerminate();
    
    double *x_range = nullptr;
    double x_range_array[2];
    
    if (xrange_value->Type() != EidosValueType::kValueNULL)
    {
        if (xrange_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires xrange to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        x_range_array[0] = xrange_value->NumericAtIndex_NOCAST(0, nullptr);
        x_range_array[1] = xrange_value->NumericAtIndex_NOCAST(1, nullptr);
        x_range = x_range_array;
        
        if (x_range[0] >= x_range[1])
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires xrange[0] < xrange[1], when a range is specified (non-NULL)." << EidosTerminate();
    }
    
    double *y_range = nullptr;
    double y_range_array[2];
    
    if (yrange_value->Type() != EidosValueType::kValueNULL)
    {
        if (yrange_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires yrange to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        y_range_array[0] = yrange_value->NumericAtIndex_NOCAST(0, nullptr);
        y_range_array[1] = yrange_value->NumericAtIndex_NOCAST(1, nullptr);
        y_range = y_range_array;
        
        if (y_range[0] >= y_range[1])
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires yrange[0] < yrange[1], when a range is specified (non-NULL)." << EidosTerminate();
    }
    
    QString xlab = QString::fromStdString(xlab_value->StringAtIndex_NOCAST(0, nullptr));
    QString ylab = QString::fromStdString(ylab_value->StringAtIndex_NOCAST(0, nullptr));
    
    double width = 0.0;
    
    if (width_value->Type() != EidosValueType::kValueNULL)
    {
        width = width_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(width) || (width <= 0.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires width to be > 0.0, or NULL." << EidosTerminate();
    }
    
    double height = 0.0;
    
    if (height_value->Type() != EidosValueType::kValueNULL)
    {
        height = height_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(height) || (height <= 0.0))
            EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_createPlot): createPlot() requires height to be > 0.0, or NULL." << EidosTerminate();
    }
    
    int showHorizontalGrid = -1;    // default for NULL; handled downstream
    
    if (showHorizontalGrid_value->Type() != EidosValueType::kValueNULL)
        showHorizontalGrid = showHorizontalGrid_value->LogicalAtIndex_NOCAST(0, nullptr);
    
    int showVerticalGrid = -1;      // default for NULL; handled downstream
    
    if (showVerticalGrid_value->Type() != EidosValueType::kValueNULL)
        showVerticalGrid = showVerticalGrid_value->LogicalAtIndex_NOCAST(0, nullptr);
    
    int showFullBox = -1;           // default for NULL; handled downstream
    
    if (showFullBox_value->Type() != EidosValueType::kValueNULL)
        showFullBox = showFullBox_value->LogicalAtIndex_NOCAST(0, nullptr);
    
    // make the plot view; note this might return an existing object
    QtSLiMGraphView_CustomPlot *plotview = controller_->eidos_createPlot(title, x_range, y_range, xlab, ylab, width, height, showHorizontalGrid, showVerticalGrid, showFullBox);
    
    // plotview owns its Eidos instance of class Plot, and keeps it across recycles
    Plot *plot = plotview->eidosPlotObject();
    
    if (!plot)
    {
        plot = new Plot(plotview);
        plotview->setEidosPlotObject(plot);
    }
    
    EidosValue_SP result_SP(EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(plot, gSLiM_Plot_Class)));
    
    return result_SP;
}

//	*********************	– (Nfs)logFileData(o<LogFile>$ logFile, is$ column)
//
EidosValue_SP SLiMgui::ExecuteMethod_logFileData(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    
    EidosValue_Object *logFile_value = (EidosValue_Object *)p_arguments[0].get();
    EidosValue *column_value = p_arguments[1].get();
    
    LogFile *logFile = (LogFile *)logFile_value->ObjectElementAtIndex_NOCAST(0, nullptr);
    
    return controller_->eidos_logFileData(logFile, column_value);
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

//	*********************	– (No<Plot>$)plotWithTitle(string$ title)
//
EidosValue_SP SLiMgui::ExecuteMethod_plotWithTitle(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *title_value = p_arguments[0].get();
    
    QString title = QString::fromStdString(title_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (title.length() == 0)
        EIDOS_TERMINATION << "ERROR (SLiMgui::ExecuteMethod_plotWithTitle): plotWithTitle() requires a non-empty plot title." << EidosTerminate();
    
    QtSLiMGraphView_CustomPlot *plotview = controller_->eidos_plotWithTitle(title);
    
    if (plotview)
    {
        Plot *plot = plotview->eidosPlotObject();
        EidosValue_SP result_SP(EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(plot, gSLiM_Plot_Class)));
        
        return result_SP;
    }
    
    return gStaticEidosValueNULL;
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
		
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_createPlot,
                                  kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Plot_Class))->AddString_S("title")
                                  ->AddNumeric_ON("xrange", gStaticEidosValueNULL)->AddNumeric_ON("yrange", gStaticEidosValueNULL)
                                  ->AddString_OS("xlab", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("x")))
                                  ->AddString_OS("ylab", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("y")))
                                  ->AddNumeric_OSN("width", gStaticEidosValueNULL)->AddNumeric_OSN("height", gStaticEidosValueNULL)
                                  ->AddLogical_OSN("showHorizontalGrid", gStaticEidosValueNULL)->AddLogical_OSN("showVerticalGrid", gStaticEidosValueNULL)
                                  ->AddLogical_OSN("showFullBox", gStaticEidosValueNULL)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_logFileData, kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString))
                                  ->AddObject_S("logFile", gSLiM_LogFile_Class)->AddIntString_S("column")));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_openDocument, kEidosValueMaskVOID))->AddString_S("filePath")));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_pauseExecution, kEidosValueMaskVOID))));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_plotWithTitle,
                                  kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Plot_Class))->AddString_S("title")));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
































