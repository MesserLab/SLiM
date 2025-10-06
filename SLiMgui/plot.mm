//
//  plot.cpp
//  SLiMgui
//
//  Created by Ben Haller on 1/30/24.
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


#include "plot.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#import "SLiMWindowController.h"

#include <unistd.h>


#pragma mark -
#pragma mark Plot
#pragma mark -

Plot::Plot(void)
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
			// The user has no way to access this property in SLiMguiLegacy
			return gStaticEidosValue_StringEmpty;
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
		case gID_abline:				return ExecuteMethod_abline(p_method_id, p_arguments, p_interpreter);
		case gID_addLegend:				return ExecuteMethod_addLegend(p_method_id, p_arguments, p_interpreter);
		case gID_axis:					return ExecuteMethod_axis(p_method_id, p_arguments, p_interpreter);
		case gID_image:					return ExecuteMethod_image(p_method_id, p_arguments, p_interpreter);
		case gID_legendLineEntry:		return ExecuteMethod_legendLineEntry(p_method_id, p_arguments, p_interpreter);
		case gID_legendPointEntry:		return ExecuteMethod_legendPointEntry(p_method_id, p_arguments, p_interpreter);
		case gID_legendSwatchEntry:		return ExecuteMethod_legendSwatchEntry(p_method_id, p_arguments, p_interpreter);
		case gID_legendTitleEntry:		return ExecuteMethod_legendTitleEntry(p_method_id, p_arguments, p_interpreter);
		case gID_lines:					return ExecuteMethod_lines(p_method_id, p_arguments, p_interpreter);
		case gID_matrix:				return ExecuteMethod_matrix(p_method_id, p_arguments, p_interpreter);
		case gID_points:				return ExecuteMethod_points(p_method_id, p_arguments, p_interpreter);
		case gID_rects:					return ExecuteMethod_rects(p_method_id, p_arguments, p_interpreter);
		case gID_segments:				return ExecuteMethod_segments(p_method_id, p_arguments, p_interpreter);
		case gID_setBorderless:			return ExecuteMethod_setBorderless(p_method_id, p_arguments, p_interpreter);
		case gID_text:					return ExecuteMethod_text(p_method_id, p_arguments, p_interpreter);
		case gEidosID_write:			return ExecuteMethod_write(p_method_id, p_arguments, p_interpreter);
		default:						return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)abline(...)
