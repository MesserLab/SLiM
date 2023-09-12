//
//  spatial_map.cpp
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


#include "spatial_map.h"
#include "spatial_kernel.h"
#include "subpopulation.h"
#include "eidos_class_Image.h"

#include "gsl_math.h"
#include "gsl_spline.h"
#include "gsl_interp2d.h"
#include "gsl_spline2d.h"


// Clamp a standardized coordinate, which should be in [0,1], to [0,1].
#define SLiMClampCoordinate(x) ((x < 0.0) ? 0.0 : ((x > 1.0) ? 1.0 : x))


#pragma mark -
#pragma mark SpatialMap
#pragma mark -

SpatialMap::SpatialMap(std::string p_name, std::string p_spatiality_string, Subpopulation *p_subpop, EidosValue *p_values, bool p_interpolate, EidosValue *p_value_range, EidosValue *p_colors) :
	name_(p_name), tag_value_(SLIM_TAG_UNSET_VALUE), spatiality_string_(p_spatiality_string), interpolate_(p_interpolate)
{
	// The spatiality string determines what dimensionality we require for subpops using us; it must be large enough to
	// encompass our spatiality ("xyz" to encompass "xz", for example).  It also determines how many dimensions of map
	// data we contain, which is spatiality_ (1, 2, or 3), and which spatial boundary components we standardize to,
	// which is spatiality_type_ (just an integer representation of spatiality_string_, really).  Finally, it copies
	// over the relevant portions of the reference subpopulation's bounds for our spatiality.
	if (spatiality_string_.compare(gEidosStr_x) == 0) {
		required_dimensionality_ = 1;
		spatiality_ = 1;
		spatiality_type_ = 1;
		bounds_a0_ = p_subpop->bounds_x0_;
		bounds_a1_ = p_subpop->bounds_x1_;
		bounds_b0_ = 0;
		bounds_b1_ = 0;
		bounds_c0_ = 0;
		bounds_c1_ = 0;
	}
	else if (spatiality_string_.compare(gEidosStr_y) == 0) {
		required_dimensionality_ = 2;
		spatiality_ = 1;
		spatiality_type_ = 2;
		bounds_a0_ = p_subpop->bounds_y0_;
		bounds_a1_ = p_subpop->bounds_y1_;
		bounds_b0_ = 0;
		bounds_b1_ = 0;
		bounds_c0_ = 0;
		bounds_c1_ = 0;
	}
	else if (spatiality_string_.compare(gEidosStr_z) == 0) {
		required_dimensionality_ = 3;
		spatiality_ = 1;
		spatiality_type_ = 3;
		bounds_a0_ = p_subpop->bounds_z0_;
		bounds_a1_ = p_subpop->bounds_z1_;
		bounds_b0_ = 0;
		bounds_b1_ = 0;
		bounds_c0_ = 0;
		bounds_c1_ = 0;
	}
	else if (spatiality_string_.compare("xy") == 0) {
		required_dimensionality_ = 2;
		spatiality_ = 2;
		spatiality_type_ = 4;
		bounds_a0_ = p_subpop->bounds_x0_;
		bounds_a1_ = p_subpop->bounds_x1_;
		bounds_b0_ = p_subpop->bounds_y0_;
		bounds_b1_ = p_subpop->bounds_y1_;
		bounds_c0_ = 0;
		bounds_c1_ = 0;
	}
	else if (spatiality_string_.compare("xz") == 0) {
		required_dimensionality_ = 3;
		spatiality_ = 2;
		spatiality_type_ = 5;
		bounds_a0_ = p_subpop->bounds_x0_;
		bounds_a1_ = p_subpop->bounds_x1_;
		bounds_b0_ = p_subpop->bounds_z0_;
		bounds_b1_ = p_subpop->bounds_z1_;
		bounds_c0_ = 0;
		bounds_c1_ = 0;
	}
	else if (spatiality_string_.compare("yz") == 0) {
		required_dimensionality_ = 3;
		spatiality_ = 2;
		spatiality_type_ = 6;
		bounds_a0_ = p_subpop->bounds_y0_;
		bounds_a1_ = p_subpop->bounds_y1_;
		bounds_b0_ = p_subpop->bounds_z0_;
		bounds_b1_ = p_subpop->bounds_z1_;
		bounds_c0_ = 0;
		bounds_c1_ = 0;
	}
	else if (spatiality_string_.compare("xyz") == 0) {
		required_dimensionality_ = 3;
		spatiality_ = 3;
		spatiality_type_ = 7;
		bounds_a0_ = p_subpop->bounds_x0_;
		bounds_a1_ = p_subpop->bounds_x1_;
		bounds_b0_ = p_subpop->bounds_y0_;
		bounds_b1_ = p_subpop->bounds_y1_;
		bounds_c0_ = p_subpop->bounds_z0_;
		bounds_c1_ = p_subpop->bounds_z1_;
	}
	else
		EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): defineSpatialMap() spatiality \"" << spatiality_string_ << "\" must be \"x\", \"y\", \"z\", \"xy\", \"xz\", \"yz\", or \"xyz\"." << EidosTerminate();
	
	TakeValuesFromEidosValue(p_values, "SpatialMap::SpatialMap", "defineSpatialMap()");
	
	// Make our color map
	const double *values_float_vec_ptr = (p_values->Type() == EidosValueType::kValueFloat) ? p_values->FloatVector()->data() : nullptr;
	const int64_t *values_integer_vec_ptr = (p_values->Type() == EidosValueType::kValueInt) ? p_values->IntVector()->data() : nullptr;
	
	assert(values_float_vec_ptr || values_integer_vec_ptr);
	
	bool range_is_null = (p_value_range->Type() == EidosValueType::kValueNULL);
	bool colors_is_null = (p_colors->Type() == EidosValueType::kValueNULL);
	
	min_value_ = 0.0;
	max_value_ = 0.0;
	n_colors_ = 0;
	
	if (!range_is_null || !colors_is_null)
	{
		if (range_is_null || colors_is_null)
			EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): defineSpatialMap() valueRange and colors must either both be supplied, or neither supplied." << EidosTerminate();
		
		if (p_value_range->Count() != 2)
			EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): defineSpatialMap() valueRange must be exactly length 2 (giving the min and max value permitted)." << EidosTerminate();
		
		// valueRange and colors were provided, so use them for coloring
		min_value_ = p_value_range->FloatAtIndex(0, nullptr);
		max_value_ = p_value_range->FloatAtIndex(1, nullptr);
		
		if (!std::isfinite(min_value_) || !std::isfinite(max_value_) || (min_value_ > max_value_))
			EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): defineSpatialMap() valueRange must be finite, and min <= max is required." << EidosTerminate();
		
		n_colors_ = p_colors->Count();
		
		if (n_colors_ < 2)
			EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): defineSpatialMap() colors must be of length >= 2." << EidosTerminate();
	}
	else
	{
		// so that we can provide a default color map, we find the value range here
		SetAutomaticColorMinMax();
	}
	
	// Allocate buffers to hold our color component vectors, if we were supplied with color info
	if (n_colors_ > 0)
	{
		red_components_ = (float *)malloc(n_colors_ * sizeof(float));
		green_components_ = (float *)malloc(n_colors_ * sizeof(float));
		blue_components_ = (float *)malloc(n_colors_ * sizeof(float));
		
		if (!red_components_ || !green_components_ || !blue_components_)
			EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		const std::string *colors_vec_ptr = p_colors->StringVector()->data();
		
		for (int colors_index = 0; colors_index < n_colors_; ++colors_index)
			Eidos_GetColorComponents(colors_vec_ptr[colors_index], red_components_ + colors_index, green_components_ + colors_index, blue_components_ + colors_index);
	}
	else
	{
		red_components_ = nullptr;
		green_components_ = nullptr;
		blue_components_ = nullptr;
	}
}

