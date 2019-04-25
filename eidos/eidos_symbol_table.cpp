//
//  eidos_symbol_table.cpp
//  Eidos
//
//  Created by Ben Haller on 4/12/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
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


#include "eidos_symbol_table.h"
#include "eidos_value.h"
#include "eidos_global.h"
#include "eidos_token.h"
#include "eidos_type_table.h"

#include <algorithm>
#include <utility>
#include <limits>
#include <cmath>


//
//	Shared pools of reusable objects for EidosSymbolTable
//
#pragma mark -
#pragma mark pool management
#pragma mark -

std::vector<EidosSymbolTableSlot *> gEidosSymbolTable_TablePool;
uint32_t gEidosSymbolTable_TablePool_table_capacity = 1024;		// adequate for most scripts; can increase dynamically

size_t MemoryUsageForSymbolTables(EidosSymbolTable *p_currentTable)
{
	size_t usage = 0;
	
	usage = gEidosSymbolTable_TablePool.size() * gEidosSymbolTable_TablePool_table_capacity * sizeof(EidosSymbolTableSlot);
	
	while (p_currentTable)
	{
		usage += p_currentTable->capacity_ * sizeof(EidosSymbolTableSlot);
		p_currentTable = p_currentTable->parent_symbol_table_;
	}
	
	return usage;
}

static inline __attribute__((always_inline)) EidosSymbolTableSlot *GetZeroedTableFromPool(uint32_t *p_capacity)
{
	// Get a zeroed table from the pool, or make a new one
	*p_capacity = gEidosSymbolTable_TablePool_table_capacity;
	
	if (gEidosSymbolTable_TablePool.size())
	{
		EidosSymbolTableSlot *ret = gEidosSymbolTable_TablePool.back();
		gEidosSymbolTable_TablePool.pop_back();
		return ret;
	}
	
	return (EidosSymbolTableSlot *)calloc(gEidosSymbolTable_TablePool_table_capacity, sizeof(EidosSymbolTableSlot));
}

static inline __attribute__((always_inline)) void FreeZeroedTableToPool(EidosSymbolTableSlot *p_table, uint32_t p_capacity)
{
	if (p_capacity > gEidosSymbolTable_TablePool_table_capacity)
	{
		// If the returning table is bigger than those in the pool, we want to increase the pool capacity
		// to match; free all tables in the pool, then do the capacity increase for the pool
		for (EidosSymbolTableSlot *table : gEidosSymbolTable_TablePool)
			free(table);
		
		gEidosSymbolTable_TablePool.clear();
		
		//std::cout << "old symbol table pool's table capacity of " << gEidosSymbolTable_TablePool_table_capacity << " increasing to " << p_capacity << std::endl;
		gEidosSymbolTable_TablePool_table_capacity = p_capacity;
	}
	else if (p_capacity < gEidosSymbolTable_TablePool_table_capacity)
	{
		// If the returning table is smaller than those in the pool, it is worthless; it's probably
		// faster to free it and calloc() a new one than to realloc() and zero the old one
		free(p_table);
		return;
	}
	
	// Free the zeroed table back to the pool
	gEidosSymbolTable_TablePool.emplace_back(p_table);
}


//
//	EidosSymbolTable
//
#pragma mark -
#pragma mark EidosSymbolTable
#pragma mark -

EidosSymbolTable::EidosSymbolTable(EidosSymbolTableType p_table_type, EidosSymbolTable *p_parent_table) : table_type_(p_table_type)
{
	// allocate the lookup table
	slots_ = GetZeroedTableFromPool(&capacity_);
	
	if (!p_parent_table)
	{
		// If no parent table is given, then we construct a base table for Eidos containing the standard constants.
		// We statically allocate our base symbols for fast setup / teardown.
#ifdef DEBUG
		if (table_type_ != EidosSymbolTableType::kEidosIntrinsicConstantsTable)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::EidosSymbolTable): (internal error) symbol tables must have a parent table, except the Eidos intrinsic constants table." << EidosTerminate(nullptr);
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
			trueConstant = new EidosSymbolTableEntry(gEidosID_T, gStaticEidosValue_LogicalT);
			falseConstant = new EidosSymbolTableEntry(gEidosID_F, gStaticEidosValue_LogicalF);
			nullConstant = new EidosSymbolTableEntry(gEidosID_NULL, gStaticEidosValueNULL);
			piConstant = new EidosSymbolTableEntry(gEidosID_PI, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(M_PI)));
			eConstant = new EidosSymbolTableEntry(gEidosID_E, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(M_E)));
			infConstant = new EidosSymbolTableEntry(gEidosID_INF, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::infinity())));
			nanConstant = new EidosSymbolTableEntry(gEidosID_NAN, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::quiet_NaN())));
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
		// If a parent table is given, we adopt it and do not add Eidos constants; they will be in the search chain
		parent_symbol_table_ = p_parent_table;
		parent_symbol_table_owned_ = false;
		
		// If the parent table is a constants table of some kind, then it is the next table in the search chain;
		// if it is a variables table, however, then it is our caller, and is not in scope for us, so we skip
		// over it and use whatever it chains onward to (which will always be a constants table, in this design).
		if (parent_symbol_table_->table_type_ == EidosSymbolTableType::kVariablesTable)
			chain_symbol_table_ = parent_symbol_table_->chain_symbol_table_;
		else
			chain_symbol_table_ = parent_symbol_table_;
		
