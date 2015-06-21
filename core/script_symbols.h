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


// This is used by ReplaceConstantSymbolEntry for fast setup / teardown
typedef std::pair<const std::string, ScriptValue*> SymbolTableEntry;


// This is what SymbolTable now uses internally
typedef struct {
	std::string *symbol_name_;
	int symbol_name_length_;			// used to make scanning of the symbol table faster
	ScriptValue *symbol_value_;
	bool symbol_is_const_;
} SymbolTableSlot;


class SymbolTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	SymbolTableSlot *symbols_;					// all our symbols
	int symbol_count_;							// the number of symbol table slots actually used
	int symbol_capacity_;						// the number of symbol table slots currently allocated
	
public:
	
	SymbolTable(const SymbolTable&) = delete;				// no copying
	SymbolTable& operator=(const SymbolTable&) = delete;	// no copying
	SymbolTable(void);										// standard constructor
	~SymbolTable(void);										// destructor
	
	// member access; these are variables defined in the global namespace
	std::vector<std::string> ReadOnlySymbols(void) const;
	std::vector<std::string> ReadWriteSymbols(void) const;
	ScriptValue *GetValueForSymbol(const std::string &p_symbol_name) const;
	ScriptValue *GetValueOrNullForSymbol(const std::string &p_symbol_name) const;		// safe to call with any string
	void SetValueForSymbol(const std::string &p_symbol_name, ScriptValue *p_value);
	void SetConstantForSymbol(const std::string &p_symbol_name, ScriptValue *p_value);
	void RemoveValueForSymbol(const std::string &p_symbol_name, bool remove_constant);
	
	// a special-purpose method used for fast setup of new symbol tables; requires an externally-owned, non-invisible ScriptValue
	void InitializeConstantSymbolEntry(SymbolTableEntry *p_new_entry);
	
	// internal
	int _SlotIndexForSymbol(const std::string &p_symbol_name, int p_key_length);
	int _AllocateNewSlot(void);
};

std::ostream &operator<<(std::ostream &p_outstream, const SymbolTable &p_symbols);


#endif /* defined(__SLiM__script_symbols__) */





































