SpatialMap::SpatialMap(std::string p_name, SpatialMap &p_original) :
	name_(p_name), tag_value_(SLIM_TAG_UNSET_VALUE), spatiality_string_(p_original.spatiality_string_), spatiality_(p_original.spatiality_), spatiality_type_(p_original.spatiality_type_), required_dimensionality_(p_original.required_dimensionality_), bounds_a0_(p_original.bounds_a0_), bounds_a1_(p_original.bounds_a1_), bounds_b0_(p_original.bounds_b0_), bounds_b1_(p_original.bounds_b1_), bounds_c0_(p_original.bounds_c0_), bounds_c1_(p_original.bounds_c1_), interpolate_(p_original.interpolate_), min_value_(p_original.min_value_), max_value_(p_original.max_value_), n_colors_(p_original.n_colors_)
{
	// Note that this does not copy the information from EidosDictionaryRetained, and it leaves tag unset
	// This is intentional (that is very instance-specific state that should arguably not be copied)
	
	// Copy over our grid dimensions
	grid_size_[0] = p_original.grid_size_[0];
	grid_size_[1] = p_original.grid_size_[1];
	grid_size_[2] = p_original.grid_size_[2];
	values_size_ = p_original.values_size_;
	
	// Copy over the map values
	values_ = (double *)malloc(values_size_ * sizeof(double));
	if (!values_)
		EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	memcpy(values_, p_original.values_, values_size_ * sizeof(double));
	
	// Copy color mapping components
	if (n_colors_)
	{
		red_components_ = (float *)malloc(n_colors_ * sizeof(float));
		green_components_ = (float *)malloc(n_colors_ * sizeof(float));
		blue_components_ = (float *)malloc(n_colors_ * sizeof(float));
		
		if (!red_components_ || !green_components_ || !blue_components_)
			EIDOS_TERMINATION << "ERROR (SpatialMap::SpatialMap): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		memcpy(red_components_, p_original.red_components_, n_colors_ * sizeof(float));
		memcpy(green_components_, p_original.green_components_, n_colors_ * sizeof(float));
		memcpy(blue_components_, p_original.blue_components_, n_colors_ * sizeof(float));
	}
	else
	{
		red_components_ = nullptr;
		green_components_ = nullptr;
		blue_components_ = nullptr;
	}
}

SpatialMap::~SpatialMap(void)
{
	if (values_)
		free(values_);
	
	if (red_components_)
		free(red_components_);
	if (green_components_)
		free(green_components_);
	if (blue_components_)
		free(blue_components_);
	
#if defined(SLIMGUI)
	if (display_buffer_)
		free(display_buffer_);
#endif
}

void SpatialMap::InvalidateDisplayBuffer(void)
{
#if defined(SLIMGUI)
	// Force a display image recache in SLiMgui
	if (display_buffer_)
	{
		free(display_buffer_);
		display_buffer_ = nullptr;
	}
#endif
}

void SpatialMap::SetAutomaticColorMinMax(void)
{
	// This assesses the minimum and maximum values for the default grayscale color ramp
	min_value_ = max_value_ = values_[0];
	
	for (int64_t values_index = 1; values_index < values_size_; ++values_index)
	{
		double value = values_[values_index];
		
		min_value_ = std::min(min_value_, value);
		max_value_ = std::max(max_value_, value);
	}
	
	if (!std::isfinite(min_value_) || !std::isfinite(max_value_))
	{
		min_value_ = 0.0;
		max_value_ = 0.0;
	}
	
	InvalidateDisplayBuffer();
}

void SpatialMap::TakeValuesFromEidosValue(EidosValue *p_values, std::string p_code_name, std::string p_eidos_name)
{
	int values_dimcount = p_values->DimensionCount();
	const int64_t *values_dim = p_values->Dimensions();
	
	if (values_dimcount != spatiality_)
		EIDOS_TERMINATION << "ERROR (" << p_code_name << "): " << p_eidos_name << " the dimensionality of the supplied vector/matrix/array does not match the spatiality defined for the map." << EidosTerminate();
	
	int dimension_index;
	values_size_ = 1;
	
	for (dimension_index = 0; dimension_index < spatiality_; ++dimension_index)
	{
		int64_t dimension_size = (values_dimcount == 1) ? p_values->Count() : values_dim[dimension_index];	// treat a vector as a 1D matrix
		
		if (dimension_size < 2)
			EIDOS_TERMINATION << "ERROR (" << p_code_name << "): " << p_eidos_name << " all dimensions of value must be of size >= 2." << EidosTerminate();
		
		grid_size_[dimension_index] = dimension_size;
		values_size_ *= dimension_size;
	}
	for ( ; dimension_index < 3; ++dimension_index)
		grid_size_[dimension_index] = 0;
	
	// Matrices and arrays use dim[0] as the number of rows, and dim[1] as the number of cols; spatial maps do the opposite,
	// following standard image conventions (by row, not by column); we therefore need to swap grid_size_[0] and grid_size_[1]
	if (spatiality_ >= 2)
		std::swap(grid_size_[0], grid_size_[1]);
	
	// Allocate a values buffer of the proper size
	values_ = (double *)malloc(values_size_ * sizeof(double));
	if (!values_)
		EIDOS_TERMINATION << "ERROR (" << p_code_name << "): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// Take the values we were passed in
	const double *values_float_vec_ptr = (p_values->Type() == EidosValueType::kValueFloat) ? p_values->FloatVector()->data() : nullptr;
	const int64_t *values_integer_vec_ptr = (p_values->Type() == EidosValueType::kValueInt) ? p_values->IntVector()->data() : nullptr;
	
	if (spatiality_ == 1)
	{
		// A vector was passed (since no matrix dimension here is allowed to have a size of 1), so no transpose/flip needed
		// The vector values will be read left to right, or bottom to top, following SLiM's Cartesian spatial coordinates
		if (values_float_vec_ptr)
			for (int64_t values_index = 0; values_index < values_size_; ++values_index)
				values_[values_index] = *(values_float_vec_ptr++);
		else
			for (int64_t values_index = 0; values_index < values_size_; ++values_index)
				values_[values_index] = *(values_integer_vec_ptr++);
	}
	else if (spatiality_ >= 2)
	{
		// A matrix/array was passed (it is no longer legal to pass a vector in the multidimensional case)
		// A transpose/flip is therefore needed, because matrices are stored by row and read top to bottom
		int64_t col_count = grid_size_[0];		// note grid_size_ got swapped above
		int64_t row_count = grid_size_[1];
		int64_t plane_count = (spatiality_ == 3) ? grid_size_[2] : 1;
		
		if (values_float_vec_ptr)
		{
			for (int64_t z = 0; z < plane_count; ++z)
			{
				int64_t plane_offset = z * (row_count * col_count);
				
				for (int64_t x = 0; x < col_count; ++x)
					for (int64_t y = 0; y < row_count; ++y)
						values_[plane_offset + x + (row_count - 1 - y) * col_count] = values_float_vec_ptr[plane_offset + y + x * row_count];
			}
		}
		else
		{
			for (int64_t z = 0; z < plane_count; ++z)
			{
				int64_t plane_offset = z * (row_count * col_count);
				
				for (int64_t x = 0; x < col_count; ++x)
					for (int64_t y = 0; y < row_count; ++y)
						values_[plane_offset + x + (row_count - 1 - y) * col_count] = values_integer_vec_ptr[plane_offset + y + x * row_count];
			}
		}
	}
	
	InvalidateDisplayBuffer();
	
	// Note that we do not change the min/max or the color map; that is up to the caller, if they wish to do so
}

