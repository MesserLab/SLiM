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
#include "slim_script_block.h"
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

SymbolTable::SymbolTable(SLiMScriptBlock *script_block)
{
	// Set up the symbol table itself
	symbol_count_ = 0;
	symbol_capacity_ = SLIM_SYMBOL_TABLE_BASE_SIZE;
	symbols_ = non_malloc_symbols;
	
	// We statically allocate our base symbols for fast setup / teardown
	static SymbolTableEntry *trueConstant = nullptr;
	static SymbolTableEntry *falseConstant = nullptr;
	static SymbolTableEntry *nullConstant = nullptr;
	static SymbolTableEntry *piConstant = nullptr;
	static SymbolTableEntry *eConstant = nullptr;
	static SymbolTableEntry *infConstant = nullptr;
	static SymbolTableEntry *nanConstant = nullptr;
	
	if (!trueConstant)
	{
		trueConstant = new SymbolTableEntry(gStr_T, gStaticScriptValue_LogicalT);
		falseConstant = new SymbolTableEntry(gStr_F, gStaticScriptValue_LogicalF);
		nullConstant = new SymbolTableEntry(gStr_NULL, gStaticScriptValueNULL);
		piConstant = new SymbolTableEntry(gStr_PI, (new ScriptValue_Float(M_PI))->SetExternallyOwned(true));
		eConstant = new SymbolTableEntry(gStr_E, (new ScriptValue_Float(M_E))->SetExternallyOwned(true));
		infConstant = new SymbolTableEntry(gStr_INF, (new ScriptValue_Float(std::numeric_limits<double>::infinity()))->SetExternallyOwned(true));
		nanConstant = new SymbolTableEntry(gStr_NAN, (new ScriptValue_Float(std::numeric_limits<double>::quiet_NaN()))->SetExternallyOwned(true));
	}
	
	if (script_block)
	{
		// Include symbols only if they are used by the script block we are being created to interpret
		if (script_block->contains_T_)
			InitializeConstantSymbolEntry(trueConstant);
		if (script_block->contains_F_)
			InitializeConstantSymbolEntry(falseConstant);
		if (script_block->contains_NULL_)
			InitializeConstantSymbolEntry(nullConstant);
		if (script_block->contains_PI_)
			InitializeConstantSymbolEntry(piConstant);
		if (script_block->contains_E_)
			InitializeConstantSymbolEntry(eConstant);
		if (script_block->contains_INF_)
			InitializeConstantSymbolEntry(infConstant);
		if (script_block->contains_NAN_)
			InitializeConstantSymbolEntry(nanConstant);
	}
	else
	{
		InitializeConstantSymbolEntry(trueConstant);
		InitializeConstantSymbolEntry(falseConstant);
		InitializeConstantSymbolEntry(nullConstant);
		InitializeConstantSymbolEntry(piConstant);
		InitializeConstantSymbolEntry(eConstant);
		InitializeConstantSymbolEntry(infConstant);
		InitializeConstantSymbolEntry(nanConstant);
	}
}

SymbolTable::~SymbolTable(void)
{
	// We delete all values that are not marked as externally owned; those that are externally owned are someone else's problem.
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		SymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		ScriptValue *value = symbol_slot->symbol_value_;
		
		if (!value->ExternallyOwned())
			delete value;
		
		if (!symbol_slot->symbol_name_externally_owned_)
			delete symbol_slot->symbol_name_;
	}
	
	if (symbols_ && (symbols_ != non_malloc_symbols))
	{
		free(symbols_);
		symbols_ = nullptr;
	}
}

std::vector<std::string> SymbolTable::ReadOnlySymbols(void) const
{
	std::vector<std::string> symbol_names;
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		SymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		if (symbol_slot->symbol_is_const_)
			symbol_names.push_back(*symbol_slot->symbol_name_);
	}
	
	return symbol_names;
}

std::vector<std::string> SymbolTable::ReadWriteSymbols(void) const
{
	std::vector<std::string> symbol_names;
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		SymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		if (!symbol_slot->symbol_is_const_)
			symbol_names.push_back(*symbol_slot->symbol_name_);
	}
	
	return symbol_names;
}

ScriptValue *SymbolTable::GetValueForSymbol(const std::string &p_symbol_name) const
{
	int key_length = (int)p_symbol_name.length();
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		SymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			if (symbol_slot->symbol_name_->compare(p_symbol_name) == 0)
				return symbol_slot->symbol_value_;
		}
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	SLIM_TERMINATION << "ERROR (SymbolTable::GetValueForSymbol): undefined identifier " << p_symbol_name << "." << slim_terminate();
	return nullptr;
}

