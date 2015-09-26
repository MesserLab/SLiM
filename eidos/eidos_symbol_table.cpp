//
//  eidos_symbol_table.cpp
//  Eidos
//
//  Created by Ben Haller on 4/12/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include "eidos_symbol_table.h"
#include "eidos_value.h"
#include "eidos_global.h"
#include "eidos_token.h"
#include "math.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


//
//	EidosSymbolTable
//
#pragma mark EidosSymbolTable

EidosSymbolTable::EidosSymbolTable(EidosSymbolUsageParamBlock *p_symbol_usage)
{
	// We statically allocate our base symbols for fast setup / teardown
	static EidosSymbolTableEntry *trueConstant = nullptr;
	static EidosSymbolTableEntry *falseConstant = nullptr;
	static EidosSymbolTableEntry *nullConstant = nullptr;
	static EidosSymbolTableEntry *piConstant = nullptr;
	static EidosSymbolTableEntry *eConstant = nullptr;
	static EidosSymbolTableEntry *infConstant = nullptr;
	static EidosSymbolTableEntry *nanConstant = nullptr;
	
	if (!trueConstant)
	{
		trueConstant = new EidosSymbolTableEntry(gEidosStr_T, gStaticEidosValue_LogicalT);
		falseConstant = new EidosSymbolTableEntry(gEidosStr_F, gStaticEidosValue_LogicalF);
		nullConstant = new EidosSymbolTableEntry(gEidosStr_NULL, gStaticEidosValueNULL);
		piConstant = new EidosSymbolTableEntry(gEidosStr_PI, EidosValue_SP(new EidosValue_Float_singleton(M_PI)));
		eConstant = new EidosSymbolTableEntry(gEidosStr_E, EidosValue_SP(new EidosValue_Float_singleton(M_E)));
		infConstant = new EidosSymbolTableEntry(gEidosStr_INF, EidosValue_SP(new EidosValue_Float_singleton(std::numeric_limits<double>::infinity())));
		nanConstant = new EidosSymbolTableEntry(gEidosStr_NAN, EidosValue_SP(new EidosValue_Float_singleton(std::numeric_limits<double>::quiet_NaN())));
	}
	
	// We can use InitializeConstantSymbolEntry() here since we obey its requirements (see header)
	if (p_symbol_usage)
	{
		// Include symbols only if they are used by the script we are being created to interpret.
		// Eidos Contexts can check for symbol usage if they wish, for maximal construct/destruct speed.
		// Symbols are defined here from least likely to most likely to be used (from guessing, not metrics),
		// to optimize the symbol table search time; the table is search from last added to first added.
		if (p_symbol_usage->contains_NAN_)
			InitializeConstantSymbolEntry(nanConstant);
		if (p_symbol_usage->contains_INF_)
			InitializeConstantSymbolEntry(infConstant);
		if (p_symbol_usage->contains_PI_)
			InitializeConstantSymbolEntry(piConstant);
		if (p_symbol_usage->contains_E_)
			InitializeConstantSymbolEntry(eConstant);
		if (p_symbol_usage->contains_NULL_)
			InitializeConstantSymbolEntry(nullConstant);
		if (p_symbol_usage->contains_F_)
			InitializeConstantSymbolEntry(falseConstant);
		if (p_symbol_usage->contains_T_)
			InitializeConstantSymbolEntry(trueConstant);
	}
	else
	{
		InitializeConstantSymbolEntry(nanConstant);
		InitializeConstantSymbolEntry(infConstant);
		InitializeConstantSymbolEntry(piConstant);
		InitializeConstantSymbolEntry(eConstant);
		InitializeConstantSymbolEntry(nullConstant);
		InitializeConstantSymbolEntry(falseConstant);
		InitializeConstantSymbolEntry(trueConstant);
	}
}

EidosSymbolTable::~EidosSymbolTable(void)
{
	EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	for (size_t symbol_index = 0; symbol_index < symbol_count; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		// free symbol names that we own
		if (!symbol_slot->symbol_name_externally_owned_)
			delete symbol_slot->symbol_name_;
	}
}

