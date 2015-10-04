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
using std::istream;
using std::ostream;


//
//	EidosSymbolTable
//
#pragma mark EidosSymbolTable

EidosSymbolTable::EidosSymbolTable(bool p_constants_table, EidosSymbolTable *p_parent_table, bool p_start_with_hash)
{
	// Set up the symbol table itself
	is_constant_table_ = p_constants_table;
	using_internal_symbols_ = !p_start_with_hash;
	internal_symbol_count_ = 0;
	
	if (!p_parent_table)
	{
		// If no parent table is given, then we construct a base table for Eidos containing the standard constants.
		// We statically allocate our base symbols for fast setup / teardown.
#ifdef DEBUG
		if (!is_constant_table_)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::EidosSymbolTable): (internal error) a symbol table with no parent table that is not constant does not make sense." << eidos_terminate(nullptr);
#endif
		
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
			piConstant = new EidosSymbolTableEntry(gEidosStr_PI, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(M_PI)));
			eConstant = new EidosSymbolTableEntry(gEidosStr_E, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(M_E)));
			infConstant = new EidosSymbolTableEntry(gEidosStr_INF, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::infinity())));
			nanConstant = new EidosSymbolTableEntry(gEidosStr_NAN, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::quiet_NaN())));
		}
		
		// We can use InitializeConstantSymbolEntry() here since we obey its requirements (see header)
		InitializeConstantSymbolEntry(*nanConstant);
		InitializeConstantSymbolEntry(*infConstant);
		InitializeConstantSymbolEntry(*piConstant);
		InitializeConstantSymbolEntry(*eConstant);
		InitializeConstantSymbolEntry(*nullConstant);
		InitializeConstantSymbolEntry(*falseConstant);
		InitializeConstantSymbolEntry(*trueConstant);
	}
	else
	{
		// If a parent table is given, we adopt it and do not add Eidos constants; they will be in the parent chain
		parent_symbol_table_ = p_parent_table;
		
#ifdef DEBUG
		if (!parent_symbol_table_->is_constant_table_)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::EidosSymbolTable): (internal error) parent symbol tables must be constant in the current design." << eidos_terminate(nullptr);
#endif
	}
}

EidosSymbolTable::~EidosSymbolTable(void)
{
	if (using_internal_symbols_)
	{
		for (size_t symbol_index = 0; symbol_index < internal_symbol_count_; ++symbol_index)
		{
			EidosSymbolTable_InternalSlot *symbol_slot = internal_symbols_ + symbol_index;
			
			// free symbol names that we own
			if (!symbol_slot->symbol_name_externally_owned_)
				delete symbol_slot->symbol_name_;
		}
	}
}

std::vector<std::string> EidosSymbolTable::_SymbolNames(bool p_include_constants, bool p_include_variables) const
{
	std::vector<std::string> symbol_names;
	
	// start with the symbols from our parent symbol table
	if (parent_symbol_table_)
		symbol_names = parent_symbol_table_->_SymbolNames(p_include_constants, p_include_variables);
	
	if ((p_include_constants && is_constant_table_) || (p_include_variables && !is_constant_table_))
	{
		if (using_internal_symbols_)
		{
			for (size_t symbol_index = 0; symbol_index < internal_symbol_count_; ++symbol_index)
				symbol_names.push_back(*internal_symbols_[symbol_index].symbol_name_);
		}
		else
		{
			for (auto symbol_slot_iter = hash_symbols_.begin(); symbol_slot_iter != hash_symbols_.end(); ++symbol_slot_iter)
				symbol_names.push_back(symbol_slot_iter->first);
		}
	}
	
	return symbol_names;
}