ScriptValue *SymbolTable::GetValueOrNullForSymbol(const std::string &p_symbol_name) const
{
	int key_length = (int)p_symbol_name.length();
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		SymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			if (symbol_slot->symbol_name_->compare(p_symbol_name) == 0)
				return symbol_slot->symbol_value_;
		}
	}
	
	return nullptr;
}

// does a fast search for the slot matching the search key; returns -1 if no match is found
int SymbolTable::_SlotIndexForSymbol(const std::string &p_symbol_name, int p_key_length)
{
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		SymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == p_key_length)
		{
			if (symbol_slot->symbol_name_->compare(p_symbol_name) == 0)
				return symbol_index;
		}
	}
	
	return -1;
}

// allocates a new slot and returns its index; it is assumed that the slot will actually be used, so symbol_count_ is incremented
int SymbolTable::_AllocateNewSlot(void)
{
	if (symbol_count_ == symbol_capacity_)
	{
		int new_symbol_capacity = symbol_capacity_ << 1;
		
		if (symbols_ == non_malloc_symbols)
		{
			// We have been living off our base buffer, so we need to malloc for the first time
			symbols_ = (SymbolTableSlot *)malloc(sizeof(SymbolTableSlot) * new_symbol_capacity);
			
			memcpy(symbols_, non_malloc_symbols, sizeof(SymbolTableSlot) * symbol_capacity_);
		}
		else
		{
			// We already have a malloced buffer, so we just need to realloc
			symbols_ = (SymbolTableSlot *)realloc(symbols_, sizeof(SymbolTableSlot) * new_symbol_capacity);
		}
		
		symbol_capacity_ = new_symbol_capacity;
	}
	
	return symbol_count_++;
}