#ifdef DEBUG
		if (table_type_ == EidosSymbolTableType::kEidosIntrinsicConstantsTable)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::EidosSymbolTable): (internal error) the Eidos intrinsic constants table cannot have a parent." << EidosTerminate(nullptr);
		if (chain_symbol_table_->table_type_ == EidosSymbolTableType::kVariablesTable)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::EidosSymbolTable): (internal error) the chained symbol table must be constant in the current design." << EidosTerminate(nullptr);
		if (parent_symbol_table_->table_type_ == EidosSymbolTableType::kINVALID_TABLE_TYPE)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::EidosSymbolTable): (internal error) zombie symbol table re-used as parent table." << EidosTerminate(nullptr);
#endif
	}
}

EidosSymbolTable::~EidosSymbolTable(void)
{
	// We do a little bit of zombie-fication here to try to catch problematic table usage patterns
	if (table_type_ == EidosSymbolTableType::kINVALID_TABLE_TYPE)
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::~EidosSymbolTable): (internal error) zombie symbol table being destructed." << EidosTerminate(nullptr);
	
	table_type_ = EidosSymbolTableType::kINVALID_TABLE_TYPE;
	
	// slots_ may have symbols defined in it, so we need to zero out the used slots for re-use.  Remember that
	// the slot at index 0 never has a value defined, and its next_ value is the start of the linked list.
	EidosSymbolTableSlot *slot = slots_;
	
	for (uint32_t index = slot->next_; index != 0; index = slot->next_)
	{
		slot->next_ = 0;
		slot = slots_ + index;
		slot->symbol_value_SP_.reset();
	}
	
	// then return the table to the pools for reuse
	FreeZeroedTableToPool(slots_, capacity_);
	
	// In general, every symbol table has its own lifetime, and a single table might be the parent table for many other
	// symbol tables (in the way that the intrinsic constants table is used as the root parent for all symbol table
	// chains in Eidos).  The exception to this is defined constants tables, which are added and inserted into the
	// symbol table chain dynamically by DefineConstantForSymbol().  They need to be freed, so the policy we have is
	// that they are owned by their child table, which has a pointer up to them.  That means that unlike other types
	// of tables, DEFINED CONSTANTS TABLES MUST NEVER BE DIRECTLY REFERENCED BY MORE THAN ONE CHILD TABLE.
	if (parent_symbol_table_owned_)
	{
		if (!parent_symbol_table_)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::~EidosSymbolTable): (internal error) owned parent symbol table was already freed." << EidosTerminate(nullptr);
		if (parent_symbol_table_->table_type_ != EidosSymbolTableType::kEidosDefinedConstantsTable)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::~EidosSymbolTable): (internal error) owned parent symbol table is of unexpected type." << EidosTerminate(nullptr);
		
		delete parent_symbol_table_;
		parent_symbol_table_ = nullptr;
		parent_symbol_table_owned_ = false;
	}
}

std::vector<std::string> EidosSymbolTable::_SymbolNames(bool p_include_constants, bool p_include_variables) const
{
	std::vector<std::string> symbol_names;
	
	// recurse to get the symbols from our chained symbol table
	if (chain_symbol_table_)
		symbol_names = chain_symbol_table_->_SymbolNames(p_include_constants, p_include_variables);
	
	if ((p_include_constants && (table_type_ != EidosSymbolTableType::kVariablesTable)) ||
		(p_include_variables && (table_type_ == EidosSymbolTableType::kVariablesTable)))
	{
		for (EidosGlobalStringID symbol = slots_->next_; symbol != 0; symbol = (slots_ + symbol)->next_)
			symbol_names.emplace_back(Eidos_StringForGlobalStringID(symbol));
	}
	
	return symbol_names;
}

