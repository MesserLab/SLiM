//
//  eidos_class_DataFrame.h
//  Eidos
//
//  Created by Ben Haller on 10/10/21.
//  Copyright (c) 2021-2023 Philipp Messer.  All rights reserved.
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
 
 The class EidosDataFrame provides a simple dataframe-like object that inherits from Dictionary
 
 */

#ifndef __Eidos__eidos_class_dataframe__
#define __Eidos__eidos_class_dataframe__


#include "eidos_value.h"


extern EidosClass *gEidosDataFrame_Class;


class EidosDataFrame : public EidosDictionaryRetained
{
private:
	typedef EidosDictionaryRetained super;
	
protected:
	// We keep a user-defined order for our keys, overriding Dictionary's sorted key order
	std::vector<std::string> sorted_keys_;
	
	virtual void Raise_UsesStringKeys() const override;
	
public:
	EidosDataFrame(const EidosDataFrame &p_original) = delete;		// no copy-construct
	EidosDataFrame& operator=(const EidosDataFrame&) = delete;		// no copying
	inline EidosDataFrame(void) { }
	inline virtual ~EidosDataFrame(void) override { }
	
	int ColumnCount(void) const { return KeyCount(); }				// the number of columns
	int RowCount(void) const;										// the number of rows (for every column)
	
	// Non-const methods: callers of these methods must ensure that ContentsChanged() is called!
	EidosDataFrame *SubsetColumns(EidosValue *index_value);
	EidosDataFrame *SubsetRows(EidosValue *index_value, bool drop = false);
	
	// custom behaviors for string/integer keys: DataFrame does not allow integer keys
	virtual bool KeysAreStrings(void) const override { return true; }
	virtual bool KeysAreIntegers(void) const override { return false; }
	
	// Provides the keys in the user-visible order: sorted for Dictionary, user-defined for DataFrame
	virtual std::vector<std::string> SortedKeys_StringKeys(void) const override;
	virtual std::vector<int64_t> SortedKeys_IntegerKeys(void) const override;
	
	// custom behaviors for addition/removal of keys (don't sort) and contents change (check row lengths)
	virtual void KeyAddedToDictionary_StringKeys(const std::string &p_key) override;
	virtual void KeyAddedToDictionary_IntegerKeys(int64_t p_key) override;
	virtual void KeyRemovedFromDictionary_StringKeys(const std::string &p_key) override;
	virtual void KeyRemovedFromDictionary_IntegerKeys(int64_t p_key) override;
	virtual void AllKeysRemoved(void) override;
	virtual void ContentsChanged(const std::string &p_operation_name) override;
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_asMatrix(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_cbind(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_rbind(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subset(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subsetColumns(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subsetRows(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class EidosDataFrame_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	EidosDataFrame_Class(const EidosDataFrame_Class &p_original) = delete;	// no copy-construct
	EidosDataFrame_Class& operator=(const EidosDataFrame_Class&) = delete;	// no copying
	inline EidosDataFrame_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
};

#endif /* __Eidos__eidos_class_dataframe__ */



















