//
//  eidos_symbol_table.h
//  Eidos
//
//  Created by Ben Haller on 4/12/15.
//  Copyright (c) 2015-2017 Philipp Messer.  All rights reserved.
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
 
 EidosSymbolTable does not use strings to identify symbols in practice, although conceptually it does.  In practice, it
 uses EidosGlobalStringID, which is an integer type that represents a uniqued string.  This allows greater speed, since
 strings get uniqued only once, and an integer key can be used from then onward.
 
 */

#ifndef __Eidos__eidos_symbol_table__
#define __Eidos__eidos_symbol_table__

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

#include "eidos_value.h"
#include "eidos_ast_node.h"

class EidosSymbolTable;
class EidosTypeTable;


// This is a shared global symbol table containing the standard Eidos constants; it should be linked to as a parent symbol table
extern EidosSymbolTable *gEidosConstantsSymbolTable;


// This is used by InitializeConstantSymbolEntry / ReinitializeConstantSymbolEntry for fast setup / teardown
typedef std::pair<EidosGlobalStringID, EidosValue_SP> EidosSymbolTableEntry;


// Used by EidosSymbolTable for the slots in its internal fixed-size symbol array
typedef struct {
	EidosValue_SP symbol_value_SP_;			// our shared pointer to the EidosValue for the symbol
	EidosGlobalStringID symbol_name_;		// the name of the symbol, but as a uniqued EidosGlobalStringID
} EidosSymbolTable_InternalSlot;


// As an optimization, EidosSymbolTable contains a small buffer within itself, of this size, to avoid malloc/free.
// The size here is just a guess as to a threshold that will allow most simple scripts sufficient room.
// Even when an EidosSymbolTable is constructed once and used a lot, this internal table is likely to be
// faster up to some threshold size, because calculating hash values for strings is not particularly fast;
// a linear search through the internal table is often faster than a single hash table lookup.  The critical
// size threshold will be platform-dependent; the point is that the internal table is a good thing.
#define EIDOS_SYMBOL_TABLE_BASE_SIZE		30


// Symbol tables can come in various types.  This is mostly hidden from clients of this class.  The intrinsic
// constants table holds Eidos constants like T, F, INF, and NAN.  The defined constants table, which should be
// the direct child of the intrinsic table, holds constants defined by the user with DefineConstantForSymbol().
// The context constants table is used by the Context for its own constants; SLiM uses it for constants like
// sim, p1, g1, m1, s1, etc.  Finally, the variables table contains all user-defined variables.  This linked-list
// design for the symbol table makes it easy to throw out variables while keeping constants, to differentiate
// between different responsibilities for constants without having to keep a tag for each defined symbol, etc.
// Clients of this class can just use the symbol table they have a reference to, which will generally be a
// variables table at the end of the chain.  Only clients that are constructing or destructing a symbol table
// setup need to know about these types and how they are supposed to interact.

// BCH 9/10/2017: Note that now, in Eidos 1.5, it is legal to have more than one variables table at the end.
// These represent nested function calls; when a new function call starts, a new table is added to the top, and
// when a function call ends that table is removed.  The outer variable scopes are hidden; searches for values
// will go from the bottommost variables table directly up to the first constants table in the chain.
enum class EidosSymbolTableType
{
	kEidosIntrinsicConstantsTable = 0,	// just one of these
	kEidosDefinedConstantsTable,		// and just one of these
	kContextConstantsTable,				// can be any number of these
	kVariablesTable,					// and finally, any number of these, for nested function calls
	kINVALID_TABLE_TYPE					// used as a sort of zombie marker in an attempt to increase code safety
};


class EidosSymbolTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	// In this design, a whole symbol table either contains constants or variables; a mix is allowed using a parent symbol table to hold constants
	EidosSymbolTableType table_type_;
	
	// This flag indicates which storage strategy we are using
	bool using_internal_symbols_;
	
	// If using_internal_symbols_==true, we are using this fixed-size symbol array
	EidosSymbolTable_InternalSlot internal_symbols_[EIDOS_SYMBOL_TABLE_BASE_SIZE];
	size_t internal_symbol_count_;
	
	// If using_internal_symbols_==false, we have switched over to this external hash table utilizing std::unordered_map.
	std::unordered_map<EidosGlobalStringID, EidosValue_SP> hash_symbols_;
	
	// Symbol tables can be chained.  This is invisible to the user; there appears to be a single global symbol table for a given
	// interpreter, which responds to all requests.  Behind the scenes, however, requests get passed up the symbol table chain
	// until a hit is found or the last symbol table declares a miss.  Adds go in the symbol table that receives the request.
	// BCH 10 Sept 2017: With the addition of user-defined functions, this gets a little more complex.  The chain_symbol_table_
	// ivar represents the next symbol table in the search chain from this table (skipping over the variables tables belonging
	// to callers up the chain, since they are not in scope).  The parent_symbol_table_ ivar is the next symbol table upward –
	// either the caller or the first constants table – but it is not used for much since scoped searching is usually desired.
	EidosSymbolTable *chain_symbol_table_ = nullptr;	// NOT OWNED
	EidosSymbolTable *parent_symbol_table_ = nullptr;	// NOT OWNED
	
	// Utility methods called by the public methods to do the real work
	std::vector<std::string> _SymbolNames(bool p_include_constants, bool p_include_variables) const;
	EidosValue_SP _GetValue(EidosGlobalStringID p_symbol_name, const EidosToken *p_symbol_token) const;
	EidosValue_SP _GetValue_IsConst(EidosGlobalStringID p_symbol_name, const EidosToken *p_symbol_token, bool *p_is_const) const;
	void _RemoveSymbol(EidosGlobalStringID p_symbol_name, bool p_remove_constant);
	void _InitializeConstantSymbolEntry(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value);
	void _SwitchToHash(void);
	