bool EidosSymbolTable::ContainsSymbol(EidosGlobalStringID p_symbol_name) const
{
	// Conceptually, this is a recursive function that walks up the symbol table chain.  Doing the recursive calls
	// is a bit slow, though, so I have unwrapped the recursion.
	
	const EidosSymbolTable *current_table = this;
	
	do
	{
		// try the current table, if the symbol is within its capacity
		if ((p_symbol_name < current_table->capacity_) && (current_table->slots_[p_symbol_name].symbol_value_SP_))
			return true;
		
		// We didn't get a hit, so try our chained table
		current_table = current_table->chain_symbol_table_;
	}
	while (current_table);
	
	return false;
}

bool EidosSymbolTable::ContainsSymbol_IsConstant(EidosGlobalStringID p_symbol_name, bool *p_is_const) const
{
	// This follows ContainsSymbol() but adds a flag indicating whether the symbol is defined as a constant.
	const EidosSymbolTable *current_table = this;
	
	do
	{
		// try the current table, if the symbol is within its capacity
		if ((p_symbol_name < current_table->capacity_) && (current_table->slots_[p_symbol_name].symbol_value_SP_))
		{
			*p_is_const = (current_table->table_type_ != EidosSymbolTableType::kVariablesTable);
			return true;
		}
		
		// We didn't get a hit, so try our chained table
		current_table = current_table->chain_symbol_table_;
	}
	while (current_table);
	
	*p_is_const = false;
	return false;
}

bool EidosSymbolTable::SymbolDefinedAnywhere(EidosGlobalStringID p_symbol_name) const
{
	// This follows ContainsSymbol() but follows parent_symbol_table_ instead of chain_symbol_table_.
	const EidosSymbolTable *current_table = this;
	
	do
	{
		// try the current table, if the symbol is within its capacity
		if ((p_symbol_name < current_table->capacity_) && (current_table->slots_[p_symbol_name].symbol_value_SP_))
			return true;
		
		// We didn't get a hit, so try our parent table
		current_table = current_table->parent_symbol_table_;
	}
	while (current_table);
	
	return false;
}

EidosValue_SP EidosSymbolTable::_GetValue(EidosGlobalStringID p_symbol_name, const EidosToken *p_symbol_token) const
{
	// Conceptually, this is a recursive function that walks up the symbol table chain.  Doing the recursive calls
	// is a bit slow, though, so I have unwrapped the recursion.
	const EidosSymbolTable *current_table = this;
	
	do
	{
		// try the current table, if the symbol is within its capacity
		if (p_symbol_name < current_table->capacity_)
		{
			EidosValue_SP slot_value(current_table->slots_[p_symbol_name].symbol_value_SP_);
			
			if (slot_value)
				return slot_value;
		}
		
		// We didn't get a hit, so try our chained table
		current_table = current_table->chain_symbol_table_;
	}
	while (current_table);
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_GetValue): undefined identifier " << Eidos_StringForGlobalStringID(p_symbol_name) << "." << EidosTerminate(p_symbol_token);
}

EidosValue *EidosSymbolTable::_GetValue_RAW(EidosGlobalStringID p_symbol_name, const EidosToken *p_symbol_token) const
{
	// This follows _GetValue() but returns a raw EidosValue * for temporary use
	const EidosSymbolTable *current_table = this;
	
	do
	{
		// try the current table, if the symbol is within its capacity
		if (p_symbol_name < current_table->capacity_)
		{
			EidosValue *slot_value = current_table->slots_[p_symbol_name].symbol_value_SP_.get();
			
			if (slot_value)
				return slot_value;
		}
		
		// We didn't get a hit, so try our chained table
		current_table = current_table->chain_symbol_table_;
	}
	while (current_table);
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_GetValue_RAW): undefined identifier " << Eidos_StringForGlobalStringID(p_symbol_name) << "." << EidosTerminate(p_symbol_token);
}

