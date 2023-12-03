//
//  QtSLiM_SLiMgui.cpp
//  SLiM
//
//  Created by Ben Haller on 12/7/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
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
	self_symbol_(gID_slimgui, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMgui_Class)))
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(getpid()));
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
		default:							return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)openDocument(string$ path)
//
EidosValue_SP SLiMgui::ExecuteMethod_openDocument(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex(0, nullptr)));
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
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
