std::vector<std::string> EidosSymbolTable::ReadOnlySymbols(void) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	std::vector<std::string> symbol_names;
	
	for (size_t symbol_index = 0; symbol_index < symbol_count; ++symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		if (symbol_slot->symbol_is_const_)
			symbol_names.push_back(*symbol_slot->symbol_name_);
	}
	
	return symbol_names;
}

std::vector<std::string> EidosSymbolTable::ReadWriteSymbols(void) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	std::vector<std::string> symbol_names;
	
	for (size_t symbol_index = 0; symbol_index < symbol_count; ++symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		if (!symbol_slot->symbol_is_const_)
			symbol_names.push_back(*symbol_slot->symbol_name_);
	}
	
	return symbol_names;
}

EidosValue_SP EidosSymbolTable::GetValueOrRaiseForToken(const EidosToken *p_symbol_token) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	const std::string &symbol_name = p_symbol_token->token_string_;
	int key_length = (int)symbol_name.size();
	const char *symbol_name_data = symbol_name.data();
	
	// This is the same logic as _SlotIndexForSymbol, but it is repeated here for speed; getting values should be super fast
	for (int symbol_index = (int)symbol_count - 1; symbol_index >= 0; --symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		// use the length of the string to make the scan fast; only compare for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			// The logic here is equivalent to symbol_slot->symbol_name_->compare(symbol_name), but runs much faster
			const char *slot_name_data = symbol_slot->symbol_name_data_;
			int char_index;
			
			for (char_index = 0; char_index < key_length; ++char_index)
				if (slot_name_data[char_index] != symbol_name_data[char_index])
					break;
			
			if (char_index == key_length)
				return symbol_slot->symbol_value_SP_;
		}
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::GetValueOrRaiseForToken): undefined identifier " << symbol_name << "." << eidos_terminate(p_symbol_token);
}

EidosValue_SP EidosSymbolTable::GetNonConstantValueOrRaiseForToken(const EidosToken *p_symbol_token) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	const std::string &symbol_name = p_symbol_token->token_string_;
	int key_length = (int)symbol_name.size();
	const char *symbol_name_data = symbol_name.data();
	
	// This is the same logic as _SlotIndexForSymbol, but it is repeated here for speed; getting values should be super fast
	for (int symbol_index = (int)symbol_count - 1; symbol_index >= 0; --symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		// use the length of the string to make the scan fast; only compare for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			// The logic here is equivalent to symbol_slot->symbol_name_->compare(symbol_name), but runs much faster
			const char *slot_name_data = symbol_slot->symbol_name_data_;
			int char_index;
			
			for (char_index = 0; char_index < key_length; ++char_index)
				if (slot_name_data[char_index] != symbol_name_data[char_index])
					break;
			
			if (char_index == key_length)
			{
				if (symbol_slot->symbol_is_const_)
					EIDOS_TERMINATION << "ERROR (EidosSymbolTable::GetNonConstantValueOrRaiseForToken): identifier " << symbol_name << " is a constant." << eidos_terminate(p_symbol_token);
				
				return symbol_slot->symbol_value_SP_;
			}
		}
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::GetValueOrRaiseForToken): undefined identifier " << symbol_name << "." << eidos_terminate(p_symbol_token);
}

EidosValue_SP EidosSymbolTable::GetValueOrRaiseForSymbol(const std::string &p_symbol_name) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	int key_length = (int)p_symbol_name.size();
	const char *symbol_name_data = p_symbol_name.data();
	
	// This is the same logic as _SlotIndexForSymbol, but it is repeated here for speed; getting values should be super fast
	for (int symbol_index = (int)symbol_count - 1; symbol_index >= 0; --symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			// The logic here is equivalent to symbol_slot->symbol_name_->compare(p_symbol_name), but runs much faster
			const char *slot_name_data = symbol_slot->symbol_name_data_;
			int char_index;
			
			for (char_index = 0; char_index < key_length; ++char_index)
				if (slot_name_data[char_index] != symbol_name_data[char_index])
					break;
			
			if (char_index == key_length)
				return symbol_slot->symbol_value_SP_;
		}
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::GetValueOrRaiseForSymbol): undefined identifier " << p_symbol_name << "." << eidos_terminate(nullptr);
}

