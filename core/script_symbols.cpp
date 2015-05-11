//
//  script_symbols.cpp
//  SLiM
//
//  Created by Ben Haller on 4/12/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include "script_symbols.h"
#include "script_value.h"
#include "slim_global.h"
#include "math.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


//
//	SymbolTable
//
#pragma mark SymbolTable

SymbolTable::SymbolTable(void)
{
	SetConstantForSymbol("T", new ScriptValue_Logical(true));
	SetConstantForSymbol("F", new ScriptValue_Logical(false));
	SetConstantForSymbol("NULL", new ScriptValue_NULL());
	SetConstantForSymbol("PI", new ScriptValue_Float(M_PI));
	SetConstantForSymbol("E", new ScriptValue_Float(M_E));
	SetConstantForSymbol("INF", new ScriptValue_Float(std::numeric_limits<double>::infinity()));
	SetConstantForSymbol("NAN", new ScriptValue_Float(std::numeric_limits<double>::quiet_NaN()));
}

SymbolTable::~SymbolTable(void)
{
	for (auto symbol_entry : constants_)
		delete symbol_entry.second;
	for (auto symbol_entry : variables_)
		delete symbol_entry.second;
}

std::vector<std::string> SymbolTable::ReadOnlySymbols(void) const
{
	std::vector<std::string> symbols;
	
	for(auto symbol_pair : constants_)
		symbols.push_back(symbol_pair.first);
	
	return symbols;
}

std::vector<std::string> SymbolTable::ReadWriteSymbols(void) const
{
	std::vector<std::string> symbols;
	
	for(auto symbol_pair : variables_)
		symbols.push_back(symbol_pair.first);
	
	return symbols;
}

