//
//  spatial_map.h
//  SLiM
//
//  Created by Ben Haller on 9/4/23.
//  Copyright (c) 2023 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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
 
 The class SpatialMap represents a map of spatial values that define a property, such as elevation, temperature,
 rainfall, habitability, food availability, or local carrying capacity, for a spatial landscape.  A spatial map
 can be a 1D, 2D, or 3D grid of values for points in space.  Besides being a sort of N-dimensional data structure
 structure to fill the void made by the lack of an array type in Eidos, it also manages interpolation, rescaling
 to fit the spatial bounds of the subpopulation, color mapping, and other miscellaneous issues.
 
 */

#ifndef __SLiM__spatial_map__
#define __SLiM__spatial_map__

#include "slim_globals.h"
#include "eidos_value.h"
#include "eidos_symbol_table.h"
#include "eidos_class_Dictionary.h"

class Subpopulation;
class SpatialKernel;


#pragma mark -
#pragma mark SpatialMap
#pragma mark -

extern EidosClass *gSLiM_SpatialMap_Class;

class SpatialMap : public EidosDictionaryRetained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryRetained super;

	void _ValuesChanged(void);
	EidosValue_SP _DeriveTemporarySpatialMapWithEidosValue(EidosValue *p_argument, const std::string &p_code_name, const std::string &p_eidos_name);
	
public:
	
	std::string name_;					// the name of the spatial map, used in SLiMgui and required to be unique
	slim_usertag_t tag_value_;			// a user-defined tag value
	
	std::string spatiality_string_;		// "x", "y", "z", "xy", "xz", "yz", or "xyz": the spatial dimensions for the map
	int spatiality_;					// 1, 2, or 3 for 1D, 2D, or 3D: the number of spatial dimensions
	int spatiality_type_;				// 1=="x", 2=="y", 3=="z", 4=="xy", 5=="yz", 6=="xz", 7=="xyz"
	bool periodic_a_, periodic_b_, periodic_c_;		// periodic boundary flags for spatiality dimensions a/b/c
	
	int required_dimensionality_;		// 1, 2, or 3 for the dimensionality we require; enough to encompass spatiality_type_
	
	double bounds_a0_, bounds_a1_;		// bounds in our first spatiality dimension
	double bounds_b0_, bounds_b1_;		// bounds in our second spatiality dimension, if used
	double bounds_c0_, bounds_c1_;		// bounds in our third spatiality dimension, if used
	
	int64_t grid_size_[3];				// the number of points in the first, second, and third spatial dimensions
	int64_t values_size_;				// the number of values in values_ (the product of grid_size_)
	double *values_ = nullptr;			// OWNED POINTER: the values for the grid points
	bool interpolate_;					// if true, the map will interpolate values; otherwise, nearest-neighbor
	double values_min_, values_max_;	// min/max of values_; re-evaluated every time our data changes
	
	int n_colors_ = 0;						// the number of color values given to map across the min/max value range
	double colors_min_, colors_max_;	// min/max for our color gradient
	float *red_components_ = nullptr;	// OWNED POINTER: red components, n_colors_ in size, from min to max value
	float *green_components_ = nullptr;	// OWNED POINTER: green components, n_colors_ in size, from min to max value
	float *blue_components_ = nullptr;	// OWNED POINTER: blue components, n_colors_ in size, from min to max value
	
#if defined(SLIMGUI)
	uint8_t *display_buffer_ = nullptr;	// OWNED POINTER: used by SLiMgui, contains RGB values for pixels in the PopulationView
	int buffer_width_, buffer_height_;	// the size of the buffer, in pixels, each of which is 3 x sizeof(uint8_t)
#endif
	
	SpatialMap(const SpatialMap&) = delete;													// no copying
	SpatialMap& operator=(const SpatialMap&) = delete;										// no copying
	SpatialMap(void) = delete;																// no null construction
	SpatialMap(std::string p_name, std::string p_spatiality_string, Subpopulation *p_subpop, EidosValue *p_values, bool p_interpolate, EidosValue *p_value_range, EidosValue *p_colors);
	SpatialMap(std::string p_name, SpatialMap &p_original);
	~SpatialMap(void);
	
	void TakeColorsFromEidosValues(EidosValue *p_value_range, EidosValue *p_colors, const std::string &p_code_name, const std::string &p_eidos_name);
	void TakeValuesFromEidosValue(EidosValue *p_values, const std::string &p_code_name, const std::string &p_eidos_name);
	void TakeOverMallocedValues(double *p_values, int64_t p_dimcount, int64_t *p_dimensions);
	bool IsCompatibleWithSubpopulation(Subpopulation *p_subpop);
	bool IsCompatibleWithMap(SpatialMap *p_map);
	bool IsCompatibleWithMapValues(SpatialMap *p_map);
	bool IsCompatibleWithValue(EidosValue *p_value);
	
	double ValueAtPoint_S1(double *p_point);
	double ValueAtPoint_S2(double *p_point);
	double ValueAtPoint_S3(double *p_point);
	void ColorForValue(double p_value, double *p_rgb_ptr);
	void ColorForValue(double p_value, float *p_rgb_ptr);
	
	void Convolve_S1(SpatialKernel &kernel);
	void Convolve_S2(SpatialKernel &kernel);
	void Convolve_S3(SpatialKernel &kernel);
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_add(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_blend(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_multiply(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subtract(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_divide(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_power(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_exp(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_changeColors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_changeValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_gridValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_interpolate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mapColor(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mapImage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mapValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_range(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_rescale(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sampleImprovedNearbyPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sampleNearbyPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_smooth(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class SpatialMap_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	SpatialMap_Class(const SpatialMap_Class &p_original) = delete;	// no copy-construct
	SpatialMap_Class& operator=(const SpatialMap_Class&) = delete;	// no copying
	inline SpatialMap_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
};


#endif /* __SLiM__spatial_map__ */