void SymbolTable::SetValueForSymbol(const std::string &p_symbol_name, ScriptValue *p_value)
{
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if ((symbol_slot >= 0) && symbols_[symbol_slot].symbol_is_const_)
		SLIM_TERMINATION << "ERROR (SymbolTable::SetValueForSymbol): Identifier '" << p_symbol_name << "' is a constant." << slim_terminate();
	
	// get a version of the value that is suitable for insertion into the symbol table
	if (p_value->InSymbolTable())
	{
		// If it's already in a symbol table, then we need to copy it, to avoid two references to the same ScriptValue.
		// This is because we don't refcount; if a ScriptValue were shared, it would be freed when the first reference was deleted.
		// This is not a concern for externally owned ScriptValues, since we don't free them anyway, so we don't need to copy them.
		if (!p_value->ExternallyOwned())
			p_value = p_value->CopyValues();
	}
	else if (p_value->Invisible())
	{
		// if it's invisible, then we need to copy it, since the original needs to stay invisible to make sure it displays correctly
		p_value = p_value->CopyValues();
	}
	
	// we set the symbol table flag so the pointer won't be deleted or reused by anybody else
	p_value->SetInSymbolTable(true);
	
	// and now set the value in the symbol table
	if (symbol_slot == -1)
	{
		symbol_slot = _AllocateNewSlot();
		
		SymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
		
		new_symbol_slot_ptr->symbol_name_ = new std::string(p_symbol_name);
		new_symbol_slot_ptr->symbol_name_length_ = key_length;
		new_symbol_slot_ptr->symbol_value_ = p_value;
		new_symbol_slot_ptr->symbol_is_const_ = false;
		new_symbol_slot_ptr->symbol_name_externally_owned_ = false;
	}
	else
	{
		SymbolTableSlot *existing_symbol_slot_ptr = symbols_ + symbol_slot;
		ScriptValue *existing_value = existing_symbol_slot_ptr->symbol_value_;
		
		// We replace the existing symbol value, of course.  Everything else gets inherited, since we're replacing the value in an existing slot;
		// we can continue using the same symbol name, name length, constness (since that is guaranteed to be false here), etc.
		if (!existing_value->ExternallyOwned())
			delete existing_value;
		
		existing_symbol_slot_ptr->symbol_value_ = p_value;
	}
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void SymbolTable::SetConstantForSymbol(const std::string &p_symbol_name, ScriptValue *p_value)
{
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if (symbol_slot >= 0)
	{
		// can't already be defined as either a constant or a variable; if you want to define a constant, you have to get there first
		if (symbols_[symbol_slot].symbol_is_const_)
			SLIM_TERMINATION << "ERROR (SymbolTable::SetConstantForSymbol): Identifier '" << p_symbol_name << "' is already a constant." << slim_terminate();
		else
			SLIM_TERMINATION << "ERROR (SymbolTable::SetConstantForSymbol): Identifier '" << p_symbol_name << "' is already a variable." << slim_terminate();
	}
	
	// get a version of the value that is suitable for insertion into the symbol table
	if (p_value->InSymbolTable())
	{
		// If it's already in a symbol table, then we need to copy it, to avoid two references to the same ScriptValue.
		// This is because we don't refcount; if a ScriptValue were shared, it would be freed when the first reference was deleted.
		// This is not a concern for externally owned ScriptValues, since we don't free them anyway, so we don't need to copy them.
		if (!p_value->ExternallyOwned())
			p_value = p_value->CopyValues();
	}
	else if (p_value->Invisible())
	{
		// if it's invisible, then we need to copy it, since the original needs to stay invisible to make sure it displays correctly
		p_value = p_value->CopyValues();
	}
	
	// we set the symbol table flag so the pointer won't be deleted or reused by anybody else
	p_value->SetInSymbolTable(true);
	
	// and now set the value in the symbol table; we know, from the check above, that we're in a new slot
	symbol_slot = _AllocateNewSlot();
	
	SymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = new std::string(p_symbol_name);
	new_symbol_slot_ptr->symbol_name_length_ = key_length;
	new_symbol_slot_ptr->symbol_value_ = p_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = false;
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void SymbolTable::RemoveValueForSymbol(const std::string &p_symbol_name, bool remove_constant)
{
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if (symbol_slot >= 0)
	{
		SymbolTableSlot *symbol_slot_ptr = symbols_ + symbol_slot;
		ScriptValue *value = symbol_slot_ptr->symbol_value_;
		
		if (symbol_slot_ptr->symbol_is_const_ && !remove_constant)
			SLIM_TERMINATION << "ERROR (SymbolTable::RemoveValueForSymbol): Identifier '" << p_symbol_name << "' is a constant and thus cannot be removed." << slim_terminate();
		
		if (!value->ExternallyOwned())
			delete value;
		
		if (!symbol_slot_ptr->symbol_name_externally_owned_)
			delete symbol_slot_ptr->symbol_name_;
		
		// delete the slot and free the name string; if we're removing the last slot, that's all we have to do
		--symbol_count_;
		
		// if we're removing an interior value, we can just replace this slot with the last slot, since we don't sort our entries
		if (symbol_slot != symbol_count_)
			*symbol_slot_ptr = symbols_[symbol_count_];
	}
}

void SymbolTable::InitializeConstantSymbolEntry(SymbolTableEntry *p_new_entry)
{
	const std::string &entry_name = p_new_entry->first;
	ScriptValue *entry_value = p_new_entry->second;
	
	if (!entry_value->ExternallyOwned() || !entry_value->InSymbolTable() || entry_value->Invisible())
		SLIM_TERMINATION << "ERROR (SymbolTable::ReplaceConstantSymbolEntry): (internal error) this method should be called only for externally-owned, non-invisible objects that are already marked as belonging to a symbol table." << slim_terminate();
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	int symbol_slot = _AllocateNewSlot();
	
	SymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = &entry_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot_ptr->symbol_name_length_ = (int)entry_name.length();
	new_symbol_slot_ptr->symbol_value_ = entry_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
}

void SymbolTable::InitializeConstantSymbolEntry(const std::string &p_symbol_name, ScriptValue *p_value)
{
	if (!p_value->ExternallyOwned() || !p_value->InSymbolTable() || p_value->Invisible())
		SLIM_TERMINATION << "ERROR (SymbolTable::ReplaceConstantSymbolEntry): (internal error) this method should be called only for externally-owned, non-invisible objects that are already marked as belonging to a symbol table." << slim_terminate();
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	int symbol_slot = _AllocateNewSlot();
	
	SymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = &p_symbol_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot_ptr->symbol_name_length_ = (int)p_symbol_name.length();
	new_symbol_slot_ptr->symbol_value_ = p_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
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
			if (first_value->IsTemporary()) delete first_value;
		}
		
		if (symbol_value->IsTemporary()) delete symbol_value;
	}
	
	return p_outstream;
}




































