EidosValue_SP EidosSymbolTable::_GetValue_IsConst(EidosGlobalStringID p_symbol_name, const EidosToken *p_symbol_token, bool *p_is_const) const
{
	// This follows _GetValue() but provides the p_is_const flag
	const EidosSymbolTable *current_table = this;
	
	do
	{
		// try the current table, if the symbol is within its capacity
		if (p_symbol_name < current_table->capacity_)
		{
			EidosValue_SP slot_value(current_table->slots_[p_symbol_name].symbol_value_SP_);
			
			if (slot_value)
			{
				*p_is_const = (current_table->table_type_ != EidosSymbolTableType::kVariablesTable);
				return slot_value;
			}
		}
		
		// We didn't get a hit, so try our chained table
		current_table = current_table->chain_symbol_table_;
	}
	while (current_table);
	
	EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_GetValue_IsConst): undefined identifier " << Eidos_StringForGlobalStringID(p_symbol_name) << "." << EidosTerminate(p_symbol_token);
}

void EidosSymbolTable::_ResizeToFitSymbol(EidosGlobalStringID p_symbol_name)
{
	uint32_t new_capacity = capacity_;
	
	while (p_symbol_name >= new_capacity)
		new_capacity <<= 1;
	
	if (new_capacity > capacity_)
	{
		slots_ = (EidosSymbolTableSlot *)realloc(slots_, new_capacity * sizeof(EidosSymbolTableSlot));
		EIDOS_BZERO(slots_ + capacity_, (new_capacity - capacity_) * sizeof(EidosSymbolTableSlot));
		capacity_ = new_capacity;
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_ResizeToFitSymbol): (internal error) unnecessary resize." << EidosTerminate();
}

void EidosSymbolTable::SetValueForSymbol(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value)
{
	// If we have the only reference to the value, we don't need to copy it; otherwise we copy, since we don't want to hold
	// onto a reference that somebody else might modify under us (or that we might modify under them, with syntaxes like
	// x[2]=...; and x=x+1;). If the value is invisible then we copy it, since the symbol table never stores invisible values.
	if ((p_value->UseCount() != 1) || p_value->Invisible())
		p_value = p_value->CopyValues();
	
	// Make sure we have capacity
	if (p_symbol_name >= capacity_)
		_ResizeToFitSymbol(p_symbol_name);
	
	// Define the symbol if it is not already defined above us
	if (!slots_[p_symbol_name].symbol_value_SP_)
	{
		// The symbol is not already defined in this table.  Before we can define it, we need to check that it is not defined in a chained table.
		// At present, we assume that if it is defined in a chained table it is a constant, which is true for now.
		if (chain_symbol_table_ && chain_symbol_table_->ContainsSymbol(p_symbol_name))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetValueForSymbol): identifier '" << Eidos_StringForGlobalStringID(p_symbol_name) << "' cannot be redefined because it is a constant." << EidosTerminate(nullptr);
		
		// set the value into the unused slot and add it to the front of the linked list
		slots_[p_symbol_name].symbol_value_SP_ = std::move(p_value);
		slots_[p_symbol_name].next_ = slots_[0].next_;
		slots_[0].next_ = p_symbol_name;
	}
	else
	{
		// set the value into the already used slot; no linked list maintenance needed
		slots_[p_symbol_name].symbol_value_SP_ = std::move(p_value);
	}
}

void EidosSymbolTable::SetValueForSymbolNoCopy(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value)
{
	// So, this is a little weird.  SetValueForSymbol() copies the passed value, as explained in its comment above.
	// If a few cases, however, we want to play funny games and prevent that copy from occurring so that we can munge
	// values directly inside a value we just set in the symbol table.  Evaluate_For() is the worst offender in this
	// because it wants to set up an index variable once and then munge its value directly each time through the loop,
	// for speed.  _ProcessSubsetAssignment() also does it in one case, where it needs to change a singleton into a
	// vector value so that it can do a subscripted assignment.  For that special purpose, this function is provided.
	// DO NOT USE THIS UNLESS YOU KNOW WHAT YOU'RE DOING!  It can lead to seriously weird behavior if used incorrectly.
	if (p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetValueForSymbolNoCopy): (internal) no copy requested with invisible value." << EidosTerminate(nullptr);
	
	// Make sure we have capacity
	if (p_symbol_name >= capacity_)
		_ResizeToFitSymbol(p_symbol_name);
	
	// Define the symbol if it is not already defined above us
	if (!slots_[p_symbol_name].symbol_value_SP_)
	{
		// The symbol is not already defined in this table.  Before we can define it, we need to check that it is not defined in a chained table.
		// At present, we assume that if it is defined in a chained table it is a constant, which is true for now.
		if (chain_symbol_table_ && chain_symbol_table_->ContainsSymbol(p_symbol_name))
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::SetValueForSymbolNoCopy): identifier '" << Eidos_StringForGlobalStringID(p_symbol_name) << "' cannot be redefined because it is a constant." << EidosTerminate(nullptr);
		
		// set the value into the unused slot and add it to the front of the linked list
		slots_[p_symbol_name].symbol_value_SP_ = std::move(p_value);
		slots_[p_symbol_name].next_ = slots_[0].next_;
		slots_[0].next_ = p_symbol_name;
	}
	else
	{
		// set the value into the already used slot; no linked list maintenance needed
		slots_[p_symbol_name].symbol_value_SP_ = std::move(p_value);
	}
}