void SpatialMap::TakeOverMallocedValues(double *p_values, int64_t p_dimcount, int64_t *p_dimensions)
{
	if (p_dimcount != spatiality_)
		EIDOS_TERMINATION << "ERROR (SpatialMap::TakeOverMallocedValues): (internal error) the dimensionality of the supplied values does not match the spatiality defined for the map." << EidosTerminate();
	
	int dimension_index;
	values_size_ = 1;
	
	for (dimension_index = 0; dimension_index < spatiality_; ++dimension_index)
	{
		int64_t dimension_size = p_dimensions[dimension_index];
		
		if (dimension_size < 2)
			EIDOS_TERMINATION << "ERROR (SpatialMap::TakeOverMallocedValues): (internal error) all dimensions of value must be of size >= 2." << EidosTerminate();
		
		grid_size_[dimension_index] = dimension_size;
		values_size_ *= dimension_size;
	}
	for ( ; dimension_index < 3; ++dimension_index)
		grid_size_[dimension_index] = 0;
	
	// Take over the passed buffer
	free(values_);
	values_ = p_values;
	
	InvalidateDisplayBuffer();
	
	// Note that we do not change the min/max or the color map; that is up to the caller, if they wish to do so
}

bool SpatialMap::IsCompatibleWithSubpopulation(Subpopulation *p_subpop)
{
	// This checks that spatiality/dimensionality and bounds are compatible between the spatial map and a given subpopulation
	int spatial_dimensionality = p_subpop->species_.SpatialDimensionality();
	
	if (spatiality_type_ == 1) {			// "x"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_x0_) ||
			(bounds_a1_ != p_subpop->bounds_x1_))
			return false;
	}
	else if (spatiality_type_ == 2) {		// "y"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_y0_) ||
			(bounds_a1_ != p_subpop->bounds_y1_))
			return false;
	}
	else if (spatiality_type_ == 3) {		// "z"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_z0_) ||
			(bounds_a1_ != p_subpop->bounds_z1_))
			return false;
	}
	else if (spatiality_type_ == 4) {		// "xy"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_x0_) ||
			(bounds_a1_ != p_subpop->bounds_x1_) ||
			(bounds_b0_ != p_subpop->bounds_y0_) ||
			(bounds_b1_ != p_subpop->bounds_y1_))
			return false;
	}
	else if (spatiality_type_ == 5) {		// "xz"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_x0_) ||
			(bounds_a1_ != p_subpop->bounds_x1_) ||
			(bounds_b0_ != p_subpop->bounds_z0_) ||
			(bounds_b1_ != p_subpop->bounds_z1_))
			return false;
	}
	else if (spatiality_type_ == 6) {		// "yz"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_y0_) ||
			(bounds_a1_ != p_subpop->bounds_y1_) ||
			(bounds_b0_ != p_subpop->bounds_z0_) ||
			(bounds_b1_ != p_subpop->bounds_z1_))
			return false;
	}
	else if (spatiality_type_ == 7) {		// "xyz"
		if ((required_dimensionality_ > spatial_dimensionality) ||
			(bounds_a0_ != p_subpop->bounds_x0_) ||
			(bounds_a1_ != p_subpop->bounds_x1_) ||
			(bounds_b0_ != p_subpop->bounds_y0_) ||
			(bounds_b1_ != p_subpop->bounds_y1_) ||
			(bounds_c0_ != p_subpop->bounds_z0_) ||
			(bounds_c1_ != p_subpop->bounds_z1_))
			return false;
	}
	
	return true;
}

double SpatialMap::ValueAtPoint_S1(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	assert (spatiality_ == 1);
	
	double x_fraction = p_point[0];
	int64_t xsize = grid_size_[0];
	
	if (interpolate_)
	{
		double x_map = x_fraction * (xsize - 1);
		int x1_map = (int)floor(x_map);
		int x2_map = (int)ceil(x_map);
		double fraction_x2 = x_map - x1_map;
		double fraction_x1 = 1.0 - fraction_x2;
		double value_x1 = values_[x1_map] * fraction_x1;
		double value_x2 = values_[x2_map] * fraction_x2;
		
		return value_x1 + value_x2;
	}
	else
	{
		int x_map = (int)round(x_fraction * (xsize - 1));
		
		return values_[x_map];
	}
}

double SpatialMap::ValueAtPoint_S2(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	assert (spatiality_ == 2);
	
	double x_fraction = p_point[0];
	double y_fraction = p_point[1];
	int64_t xsize = grid_size_[0];
	int64_t ysize = grid_size_[1];
	
	if (interpolate_)
	{
		double x_map = x_fraction * (xsize - 1);
		double y_map = y_fraction * (ysize - 1);
		int x1_map = (int)floor(x_map);
		int y1_map = (int)floor(y_map);
		int x2_map = (int)ceil(x_map);
		int y2_map = (int)ceil(y_map);
		double fraction_x2 = x_map - x1_map;
		double fraction_x1 = 1.0 - fraction_x2;
		double fraction_y2 = y_map - y1_map;
		double fraction_y1 = 1.0 - fraction_y2;
		double value_x1_y1 = values_[x1_map + y1_map * xsize] * fraction_x1 * fraction_y1;
		double value_x2_y1 = values_[x2_map + y1_map * xsize] * fraction_x2 * fraction_y1;
		double value_x1_y2 = values_[x1_map + y2_map * xsize] * fraction_x1 * fraction_y2;
		double value_x2_y2 = values_[x2_map + y2_map * xsize] * fraction_x2 * fraction_y2;
		
		return value_x1_y1 + value_x2_y1 + value_x1_y2 + value_x2_y2;
	}
	else
	{
		int x_map = (int)round(x_fraction * (xsize - 1));
		int y_map = (int)round(y_fraction * (ysize - 1));
		
		return values_[x_map + y_map * xsize];
	}
}