//
EidosValue_SP Plot::ExecuteMethod_abline(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)addLegend(...)
//
EidosValue_SP Plot::ExecuteMethod_addLegend(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)axis(...)
//
EidosValue_SP Plot::ExecuteMethod_axis(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)image(...)
//
EidosValue_SP Plot::ExecuteMethod_image(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)legendLineEntry(...)
//
EidosValue_SP Plot::ExecuteMethod_legendLineEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)legendPointEntry(...)
//
EidosValue_SP Plot::ExecuteMethod_legendPointEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)legendSwatchEntry(...)
//
EidosValue_SP Plot::ExecuteMethod_legendSwatchEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)legendTitleEntry(...)
//
EidosValue_SP Plot::ExecuteMethod_legendTitleEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)lines(...)
//
EidosValue_SP Plot::ExecuteMethod_lines(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)matrix(...)
//
EidosValue_SP Plot::ExecuteMethod_matrix(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)points(...)
//
EidosValue_SP Plot::ExecuteMethod_points(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)rects(...)
//
EidosValue_SP Plot::ExecuteMethod_rects(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)segments(...)
//
EidosValue_SP Plot::ExecuteMethod_segments(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)text(...)
//
EidosValue_SP Plot::ExecuteMethod_text(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setBorderless(...)
//
EidosValue_SP Plot::ExecuteMethod_setBorderless(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
	return gStaticEidosValueVOID;
}

//	*********************	– (void)write(string$ filePath)
//
EidosValue_SP Plot::ExecuteMethod_write(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The user has no way to call this method in SLiMguiLegacy, since createPlot() does not return a Plot object.
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
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_title,			true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
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
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_abline, kEidosValueMaskVOID))
							  ->AddNumeric_ON("a", gStaticEidosValueNULL)->AddNumeric_ON("b", gStaticEidosValueNULL)
							  ->AddNumeric_ON("h", gStaticEidosValueNULL)->AddNumeric_ON("v", gStaticEidosValueNULL)
							  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addLegend, kEidosValueMaskVOID))
							  ->AddString_OSN("position", gStaticEidosValueNULL)->AddInt_OSN("inset", gStaticEidosValueNULL)
							  ->AddNumeric_OSN("labelSize", gStaticEidosValueNULL)->AddNumeric_OSN("lineHeight", gStaticEidosValueNULL)
							  ->AddNumeric_OSN("graphicsWidth", gStaticEidosValueNULL)->AddNumeric_OSN("exteriorMargin", gStaticEidosValueNULL)
							  ->AddNumeric_OSN("interiorMargin", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_axis, kEidosValueMaskVOID))
							  ->AddInt_S("side")->AddNumeric_ON("at", gStaticEidosValueNULL)
							  ->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional, "labels", nullptr, gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_image, kEidosValueMaskVOID))
							  ->AddObject_S(gStr_image, nullptr)->AddNumeric_S("x1")->AddNumeric_S("y1")->AddNumeric_S("x2")->AddNumeric_S("y2")
							  ->AddLogical_OS("flipped", gStaticEidosValue_LogicalF)->AddFloat_OS("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_legendLineEntry, kEidosValueMaskVOID))
							  ->AddString_S("label")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_legendPointEntry, kEidosValueMaskVOID))
							  ->AddString_S("label")->AddInt_OS("symbol", gStaticEidosValue_Integer0)
							  ->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddString_OS("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
							  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)->AddNumeric_OS("size", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_legendSwatchEntry, kEidosValueMaskVOID))
							  ->AddString_S("label")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red"))));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_legendTitleEntry, kEidosValueMaskVOID))
							  ->AddString_S("label"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_lines, kEidosValueMaskVOID))
							  ->AddNumeric("x")->AddNumeric("y")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)->AddFloat_OS("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_matrix, kEidosValueMaskVOID))
							  ->AddNumeric("matrix")->AddNumeric_S("x1")->AddNumeric_S("y1")->AddNumeric_S("x2")->AddNumeric_S("y2")
							  ->AddLogical_OS("flipped", gStaticEidosValue_LogicalF)->AddNumeric_ON("valueRange", gStaticEidosValueNULL)
							  ->AddString_OSN("colors", gStaticEidosValueNULL)->AddFloat_OS("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_points, kEidosValueMaskVOID))
							  ->AddNumeric("x")->AddNumeric("y")->AddInt_O("symbol", gStaticEidosValue_Integer0)
							  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddString_O("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
							  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddNumeric_O("size", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_rects, kEidosValueMaskVOID))
							  ->AddNumeric("x1")->AddNumeric("y1")->AddNumeric("x2")->AddNumeric("y2")
							  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddString_O("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
							  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_segments, kEidosValueMaskVOID))
							  ->AddNumeric("x1")->AddNumeric("y1")->AddNumeric("x2")->AddNumeric("y2")
							  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
							  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setBorderless, kEidosValueMaskVOID))
							  ->AddNumeric_OS("marginLeft", gStaticEidosValue_Float0)->AddNumeric_OS("marginTop", gStaticEidosValue_Float0)
							  ->AddNumeric_OS("marginRight", gStaticEidosValue_Float0)->AddNumeric_OS("marginBottom", gStaticEidosValue_Float0));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_text, kEidosValueMaskVOID))
							  ->AddNumeric("x")->AddNumeric("y")->AddString("labels")
							  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
							  ->AddNumeric_O("size", EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(10)))
							  ->AddNumeric_ON("adj", gStaticEidosValueNULL)->AddFloat_O("alpha", gStaticEidosValue_Float1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_write, kEidosValueMaskVOID))
							  ->AddString_S(gEidosStr_filePath));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}






















