//
//  eidos_symbol_table.h
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

/*
 
 A symbol table is basically just a map of identifiers to EidosValue objects.  However, there are some additional smarts,
 particularly where speed is concerned.  The goal is to be able to both add symbols and look up symbols as fast as
 possible.  EidosValue objects are tracked with smart pointers for refcounting.  There are also optimizations for
 setting up a symbol table with standard constants and variables using globally allocated strings for speed.
 
 As per the recommendations at http://herbsutter.com/2013/06/05/gotw-91-solution-smart-pointer-parameters/ I have
 designed the API to take EidosValue_SP parameters when setting the value of a symbol.  This creates a new smart pointer,
 which takes shared ownership of the object at the point of the call.  These functions then use std::move internally
 to move the smart pointer into the symbol table's internal data structures.  If a caller wishes to surrender a temporary
 object to the symbol table, they should use std::move in their call, and if I understand C++11 correctly, move semantics
 will be used the whole way along and a refcount increment/decrement will not happen at all.  EidosValue_SP values are
 returned, too, rather than references, since the caller will be taking their shared ownership of the object.
 
 EidosSymbolTable stores symbols internally and searches them linearly unless (1) it gets more than a threshold number of
 symbols, or (2) it is told at construction not to.  If either of those conditions is met, it will switch to using a
 std::unordered_map to store its symbols and search for them with a hash lookup.  The goal here is to allow small symbol
 tables to be made and discarded very quickly, for applications such as callbacks where the cost of constructing a
 std::unordered_map would be very large compared to the speedup of a few lookups, but to also offer hash table performance
 for symbols tables that are larger or longer-lived.
 
 */

#ifndef __Eidos__eidos_symbol_table__
#define __Eidos__eidos_symbol_table__

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

#include "eidos_value.h"


// This is used by InitializeConstantSymbolEntry / ReinitializeConstantSymbolEntry for fast setup / teardown
typedef std::pair<const std::string, EidosValue_SP> EidosSymbolTableEntry;


// Used by EidosSymbolTable for the slots in its internal fixed-size symbol array
typedef struct {
	EidosValue_SP symbol_value_SP_;			// our shared pointer to the EidosValue for the symbol
	const std::string *symbol_name_;		// ownership is defined by symbol_name_externally_owned_, below
	const char *symbol_name_data_;			// used to make scanning of the symbol table faster
	uint16_t symbol_name_length_;			// used to make scanning of the symbol table faster
	uint8_t symbol_name_externally_owned_;	// if F, we delete on dealloc; if T, we took a pointer to an external string
	uint8_t symbol_is_const_;				// T if const, F is variable
} EidosSymbolTable_InternalSlot;


// Used by EidosSymbolTable for the slots in its external std::unordered_map
typedef struct {
	EidosValue_SP symbol_value_SP_;			// our shared pointer to the EidosValue for the symbol
	uint8_t symbol_is_const_;				// T if const, F is variable
} EidosSymbolTable_ExternalSlot;


typedef struct {
	bool contains_T_ = false;					// "T"
	bool contains_F_ = false;					// "F"
	bool contains_NULL_ = false;				// "NULL"
	bool contains_PI_ = false;					// "PI"
	bool contains_E_ = false;					// "E"
	bool contains_INF_ = false;					// "INF"
	bool contains_NAN_ = false;					// "NAN"
} EidosSymbolUsageParamBlock;


// As an optimization, EidosSymbolTable contains a small buffer within itself, of this size, to avoid malloc/free
// The size here is just a guess as to a threshold that will allow most simple scripts sufficient room
#define EIDOS_SYMBOL_TABLE_BASE_SIZE		30


class EidosSymbolTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	// This flag indicates which storage strategy we are using
	bool using_internal_symbols_;
	
	// If using_internal_symbols_==true, we are using this fixed-size symbol array
	EidosSymbolTable_InternalSlot internal_symbols_[EIDOS_SYMBOL_TABLE_BASE_SIZE];
	size_t internal_symbol_count_;
	
	// If using_internal_symbols_==false, we have switched over to this external hash table using std::unordered_map.
	// It is likely that a much faster hash table for our purposes could be implemented.  FIXME
	std::unordered_map<std::string, EidosSymbolTable_ExternalSlot> hash_symbols_;
	
public:
	
	EidosSymbolTable(const EidosSymbolTable&) = delete;											// no copying
	EidosSymbolTable& operator=(const EidosSymbolTable&) = delete;								// no copying
	explicit EidosSymbolTable(EidosSymbolUsageParamBlock *p_symbol_usage=nullptr, bool p_start_with_hash=false);			// standard constructor
	~EidosSymbolTable(void);																	// destructor
	
	// symbol access; these are variables defined in the global namespace
	std::vector<std::string> ReadOnlySymbols(void) const;
	std::vector<std::string> ReadWriteSymbols(void) const;
	
	EidosValue_SP GetValueOrRaiseForToken(const EidosToken *p_symbol_token) const;				// raise will use the token
	EidosValue_SP GetNonConstantValueOrRaiseForToken(const EidosToken *p_symbol_token) const;	// same but raises if the value is constant
	EidosValue_SP GetValueOrRaiseForSymbol(const std::string &p_symbol_name) const;				// raise will just call eidos_terminate()
	EidosValue_SP GetValueOrNullForSymbol(const std::string &p_symbol_name) const;				// safe to call with any string
	
	void SetValueForSymbol(const std::string &p_symbol_name, EidosValue_SP p_value);
	void SetConstantForSymbol(const std::string &p_symbol_name, EidosValue_SP p_value);
	
	void RemoveValueForSymbol(const std::string &p_symbol_name, bool p_remove_constant);
	
	// Special-purpose methods used for fast setup of new symbol tables with constants.
	//
	// These methods assume (1) that the name string is a global constant that does not need to be copied and
	// has infinite lifespan, and (2) that the EidosValue passed in is not invisible and is thus suitable for
	// direct use in the symbol table; no copy will be made of the value.  These are not general-purpose methods,
	// they are specifically for the very specialized init case of setting up a table with standard entries.
	void InitializeConstantSymbolEntry(EidosSymbolTableEntry *p_new_entry);
	void InitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue_SP p_value);
	
	// Slightly slower versions of the above that check for the pre-existence of the symbol to avoid reinserting it
	void ReinitializeConstantSymbolEntry(EidosSymbolTableEntry *p_new_entry);
	void ReinitializeConstantSymbolEntry(const std::string &p_symbol_name, EidosValue_SP p_value);
	
	// internal methods
	int _SlotIndexForSymbol(int p_key_length, const char *p_symbol_name_data) const;
	void _SwitchToHash(void);
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosSymbolTable &p_symbols);


#endif /* defined(__Eidos__eidos_symbol_table__) */





































































