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
	// Set up the symbol table itself
	symbol_count_ = 0;
	symbol_capacity_ = EIDOS_SYMBOL_TABLE_BASE_SIZE;
	symbols_ = non_malloc_symbols;
	
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
		trueConstant = new EidosSymbolTableEntry(gStr_T, gStaticEidosValue_LogicalT);
		falseConstant = new EidosSymbolTableEntry(gStr_F, gStaticEidosValue_LogicalF);
		nullConstant = new EidosSymbolTableEntry(gStr_NULL, gStaticEidosValueNULL);
		piConstant = new EidosSymbolTableEntry(gStr_PI, (new EidosValue_Float_singleton_const(M_PI))->SetExternalPermanent());
		eConstant = new EidosSymbolTableEntry(gStr_E, (new EidosValue_Float_singleton_const(M_E))->SetExternalPermanent());
		infConstant = new EidosSymbolTableEntry(gStr_INF, (new EidosValue_Float_singleton_const(std::numeric_limits<double>::infinity()))->SetExternalPermanent());
		nanConstant = new EidosSymbolTableEntry(gStr_NAN, (new EidosValue_Float_singleton_const(std::numeric_limits<double>::quiet_NaN()))->SetExternalPermanent());
	}
	
	// We can use InitializeConstantSymbolEntry() here because we know the objects will live longer than the symbol table, and
	// we know that their values will not change, so we meet the requirements for that method.  Be careful changing this code!
	if (p_symbol_usage)
	{
		// Include symbols only if they are used by the script we are being created to interpret.
		// Eidos Contexts can check for symbol usage if they wish, for maximal construct/destruct speed.
		if (p_symbol_usage->contains_T_)
			InitializeConstantSymbolEntry(trueConstant);
		if (p_symbol_usage->contains_F_)
			InitializeConstantSymbolEntry(falseConstant);
		if (p_symbol_usage->contains_NULL_)
			InitializeConstantSymbolEntry(nullConstant);
		if (p_symbol_usage->contains_PI_)
			InitializeConstantSymbolEntry(piConstant);
		if (p_symbol_usage->contains_E_)
			InitializeConstantSymbolEntry(eConstant);
		if (p_symbol_usage->contains_INF_)
			InitializeConstantSymbolEntry(infConstant);
		if (p_symbol_usage->contains_NAN_)
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

EidosSymbolTable::~EidosSymbolTable(void)
{
	// We delete all values that are not marked as externally owned permanent; those are someone else's problem.
	// Note that we assume that objects marked externally owned temporary are owned BY US; we must make sure that
	// is true, otherwise we will end up deleting somebody else's pointer.
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		EidosValue *value = symbol_slot->symbol_value_;
		
		if (!value->IsExternalPermanent())
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

std::vector<std::string> EidosSymbolTable::ReadOnlySymbols(void) const
{
	std::vector<std::string> symbol_names;
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		if (symbol_slot->symbol_is_const_)
			symbol_names.push_back(*symbol_slot->symbol_name_);
	}
	
	return symbol_names;
}

std::vector<std::string> EidosSymbolTable::ReadWriteSymbols(void) const
{
	std::vector<std::string> symbol_names;
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		if (!symbol_slot->symbol_is_const_)
			symbol_names.push_back(*symbol_slot->symbol_name_);
	}
	
	return symbol_names;
}

EidosValue *EidosSymbolTable::GetValueForSymbol(const std::string &p_symbol_name) const
{
	int key_length = (int)p_symbol_name.length();
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == key_length)
		{
			if (symbol_slot->symbol_name_->compare(p_symbol_name) == 0)
				return symbol_slot->symbol_value_;
		}
	}
	
	//std::cerr << "ValueForIdentifier: Symbol table: " << *this;
	//std::cerr << "Symbol returned for identifier " << p_identifier << " == (" << result->Type() << ") " << *result << endl;
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::GetValueForSymbol): undefined identifier " << p_symbol_name << "." << eidos_terminate();
	return nullptr;
}

