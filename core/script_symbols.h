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

#ifndef __SLiM__script_symbols__
#define __SLiM__script_symbols__

#include <iostream>
#include <vector>
#include <string>
#include <map>


class ScriptValue;
class SymbolHost;


// These classes represent an lvalue reference: a symbol host representing a scope or a value-containing object,
// and a value specifier: an identifier (foo.bar : foo is the symbol host, "bar" is the identifier) or a
// subscript index (foo[7] : foo is the symbol host, index 7 is the subscript index).
class LValueReference
{
protected:
	SymbolHost *symbol_host_;
public:
	LValueReference(const LValueReference&) = delete;								// no copying
	LValueReference& operator=(const LValueReference&) = delete;					// no copying
	LValueReference(void) = delete;													// no default constructor
	virtual ~LValueReference(void);
	
	explicit LValueReference(SymbolHost *p_symbol_host);
	
	virtual void SetLValueToValue(ScriptValue *p_rvalue) const = 0;
};

class LValueMemberReference : public LValueReference
{
private:
	std::string member_identifier_;
public:
	LValueMemberReference(const LValueMemberReference&) = delete;					// no copying
	LValueMemberReference& operator=(const LValueMemberReference&) = delete;		// no copying
	LValueMemberReference(void) = delete;											// no default constructor
	virtual ~LValueMemberReference(void);
	
	LValueMemberReference(SymbolHost *p_symbol_host, std::string p_member_identifier);
	
	virtual void SetLValueToValue(ScriptValue *p_rvalue) const;
};

class LValueSubscriptReference : public LValueReference
{
private:
	int subscript_index_;
public:
	LValueSubscriptReference(const LValueSubscriptReference&) = delete;				// no copying
	LValueSubscriptReference& operator=(const LValueSubscriptReference&) = delete;	// no copying
	LValueSubscriptReference(void) = delete;										// no default constructor
	virtual ~LValueSubscriptReference(void);
	
	LValueSubscriptReference(SymbolHost *p_symbol_host, int p_subscript_index);
	
	virtual void SetLValueToValue(ScriptValue *p_rvalue) const;
};


// This class represents any kind of symbol-bearing object: a symbol table, or an object that has members,
// or an object that can be subscripted to get a ScriptValue.  All ScriptValue objects are SymbolHosts,
// which is why the class is defined here.  Note that SymbolHost is an abstract base class.
class SymbolHost
{
public:
	SymbolHost(const SymbolHost &original) = delete;		// no copying
	SymbolHost& operator=(const SymbolHost&) = delete;		// no copying
	
	SymbolHost(void) = default;								// null constructor for base class
	virtual ~SymbolHost(void) = 0;							// virtual destructor
	
	// subscript access
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const = 0;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value) = 0;
	
	// member access
	virtual std::vector<std::string> ReadOnlyMembers(void) const = 0;
	virtual std::vector<std::string> ReadWriteMembers(void) const = 0;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name) const = 0;
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value) = 0;
	
	void Print(std::ostream &p_outstream) const;
};

std::ostream &operator<<(std::ostream &p_outstream, const SymbolHost &p_symbols);


// A symbol table is basically just a C++ map of identifiers to ScriptValue objects
class SymbolTable : public SymbolHost
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
private:
	
	std::map<const std::string, ScriptValue*> symbols_;
	
public:
	
	SymbolTable(const SymbolTable&) = delete;				// no copying
	SymbolTable& operator=(const SymbolTable&) = delete;	// no copying
	SymbolTable(void);										// standard constructor
	virtual ~SymbolTable(void);								// destructor
	
	// subscript access; these are meaningless, and so are defined to raise
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	// member access; these are variables defined in the global namespace
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name) const;
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
};


#endif /* defined(__SLiM__script_symbols__) */





































