double SpatialMap::ValueAtPoint_S3(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	assert (spatiality_ == 3);
	
	double x_fraction = p_point[0];
	double y_fraction = p_point[1];
	double z_fraction = p_point[2];
	int64_t xsize = grid_size_[0];
	int64_t ysize = grid_size_[1];
	int64_t zsize = grid_size_[2];
	
	if (interpolate_)
	{
		double x_map = x_fraction * (xsize - 1);
		double y_map = y_fraction * (ysize - 1);
		double z_map = z_fraction * (zsize - 1);
		int x1_map = (int)floor(x_map);
		int y1_map = (int)floor(y_map);
		int z1_map = (int)floor(z_map);
		int x2_map = (int)ceil(x_map);
		int y2_map = (int)ceil(y_map);
		int z2_map = (int)ceil(z_map);
		double fraction_x2 = x_map - x1_map;
		double fraction_x1 = 1.0 - fraction_x2;
		double fraction_y2 = y_map - y1_map;
		double fraction_y1 = 1.0 - fraction_y2;
		double fraction_z2 = z_map - z1_map;
		double fraction_z1 = 1.0 - fraction_z2;
		double value_x1_y1_z1 = values_[x1_map + y1_map * xsize + z1_map * xsize * ysize] * fraction_x1 * fraction_y1 * fraction_z1;
		double value_x2_y1_z1 = values_[x2_map + y1_map * xsize + z1_map * xsize * ysize] * fraction_x2 * fraction_y1 * fraction_z1;
		double value_x1_y2_z1 = values_[x1_map + y2_map * xsize + z1_map * xsize * ysize] * fraction_x1 * fraction_y2 * fraction_z1;
		double value_x2_y2_z1 = values_[x2_map + y2_map * xsize + z1_map * xsize * ysize] * fraction_x2 * fraction_y2 * fraction_z1;
		double value_x1_y1_z2 = values_[x1_map + y1_map * xsize + z2_map * xsize * ysize] * fraction_x1 * fraction_y1 * fraction_z2;
		double value_x2_y1_z2 = values_[x2_map + y1_map * xsize + z2_map * xsize * ysize] * fraction_x2 * fraction_y1 * fraction_z2;
		double value_x1_y2_z2 = values_[x1_map + y2_map * xsize + z2_map * xsize * ysize] * fraction_x1 * fraction_y2 * fraction_z2;
		double value_x2_y2_z2 = values_[x2_map + y2_map * xsize + z2_map * xsize * ysize] * fraction_x2 * fraction_y2 * fraction_z2;
		
		return value_x1_y1_z1 + value_x2_y1_z1 + value_x1_y2_z1 + value_x2_y2_z1 + value_x1_y1_z2 + value_x2_y1_z2 + value_x1_y2_z2 + value_x2_y2_z2;
	}
	else
	{
		int x_map = (int)round(x_fraction * (xsize - 1));
		int y_map = (int)round(y_fraction * (ysize - 1));
		int z_map = (int)round(z_fraction * (zsize - 1));
		
		return values_[x_map + y_map * xsize + z_map * xsize * ysize];
	}
}

void SpatialMap::ColorForValue(double p_value, double *p_rgb_ptr)
{
	if (n_colors_ == 0)
	{
		// this is the case when a color table was not defined; here, min could equal max
		// in this case, all values in the map should fall in the interval [min_value_, max_value_]
		double value_fraction = ((min_value_ < max_value_) ? ((p_value - min_value_) / (max_value_ - min_value_)) : 0.0);
		p_rgb_ptr[0] = value_fraction;
		p_rgb_ptr[1] = value_fraction;
		p_rgb_ptr[2] = value_fraction;
	}
	else
	{
		// this is the case when a color table was defined; here, min < max (BCH 10/20/2021: now, can be equal here too)
		// in this case, values in the map may fall outside the interval [min_value_, max_value_]
		double value_fraction = ((min_value_ < max_value_) ? ((p_value - min_value_) / (max_value_ - min_value_)) : 0.0);
		double color_index = value_fraction * (n_colors_ - 1);
		int color_index_1 = (int)floor(color_index);
		int color_index_2 = (int)ceil(color_index);
		
		if (color_index_1 < 0) color_index_1 = 0;
		if (color_index_1 >= n_colors_) color_index_1 = n_colors_ - 1;
		if (color_index_2 < 0) color_index_2 = 0;
		if (color_index_2 >= n_colors_) color_index_2 = n_colors_ - 1;
		
		double color_2_weight = color_index - color_index_1;
		double color_1_weight = 1.0F - color_2_weight;
		
		double red1 = red_components_[color_index_1];
		double green1 = green_components_[color_index_1];
		double blue1 = blue_components_[color_index_1];
		double red2 = red_components_[color_index_2];
		double green2 = green_components_[color_index_2];
		double blue2 = blue_components_[color_index_2];
		
		p_rgb_ptr[0] = (red1 * color_1_weight + red2 * color_2_weight);
		p_rgb_ptr[1] = (green1 * color_1_weight + green2 * color_2_weight);
		p_rgb_ptr[2] = (blue1 * color_1_weight + blue2 * color_2_weight);
	}
}

void SpatialMap::ColorForValue(double p_value, float *p_rgb_ptr)
{
	if (n_colors_ == 0)
	{
		// this is the case when a color table was not defined; here, min could equal max
		// in this case, all values in the map should fall in the interval [min_value_, max_value_]
		float value_fraction = (float)((min_value_ < max_value_) ? ((p_value - min_value_) / (max_value_ - min_value_)) : 0.0);
		p_rgb_ptr[0] = value_fraction;
		p_rgb_ptr[1] = value_fraction;
		p_rgb_ptr[2] = value_fraction;
	}
	else
	{
		// this is the case when a color table was defined; here, min < max (BCH 10/20/2021: now, can be equal here too)
		// in this case, values in the map may fall outside the interval [min_value_, max_value_]
		double value_fraction = ((min_value_ < max_value_) ? ((p_value - min_value_) / (max_value_ - min_value_)) : 0.0);
		double color_index = value_fraction * (n_colors_ - 1);
		int color_index_1 = (int)floor(color_index);
		int color_index_2 = (int)ceil(color_index);
		
		if (color_index_1 < 0) color_index_1 = 0;
		if (color_index_1 >= n_colors_) color_index_1 = n_colors_ - 1;
		if (color_index_2 < 0) color_index_2 = 0;
		if (color_index_2 >= n_colors_) color_index_2 = n_colors_ - 1;
		
		double color_2_weight = color_index - color_index_1;
		double color_1_weight = 1.0F - color_2_weight;
		
		double red1 = red_components_[color_index_1];
		double green1 = green_components_[color_index_1];
		double blue1 = blue_components_[color_index_1];
		double red2 = red_components_[color_index_2];
		double green2 = green_components_[color_index_2];
		double blue2 = blue_components_[color_index_2];
		
		p_rgb_ptr[0] = (float)(red1 * color_1_weight + red2 * color_2_weight);
		p_rgb_ptr[1] = (float)(green1 * color_1_weight + green2 * color_2_weight);
		p_rgb_ptr[2] = (float)(blue1 * color_1_weight + blue2 * color_2_weight);
	}
}

void SpatialMap::Convolve_S1(SpatialKernel &kernel)
{
	if (spatiality_ != 1)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S1): (internal error) map spatiality 1 required." << EidosTerminate();
	if (kernel.dimensionality_ != 1)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S1): (internal error) kernel dimensionality 1 required." << EidosTerminate();
	
	int64_t kernel_dim_a = kernel.dim[0];
	
	if ((kernel_dim_a < 1) || (kernel_dim_a % 2 == 0))
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S1): (internal error) kernel dimensions must be odd." << EidosTerminate();
	
	int64_t dim_a = grid_size_[0];
	double *new_values = (double *)malloc(dim_a * sizeof(double));
	
	if (!new_values)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S1): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// this assumes the kernel's dimensions are symmetrical around its center, and relies on rounding (which is guaranteed)
	int64_t kernel_a_offset = -(kernel_dim_a / 2);
	double *kernel_values = kernel.values_;
	double *new_values_ptr = new_values;
	
	for (int64_t a = 0; a < dim_a; ++a)
	{
		double coverage = ((a == 0) || (a == dim_a - 1)) ? 0.5 : 1.0;
		
		// calculate the kernel's effect at point (a)
		double kernel_total = 0.0;
		double conv_total = 0.0;
		
		for (int64_t kernel_a = 0; kernel_a < kernel_dim_a; kernel_a++)
		{
			int64_t conv_a = a + kernel_a + kernel_a_offset;
			
			// clip to bounds
			if ((conv_a < 0) || (conv_a >= dim_a))
				continue;
			
			// this point is within bounds; add it in to the totals
			double kernel_value = kernel_values[kernel_a] * coverage;
			double pixel_value = values_[conv_a];
			
			// we keep a total of the kernel values that were within bounds, for this point
			kernel_total += kernel_value;
			
			// and we keep a total of the convolution - kernel values times pixel values
			conv_total += kernel_value * pixel_value;
		}
		
		*(new_values_ptr++) = ((kernel_total > 0) ? (conv_total / kernel_total) : 0);
	}
	
	TakeOverMallocedValues(new_values, 1, grid_size_);	// takes new_values from us
}