EidosValue *EidosSymbolTable::GetValueOrNullForSymbol(const std::string &p_symbol_name) const
{
	int key_length = (int)p_symbol_name.length();
	
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
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
int EidosSymbolTable::_SlotIndexForSymbol(const std::string &p_symbol_name, int p_key_length)
{
	for (int symbol_index = 0; symbol_index < symbol_count_; ++symbol_index)
	{
		EidosSymbolTableSlot *symbol_slot = symbols_ + symbol_index;
		
		// use the length of the string to make the scan fast; only call compare() for equal-length strings
		if (symbol_slot->symbol_name_length_ == p_key_length)
		{
			if (symbol_slot->symbol_name_->compare(p_symbol_name) == 0)
				return symbol_index;
		}
	}
	
	return -1;
}

// increases capacity (by a factor of two) to accommodate the addition of new symbols
void EidosSymbolTable::_CapacityIncrease(void)
{
	int new_symbol_capacity = symbol_capacity_ << 1;
	
	if (symbols_ == non_malloc_symbols)
	{
		// We have been living off our base buffer, so we need to malloc for the first time
		symbols_ = (EidosSymbolTableSlot *)malloc(sizeof(EidosSymbolTableSlot) * new_symbol_capacity);
		
		memcpy(symbols_, non_malloc_symbols, sizeof(EidosSymbolTableSlot) * symbol_capacity_);
	}
	else
	{
		// We already have a malloced buffer, so we just need to realloc
		symbols_ = (EidosSymbolTableSlot *)realloc(symbols_, sizeof(EidosSymbolTableSlot) * new_symbol_capacity);
	}
	
	symbol_capacity_ = new_symbol_capacity;
}

void EidosSymbolTable::SetValueForSymbol(const std::string &p_symbol_name, EidosValue *p_value)
{
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if ((symbol_slot >= 0) && symbols_[symbol_slot].symbol_is_const_)
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetValueForSymbol): Identifier '" << p_symbol_name << "' is a constant." << eidos_terminate();
	
	// get a version of the value that is suitable for insertion into the symbol table
	if (p_value->IsExternalTemporary() || p_value->Invisible())
	{
		// If it is marked as external-temporary, somebody else owns it, but it might go away at any time.  In that case,
		// we need to copy it.  This is not a concern for external-permanent EidosValues, since they are guaranteed to
		// live longer than us.  Probably these two flags should never be set at the same time, but who knows.  Similarly,
		// if it's invisible then we need to copy it, since the original needs to stay invisible to display correctly.
		p_value = p_value->CopyValues();
		
		// We set the external temporary flag on our copy so the pointer won't be deleted or reused by anybody else.
		p_value->SetExternalTemporary();
	}
	else if (!p_value->IsExternalPermanent())
	{
		// TAKING OWNERSHIP!  The object was a temporary; now it is ours!  The fact that we are allowed to do this to
		// temporary objects is part of our contract with our caller.  This means that anybody who creates a temporary
		// object, or gets handed a pointer to one, cannot assume that it will remain temporary; if there is any chance
		// that a symbol table has gotten its sticky hands on the object, then the caller must check IsTemporary() before
		// deleting their pointer.
		p_value->SetExternalTemporary();
	}
	// else p_value->ExternalPermanent(), which means we can just use the pointer.  Note we do not set ExternalTemporary() on
	// the object in this case; it is not ours, we are just keeping a pointer to it, and we don't want both flags set at once.
	
	// and now set the value in the symbol table
	if (symbol_slot == -1)
	{
		symbol_slot = AllocateNewSlot();
		
		EidosSymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
		
		new_symbol_slot_ptr->symbol_name_ = new std::string(p_symbol_name);
		new_symbol_slot_ptr->symbol_name_length_ = key_length;
		new_symbol_slot_ptr->symbol_value_ = p_value;
		new_symbol_slot_ptr->symbol_is_const_ = false;
		new_symbol_slot_ptr->symbol_name_externally_owned_ = false;
	}
	else
	{
		EidosSymbolTableSlot *existing_symbol_slot_ptr = symbols_ + symbol_slot;
		EidosValue *existing_value = existing_symbol_slot_ptr->symbol_value_;
		
		// We replace the existing symbol value, of course.  Everything else gets inherited, since we're replacing the value in an existing slot;
		// we can continue using the same symbol name, name length, constness (since that is guaranteed to be false here), etc.
		if (!existing_value->IsExternalPermanent())
			delete existing_value;
		
		existing_symbol_slot_ptr->symbol_value_ = p_value;
	}
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void EidosSymbolTable::SetConstantForSymbol(const std::string &p_symbol_name, EidosValue *p_value)
{
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if (symbol_slot >= 0)
	{
		// can't already be defined as either a constant or a variable; if you want to define a constant, you have to get there first
		if (symbols_[symbol_slot].symbol_is_const_)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetConstantForSymbol): Identifier '" << p_symbol_name << "' is already a constant." << eidos_terminate();
		else
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetConstantForSymbol): Identifier '" << p_symbol_name << "' is already a variable." << eidos_terminate();
	}
	
	// get a version of the value that is suitable for insertion into the symbol table
	if (p_value->IsExternalTemporary() || p_value->Invisible())
	{
		// If it is marked as external-temporary, somebody else owns it, but it might go away at any time.  In that case,
		// we need to copy it.  This is not a concern for external-permanent EidosValues, since they are guaranteed to
		// live longer than us.  Probably these two flags should never be set at the same time, but who knows.  Similarly,
		// if it's invisible then we need to copy it, since the original needs to stay invisible to display correctly.
		p_value = p_value->CopyValues();
		
		// We set the external temporary flag on our copy so the pointer won't be deleted or reused by anybody else.
		p_value->SetExternalTemporary();
	}
	else if (!p_value->IsExternalPermanent())
	{
		// TAKING OWNERSHIP!  The object was a temporary; now it is ours!  The fact that we are allowed to do this to
		// temporary objects is part of our contract with our caller.  This means that anybody who creates a temporary
		// object, or gets handed a pointer to one, cannot assume that it will remain temporary; if there is any chance
		// that a symbol table has gotten its sticky hands on the object, then the caller must check IsTemporary() before
		// deleting their pointer.
		p_value->SetExternalTemporary();
	}
	// else p_value->ExternalPermanent(), which means we can just use the pointer.  Note we do not set ExternalTemporary() on
	// the object in this case; it is not ours, we are just keeping a pointer to it, and we don't want both flags set at once.
	
	// and now set the value in the symbol table; we know, from the check above, that we're in a new slot
	symbol_slot = AllocateNewSlot();
	
	EidosSymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = new std::string(p_symbol_name);
	new_symbol_slot_ptr->symbol_name_length_ = key_length;
	new_symbol_slot_ptr->symbol_value_ = p_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = false;
	
	//std::cerr << "SetValueForIdentifier: Symbol table: " << *this << endl;
}