EidosValue_SP EidosSymbolTable::GetValueOrNullForSymbol(const std::string &p_symbol_name) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	int key_length = (int)p_symbol_name.size();
	const char *symbol_name_data = p_symbol_name.data();
	
	// This is the same logic as _SlotIndexForSymbol, but it is repeated here for speed; getting values should be super fast
	for (int symbol_index = (int)symbol_count - 1; symbol_index >= 0; --symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			// The logic here is equivalent to symbol_slot->symbol_name_->compare(p_symbol_name), but runs much faster
			const char *slot_name_data = symbol_slot->symbol_name_data_;
			int char_index;
			
			for (char_index = 0; char_index < key_length; ++char_index)
				if (slot_name_data[char_index] != symbol_name_data[char_index])
					break;
			
			if (char_index == key_length)
				return symbol_slot->symbol_value_SP_;
		}
	}
	
	return EidosValue_SP(nullptr);
}

// does a fast search for the slot matching the search key; returns -1 if no match is found
int EidosSymbolTable::_SlotIndexForSymbol(int p_symbol_name_length, const char *p_symbol_name_data) const
{
	const EidosSymbolTableSlot *symbols = symbol_vec.data();
	size_t symbol_count = symbol_vec.size();
	
	// search through the symbol table in reverse order, most-recently-defined symbols first
	for (int symbol_index = (int)symbol_count - 1; symbol_index >= 0; --symbol_index)
	{
		const EidosSymbolTableSlot *symbol_slot = symbols + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == p_symbol_name_length)
		{
			// The logic here is equivalent to symbol_slot->symbol_name_->compare(p_symbol_name), but runs much faster
			// since most symbols will fail the comparison on the first character, avoiding the function call and setup
			const char *slot_name_data = symbol_slot->symbol_name_data_;
			int char_index;
			
			for (char_index = 0; char_index < p_symbol_name_length; ++char_index)
				if (slot_name_data[char_index] != p_symbol_name_data[char_index])
					break;
			
			if (char_index == p_symbol_name_length)
				return symbol_index;
		}
	}
	
	return -1;
}

void EidosSymbolTable::SetValueForSymbol(const std::string &p_symbol_name, EidosValue_SP p_value)
{
	int key_length = (int)p_symbol_name.size();
	const char *symbol_name_data = p_symbol_name.data();
	int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
	
	// if it's invisible then we set it to visible, since the symbol table never stores invisible values
	// this is safe since the result of assignments in Eidos is NULL, not the value set
	if (p_value->Invisible())
		p_value = p_value->CopyValues();
	
	// and now set the value in the symbol table
	if (symbol_slot == -1)
	{
		EidosSymbolTableSlot new_symbol_slot;
		
		new_symbol_slot.symbol_value_SP_ = std::move(p_value);
		new_symbol_slot.symbol_name_ = new std::string(p_symbol_name);
		new_symbol_slot.symbol_name_data_ = new_symbol_slot.symbol_name_->data();
		new_symbol_slot.symbol_name_length_ = key_length;
		new_symbol_slot.symbol_name_externally_owned_ = false;
		new_symbol_slot.symbol_is_const_ = false;
		
		symbol_vec.push_back(std::move(new_symbol_slot));
	}
	else
	{
		EidosSymbolTableSlot *existing_symbol_slot_ptr = &(symbol_vec[symbol_slot]);
		
		if ((symbol_slot >= 0) && existing_symbol_slot_ptr->symbol_is_const_)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetValueForSymbol): identifier '" << p_symbol_name << "' is a constant." << eidos_terminate(nullptr);
		
		// We replace the existing symbol value, of course.  Everything else gets inherited, since we're replacing the value in an existing slot;
		// we can continue using the same symbol name, name length, constness (since that is guaranteed to be false here), etc.
		existing_symbol_slot_ptr->symbol_value_SP_ = std::move(p_value);
	}
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void EidosSymbolTable::SetConstantForSymbol(const std::string &p_symbol_name, EidosValue_SP p_value)
{
	int key_length = (int)p_symbol_name.size();
	const char *symbol_name_data = p_symbol_name.data();
	int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
	
	if (symbol_slot >= 0)
	{
		// can't already be defined as either a constant or a variable; if you want to define a constant, you have to get there first
		if (symbol_vec[symbol_slot].symbol_is_const_)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetConstantForSymbol): (internal error) identifier '" << p_symbol_name << "' is already a constant." << eidos_terminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetConstantForSymbol): (internal error) identifier '" << p_symbol_name << "' is already a variable." << eidos_terminate(nullptr);
	}
	
	// if it's invisible then we set it to visible, since the symbol table never stores invisible values
	// this is safe since the result of assignments in Eidos is NULL, not the value set
	if (p_value->Invisible())
		p_value = p_value->CopyValues();
	
	// and now set the value in the symbol table; we know, from the check above, that we're in a new slot
	EidosSymbolTableSlot new_symbol_slot;
	
	new_symbol_slot.symbol_value_SP_ = std::move(p_value);
	new_symbol_slot.symbol_name_ = new std::string(p_symbol_name);
	new_symbol_slot.symbol_name_length_ = key_length;
	new_symbol_slot.symbol_name_data_ = new_symbol_slot.symbol_name_->data();
	new_symbol_slot.symbol_name_externally_owned_ = false;
	new_symbol_slot.symbol_is_const_ = true;
	
	symbol_vec.push_back(std::move(new_symbol_slot));
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void EidosSymbolTable::RemoveValueForSymbol(const std::string &p_symbol_name, bool p_remove_constant)
{
	int key_length = (int)p_symbol_name.size();
	const char *symbol_name_data = p_symbol_name.data();
	int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
	
	if (symbol_slot >= 0)
	{
		EidosSymbolTableSlot *existing_symbol_slot_ptr = &(symbol_vec[symbol_slot]);
		
		if (existing_symbol_slot_ptr->symbol_is_const_ && !p_remove_constant)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::RemoveValueForSymbol): identifier '" << p_symbol_name << "' is a constant and thus cannot be removed." << eidos_terminate(nullptr);
		
		// free the name string
		if (!existing_symbol_slot_ptr->symbol_name_externally_owned_)
			delete existing_symbol_slot_ptr->symbol_name_;
		
		// remove the slot from the vector
		symbol_vec.erase(symbol_vec.begin() + symbol_slot);
	}
}

