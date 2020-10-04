//
//  slim_gui.h
//  SLiMgui
//
//  Created by Ben Haller on 1/19/19.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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

/*
 
 The class SLiMGUI represents the SLiMgui application's Eidos interface.  It is availabe only when running under SLiMgui.
 
 */

#ifndef __SLiM__slim_gui__
#define __SLiM__slim_gui__


#include <vector>
#include <string>
#include <map>

#include "eidos_value.h"
#include "eidos_symbol_table.h"
#include "slim_globals.h"

@class SLiMWindowController;


extern EidosObjectClass *gSLiM_SLiMgui_Class;


class SLiMgui : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
public:
	
	SLiMSim &sim_;								// We have a reference to our simulation object
	SLiMWindowController *controller_;			// We have a reference to the SLiMgui window controller for our simulation
	
	EidosSymbolTableEntry self_symbol_;			// for fast setup of the symbol table
	
	SLiMgui(const SLiMgui&) = delete;					// no copying
	SLiMgui& operator=(const SLiMgui&) = delete;		// no copying
	SLiMgui(void) = delete;								// no null construction
	SLiMgui(SLiMSim &p_sim, SLiMWindowController *p_controller);
	~SLiMgui(void);
	
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; }
	
	virtual const EidosObjectClass *Class(void) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_openDocument(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pauseExecution(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};


#endif /* __SLiM__slim_gui__ */







