void EidosSymbolTable::DefineConstantForSymbol(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value)
{
	// First make sure this symbol is not in use as either a variable or a constant
	// We use SymbolDefinedAnywhere() because defined constants cannot conflict with any symbol defined anywhere, whether
	// currently in scope or not – as soon as the conflicting scope comes into scope, the conflict will be manifest.
	if (SymbolDefinedAnywhere(p_symbol_name))
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::DefineConstantForSymbol): identifier '" << Eidos_StringForGlobalStringID(p_symbol_name) << "' is already defined." << EidosTerminate(nullptr);
	
	// Search through our chain for a defined constants table; if we don't find one, add one
	EidosSymbolTable *definedConstantsTable;
	
	for (definedConstantsTable = this; definedConstantsTable != nullptr; definedConstantsTable = definedConstantsTable->chain_symbol_table_)
		if (definedConstantsTable->table_type_ == EidosSymbolTableType::kEidosDefinedConstantsTable)
			break;
	
	if (!definedConstantsTable)
	{
		EidosSymbolTable *childTable;
		
		for (childTable = this; childTable != nullptr; childTable = childTable->parent_symbol_table_)
			if (childTable->parent_symbol_table_ && childTable->parent_symbol_table_->table_type_ == EidosSymbolTableType::kEidosIntrinsicConstantsTable)
				break;
		
		if (!childTable)
			EIDOS_TERMINATION << "ERROR (EidosSymbolTable::DefineConstantForSymbol): (internal) could not find child symbol table of the intrinsic constants table." << EidosTerminate(nullptr);
		
		EidosSymbolTable *intrinsicConstantsTable = childTable->parent_symbol_table_;
		
		// Make a defined constants table and insert it in between; this table will be
		// owned by childTable, which will free it whenever childTable is destructed
		definedConstantsTable = new EidosSymbolTable(EidosSymbolTableType::kEidosDefinedConstantsTable, intrinsicConstantsTable);
		childTable->parent_symbol_table_ = definedConstantsTable;
		childTable->parent_symbol_table_owned_ = true;
		childTable->chain_symbol_table_ = definedConstantsTable;
		
		// There may be intervening tables that chain up to the intrinsic constants table;
		// they need to be fixed to now chain up to the defined constants table instead.
		EidosSymbolTable *patchTable;
		
		for (patchTable = this; patchTable != definedConstantsTable; patchTable = patchTable->parent_symbol_table_)
			if (patchTable->chain_symbol_table_ == intrinsicConstantsTable)
				patchTable->chain_symbol_table_ = definedConstantsTable;
	}
	
	// If we have the only reference to the value, we don't need to copy it; otherwise we copy, since we don't want to hold
	// onto a reference that somebody else might modify under us (or that we might modify under them, with syntaxes like
	// x[2]=...; and x=x+1;). If the value is invisible then we copy it, since the symbol table never stores invisible values.
	if ((p_value->UseCount() != 1) || p_value->Invisible())
		p_value = p_value->CopyValues();
	
	// Then ask the defined constants table to add the constant
	definedConstantsTable->InitializeConstantSymbolEntry(p_symbol_name, std::move(p_value));
}