ScriptValue *SymbolTable::GetValueForSymbol(const std::string &p_symbol_name) const
{
	ScriptValue *result = nullptr;
	
	// first look in our constants
	auto constant_iter = constants_.find(p_symbol_name);
	
	if (constant_iter != constants_.end())
	{
		// got a hit; extract it from the iterator and the map pair
		result = (*constant_iter).second;
	}
	else
	{
		// no hit; check in our variables
		auto variable_iter = variables_.find(p_symbol_name);
		
		if (variable_iter != variables_.end())
			result = (*variable_iter).second;
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	if (!result)
		SLIM_TERMINATION << "ERROR (SymbolTable::GetValueForSymbol): undefined identifier " << p_symbol_name << "." << endl << slim_terminate();
	
	return result;
}

void SymbolTable::SetValueForSymbol(const std::string &p_symbol_name, ScriptValue *p_value)
{
	// check that we're not trying to overwrite a constant
	auto constant_iter = constants_.find(p_symbol_name);
	
	if (constant_iter != constants_.end())
		SLIM_TERMINATION << "ERROR (SymbolTable::SetValueForSymbol): Identifier '" << p_symbol_name << "' is a constant." << endl << slim_terminate();
	
	// get a version of the value that is suitable for insertion into the symbol table
	if (p_value->InSymbolTable())
	{
		// if it's already in a symbol table, then we need to copy it, to avoid two references to the same ScriptValue
		p_value = p_value->CopyValues();
	}
	else if (p_value->Invisible())
	{
		// if it's invisible, then we need to copy it, since the original needs to stay invisible to make sure it displays correctly
		// it might be possible to set the invisible flag directly instead, since assignment returns NULL; could revisit this...
		p_value = p_value->CopyValues();
	}
	
	// we set the symbol table flag so the pointer won't be deleted or reused by anybody else
	p_value->SetInSymbolTable(true);
	
	// and now set the value in the symbol table
	variables_[p_symbol_name] = p_value;
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void SymbolTable::SetConstantForSymbol(const std::string &p_symbol_name, ScriptValue *p_value)
{
	// check that we're not trying to overwrite a constant
	auto constant_iter = constants_.find(p_symbol_name);
	
	if (constant_iter != constants_.end())
		SLIM_TERMINATION << "ERROR (SymbolTable::SetConstantForSymbol): Identifier '" << p_symbol_name << "' is already a constant." << endl << slim_terminate();
	
	// check that we're not trying to overwrite a variable; if you want to define a constant, you have to get there first
	auto variable_iter = variables_.find(p_symbol_name);
	
	if (variable_iter != variables_.end())
		SLIM_TERMINATION << "ERROR (SymbolTable::SetConstantForSymbol): Identifier '" << p_symbol_name << "' is already a variable." << endl << slim_terminate();
	
	// get a version of the value that is suitable for insertion into the symbol table
	if (p_value->InSymbolTable())
	{
		// if it's already in a symbol table, then we need to copy it, to avoid two references to the same ScriptValue
		p_value = p_value->CopyValues();
	}
	else if (p_value->Invisible())
	{
		// if it's invisible, then we need to copy it, since the original needs to stay invisible to make sure it displays correctly
		// it might be possible to set the invisible flag directly instead, since assignment returns NULL; could revisit this...
		p_value = p_value->CopyValues();
	}
	
	// we set the symbol table flag so the pointer won't be deleted or reused by anybody else
	p_value->SetInSymbolTable(true);
	
	// and now set the value in the symbol table
	constants_[p_symbol_name] = p_value;
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void SymbolTable::RemoveValueForSymbol(const std::string &p_symbol_name, bool remove_constant)
{
	{
		auto constant_iter = constants_.find(p_symbol_name);
		
		if (constant_iter != constants_.end())
		{
			if (remove_constant)
			{
				ScriptValue *value = constant_iter->second;
				
				constants_.erase(constant_iter);
				
				value->SetInSymbolTable(false);
				delete value;
			}
			else
				SLIM_TERMINATION << "ERROR (SymbolTable::RemoveValueForSymbol): Identifier '" << p_symbol_name << "' is a constant and thus cannot be removed." << endl << slim_terminate();
		}
	}
	
	{
		auto variables_iter = variables_.find(p_symbol_name);
		
		if (variables_iter != variables_.end())
		{
			ScriptValue *value = variables_iter->second;
			
			variables_.erase(variables_iter);
			
			value->SetInSymbolTable(false);
			delete value;
		}
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const SymbolTable &p_symbols)
{
	std::vector<std::string> read_only_symbol_names = p_symbols.ReadOnlySymbols();
	std::vector<std::string> read_write_symbol_names = p_symbols.ReadWriteSymbols();
	std::vector<std::string> symbol_names;
	
	symbol_names.insert(symbol_names.end(), read_only_symbol_names.begin(), read_only_symbol_names.end());
	symbol_names.insert(symbol_names.end(), read_write_symbol_names.begin(), read_write_symbol_names.end());
	std::sort(symbol_names.begin(), symbol_names.end());
	
	for (auto symbol_name_iter = symbol_names.begin(); symbol_name_iter != symbol_names.end(); ++symbol_name_iter)
	{
		const std::string symbol_name = *symbol_name_iter;
		ScriptValue *symbol_value = p_symbols.GetValueForSymbol(symbol_name);
		int symbol_count = symbol_value->Count();
		bool is_const = std::find(read_only_symbol_names.begin(), read_only_symbol_names.end(), symbol_name) != read_only_symbol_names.end();
		
		if (symbol_count <= 2)
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *symbol_value << endl;
		else
		{
			ScriptValue *first_value = symbol_value->GetValueAtIndex(0);
			ScriptValue *second_value = symbol_value->GetValueAtIndex(1);
			
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *first_value << " " << *second_value << " ... (" << symbol_count << " values)" << endl;
			if (!first_value->InSymbolTable()) delete first_value;
		}
		
		if (!symbol_value->InSymbolTable()) delete symbol_value;
	}
	
	return p_outstream;
}




































































