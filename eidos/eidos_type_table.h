//
//  eidos_type_table.h
//  Eidos
//
//  Created by Ben Haller on 5/8/16.
//  Copyright (c) 2016-2025 Benjamin C. Haller.  All rights reserved.
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
 
 A type table is very much like a symbol table, except that it does not keep values for its symbols, just type information.
 This is used for type-smart code completion in EidosScribe and SLiMgui.  At present EidosTypeTable is implemented with
 a hash table, since we need no additional functionality from it.
 
 */

#ifndef __Eidos__eidos_type_table__
#define __Eidos__eidos_type_table__

#include <iostream>
#include <vector>
#include <string>

#include "eidos_value.h"


#include "eidos_globals.h"
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
typedef robin_hood::unordered_flat_map<EidosGlobalStringID, EidosTypeSpecifier> EidosTypeTableSymbols;
typedef robin_hood::pair<EidosGlobalStringID, EidosTypeSpecifier> EidosTypeTableEntry;
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_map>
typedef std::unordered_map<EidosGlobalStringID, EidosTypeSpecifier> EidosTypeTableSymbols;
typedef std::pair<EidosGlobalStringID, EidosTypeSpecifier> EidosTypeTableEntry;
#endif


class EidosTypeTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	EidosTypeTableSymbols hash_symbols_;
	
public:
	
	EidosTypeTable(const EidosTypeTable &originalTable);								// copy all defined symbols from an original table to a derived table
	EidosTypeTable& operator=(const EidosTypeTable&) = delete;							// no copying
	explicit EidosTypeTable(void);														// standard constructor
	virtual ~EidosTypeTable(void);
	
	// symbol access; these are variables defined in the global namespace
	std::vector<std::string> AllSymbols(void) const;
	std::vector<EidosGlobalStringID> AllSymbolIDs(void) const;
	
	// Test for containing a value for a symbol
	virtual bool ContainsSymbol(EidosGlobalStringID p_symbol_name) const;
	
	// Set as a variable
	void SetTypeForSymbol(EidosGlobalStringID p_symbol_name, EidosTypeSpecifier p_type);
	
	// Remove symbols
	void RemoveTypeForSymbol(EidosGlobalStringID p_symbol_name);
	void RemoveSymbolsOfClass(const EidosClass *p_object_class);
	void RemoveAllSymbols(void);
	
	// Get the type for a symbol
	virtual EidosTypeSpecifier GetTypeForSymbol(EidosGlobalStringID p_symbol_name) const;
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosTypeTable &p_symbols);


#endif /* __Eidos__eidos_type_table__ */






















