void EidosSymbolTable::RemoveValueForSymbol(const std::string &p_symbol_name, bool remove_constant)
{
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if (symbol_slot >= 0)
	{
		EidosSymbolTableSlot *symbol_slot_ptr = symbols_ + symbol_slot;
		EidosValue *value = symbol_slot_ptr->symbol_value_;
		
		if (symbol_slot_ptr->symbol_is_const_ && !remove_constant)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::RemoveValueForSymbol): Identifier '" << p_symbol_name << "' is a constant and thus cannot be removed." << eidos_terminate();
		
		// see comment on our destructor, above
		if (!value->IsExternalPermanent())
			delete value;
		
		// delete the slot and free the name string; if we're removing the last slot, that's all we have to do
		if (!symbol_slot_ptr->symbol_name_externally_owned_)
			delete symbol_slot_ptr->symbol_name_;
		
		--symbol_count_;
		
		// if we're removing an interior value, we can just replace this slot with the last slot, since we don't sort our entries
		if (symbol_slot != symbol_count_)
			*symbol_slot_ptr = symbols_[symbol_count_];
	}
}

void EidosSymbolTable::InitializeConstantSymbolEntry(EidosSymbolTableEntry *p_new_entry)
{
	const std::string &entry_name = p_new_entry->first;
	EidosValue *entry_value = p_new_entry->second;
	
#ifdef DEBUG
	if (!entry_value->IsExternalPermanent() || entry_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::InitializeConstantSymbolEntry): (internal error) this method should be called only for external-permanent, non-invisible objects." << eidos_terminate();
#endif
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	int symbol_slot = AllocateNewSlot();
	
	EidosSymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = &entry_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot_ptr->symbol_name_length_ = (int)entry_name.length();
	new_symbol_slot_ptr->symbol_value_ = entry_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
	
	// Note that we are left with a symbol for which external_temporary_ is false, but external_permanent_
	// is true.  This represents a symbol that we do not own, but that has been guaranteed by the external
	// owner to live longer than us, so we can take a pointer to it safely.  We do the same thing if an
	// external-permanent object is set on us using SetValueForSymbol().  See the memory management notes
	// in script_value.h, and the comments above.
}

void EidosSymbolTable::InitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue *p_value)
{
#ifdef DEBUG
	if (!p_value->IsExternalPermanent() || p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::InitializeConstantSymbolEntry): (internal error) this method should be called only for external-permanent, non-invisible objects." << eidos_terminate();
#endif
	
	// we assume that this symbol is not yet defined, for maximal set-up speed
	int symbol_slot = AllocateNewSlot();
	
	EidosSymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = &p_symbol_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot_ptr->symbol_name_length_ = (int)p_symbol_name.length();
	new_symbol_slot_ptr->symbol_value_ = p_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
	
	// Note that we are left with a symbol for which external_temporary_ is false, but external_permanent_
	// is true.  This represents a symbol that we do not own, but that has been guaranteed by the external
	// owner to live longer than us, so we can take a pointer to it safely.  We do the same thing if an
	// external-permanent object is set on us using SetValueForSymbol().  See the memory management notes
	// in script_value.h, and the comments above.
}

