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
//	LValueReference
//
#pragma mark LValueReference

LValueReference::LValueReference(SymbolHost *p_symbol_host) : symbol_host_(p_symbol_host)
{
}

LValueReference::~LValueReference(void)
{
}


//
//	LValueMemberReference
//
#pragma mark LValueMemberReference

LValueMemberReference::LValueMemberReference(SymbolHost *p_symbol_host, std::string p_member_identifier) :
LValueReference(p_symbol_host), member_identifier_(p_member_identifier)
{
}

LValueMemberReference::~LValueMemberReference(void)
{
}

void LValueMemberReference::SetLValueToValue(ScriptValue *p_rvalue) const
{
	symbol_host_->SetValueForMember(member_identifier_, p_rvalue);
}


//
//	LValueSubscriptReference
//
#pragma mark LValueSubscriptReference

LValueSubscriptReference::LValueSubscriptReference(SymbolHost *p_symbol_host, int p_subscript_index) :
LValueReference(p_symbol_host), subscript_index_(p_subscript_index)
{
}

LValueSubscriptReference::~LValueSubscriptReference(void)
{
}

void LValueSubscriptReference::SetLValueToValue(ScriptValue *p_rvalue) const
{
	symbol_host_->SetValueAtIndex(subscript_index_, p_rvalue);
}


//
//	SymbolHost
//
#pragma mark SymbolHost

SymbolHost::~SymbolHost(void)
{
}

void SymbolHost::Print(std::ostream &p_outstream) const
{
	std::vector<std::string> read_only_member_names = ReadOnlyMembers();
	std::vector<std::string> read_write_member_names = ReadWriteMembers();
	
	for (auto member_name_iter = read_only_member_names.begin(); member_name_iter != read_only_member_names.end(); ++member_name_iter)
	{
		const std::string member_name = *member_name_iter;
		ScriptValue *member_value = GetValueForMember(member_name);
		int member_count = member_value->Count();
		
		if (member_count <= 1)
			p_outstream << member_name << " => (" << member_value->Type() << ") " << *member_value << endl;
		else
			p_outstream << member_name << " => (" << member_value->Type() << ") (" << member_count << " values)" << endl;
	}
	for (auto member_name_iter = read_write_member_names.begin(); member_name_iter != read_write_member_names.end(); ++member_name_iter)
	{
		const std::string member_name = *member_name_iter;
		ScriptValue *member_value = GetValueForMember(member_name);
		int member_count = member_value->Count();
		
		if (member_count <= 1)
			p_outstream << member_name << " -> (" << member_value->Type() << ") " << *member_value << endl;
		else
			p_outstream << member_name << " -> (" << member_value->Type() << ") (" << member_count << " values)" << endl;
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const SymbolHost &p_symbols)
{
	p_symbols.Print(p_outstream);
	
	return p_outstream;
}


//
//	SymbolTable
//
#pragma mark SymbolTable

SymbolTable::SymbolTable(void)
{
	SetConstantForMember("T", new ScriptValue_Logical(true));
	SetConstantForMember("F", new ScriptValue_Logical(false));
	SetConstantForMember("NULL", new ScriptValue_NULL());
	SetConstantForMember("PI", new ScriptValue_Float(M_PI));
	SetConstantForMember("E", new ScriptValue_Float(M_E));
	SetConstantForMember("INF", new ScriptValue_Float(std::numeric_limits<double>::infinity()));
	SetConstantForMember("NAN", new ScriptValue_Float(std::numeric_limits<double>::quiet_NaN()));
}

SymbolTable::~SymbolTable(void)
{
	for (auto symbol_entry : constants_)
		delete symbol_entry.second;
	for (auto symbol_entry : variables_)
		delete symbol_entry.second;
}

std::vector<std::string> SymbolTable::ReadOnlyMembers(void) const
{
	std::vector<std::string> members;
	
	for(auto symbol_pair : constants_)
		members.push_back(symbol_pair.first);
	
	return members;
}

std::vector<std::string> SymbolTable::ReadWriteMembers(void) const
{
	std::vector<std::string> members;
	
	for(auto symbol_pair : variables_)
		members.push_back(symbol_pair.first);
	
	return members;
}

ScriptValue *SymbolTable::GetValueAtIndex(const int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR (GetValueAtIndex): Subscripting of the global namespace is not allowed." << endl << slim_terminate();
	return nullptr;
}

void SymbolTable::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR (GetValueAtIndex): Subscripting of the global namespace is not allowed." << endl << slim_terminate();
}

ScriptValue *SymbolTable::GetValueForMember(const std::string &p_member_name) const
{
	ScriptValue *result = nullptr;
	
	// first look in our constants
	auto constant_iter = constants_.find(p_member_name);
	
	if (constant_iter != constants_.end())
	{
		// got a hit; extract it from the iterator and the map pair
		result = (*constant_iter).second;
	}
	else
	{
		// no hit; check in our variables
		auto variable_iter = variables_.find(p_member_name);
		
		result = (variable_iter == variables_.end()) ? nullptr : (*variable_iter).second;
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	return result;
}

void SymbolTable::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	// check that we're not trying to overwrite a constant
	auto constant_iter = constants_.find(p_member_name);
	
	if (constant_iter != constants_.end())
		SLIM_TERMINATION << "ERROR (SetValueForMember): Identifier '" << p_member_name << "' is a constant." << endl << slim_terminate();
	
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
	else
	{
		// otherwise, we set the symbol table flag so the pointer won't be deleted or reused by anybody else
		p_value->SetInSymbolTable(true);
	}
	
	// and now set the value in the symbol table
	variables_[p_member_name] = p_value;
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void SymbolTable::SetConstantForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	// check that we're not trying to overwrite a constant
	auto constant_iter = constants_.find(p_member_name);
	
	if (constant_iter != constants_.end())
		SLIM_TERMINATION << "ERROR (SetConstantForMember): Identifier '" << p_member_name << "' is already a constant." << endl << slim_terminate();
	
	// check that we're not trying to overwrite a variable; if you want to define a constant, you have to get there first
	auto variable_iter = variables_.find(p_member_name);
	
	if (variable_iter != variables_.end())
		SLIM_TERMINATION << "ERROR (SetConstantForMember): Identifier '" << p_member_name << "' is already a variable." << endl << slim_terminate();
	
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
	else
	{
		// otherwise, we set the symbol table flag so the pointer won't be deleted or reused by anybody else
		p_value->SetInSymbolTable(true);
	}
	
	// and now set the value in the symbol table
	constants_[p_member_name] = p_value;
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void SymbolTable::RemoveValueForMember(const std::string &p_member_name)
{
	{
		auto constant_iter = constants_.find(p_member_name);
		
		if (constant_iter != constants_.end())
		{
			ScriptValue *value = constant_iter->second;
			
			constants_.erase(constant_iter);
			
			value->SetInSymbolTable(false);
			delete value;
		}
	}
	
	{
		auto variables_iter = variables_.find(p_member_name);
		
		if (variables_iter != variables_.end())
		{
			ScriptValue *value = variables_iter->second;
			
			variables_.erase(variables_iter);
			
			value->SetInSymbolTable(false);
			delete value;
		}
	}
}





































































