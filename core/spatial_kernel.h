//
//  spatial_kernel.h
//  SLiM
//
//  Created by Ben Haller on 9/9/23.
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
 
 The class SpatialKernel represents a spatial kernel of some shape.  It is used by both InteractionType and SpatialMap
 to represent the kernels they use internally.  It is not visible in Eidos, at least for now.
 
 */

#ifndef __SLiM__spatial_kernel__
#define __SLiM__spatial_kernel__

#include <vector>
#include <cstdint>

#include "eidos_value.h"
#include "spatial_map.h"


// This enumeration represents a type of interaction function (IF) that an
// interaction type can use to convert distances to interaction strengths
enum class SpatialKernelType : char {
	kFixed = 0,		// "f"
	kLinear,		// "l"
	kExponential,	// "e"
	kNormal,		// "n"
	kCauchy,		// "c"
	kStudentsT,		// "t"
};

std::ostream& operator<<(std::ostream& p_out, SpatialKernelType p_kernel_type);


#pragma mark -
#pragma mark SpatialKernel
#pragma mark -

class SpatialKernel
{
public:
	// core kernel definition
	int dimensionality_;			// 1, 2, or 3: how many dimensions the kernel data is
	double max_distance_;			// the maximum spatial distance out to which the kernel stretches
	double pixels_to_spatial_a_;	// multiply by this to convert pixels to spatial scale for a
	double pixels_to_spatial_b_;	// multiply by this to convert pixels to spatial scale for b
	double pixels_to_spatial_c_;	// multiply by this to convert pixels to spatial scale for c
	
	SpatialKernelType kernel_type_;				// the kernel type to use
	double kernel_param1_, kernel_param2_, kernel_param3_;	// parameters for that kernel type (not all of which may be used)
	double n_2param2sq_;						// for type "n", precalc 2.0 * kernel_param2_ * kernel_param2_
	
	// discrete grid values; set up only if CalculateGridValues() is called
	double *values_ = nullptr;		// raw kernel pixel data, malloced
	int64_t dim[3] = {0, 0, 0};		// pixel dimensions of values_ for 1, 2, or 3 axes
	
	// calculate t-distribution PDF values in our fashion, for which the function is normalized to a maximum value
	// we don't use the GSL for this, because it does two gamma-function calculations that we don't need (they normalize away)
	static inline double tdist(double x, double max, double nu, double tau) {
		double x_over_tau = x / tau;
		return max / pow(1.0 + x_over_tau * x_over_tau / nu, -(nu + 1.0) / 2.0);
	};
	
public:
	SpatialKernel(const SpatialKernel&) = delete;							// no copying
	SpatialKernel& operator=(const SpatialKernel&) = delete;				// no copying
	SpatialKernel(void) = delete;											// no null construction
	SpatialKernel(int p_dimensionality, double p_maxDistance, const std::vector<EidosValue_SP> &p_arguments, int p_first_kernel_arg, bool p_expect_max_density);
	~SpatialKernel(void);
	
	void CalculateGridValues(SpatialMap &p_map);
	double DensityForDistance(double p_distance);
	void DrawDisplacement_S1(double *displacement);
	void DrawDisplacement_S2(double *displacement);
	void DrawDisplacement_S3(double *displacement);
	
	friend void SpatialMap::Convolve_S1(SpatialKernel &kernel);
	friend void SpatialMap::Convolve_S2(SpatialKernel &kernel);
	friend void SpatialMap::Convolve_S3(SpatialKernel &kernel);
};

std::ostream& operator<<(std::ostream& p_out, SpatialKernel &p_kernel);


#endif /* __SLiM__spatial_kernel__ */