void EidosSymbolTable::ReinitializeConstantSymbolEntry(EidosSymbolTableEntry *p_new_entry)
{
	const std::string &entry_name = p_new_entry->first;
	EidosValue *entry_value = p_new_entry->second;
	
#ifdef DEBUG
	if (!entry_value->IsExternalPermanent() || entry_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): (internal error) this method should be called only for external-permanent, non-invisible objects." << eidos_terminate();
#endif
	
	// check whether the symbol is already defined; if so, it should be identical or we raise
	int key_length = (int)entry_name.length();
	int symbol_slot = _SlotIndexForSymbol(entry_name, key_length);
	
	if (symbol_slot >= 0)
	{
		EidosSymbolTableSlot *old_slot = symbols_ + symbol_slot;
		
		if ((!old_slot->symbol_is_const_) || (!old_slot->symbol_name_externally_owned_) || (old_slot->symbol_value_ != entry_value))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): Identifier '" << entry_name << "' is already defined, but the existing entry does not match." << eidos_terminate();
		
		// a matching slot already exists, so we can just return
		return;
	}
	
	// ok, it is not defined so we need to define it
	symbol_slot = AllocateNewSlot();
	
	EidosSymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = &entry_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot_ptr->symbol_name_length_ = (int)entry_name.length();
	new_symbol_slot_ptr->symbol_value_ = entry_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
	
	// Note that we are left with a symbol for which external_temporary_ is false, but external_permanent_
	// is true.  This represents a symbol that we do not own, but that has been guaranteed by the external
	// owner to live longer than us, so we can take a pointer to it safely.  We do the same thing if an
	// external-permanent object is set on us using SetValueForSymbol().  See the memory management notes
	// in script_value.h, and the comments above.
}

void EidosSymbolTable::ReinitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue *p_value)
{
#ifdef DEBUG
	if (!p_value->IsExternalPermanent() || p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): (internal error) this method should be called only for external-permanent, non-invisible objects." << eidos_terminate();
#endif
	
	// check whether the symbol is already defined; if so, it should be identical or we raise
	int key_length = (int)p_symbol_name.length();
	int symbol_slot = _SlotIndexForSymbol(p_symbol_name, key_length);
	
	if (symbol_slot >= 0)
	{
		EidosSymbolTableSlot *old_slot = symbols_ + symbol_slot;
		
		if ((!old_slot->symbol_is_const_) || (!old_slot->symbol_name_externally_owned_) || (old_slot->symbol_value_ != p_value))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::ReinitializeConstantSymbolEntry): Identifier '" << p_symbol_name << "' is already defined, but the existing entry does not match." << eidos_terminate();
		
		// a matching slot already exists, so we can just return
		return;
	}
	
	// ok, it is not defined so we need to define it
	symbol_slot = AllocateNewSlot();
	
	EidosSymbolTableSlot *new_symbol_slot_ptr = symbols_ + symbol_slot;
	
	new_symbol_slot_ptr->symbol_name_ = &p_symbol_name;		// take a pointer to the external object, which must live longer than us!
	new_symbol_slot_ptr->symbol_name_length_ = (int)p_symbol_name.length();
	new_symbol_slot_ptr->symbol_value_ = p_value;
	new_symbol_slot_ptr->symbol_is_const_ = true;
	new_symbol_slot_ptr->symbol_name_externally_owned_ = true;
	
	// Note that we are left with a symbol for which external_temporary_ is false, but external_permanent_
	// is true.  This represents a symbol that we do not own, but that has been guaranteed by the external
	// owner to live longer than us, so we can take a pointer to it safely.  We do the same thing if an
	// external-permanent object is set on us using SetValueForSymbol().  See the memory management notes
	// in script_value.h, and the comments above.
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
		EidosValue *symbol_value = p_symbols.GetValueForSymbol(symbol_name);
		int symbol_count = symbol_value->Count();
		bool is_const = std::find(read_only_symbol_names.begin(), read_only_symbol_names.end(), symbol_name) != read_only_symbol_names.end();
		
		if (symbol_count <= 2)
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *symbol_value << endl;
		else
		{
			EidosValue *first_value = symbol_value->GetValueAtIndex(0);
			EidosValue *second_value = symbol_value->GetValueAtIndex(1);
			
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *first_value << " " << *second_value << " ... (" << symbol_count << " values)" << endl;
			if (first_value->IsTemporary()) delete first_value;
		}
		
		if (symbol_value->IsTemporary()) delete symbol_value;
	}
	
	return p_outstream;
}




































































