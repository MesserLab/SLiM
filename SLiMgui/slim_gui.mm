//
//  slim_gui.cpp
//  SLiMgui
//
//  Created by Ben Haller on 1/19/19.
//  Copyright (c) 2017-2019 Philipp Messer.  All rights reserved.
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
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#import "SLiMWindowController.h"


#pragma mark -
#pragma mark SLiMGUI
#pragma mark -

SLiMgui::SLiMgui(SLiMSim &p_sim, SLiMWindowController *p_controller) :
	sim_(p_sim),
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

const EidosObjectClass *SLiMgui::Class(void) const
{
	return gSLiM_SLiMgui_Class;
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
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void SLiMgui::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP SLiMgui::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_pause:						return ExecuteMethod_pause(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:							return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	– (void)pause(void)
//
EidosValue_SP SLiMgui::ExecuteMethod_pause(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	
	[controller_ eidos_pause];
	
	return gStaticEidosValueVOID;
}


//
//	SLiMgui_Class
//
#pragma mark -
#pragma mark SLiMgui_Class
#pragma mark -

class SLiMgui_Class : public EidosObjectClass
{
	public:
	SLiMgui_Class(const SLiMgui_Class &p_original) = delete;	// no copy-construct
	SLiMgui_Class& operator=(const SLiMgui_Class&) = delete;	// no copying
	inline SLiMgui_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
};

EidosObjectClass *gSLiM_SLiMgui_Class = new SLiMgui_Class();


const std::string &SLiMgui_Class::ElementType(void) const
{
	return gStr_SLiMgui;
}

const std::vector<const EidosPropertySignature *> *SLiMgui_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pid,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<const EidosMethodSignature *> *SLiMgui_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pause, kEidosValueMaskVOID)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}






