void SpatialMap::Convolve_S2(SpatialKernel &kernel)
{
	if (spatiality_ != 2)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S2): (internal error) map spatiality 2 required." << EidosTerminate();
	if (kernel.dimensionality_ != 2)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S2): (internal error) kernel dimensionality 2 required." << EidosTerminate();
	
	int64_t kernel_dim_a = kernel.dim[0];
	int64_t kernel_dim_b = kernel.dim[1];
	
	if ((kernel_dim_a < 1) || (kernel_dim_a % 2 == 0) ||
		(kernel_dim_b < 1) || (kernel_dim_b % 2 == 0))
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S2): (internal error) kernel dimensions must be odd." << EidosTerminate();
	
	int64_t dim_a = grid_size_[0], dim_b = grid_size_[1];
	double *new_values = (double *)malloc(dim_a * dim_b * sizeof(double));
	
	if (!new_values)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S2): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// this assumes the kernel's dimensions are symmetrical around its center, and relies on rounding (which is guaranteed)
	int64_t kernel_a_offset = -(kernel_dim_a / 2), kernel_b_offset = -(kernel_dim_b / 2);
	double *kernel_values = kernel.values_;
	double *new_values_ptr = new_values;
	
	for (int64_t b = 0; b < dim_b; ++b)
	{
		double coverage_b = ((b == 0) || (b == dim_b - 1)) ? 0.5 : 1.0;
		
		for (int64_t a = 0; a < dim_a; ++a)
		{
			double coverage_a = ((a == 0) || (a == dim_a - 1)) ? 0.5 : 1.0;
			double coverage = coverage_a * coverage_b;					// handles partial coverage at the edges of the spatial map
			
			// calculate the kernel's effect at point (a,b)
			double kernel_total = 0.0;
			double conv_total = 0.0;
			
			for (int64_t kernel_a = 0; kernel_a < kernel_dim_a; kernel_a++)
			{
				int64_t conv_a = a + kernel_a + kernel_a_offset;
				
				// handle bounds: either clip or wrap
				if ((conv_a < 0) || (conv_a >= dim_a))
					continue;
				
				for (int64_t kernel_b = 0; kernel_b < kernel_dim_b; kernel_b++)
				{
					int64_t conv_b = b + kernel_b + kernel_b_offset;
					
					// handle bounds: either clip or wrap
					if ((conv_b < 0) || (conv_b >= dim_b))
						continue;
					
					// this point is within bounds; add it in to the totals
					double kernel_value = kernel_values[kernel_a + kernel_b * kernel_dim_a] * coverage;
					double pixel_value = values_[conv_a + conv_b * dim_a];
					
					// we keep a total of the kernel values that were within bounds, for this point
					kernel_total += kernel_value;
					
					// and we keep a total of the convolution - kernel values times pixel values
					conv_total += kernel_value * pixel_value;
				}
			}
			
			*(new_values_ptr++) = ((kernel_total > 0) ? (conv_total / kernel_total) : 0);
		}
	}
	
	TakeOverMallocedValues(new_values, 2, grid_size_);	// takes new_values from us
}

void SpatialMap::Convolve_S3(SpatialKernel &kernel)
{
	if (spatiality_ != 3)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S3): (internal error) map spatiality 3 required." << EidosTerminate();
	if (kernel.dimensionality_ != 3)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S3): (internal error) kernel dimensionality 3 required." << EidosTerminate();
	
	int64_t kernel_dim_a = kernel.dim[0];
	int64_t kernel_dim_b = kernel.dim[1];
	int64_t kernel_dim_c = kernel.dim[2];
	
	if ((kernel_dim_a < 1) || (kernel_dim_a % 2 == 0) ||
		(kernel_dim_b < 1) || (kernel_dim_b % 2 == 0) ||
		(kernel_dim_c < 1) || (kernel_dim_c % 2 == 0))
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S3): (internal error) kernel dimensions must be odd." << EidosTerminate();
	
	int64_t dim_a = grid_size_[0], dim_b = grid_size_[1], dim_c = grid_size_[2];
	double *new_values = (double *)malloc(dim_a * dim_b * dim_c * sizeof(double));
	
	if (!new_values)
		EIDOS_TERMINATION << "ERROR (SpatialMap::Convolve_S3): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// this assumes the kernel's dimensions are symmetrical around its center, and relies on rounding (which is guaranteed)
	int64_t kernel_a_offset = -(kernel_dim_a / 2), kernel_b_offset = -(kernel_dim_b / 2), kernel_c_offset = -(kernel_dim_c / 2);
	double *kernel_values = kernel.values_;
	double *new_values_ptr = new_values;
	
	for (int64_t c = 0; c < dim_c; ++c)
	{
		double coverage_c = ((c == 0) || (c == dim_c - 1)) ? 0.5 : 1.0;
		
		for (int64_t b = 0; b < dim_b; ++b)
		{
			double coverage_b = ((b == 0) || (b == dim_b - 1)) ? 0.5 : 1.0;
			
			for (int64_t a = 0; a < dim_a; ++a)
			{
				double coverage_a = ((a == 0) || (a == dim_a - 1)) ? 0.5 : 1.0;
				double coverage = coverage_a * coverage_b * coverage_c;			// handles partial coverage at the edges of the spatial map
				
				// calculate the kernel's effect at point (a,b,c)
				double kernel_total = 0.0;
				double conv_total = 0.0;
				
				for (int64_t kernel_a = 0; kernel_a < kernel_dim_a; kernel_a++)
				{
					int64_t conv_a = a + kernel_a + kernel_a_offset;
					
					// handle bounds: either clip or wrap
					if ((conv_a < 0) || (conv_a >= dim_a))
						continue;
					
					for (int64_t kernel_b = 0; kernel_b < kernel_dim_b; kernel_b++)
					{
						int64_t conv_b = b + kernel_b + kernel_b_offset;
						
						// handle bounds: either clip or wrap
						if ((conv_b < 0) || (conv_b >= dim_b))
							continue;
						
						for (int64_t kernel_c = 0; kernel_c < kernel_dim_c; kernel_c++)
						{
							int64_t conv_c = c + kernel_c + kernel_c_offset;
							
							// handle bounds: either clip or wrap
							if ((conv_c < 0) || (conv_c >= dim_c))
								continue;
							
							// this point is within bounds; add it in to the totals
							double kernel_value = kernel_values[kernel_a + kernel_b * kernel_dim_a + kernel_c * kernel_dim_a * kernel_dim_b] * coverage;
							double pixel_value = values_[conv_a + conv_b * dim_a + conv_c * dim_a * dim_b];
							
							// we keep a total of the kernel values that were within bounds, for this point
							kernel_total += kernel_value;
							
							// and we keep a total of the convolution - kernel values times pixel values
							conv_total += kernel_value * pixel_value;
						}
					}
				}
				
				*(new_values_ptr++) = ((kernel_total > 0) ? (conv_total / kernel_total) : 0);
			}
		}
	}
	
	TakeOverMallocedValues(new_values, 3, grid_size_);	// takes new_values from us
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *SpatialMap::Class(void) const
{
	return gSLiM_SpatialMap_Class;
}