void EidosSymbolTable::_RemoveSymbol(EidosGlobalStringID p_symbol_name, bool p_remove_constant)
{
	if (p_symbol_name < capacity_)
	{
		EidosSymbolTableSlot *slot = slots_ + p_symbol_name;
		
		if (slot->symbol_value_SP_)
		{
			// We found the symbol in ourselves, so remove it unless we are a constant table
			if (table_type_ != EidosSymbolTableType::kVariablesTable)
			{
				if (table_type_ == EidosSymbolTableType::kEidosIntrinsicConstantsTable)
					EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_RemoveSymbol): identifier '" << Eidos_StringForGlobalStringID(p_symbol_name) << "' is an intrinsic Eidos constant and thus cannot be removed." << EidosTerminate(nullptr);
				if (!p_remove_constant)
					EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_RemoveSymbol): identifier '" << Eidos_StringForGlobalStringID(p_symbol_name) << "' is a constant and thus cannot be removed." << EidosTerminate(nullptr);
			}
			
			slot->symbol_value_SP_.reset();
			
			// Now we need to fix the linked list, which is O(n): we have to find the previous entry that points to this entry
			EidosGlobalStringID index = 0;
			
			do
			{
				EidosSymbolTableSlot *search_slot = slots_ + index;
				EidosGlobalStringID search_next = search_slot->next_;
				
				if (search_next == p_symbol_name)
				{
					search_slot->next_ = slot->next_;
					slot->next_ = 0;
					break;
				}
				
				index = search_next;
			}
			while (index != 0);
			
			return;
		}
	}
	
	// If it wasn't defined in us, then it might be defined in the chain
	if (chain_symbol_table_)
		chain_symbol_table_->_RemoveSymbol(p_symbol_name, p_remove_constant);
}

void EidosSymbolTable::_InitializeConstantSymbolEntry(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value)
{
#ifdef DEBUG
	if (p_value->Invisible())
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_InitializeConstantSymbolEntry): (internal error) this method should be called only for non-invisible objects." << EidosTerminate(nullptr);
	if (table_type_ == EidosSymbolTableType::kVariablesTable)
		EIDOS_TERMINATION << "ERROR (EidosSymbolTable::_InitializeConstantSymbolEntry): (internal error) this method should be called only on constant symbol tables." << EidosTerminate(nullptr);
#endif
	
	// Make sure we have capacity
	if (p_symbol_name >= capacity_)
		_ResizeToFitSymbol(p_symbol_name);
	
	// We assume that this symbol is not yet defined, for maximal set-up speed
	slots_[p_symbol_name].symbol_value_SP_ = std::move(p_value);
	slots_[p_symbol_name].next_ = slots_[0].next_;
	slots_[0].next_ = p_symbol_name;
}

void EidosSymbolTable::AddSymbolsToTypeTable(EidosTypeTable *p_type_table) const
{
	// recurse to get the symbols from our chained symbol table
	if (chain_symbol_table_)
		chain_symbol_table_->AddSymbolsToTypeTable(p_type_table);
	
	for (EidosGlobalStringID symbol = slots_->next_; symbol != 0; )
	{
		EidosSymbolTableSlot *slot = slots_ + symbol;
		EidosValue *symbol_value = slot->symbol_value_SP_.get();
		EidosValueType symbol_type = symbol_value->Type();
		EidosValueMask symbol_type_mask = (1 << (int)symbol_type);
		const EidosObjectClass *symbol_class = ((symbol_type == EidosValueType::kValueObject) ? ((EidosValue_Object *)symbol_value)->Class() : nullptr);
		EidosTypeSpecifier symbol_type_specifier = EidosTypeSpecifier{symbol_type_mask, symbol_class};
		
		p_type_table->SetTypeForSymbol(symbol, symbol_type_specifier);
		symbol = slot->next_;
	}
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
		EidosValue_SP symbol_value = p_symbols.GetValueOrRaiseForSymbol(Eidos_GlobalStringIDForString(symbol_name));
		int symbol_count = symbol_value->Count();
		bool is_const = std::find(read_only_symbol_names.begin(), read_only_symbol_names.end(), symbol_name) != read_only_symbol_names.end();
		
		if (symbol_count <= 2)
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *symbol_value << std::endl;
		else
		{
			EidosValue_SP first_value = symbol_value->GetValueAtIndex(0, nullptr);
			EidosValue_SP second_value = symbol_value->GetValueAtIndex(1, nullptr);
			
			p_outstream << symbol_name << (is_const ? " => (" : " -> (") << symbol_value->Type() << ") " << *first_value << " " << *second_value << " ... (" << symbol_count << " values)" << std::endl;
		}
	}
	
	return p_outstream;
}




































