void EidosSymbolTable::InitializeConstantSymbolEntry(EidosSymbolTableEntry *p_new_entry)
{
#ifdef DEBUG
	if (p_new_entry->second->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::InitializeConstantSymbolEntry): (internal error) this method should be called only for non-invisible objects." << eidos_terminate(nullptr);
#endif
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	const std::string &entry_name = p_new_entry->first;
	EidosSymbolTableSlot new_symbol_slot;
	
	new_symbol_slot.symbol_value_SP_ = p_new_entry->second;
	new_symbol_slot.symbol_name_ = &entry_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot.symbol_name_length_ = (int)entry_name.size();
	new_symbol_slot.symbol_name_data_ = new_symbol_slot.symbol_name_->data();
	new_symbol_slot.symbol_name_externally_owned_ = true;
	new_symbol_slot.symbol_is_const_ = true;
	
	symbol_vec.push_back(std::move(new_symbol_slot));
}

void EidosSymbolTable::InitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue_SP p_value)
{
#ifdef DEBUG
	if (p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::InitializeConstantSymbolEntry): (internal error) this method should be called only for non-invisible objects." << eidos_terminate(nullptr);
#endif
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	EidosSymbolTableSlot new_symbol_slot;
	
	new_symbol_slot.symbol_value_SP_ = std::move(p_value);
	new_symbol_slot.symbol_name_ = &p_symbol_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot.symbol_name_length_ = (int)p_symbol_name.size();
	new_symbol_slot.symbol_name_data_ = p_symbol_name.data();
	new_symbol_slot.symbol_name_externally_owned_ = true;
	new_symbol_slot.symbol_is_const_ = true;
	
	symbol_vec.push_back(std::move(new_symbol_slot));
}