void SpatialMap::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName() << "<\'" << name_ << "\'>";
}

EidosValue_SP SpatialMap::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_gridDimensions:
		{
			switch (spatiality_)
			{
				case 1: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{grid_size_[0]});
				case 2: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{grid_size_[0], grid_size_[1]});
				case 3: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{grid_size_[0], grid_size_[1], grid_size_[2]});
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_name:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name_));
		}
		case gID_spatialBounds:
		{
			switch (spatiality_)
			{
				case 1: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_a0_, bounds_a1_});
				case 2: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_a0_, bounds_b0_, bounds_a1_, bounds_b1_});
				case 3: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_a0_, bounds_b0_, bounds_c0_, bounds_a1_, bounds_b1_, bounds_c1_});
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_spatiality:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(spatiality_string_));
		}
			
			// variables
		case gID_interpolate:
		{
			return (interpolate_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (SpatialMap::GetProperty): property tag accessed on spatial map before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void SpatialMap::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_interpolate:
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			interpolate_ = value;
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP SpatialMap::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_changeValues:			return ExecuteMethod_changeValues(p_method_id, p_arguments, p_interpreter);
		case gID_interpolate:			return ExecuteMethod_interpolate(p_method_id, p_arguments, p_interpreter);
		case gID_mapColor:				return ExecuteMethod_mapColor(p_method_id, p_arguments, p_interpreter);
		case gID_mapImage:				return ExecuteMethod_mapImage(p_method_id, p_arguments, p_interpreter);
		case gID_mapValue:				return ExecuteMethod_mapValue(p_method_id, p_arguments, p_interpreter);
		case gID_smooth:				return ExecuteMethod_smooth(p_method_id, p_arguments, p_interpreter);
		default:						return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (void)changeValues(numeric values)
//
EidosValue_SP SpatialMap::ExecuteMethod_changeValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *values = p_arguments[0].get();
	
	TakeValuesFromEidosValue(values, "SpatialMap::ExecuteMethod_changeValues", "changeValues()");
	
	// Reassess our min and max if we're using the default grayscale color map;
	// otherwise they are user-supplied and should not be modified
	if (n_colors_ == 0)
		SetAutomaticColorMinMax();
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)interpolate(integer$ factor, [string$ method = "linear"])
//
EidosValue_SP SpatialMap::ExecuteMethod_interpolate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *factor_value = p_arguments[0].get();
	int64_t factor = factor_value->IntAtIndex(0, nullptr);
	
	// the upper limit here is arbitrary, but the goal is to prevent users from blowing up their memory usage unintentionally
	if ((factor < 2) || (factor > 10001))
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): interpolate() requires factor to be in [2, 10001], rather arbitrarily." << EidosTerminate();
	
	EidosValue_String *method_value = (EidosValue_String *)p_arguments[1].get();
	const std::string &method_string = method_value->StringRefAtIndex(0, nullptr);
	int method;
	
	if (method_string == "nearest")
		method = 0;
	else if (method_string == "linear")
		method = 1;
	else if (method_string == "cubic")
		method = 2;
	else
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): interpolate() requires method to be 'nearest', 'linear', or 'cubic'." << EidosTerminate();
	
	if ((method == 0) || (method == 1))
	{
		// These methods are supported directly by ValueAtPoint_S1() / ValueAtPoint_S2() / ValueAtPoint_S3()
		
		// Temporarily force interpolation on
		bool old_interpolate = interpolate_;
		interpolate_ = (method ? true : false);
		
		// Generate the new values and set them
		switch (spatiality_)
		{
			case 1:
			{
				int64_t dim_a = (factor * (grid_size_[0] - 1)) + 1;
				double *new_values = (double *)malloc(dim_a * sizeof(double));
				double *new_values_ptr = new_values;
				double point_vec[1];
				
				if (!new_values)
					EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				
				for (int64_t a = 0; a < dim_a; ++a)
				{
					point_vec[0] = a / (double)(dim_a - 1);
					
					*(new_values_ptr++) = ValueAtPoint_S1(point_vec);
				}
				
				TakeOverMallocedValues(new_values, 1, &dim_a);	// takes new_values from us
				break;
			}
			case 2:
			{
				int64_t dim_a = (factor * (grid_size_[0] - 1)) + 1, dim_b = (factor * (grid_size_[1] - 1)) + 1;
				double *new_values = (double *)malloc(dim_a * dim_b * sizeof(double));
				double *new_values_ptr = new_values;
				double point_vec[2];
				
				if (!new_values)
					EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				
				for (int64_t b = 0; b < dim_b; ++b)
				{
					point_vec[1] = b / (double)(dim_b - 1);
					
					for (int64_t a = 0; a < dim_a; ++a)
					{
						point_vec[0] = a / (double)(dim_a - 1);
						
						*(new_values_ptr++) = ValueAtPoint_S2(point_vec);
					}
				}
				
				int64_t dims[2] = {dim_a, dim_b};
				TakeOverMallocedValues(new_values, 2, dims);	// takes new_values from us
				break;
			}
			case 3:
			{
				int64_t dim_a = (factor * (grid_size_[0] - 1)) + 1, dim_b = (factor * (grid_size_[1] - 1)) + 1, dim_c = (factor * (grid_size_[2] - 1)) + 1;
				double *new_values = (double *)malloc(dim_a * dim_b * dim_c * sizeof(double));
				double *new_values_ptr = new_values;
				double point_vec[3];
				
				if (!new_values)
					EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				
				for (int64_t c = 0; c < dim_c; ++c)
				{
					point_vec[2] = c / (double)(dim_c - 1);
					
					for (int64_t b = 0; b < dim_b; ++b)
					{
						point_vec[1] = b / (double)(dim_b - 1);
						
						for (int64_t a = 0; a < dim_a; ++a)
						{
							point_vec[0] = a / (double)(dim_a - 1);
							
							*(new_values_ptr++) = ValueAtPoint_S3(point_vec);
						}
					}
				}
				
				int64_t dims[3] = {dim_a, dim_b, dim_c};
				TakeOverMallocedValues(new_values, 3, dims);	// takes new_values from us
				break;
			}
			default: break;
		}
		
		// Restore the user's interpolation value
		interpolate_ = old_interpolate;
		
		// Min and max for the default grayscale map do not need to be fixed; new values are all intermediate
	}
	else	// method == 3
	{
		// This is cubic/bicubic interpolation; we use the GSL to do this for us
		switch (spatiality_)
		{
			case 1:
			{
				// cubic interpolation
				// FIXME should correctly handle periodic boundaries
				int64_t dim_a = (factor * (grid_size_[0] - 1)) + 1;
				double *new_values = (double *)malloc(dim_a * sizeof(double));
				double *x = (double *)malloc(grid_size_[0] * sizeof(double));
				double *y = (double *)malloc(grid_size_[0] * sizeof(double));
				
				if (!new_values || !x || !y)
					EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				
				// set up coordinates on our grid, not in user coordinates, for simplicity
				for (int i = 0; i < grid_size_[0]; ++i)
				{
					x[i] = i;
					y[i] = values_[i];
				}
				
				gsl_interp_accel *acc = gsl_interp_accel_alloc();
				gsl_spline *spline = gsl_spline_alloc(gsl_interp_cspline, grid_size_[0]);
				double *new_values_ptr = new_values;
				double scale = 1.0 / factor;
				
				gsl_spline_init(spline, x, y, grid_size_[0]);
				
				for (int64_t a = 0; a < dim_a; ++a)
					*(new_values_ptr++) = gsl_spline_eval(spline, a * scale, acc);
				
				gsl_spline_free(spline);
				gsl_interp_accel_free(acc);
				free(x);
				free(y);
				
				TakeOverMallocedValues(new_values, 1, &dim_a);	// takes new_values from us
				break;
			}
			case 2:
			{
				// bicubic interpolation
				// FIXME should correctly handle periodic boundaries
				if ((grid_size_[0] < 4) || (grid_size_[1] < 4))
					EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): bicubic interpolation requires a starting map with a grid size at least 4x4." << EidosTerminate(nullptr);
				
				int64_t dim_a = (factor * (grid_size_[0] - 1)) + 1, dim_b = (factor * (grid_size_[1] - 1)) + 1;
				double *new_values = (double *)malloc(dim_a * dim_b * sizeof(double));
				double *x = (double *)malloc(grid_size_[0] * sizeof(double));
				double *y = (double *)malloc(grid_size_[1] * sizeof(double));
				double *z = (double *)malloc(grid_size_[0] * grid_size_[1] * sizeof(double));
				
				if (!new_values || !x || !y || !z)
					EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				
				// set up coordinates on our grid, not in user coordinates, for simplicity
				for (int i = 0; i < grid_size_[0]; ++i)
					x[i] = i;
				for (int i = 0; i < grid_size_[1]; ++i)
					y[i] = i;
				
				const gsl_interp2d_type *T = gsl_interp2d_bicubic;
				gsl_spline2d *spline = gsl_spline2d_alloc(T, grid_size_[0], grid_size_[1]);
				gsl_interp_accel *xacc = gsl_interp_accel_alloc();
				gsl_interp_accel *yacc = gsl_interp_accel_alloc();
				double *new_values_ptr = new_values;
				double scale = 1.0 / factor;
				
				for (int64_t b = 0; b < grid_size_[1]; ++b)
					for (int64_t a = 0; a < grid_size_[0]; ++a)
						gsl_spline2d_set(spline, z, a, b, values_[a + b * grid_size_[0]]);
				
				gsl_spline2d_init(spline, x, y, z, grid_size_[0], grid_size_[1]);
				
				for (int64_t b = 0; b < dim_b; ++b)
					for (int64_t a = 0; a < dim_a; ++a)
						*(new_values_ptr++) = gsl_spline2d_eval(spline, a * scale, b * scale, xacc, yacc);
				
				gsl_spline2d_free(spline);
				gsl_interp_accel_free(xacc);
				gsl_interp_accel_free(yacc);
				free(x);
				free(y);
				free(z);
				
				int64_t dims[2] = {dim_a, dim_b};
				TakeOverMallocedValues(new_values, 2, dims);	// takes new_values from us
				break;
			}
			case 3:
			{
				// tricubic interpolation - not supported by the GSL
				EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_interpolate): cubic interpolation is not supported for 3D spatial maps." << EidosTerminate(nullptr);
				break;
			}
			default: break;
		}
		
		// Cubic interpolation can produce interpolated values that are out of the original range
		// Reassess our min and max if we're using the default grayscale color map;
		// otherwise they are user-supplied and should not be modified
		if (n_colors_ == 0)
			SetAutomaticColorMinMax();
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	- (string)mapColor(numeric value)
//
EidosValue_SP SpatialMap::ExecuteMethod_mapColor(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *values = p_arguments[0].get();
	
	// mapColor() does not utilize the default grayscale ramp; if the user wants color, they need to set up a color map
	if (n_colors_ == 0)
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_mapColor): mapColor() no color map defined for spatial map." << EidosTerminate();
	
	int value_count = values->Count();
	EidosValue_String_vector *string_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(value_count);
	EidosValue_SP result_SP = EidosValue_SP(string_return);
	
	for (slim_popsize_t value_index = 0; value_index < value_count; value_index++)
	{
		double value = values->FloatAtIndex(value_index, nullptr);
		double rgb[3];
		char hex_chars[8];
		
		ColorForValue(value, rgb);
		Eidos_GetColorString(rgb[0], rgb[1], rgb[2], hex_chars);
		string_return->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(object<Image>$)mapImage([Ni$ width = NULL], [Ni$ height = NULL], [logical$ centers = F], [logical$ color = T])
//
EidosValue_SP SpatialMap::ExecuteMethod_mapImage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *width_value = p_arguments[0].get();
	EidosValue *height_value = p_arguments[1].get();
	EidosValue *centers_value = p_arguments[2].get();
	EidosValue *color_value = p_arguments[3].get();
	
	if (spatiality_ != 2)
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_mapImage): mapImage() can only generate an image for 2D spatial maps." << EidosTerminate();
	
	int64_t image_width = grid_size_[0], image_height = grid_size_[1];
	
	if (width_value->Type() != EidosValueType::kValueNULL)
		image_width = width_value->IntAtIndex(0, nullptr);
	
	if (height_value->Type() != EidosValueType::kValueNULL)
		image_height = height_value->IntAtIndex(0, nullptr);
	
	if ((image_width <= 0) || (image_width > 100000) || (image_height <= 0) || (image_height > 100000))
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_mapImage): mapImage() requires width and height values to be in [1, 100000]." << EidosTerminate();
	
	bool color = color_value->LogicalAtIndex(0, nullptr);
	
	if (color && (n_colors_ == 0))
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_mapImage): mapImage() requires a defined color map for the spatial map with color=T; use color=F to get a grayscale image, or define a color map in SpatialMap()." << EidosTerminate();
	
	EidosImage *image = new EidosImage(image_width, image_height, !color);
	unsigned char * const data = image->Data();
	unsigned char *data_ptr = data;
	bool centers = centers_value->LogicalAtIndex(0, nullptr);
	
	if (centers)
	{
		// grid lines are defined at [0, ..., 1] with (image_width + 1) values,
		// and [0, ..., 1] with (image_height + 1) values, and samples are
		// taken at the midpoints between the grid lines
		double point[2];
		
		for (int y = 0; y < image_height; ++y)
		{
			point[1] = 1.0 - ((y + 0.5) / (double)image_height);  // (y/image_height + (y+1)/image_height) / 2
			
			for (int x = 0; x < image_width; ++x)
			{
				point[0] = (x + 0.5) / (double)image_width;  // (x/image_width + (x+1)/image_width) / 2
				
				double map_value = ValueAtPoint_S2(point);
				
				if (color)
				{
					double rgb[3];
					
					ColorForValue(map_value, rgb);
					
					*(data_ptr++) = (unsigned char)round(std::min(std::max(rgb[0], 0.0), 1.0) * 255.0);
					*(data_ptr++) = (unsigned char)round(std::min(std::max(rgb[1], 0.0), 1.0) * 255.0);
					*(data_ptr++) = (unsigned char)round(std::min(std::max(rgb[2], 0.0), 1.0) * 255.0);
				}
				else
				{
					*(data_ptr++) = (unsigned char)round(std::min(std::max(map_value, 0.0), 1.0) * 255.0);
				}
			}
		}
	}
	else
	{
		// grid lines are defined at [0, ..., 1] with image_width values,
		// and [0, ..., 1] with image_height values, and samples are
		// taken at the grid lines
		double point[2];
		
		for (int y = 0; y < image_height; ++y)
		{
			point[1] = 1.0 - (y / (double)(image_height - 1));
			
			for (int x = 0; x < image_width; ++x)
			{
				point[0] = x / (double)(image_width - 1);
				
				double map_value = ValueAtPoint_S2(point);
				
				if (color)
				{
					double rgb[3];
					
					ColorForValue(map_value, rgb);
					
					*(data_ptr++) = (unsigned char)round(std::min(std::max(rgb[0], 0.0), 1.0) * 255.0);
					*(data_ptr++) = (unsigned char)round(std::min(std::max(rgb[1], 0.0), 1.0) * 255.0);
					*(data_ptr++) = (unsigned char)round(std::min(std::max(rgb[2], 0.0), 1.0) * 255.0);
				}
				else
				{
					*(data_ptr++) = (unsigned char)round(std::min(std::max(map_value, 0.0), 1.0) * 255.0);
				}
			}
		}
	}
	
	EidosValue_SP result_SP(EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(image, gEidosImage_Class)));
	
	// image is now retained by result_SP, so we can release it
	image->Release();
	
	return result_SP;
}