bool EidosSymbolTable::ContainsSymbol(const std::string &p_symbol_name) const
{
	if (using_internal_symbols_)
	{
		int key_length = (int)p_symbol_name.size();
		const char *symbol_name_data = p_symbol_name.data();
		
		// This is the same logic as _SlotIndexForSymbol, but it is repeated here for speed; getting values should be super fast
		for (int symbol_index = (int)internal_symbol_count_ - 1; symbol_index >= 0; --symbol_index)
		{
			const EidosSymbolTable_InternalSlot *symbol_slot = internal_symbols_ + symbol_index;
			
			// use the length of the string to make the scan fast; only compare for equal-length strings
			if (symbol_slot->symbol_name_length_ == key_length)
			{
				// The logic here is equivalent to symbol_slot->symbol_name_->compare(p_symbol_name), but runs much faster
				const char *slot_name_data = symbol_slot->symbol_name_data_;
				int char_index;
				
				for (char_index = 0; char_index < key_length; ++char_index)
					if (slot_name_data[char_index] != symbol_name_data[char_index])
						break;
				
				if (char_index == key_length)
					return true;
			}
		}
	}
	else
	{
		auto symbol_slot_iter = hash_symbols_.find(p_symbol_name);
		
		if (symbol_slot_iter != hash_symbols_.end())
			return true;
	}
	
	// We didn't get a hit, so try our parent table
	if (parent_symbol_table_)
		return parent_symbol_table_->ContainsSymbol(p_symbol_name);
	
	return false;
}

EidosValue_SP EidosSymbolTable::_GetValue(const std::string &p_symbol_name, const EidosToken *p_symbol_token) const
{
	if (using_internal_symbols_)
	{
		int key_length = (int)p_symbol_name.size();
		const char *symbol_name_data = p_symbol_name.data();
		
		// This is the same logic as _SlotIndexForSymbol, but it is repeated here for speed; getting values should be super fast
		for (int symbol_index = (int)internal_symbol_count_ - 1; symbol_index >= 0; --symbol_index)
		{
			const EidosSymbolTable_InternalSlot *symbol_slot = internal_symbols_ + symbol_index;
			
			// use the length of the string to make the scan fast; only compare for equal-length strings
			if (symbol_slot->symbol_name_length_ == key_length)
			{
				// The logic here is equivalent to symbol_slot->symbol_name_->compare(p_symbol_name), but runs much faster
				const char *slot_name_data = symbol_slot->symbol_name_data_;
				int char_index;
				
				for (char_index = 0; char_index < key_length; ++char_index)
					if (slot_name_data[char_index] != symbol_name_data[char_index])
						break;
				
				if (char_index == key_length)
				{
					last_get_was_const_ = is_constant_table_;
					return symbol_slot->symbol_value_SP_;
				}
			}
		}
	}
	else
	{
		auto symbol_slot_iter = hash_symbols_.find(p_symbol_name);
		
		if (symbol_slot_iter != hash_symbols_.end())
		{
			last_get_was_const_ = is_constant_table_;
			return symbol_slot_iter->second;
		}
	}
	
	// We didn't get a hit, so try our parent table
	if (parent_symbol_table_)
	{
		EidosValue_SP parent_result = parent_symbol_table_->_GetValue(p_symbol_name, p_symbol_token);
		
		if (parent_result)
			return parent_result;	// the parent should have set last_get_was_const_ already
	}
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_GetValue): undefined identifier " << p_symbol_name << "." << eidos_terminate(p_symbol_token);
}