public:
	
	EidosSymbolTable(const EidosSymbolTable&) = delete;																	// no copying
	EidosSymbolTable& operator=(const EidosSymbolTable&) = delete;														// no copying
	explicit EidosSymbolTable(EidosSymbolTableType p_table_type, EidosSymbolTable *p_parent_table, bool p_start_with_hash=false);	// standard constructor
	~EidosSymbolTable(void);																							// destructor
	
	// symbol access; these are variables defined in the global namespace
	inline __attribute__((always_inline)) std::vector<std::string> ReadOnlySymbols(void) const { return _SymbolNames(true, false); }
	inline __attribute__((always_inline)) std::vector<std::string> ReadWriteSymbols(void) const { return _SymbolNames(false, true); }
	inline __attribute__((always_inline)) std::vector<std::string> AllSymbols(void) const { return _SymbolNames(true, true); }
	
	// Test for containing a value for a symbol; ContainsSymbol() searches through the chain of symbol tables that are in scope, whereas
	// SymbolDefinedAnywhere() looks through all parent tables regardless of scope.
	bool ContainsSymbol(EidosGlobalStringID p_symbol_name) const;
	bool ContainsSymbol_IsConstant(EidosGlobalStringID p_symbol_name, bool *p_is_const) const;
	bool SymbolDefinedAnywhere(EidosGlobalStringID p_symbol_name) const;
	
	// Set as a variable (raises if already defined as a constant); the NoCopy version is *not* what you want, almost certainly (see it for comments)
	void SetValueForSymbol(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value);
	void SetValueForSymbolNoCopy(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value);
	
	// Set as a constant (raises if already defined as a variable or a constant); adds to the kEidosDefinedConstantsTable, creating it if necessary
	void DefineConstantForSymbol(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value);
	
	// Remove symbols; RemoveValueForSymbol() will raise if the symbol is a constant
	inline __attribute__((always_inline)) void RemoveValueForSymbol(EidosGlobalStringID p_symbol_name) { _RemoveSymbol(p_symbol_name, false); }
	inline __attribute__((always_inline)) void RemoveConstantForSymbol(EidosGlobalStringID p_symbol_name) { _RemoveSymbol(p_symbol_name, true); }
	
	// Get a value, with an optional token used if the call raises due to an undefined symbol
	inline __attribute__((always_inline)) EidosValue_SP GetValueOrRaiseForASTNode(const EidosASTNode *p_symbol_node) const { return _GetValue(p_symbol_node->cached_stringID_, p_symbol_node->token_); }
	inline __attribute__((always_inline)) EidosValue_SP GetValueOrRaiseForSymbol(EidosGlobalStringID p_symbol_name) const { return _GetValue(p_symbol_name, nullptr); }
	
	// Special getters that return a boolean flag, true if the fetched symbol is a constant
	inline __attribute__((always_inline)) EidosValue_SP GetValueOrRaiseForASTNode_IsConst(const EidosASTNode *p_symbol_node, bool *p_is_const) const { return _GetValue_IsConst(p_symbol_node->cached_stringID_, p_symbol_node->token_, p_is_const); }
	inline __attribute__((always_inline)) EidosValue_SP GetValueOrRaiseForSymbol_IsConst(EidosGlobalStringID p_symbol_name, bool *p_is_const) const { return _GetValue_IsConst(p_symbol_name, nullptr, p_is_const); }
	
	// Special-purpose methods used for fast setup of new symbol tables with constants.
	//
	// These methods assume (1) that the name string is a global constant that does not need to be copied and
	// has infinite lifespan, and (2) that the EidosValue passed in is not invisible and is thus suitable for
	// direct use in the symbol table; no copy will be made of the value.  These are not general-purpose methods,
	// they are specifically for the very specialized init case of setting up a table with standard entries.
	inline __attribute__((always_inline)) void InitializeConstantSymbolEntry(EidosSymbolTableEntry &p_new_entry) { _InitializeConstantSymbolEntry(p_new_entry.first, p_new_entry.second); }
	inline __attribute__((always_inline)) void InitializeConstantSymbolEntry(EidosGlobalStringID p_symbol_name, EidosValue_SP p_value) { _InitializeConstantSymbolEntry(p_symbol_name, std::move(p_value)); }
	
	// A utility method to add entries for defined symbols into an EidosTypeTable
	void AddSymbolsToTypeTable(EidosTypeTable *p_type_table) const;
	
	// Direct access to the symbol table chain.  This should only be necessary for clients that are manipulating
	// the symbol table chain themselves in some way, since normally the chain is encapsulated by this class.
	inline __attribute__((always_inline)) EidosSymbolTable *ChainSymbolTable(void) { return chain_symbol_table_; }
	inline __attribute__((always_inline)) EidosSymbolTable *ParentSymbolTable(void) { return parent_symbol_table_; }
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosSymbolTable &p_symbols);


#endif /* defined(__Eidos__eidos_symbol_table__) */





































































