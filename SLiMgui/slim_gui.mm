//
//  slim_gui.cpp
//  SLiMgui
//
//  Created by Ben Haller on 1/19/19.
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


#include "slim_gui.h"
#include "plot.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#import "SLiMWindowController.h"

#include <unistd.h>


#pragma mark -
#pragma mark SLiMgui
#pragma mark -

SLiMgui::SLiMgui(Community &p_community, SLiMWindowController *p_controller) :
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
		case gID_openDocument:				return ExecuteMethod_openDocument(p_method_id, p_arguments, p_interpreter);
		case gID_pauseExecution:			return ExecuteMethod_pauseExecution(p_method_id, p_arguments, p_interpreter);
		case gID_plotWithTitle:				return ExecuteMethod_plotWithTitle(p_method_id, p_arguments, p_interpreter);
		default:							return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (No<Plot>$)createPlot(...)
//
EidosValue_SP SLiMgui::ExecuteMethod_createPlot(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	static bool beenHere = false;
	
	if (!beenHere)
	{
		// Emit a warning that this API is unsupported in SLiMguiLegacy.  We warn, not error, so the user does not have to modify
		// their script to run it in SLiMguiLegacy when it is designed to run under QtSLiM; they just won't get a plot window.
		std::cerr << "WARNING (SLiMgui::ExecuteMethod_createPlot): createPlot() is not supported in SLiMguiLegacy, and does nothing." << std::endl;
		beenHere = true;
	}
	
	return gStaticEidosValueNULL;
}

//	*********************	– (void)openDocument(string$ path)
//
EidosValue_SP SLiMgui::ExecuteMethod_openDocument(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	EidosValue_String *filePath_value = (EidosValue_String *)p_arguments[0].get();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringRefAtIndex_NOCAST(0, nullptr)));
	NSString *filePath = [NSString stringWithUTF8String:file_path.c_str()];
	
	[controller_ eidos_openDocument:filePath];
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)pauseExecution(void)
//
EidosValue_SP SLiMgui::ExecuteMethod_pauseExecution(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	[controller_ eidos_pauseExecution];
	
	return gStaticEidosValueVOID;
}

//	*********************	– (No<Plot>$)plotWithTitle(...)
//
EidosValue_SP SLiMgui::ExecuteMethod_plotWithTitle(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	static bool beenHere = false;
	
	if (!beenHere)
	{
		// Emit a warning that this API is unsupported in SLiMguiLegacy.  We warn, not error, so the user does not have to modify
		// their script to run it in SLiMguiLegacy when it is designed to run under QtSLiM; they just won't get a plot window.
		std::cerr << "WARNING (SLiMgui::ExecuteMethod_plotWithTitle): plotWithTitle() is not supported in SLiMguiLegacy, and does nothing." << std::endl;
		beenHere = true;
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
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pid,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
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
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_createPlot, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Plot_Class))->AddString_S("title")
							  ->AddNumeric_ON("xrange", gStaticEidosValueNULL)->AddNumeric_ON("yrange", gStaticEidosValueNULL)
							  ->AddString_OS("xlab", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("x")))
							  ->AddString_OS("ylab", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("y")))
							  ->AddNumeric_OSN("width", gStaticEidosValueNULL)->AddNumeric_OSN("height", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_openDocument, kEidosValueMaskVOID))->AddString_S(gEidosStr_filePath));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pauseExecution, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_plotWithTitle, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Plot_Class))->AddString_S("title"));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}






















