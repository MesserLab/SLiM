//
//  eidos_type_table.cpp
//  Eidos
//
//  Created by Ben Haller on 5/8/16.
//  Copyright (c) 2016-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.


#include "eidos_type_table.h"

#include <algorithm>
#include <utility>


EidosTypeTable::EidosTypeTable(void)
{
	// Construct a base table for Eidos containing the standard constants
	SetTypeForSymbol(gEidosID_T, EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
	SetTypeForSymbol(gEidosID_F, EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
	SetTypeForSymbol(gEidosID_NULL, EidosTypeSpecifier{kEidosValueMaskNULL, nullptr});
	SetTypeForSymbol(gEidosID_PI, EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
	SetTypeForSymbol(gEidosID_E, EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
	SetTypeForSymbol(gEidosID_INF, EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
	SetTypeForSymbol(gEidosID_NAN, EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
}

EidosTypeTable::~EidosTypeTable(void)
{
}

std::vector<std::string> EidosTypeTable::AllSymbols(void) const
{
	std::vector<std::string> symbol_names;
	
	for (auto symbol_slot_iter = hash_symbols_.begin(); symbol_slot_iter != hash_symbols_.end(); ++symbol_slot_iter)
	{
		EidosGlobalStringID string_id = symbol_slot_iter->first;
		const std::string &string_ref = Eidos_StringForGlobalStringID(string_id);
		
		symbol_names.emplace_back(string_ref);
	}
	
	return symbol_names;
}

std::vector<EidosGlobalStringID> EidosTypeTable::AllSymbolIDs(void) const
{
	std::vector<EidosGlobalStringID> symbol_ids;
	
	for (auto symbol_slot_iter = hash_symbols_.begin(); symbol_slot_iter != hash_symbols_.end(); ++symbol_slot_iter)
	{
		EidosGlobalStringID string_id = symbol_slot_iter->first;
		
		symbol_ids.emplace_back(string_id);
	}
	
	return symbol_ids;
}

bool EidosTypeTable::ContainsSymbol(EidosGlobalStringID p_symbol_name) const
{
	if (hash_symbols_.find(p_symbol_name) != hash_symbols_.end())
		return true;
	
	return false;
}

void EidosTypeTable::SetTypeForSymbol(EidosGlobalStringID p_symbol_name, EidosTypeSpecifier p_type)
{
	// We decline to track variables if their type is "none".  This prevents some sorts of parsing garbage from getting
	// into the table, and it also prevents "z" from being visible as a completion option when the user has typed "z ="
	// and then hit escape; that produces "z = <bad node>", which results in a call to us to define z with the type
	// kEidosValueMaskNone, providing the user with the option of completing the statement as "z = z", which is dumb.
	// Note that we do this whether the variable has been previously defined or not, so that we don't replace useful
	// type information with useless garbage information; for example, in "x = sim; x = " we want to know that x is
	// of type SLiMSim, not replace that knowledge with junk.  This means that variables will retain their previous
	// type whenever they are set to kEidosValueMaskNone, even when that might be legitimate; that's OK, I think,
	// particularly since there presently really aren't any "legitimate" uses of kEidosValueMaskNone – it always
	// represents some kind of parse error, misuse of operators, unknown functions/methods, etc.
	if (p_type.type_mask != kEidosValueMaskNone)
	{
		auto existing_symbol_slot_iter = hash_symbols_.find(p_symbol_name);
		
		if (existing_symbol_slot_iter == hash_symbols_.end())
		{
			hash_symbols_.insert(EidosTypeTableEntry(p_symbol_name, std::move(p_type)));
		}
		else
		{
			existing_symbol_slot_iter->second = std::move(p_type);
		}
	}
}

void EidosTypeTable::RemoveTypeForSymbol(EidosGlobalStringID p_symbol_name)
{
	auto remove_iter = hash_symbols_.find(p_symbol_name);
	
	if (remove_iter != hash_symbols_.end())
		hash_symbols_.erase(remove_iter);
}

void EidosTypeTable::RemoveSymbolsOfClass(const EidosObjectClass *p_object_class)
{
	std::vector<EidosGlobalStringID> symbolIDs = AllSymbolIDs();
	
	for (EidosGlobalStringID symbolID : symbolIDs)
	{
		EidosTypeSpecifier type = GetTypeForSymbol(symbolID);
		
		if ((type.type_mask & kEidosValueMaskObject) && (type.object_class == p_object_class))
			RemoveTypeForSymbol(symbolID);
	}
}

void EidosTypeTable::RemoveAllSymbols(void)
{
	hash_symbols_.clear();
}

EidosTypeSpecifier EidosTypeTable::GetTypeForSymbol(EidosGlobalStringID p_symbol_name) const
{
	auto symbol_slot_iter = hash_symbols_.find(p_symbol_name);
	
	if (symbol_slot_iter != hash_symbols_.end())
		return symbol_slot_iter->second;
	
	// Since we don't allow kEidosValueMaskNone to be set in SetTypeForSymbol(), it is a reliable "not found" marker
	return EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
}

// This is unused except by debugging code and in the debugger itself
std::ostream &operator<<(std::ostream &p_outstream, const EidosTypeTable &p_symbols)
{
	std::vector<std::string> symbol_names = p_symbols.AllSymbols();
	
	std::sort(symbol_names.begin(), symbol_names.end());
	
	for (auto symbol_name_iter = symbol_names.begin(); symbol_name_iter != symbol_names.end(); ++symbol_name_iter)
	{
		const std::string &symbol_name = *symbol_name_iter;
		EidosTypeSpecifier symbol_type = p_symbols.GetTypeForSymbol(Eidos_GlobalStringIDForString(symbol_name));
		
		p_outstream << symbol_name << " ~> (" << StringForEidosValueMask(symbol_type.type_mask, symbol_type.object_class, "", nullptr) << ") " << std::endl;
	}
	
	return p_outstream;
}































