//	*********************	- (float)mapValue(float point)
//
EidosValue_SP SpatialMap::ExecuteMethod_mapValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point = p_arguments[0].get();
	
	EidosValue_Float_vector *float_result = nullptr;
	EidosValue_Float_singleton *float_singleton_result = nullptr;
	
	// Note that point is required to already be in terms of our spatiality; if we are an "xz" map, it must contain "xz" values
	int x_count;
	
	if (point->Count() == spatiality_)
	{
		x_count = 1;
		float_singleton_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.0));
	}
	else if (point->Count() % spatiality_ == 0)
	{
		x_count = point->Count() / spatiality_;
		float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
	}
	else
		EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_mapValue): mapValue() length of point must match spatiality of map " << name_ << ", or be a multiple thereof." << EidosTerminate();
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_SPATIAL_MAP_VALUE);
#pragma omp parallel for schedule(static) default(none) shared(x_count, float_singleton_result) firstprivate(point, float_result) if(x_count >= EIDOS_OMPMIN_SPATIAL_MAP_VALUE) num_threads(thread_count)
	for (int value_index = 0; value_index < x_count; ++value_index)
	{
		// We need to use the correct spatial bounds for each coordinate, which depends upon our exact spatiality
		// There is doubtless a way to make this code smarter, but brute force is sometimes best...
		double map_value = 0;
		
		switch (spatiality_)
		{
			case 1:
			{
				double point_vec[1];
				int value_offset = value_index;
				
				double a = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_a0_) / (bounds_a1_ - bounds_a0_);
				point_vec[0] = SLiMClampCoordinate(a);
				
				map_value = ValueAtPoint_S1(point_vec);
				break;
			}
			case 2:
			{
				double point_vec[2];
				int value_offset = value_index * 2;
				
				double a = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_a0_) / (bounds_a1_ - bounds_a0_);
				point_vec[0] = SLiMClampCoordinate(a);
				
				double b = (point->FloatAtIndex(1 + value_offset, nullptr) - bounds_b0_) / (bounds_b1_ - bounds_b0_);
				point_vec[1] = SLiMClampCoordinate(b);
				
				map_value = ValueAtPoint_S2(point_vec);
				break;
			}
			case 3:
			{
				double point_vec[3];
				int value_offset = value_index * 3;
				
				double a = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_a0_) / (bounds_a1_ - bounds_a0_);
				point_vec[0] = SLiMClampCoordinate(a);
				
				double b = (point->FloatAtIndex(1 + value_offset, nullptr) - bounds_b0_) / (bounds_b1_ - bounds_b0_);
				point_vec[1] = SLiMClampCoordinate(b);
				
				double c = (point->FloatAtIndex(2 + value_offset, nullptr) - bounds_c0_) / (bounds_c1_ - bounds_c0_);
				point_vec[2] = SLiMClampCoordinate(c);
				
				map_value = ValueAtPoint_S3(point_vec);
				break;
			}
		}
		
		if (float_result)
			float_result->set_float_no_check(map_value, value_index);
		else
			float_singleton_result->SetValue(map_value);
	}
	
	if (float_result)
		return EidosValue_SP(float_result);
	else
		return EidosValue_SP(float_singleton_result);
}