// does a fast search for the slot matching the search key; returns -1 if no match is found
int EidosSymbolTable::_SlotIndexForSymbol(int p_symbol_name_length, const char *p_symbol_name_data) const
{
	// search through the symbol table in reverse order, most-recently-defined symbols first
	for (int symbol_index = (int)internal_symbol_count_ - 1; symbol_index >= 0; --symbol_index)
	{
		const EidosSymbolTable_InternalSlot *symbol_slot = internal_symbols_ + symbol_index;
		
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

void EidosSymbolTable::_SwitchToHash(void)
{
	if (using_internal_symbols_)
	{
		for (size_t symbol_index = 0; symbol_index < internal_symbol_count_; ++symbol_index)
		{
			EidosSymbolTable_InternalSlot &old_symbol_slot = internal_symbols_[symbol_index];
			
			// if we own the symbol name, we can move it to the new hash table entry, otherwise a copy is necessary
			if (old_symbol_slot.symbol_name_externally_owned_)
				hash_symbols_.insert(std::pair<std::string, EidosValue_SP>(*old_symbol_slot.symbol_name_, std::move(old_symbol_slot.symbol_value_SP_)));
			else
				hash_symbols_.insert(std::pair<std::string, EidosValue_SP>(std::move(*old_symbol_slot.symbol_name_), std::move(old_symbol_slot.symbol_value_SP_)));
			
			// clean up the built-in slot; probably mostly unnecessary, but prevents hard-to-find bugs
			old_symbol_slot.symbol_value_SP_.reset();
			old_symbol_slot.symbol_name_ = nullptr;
			old_symbol_slot.symbol_name_data_ = nullptr;
		}
		
		using_internal_symbols_ = false;
		internal_symbol_count_ = 0;
	}
}

void EidosSymbolTable::SetValueForSymbol(const std::string &p_symbol_name, EidosValue_SP p_value)
{
	// If it's invisible then we copy it, since the symbol table never stores invisible values
	if (p_value->Invisible())
		p_value = p_value->CopyValues();
	
	if (using_internal_symbols_)
	{
		int key_length = (int)p_symbol_name.size();
		const char *symbol_name_data = p_symbol_name.data();
		int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
		
		if (symbol_slot == -1)
		{
			// The symbol is not already defined in this table.  Before we can define it, we need to check that it is not defined in a parent table.
			// At present, we assume that if it is defined in a parent table it is a constant, which is true for now.
			if (parent_symbol_table_ && parent_symbol_table_->ContainsSymbol(p_symbol_name))
				EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_SetValue): identifier '" << p_symbol_name << "' cannot be redefined because it is a constant." << eidos_terminate(nullptr);
			
			if (internal_symbol_count_ < EIDOS_SYMBOL_TABLE_BASE_SIZE)
			{
				EidosSymbolTable_InternalSlot *new_symbol_slot_ptr = internal_symbols_ + internal_symbol_count_;
				std::string *string_copy = new std::string(p_symbol_name);
				const char *string_copy_data_ = string_copy->data();
				
				new_symbol_slot_ptr->symbol_value_SP_ = std::move(p_value);
				new_symbol_slot_ptr->symbol_name_ = string_copy;
				new_symbol_slot_ptr->symbol_name_data_ = string_copy_data_;
				new_symbol_slot_ptr->symbol_name_length_ = (uint16_t)key_length;
				new_symbol_slot_ptr->symbol_name_externally_owned_ = false;
				
				++internal_symbol_count_;
				return;
			}
			
			_SwitchToHash();
		}
		else
		{
			// The symbol is already defined in this table, so it can be redefined (illegal cases were caught above already).
			// We replace the existing symbol value, of course.  Everything else gets inherited, since we're replacing the value in an existing slot;
			// we can continue using the same symbol name, name length, constness (since that is guaranteed to be false here), etc.
			internal_symbols_[symbol_slot].symbol_value_SP_ = std::move(p_value);
			return;
		}
	}
	
	// fall-through to the hash table case
	auto existing_symbol_slot_iter = hash_symbols_.find(p_symbol_name);
	
	if (existing_symbol_slot_iter == hash_symbols_.end())
	{
		// The symbol is not already defined in this table.  Before we can define it, we need to check that it is not defined in a parent table.
		// At present, we assume that if it is defined in a parent table it is a constant, which is true for now.
		if (parent_symbol_table_ && parent_symbol_table_->ContainsSymbol(p_symbol_name))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_SetValue): identifier '" << p_symbol_name << "' cannot be redefined because it is a constant." << eidos_terminate(nullptr);
		
		hash_symbols_.insert(std::pair<std::string, EidosValue_SP>(p_symbol_name, std::move(p_value)));
	}
	else
	{
		existing_symbol_slot_iter->second = std::move(p_value);
	}
}

