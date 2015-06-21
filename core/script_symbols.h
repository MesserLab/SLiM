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
 
 A symbol table is basically just a C++ map of identifiers to ScriptValue objects.  However, there are some additional smarts,
 particularly where memory management is concerned.  ScriptValue objects can have one of three memory management statuses:
 (1) temporary, such that the current scope owns the value and should delete it before exiting, (2) externally owned, such that
 the SymbolTable machinery just handles the pointer but does not delete it, or (3) ina symbol table, such that temporary users
 of the object do not delete it, but the SymbolTable will delete it if it is removed from the table.
 
 */

#ifndef __SLiM__script_symbols__
#define __SLiM__script_symbols__

#include <iostream>
#include <vector>
#include <string>
#include <map>


class ScriptValue;


class SymbolTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	std::map<const std::string, ScriptValue*> constants_;
	std::map<const std::string, ScriptValue*> variables_;
	
public:
	
	SymbolTable(const SymbolTable&) = delete;				// no copying
	SymbolTable& operator=(const SymbolTable&) = delete;	// no copying
	SymbolTable(void);										// standard constructor
	virtual ~SymbolTable(void);								// destructor
	
	// member access; these are variables defined in the global namespace
	virtual std::vector<std::string> ReadOnlySymbols(void) const;
	virtual std::vector<std::string> ReadWriteSymbols(void) const;
	virtual ScriptValue *GetValueForSymbol(const std::string &p_symbol_name) const;
	virtual ScriptValue *GetValueOrNullForSymbol(const std::string &p_symbol_name) const;		// safe to call with any string
	virtual void SetValueForSymbol(const std::string &p_symbol_name, ScriptValue *p_value);
	virtual void SetConstantForSymbol(const std::string &p_symbol_name, ScriptValue *p_value);
	virtual void RemoveValueForSymbol(const std::string &p_symbol_name, bool remove_constant);
};

std::ostream &operator<<(std::ostream &p_outstream, const SymbolTable &p_symbols);


#endif /* defined(__SLiM__script_symbols__) */





































































