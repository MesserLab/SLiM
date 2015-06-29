//
//  script_symbols.h
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

/*
 
 A symbol table is basically just a map of identifiers to ScriptValue objects.  However, there are some additional smarts,
 particularly where memory management is concerned.  ScriptValue objects can have one of three memory management statuses:
 (1) temporary, such that the current scope owns the value and should delete it before exiting, (2) externally owned, such that
 the SymbolTable machinery just handles the pointer but does not delete it, or (3) in a symbol table, such that temporary users
 of the object do not delete it, but the SymbolTable will delete it if it is removed from the table.  It used to be implemented
 with C++'s std::map, but that was very slow to construct and destruct, so I've shifted to a pure C implementation for speed.
 
 */

#ifndef __SLiM__script_symbols__
#define __SLiM__script_symbols__

#include <iostream>
#include <vector>
#include <string>
#include <map>


class ScriptValue;
class SLiMScriptBlock;


// This is used by ReplaceConstantSymbolEntry for fast setup / teardown
typedef std::pair<const std::string, ScriptValue*> SymbolTableEntry;


// This is what SymbolTable now uses internally
typedef struct {
	const std::string *symbol_name_;	// ownership is defined by symbol_name_externally_owned_, below
	int symbol_name_length_;			// used to make scanning of the symbol table faster
	ScriptValue *symbol_value_;			// ownership is defined by the flags in ScriptValue
	bool symbol_is_const_;				// T if const, F is variable
	bool symbol_name_externally_owned_;	// if F, we delete on dealloc; if T, we took a pointer to an external string
} SymbolTableSlot;

// As an optimization, SymbolTable contains a small buffer within itself, of this size, to avoid malloc/free
// The size here is just a guess as to a threshold that will allow most simple scripts sufficient room
#define SLIM_SYMBOL_TABLE_BASE_SIZE		30

class SymbolTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	SymbolTableSlot *symbols_;					// all our symbols
	int symbol_count_;							// the number of symbol table slots actually used
	int symbol_capacity_;						// the number of symbol table slots currently allocated
	
	SymbolTableSlot non_malloc_symbols[SLIM_SYMBOL_TABLE_BASE_SIZE];	// a base buffer, used to avoid malloc/free for simple scripts
	
public:
	
	SymbolTable(const SymbolTable&) = delete;							// no copying
	SymbolTable& operator=(const SymbolTable&) = delete;				// no copying
	explicit SymbolTable(SLiMScriptBlock *script_block = nullptr);		// standard constructor
	~SymbolTable(void);													// destructor
	
	// member access; these are variables defined in the global namespace
	std::vector<std::string> ReadOnlySymbols(void) const;
	std::vector<std::string> ReadWriteSymbols(void) const;
	ScriptValue *GetValueForSymbol(const std::string &p_symbol_name) const;
	ScriptValue *GetValueOrNullForSymbol(const std::string &p_symbol_name) const;		// safe to call with any string
	void SetValueForSymbol(const std::string &p_symbol_name, ScriptValue *p_value);
	void SetConstantForSymbol(const std::string &p_symbol_name, ScriptValue *p_value);
	void RemoveValueForSymbol(const std::string &p_symbol_name, bool remove_constant);
	
	// Special-purpose methods used for fast setup of new symbol tables; these methods require an externally-owned, non-invisible
	// ScriptValue that is guaranteed by the caller to live longer than we will live; no copy is made of the value!
	// The name string in the SymbolTableEntry is assumed to be statically defined, or long-lived enough that we can take a pointer to it
	// for our entire lifetime; so these are not general-purpose methods, they are specifically for a very specialized init case!
	void InitializeConstantSymbolEntry(SymbolTableEntry *p_new_entry);
	void InitializeConstantSymbolEntry(const std::string &p_symbol_name, ScriptValue *p_value);
	
	// internal
	int _SlotIndexForSymbol(const std::string &p_symbol_name, int p_key_length);
	inline int AllocateNewSlot(void)	{ if (symbol_count_ == symbol_capacity_) _CapacityIncrease(); return symbol_count_++; };
	void _CapacityIncrease(void);
};

std::ostream &operator<<(std::ostream &p_outstream, const SymbolTable &p_symbols);


#endif /* defined(__SLiM__script_symbols__) */





































