void EidosSymbolTable::_RemoveSymbol(const std::string &p_symbol_name, bool p_remove_constant)
{
	if (using_internal_symbols_)
	{
		int key_length = (int)p_symbol_name.size();
		const char *symbol_name_data = p_symbol_name.data();
		int symbol_slot = _SlotIndexForSymbol(key_length, symbol_name_data);
		
		if (symbol_slot >= 0)
		{
			if (is_constant_table_ && !p_remove_constant)
				EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_RemoveSymbol): identifier '" << p_symbol_name << "' is a constant and thus cannot be removed." << eidos_terminate(nullptr);
			
			EidosSymbolTable_InternalSlot *existing_symbol_slot_ptr = internal_symbols_ + symbol_slot;
			
			// clean up the slot being removed
			existing_symbol_slot_ptr->symbol_value_SP_.reset();
			
			if (!existing_symbol_slot_ptr->symbol_name_externally_owned_)
				delete existing_symbol_slot_ptr->symbol_name_;
			existing_symbol_slot_ptr->symbol_name_ = nullptr;
			existing_symbol_slot_ptr->symbol_name_data_ = nullptr;
			
			// remove the slot from the vector
			EidosSymbolTable_InternalSlot *backfill_symbol_slot_ptr = internal_symbols_ + (--internal_symbol_count_);
			
			if (existing_symbol_slot_ptr != backfill_symbol_slot_ptr)
			{
				*existing_symbol_slot_ptr = std::move(*backfill_symbol_slot_ptr);
				
				// clean up the backfill slot; probably unnecessary, but prevents hard-to-find bugs
				backfill_symbol_slot_ptr->symbol_value_SP_.reset();
				backfill_symbol_slot_ptr->symbol_name_ = nullptr;
				backfill_symbol_slot_ptr->symbol_name_data_ = nullptr;
			}
		}
		else
		{
			// If it wasn't defined in us, then it might be defined in a parent
			if (parent_symbol_table_)
				parent_symbol_table_->_RemoveSymbol(p_symbol_name, p_remove_constant);
		}
	}
	else
	{
		auto remove_iter = hash_symbols_.find(p_symbol_name);
		
		if (remove_iter != hash_symbols_.end())
		{
			// We found the symbol in ourselves, so remove it unless we are a constant table
			if (is_constant_table_ && !p_remove_constant)
				EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_RemoveSymbol): identifier '" << p_symbol_name << "' is a constant and thus cannot be removed." << eidos_terminate(nullptr);
			
			hash_symbols_.erase(remove_iter);
		}
		else
		{
			// If it wasn't defined in us, then it might be defined in a parent
			if (parent_symbol_table_)
				parent_symbol_table_->_RemoveSymbol(p_symbol_name, p_remove_constant);
		}
	}
}

void EidosSymbolTable::_InitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue_SP p_value)
{
#ifdef DEBUG
	if (p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_InitializeConstantSymbolEntry): (internal error) this method should be called only for non-invisible objects." << eidos_terminate(nullptr);
	if (!is_constant_table_)
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_InitializeConstantSymbolEntry): (internal error) this method should be called only on constant symbol tables." << eidos_terminate(nullptr);
#endif
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	if (using_internal_symbols_)
	{
		if (internal_symbol_count_ < EIDOS_SYMBOL_TABLE_BASE_SIZE)
		{
			EidosSymbolTable_InternalSlot *new_symbol_slot_ptr = internal_symbols_ + internal_symbol_count_;
			
			new_symbol_slot_ptr->symbol_value_SP_ = std::move(p_value);
			new_symbol_slot_ptr->symbol_name_ = &p_symbol_name;		// take a pointer to the external object, which must live longer than us!
			new_symbol_slot_ptr->symbol_name_length_ = (uint16_t)p_symbol_name.size();
			new_symbol_slot_ptr->symbol_name_data_ = p_symbol_name.data();
			new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
			
			++internal_symbol_count_;
			return;
		}
		
		_SwitchToHash();
	}
	
	// fall-through to the hash table case
	hash_symbols_.insert(std::pair<std::string, EidosValue_SP>(p_symbol_name, std::move(p_value)));
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




































