void EidosSymbolTable::ReinitializeConstantSymbolEntry(EidosSymbolTableEntry *p_new_entry)
{
#ifdef DEBUG
	if (p_new_entry->second->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): (internal error) this method should be called only for external-permanent, non-invisible objects." << eidos_terminate(nullptr);
#endif
	
	// check whether the symbol is already defined; if so, it should be identical or we raise
	const std::string &entry_name = p_new_entry->first;
	int key_length = (int)entry_name.size();
	const char *symbol_name_data = entry_name.data();
	int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
	
	if (symbol_slot >= 0)
	{
		EidosSymbolTableSlot *old_slot = &(symbol_vec[symbol_slot]);
		
		if ((!old_slot->symbol_is_const_) || (!old_slot->symbol_name_externally_owned_) || (old_slot->symbol_value_SP_.get() != p_new_entry->second.get()))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): (internal error) identifier '" << entry_name << "' is already defined, but the existing entry does not match." << eidos_terminate(nullptr);
		
		// a matching slot already exists, so we can just return
		return;
	}
	
	// ok, it is not defined so we need to define it
	EidosSymbolTableSlot new_symbol_slot;
	
	new_symbol_slot.symbol_value_SP_ = p_new_entry->second;
	new_symbol_slot.symbol_name_ = &entry_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot.symbol_name_length_ = (int)entry_name.size();
	new_symbol_slot.symbol_name_data_ = new_symbol_slot.symbol_name_->data();
	new_symbol_slot.symbol_name_externally_owned_ = true;
	new_symbol_slot.symbol_is_const_ = true;
	
	symbol_vec.push_back(std::move(new_symbol_slot));
}

void EidosSymbolTable::ReinitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue_SP p_value)
{
#ifdef DEBUG
	if (p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): (internal error) this method should be called only for external-permanent, non-invisible objects." << eidos_terminate(nullptr);
#endif
	
	// check whether the symbol is already defined; if so, it should be identical or we raise
	int key_length = (int)p_symbol_name.size();
	const char *symbol_name_data = p_symbol_name.data();
	int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
	
	if (symbol_slot >= 0)
	{
		EidosSymbolTableSlot *old_slot = &(symbol_vec[symbol_slot]);
		
		if ((!old_slot->symbol_is_const_) || (!old_slot->symbol_name_externally_owned_) || (old_slot->symbol_value_SP_.get() != p_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): (internal error) identifier '" << p_symbol_name << "' is already defined, but the existing entry does not match." << eidos_terminate(nullptr);
		
		// a matching slot already exists, so we can just return
		return;
	}
	
	// ok, it is not defined so we need to define it
	EidosSymbolTableSlot new_symbol_slot;
	
	new_symbol_slot.symbol_value_SP_ = std::move(p_value);
	new_symbol_slot.symbol_name_ = &p_symbol_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot.symbol_name_length_ = (int)p_symbol_name.size();
	new_symbol_slot.symbol_name_data_ = p_symbol_name.data();
	new_symbol_slot.symbol_name_externally_owned_ = true;
	new_symbol_slot.symbol_is_const_ = true;
	
	symbol_vec.push_back(std::move(new_symbol_slot));
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosSymbolTable &p_symbols)
{
	std::vector<std::string> read_only_symbol_names = p_symbols.ReadOnlySymbols();
	std::vector<std::string> read_write_symbol_names = p_symbols.ReadWriteSymbols();
	std::vector<std::string> symbol_names;
	
	symbol_names.insert(symbol_names.end(), read_only_symbol_names.begin(), read_only_symbol_names.end());
	symbol_names.insert(symbol_names.end(), read_write_symbol_names.begin(), read_write_symbol_names.end());
	std::sort(symbol_names.begin(), symbol_names.end());
	
	for (auto symbol_name_iter = symbol_names.begin(); symbol_name_iter != symbol_names.end(); ++symbol_name_iter)
	{
		const std::string &symbol_name = *symbol_name_iter;
		EidosValue_SP symbol_value = p_symbols.GetValueOrRaiseForSymbol(symbol_name);
		int symbol_count = symbol_value->Count();
		bool is_const = std::find(read_only_symbol_names.begin(), read_only_symbol_names.end(), symbol_name) != read_only_symbol_names.end();
		
		if (symbol_count <= 2)
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *symbol_value << endl;
		else
		{
			EidosValue_SP first_value = symbol_value->GetValueAtIndex(0, nullptr);
			EidosValue_SP second_value = symbol_value->GetValueAtIndex(1, nullptr);
			
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *first_value << " " << *second_value << " ... (" << symbol_count << " values)" << endl;
		}
	}
	
	return p_outstream;
}




































