//	*********************	- (void)smooth(float$ maxDistance, string$ functionType, ...)
//
EidosValue_SP SpatialMap::ExecuteMethod_smooth(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// Our arguments go to Kernel::Kernel(), which creates the kernel object that we use
	EidosValue *maxDistance_value = p_arguments[0].get();
	double max_distance = maxDistance_value->FloatAtIndex(0, nullptr);
	
	SpatialKernel kernel(spatiality_, max_distance, p_arguments, 1);	// uses our arguments starting at index 1
	
	// Ask the kernel to create a discrete grid of values, at our spatial scale (we define the
	// relationship between spatial bounds and pixels, used by the kernel to make its grid)
	kernel.CalculateGridValues(*this);
	
	//std::cout << kernel << std::endl;
	
	// Generate the new spatial map values and set them into ourselves
	switch (spatiality_)
	{
		case 1:
			Convolve_S1(kernel);	break;
		case 2:
			Convolve_S2(kernel);	break;
		case 3:
			Convolve_S3(kernel);	break;
			
		default:					break;
	}
	
	// Reassess our min and max if we're using the default grayscale color map;
	// otherwise they are user-supplied and should not be modified
	if (n_colors_ == 0)
		SetAutomaticColorMinMax();
	
	return gStaticEidosValueVOID;
}



//
//	Object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	(object<SpatialMap>$)SpatialMap(...)
//	(object<SpatialMap>$)SpatialMap(string$ name, object<SpatialMap>$ map)
static EidosValue_SP SLiM_Instantiate_SpatialMap(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_String *name_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &name = name_value->StringRefAtIndex(0, nullptr);
	
	EidosValue *map_value = p_arguments[1].get();
	SpatialMap *map = (SpatialMap *)map_value->ObjectElementAtIndex(0, nullptr);
	
	SpatialMap *objectElement = new SpatialMap(name, *map);
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gSLiM_SpatialMap_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}


//
//	SpatialMap_Class
//
#pragma mark -
#pragma mark SpatialMap_Class
#pragma mark -

EidosClass *gSLiM_SpatialMap_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *SpatialMap_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("SpatialMap_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_gridDimensions,			true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_interpolate,			false,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_name,					true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialBounds,			true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatiality,				true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *SpatialMap_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("SpatialMap_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_changeValues, kEidosValueMaskVOID))->AddNumeric("values"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interpolate, kEidosValueMaskVOID))->AddInt_S("factor")->AddString_OS("method", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("linear"))));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mapColor, kEidosValueMaskString))->AddNumeric("value"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mapImage, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosImage_Class))->AddInt_OSN(gEidosStr_width, gStaticEidosValueNULL)->AddInt_OSN(gEidosStr_height, gStaticEidosValueNULL)->AddLogical_OS("centers", gStaticEidosValue_LogicalF)->AddLogical_OS(gEidosStr_color, gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mapValue, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_smooth, kEidosValueMaskVOID))->AddFloat_S("maxDistance")->AddString_S("functionType")->AddEllipsis());
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *SpatialMap_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("SpatialMap_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_SpatialMap, SLiM_Instantiate_SpatialMap, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SpatialMap_Class))->AddString_S("name")->AddObject_S("map", gSLiM_SpatialMap_Class));
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}






