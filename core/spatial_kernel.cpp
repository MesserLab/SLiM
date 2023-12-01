//
//  spatial_kernel.cpp
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


#include "spatial_kernel.h"
#include "eidos_value.h"
#include "eidos_rng.h"

#include <string>
#include <algorithm>


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, SpatialKernelType p_kernel_type)
{
	switch (p_kernel_type)
	{
		case SpatialKernelType::kFixed:			p_out << gStr_f;		break;
		case SpatialKernelType::kLinear:		p_out << gStr_l;		break;
		case SpatialKernelType::kExponential:	p_out << gStr_e;		break;
		case SpatialKernelType::kNormal:		p_out << gEidosStr_n;	break;
		case SpatialKernelType::kCauchy:		p_out << gEidosStr_c;	break;
		case SpatialKernelType::kStudentsT:		p_out << gEidosStr_t;	break;
	}
	
	return p_out;
}


#pragma mark -
#pragma mark SpatialKernel
#pragma mark -

SpatialKernel::SpatialKernel(int p_dimensionality, double p_maxDistance, const std::vector<EidosValue_SP> &p_arguments, int p_first_kernel_arg, bool p_expect_max_density) : dimensionality_(p_dimensionality), max_distance_(p_maxDistance)
{
	// This constructs a kernel from the arguments given, beginning at argument p_first_kernel_arg.
	// For example, take the smooth() method of SpatialKernel:
	//
	//	- (void)smooth(float$ maxDistance, string$ functionType, ...)
	//
	// It parses out maxDistance and passes it to us; it then forwards its remaining
	// arguments, with p_first_kernel_arg == 1, to define the shape of the kernel it wants.
	// The ellipsis arguments are patterned after setInteractionFunction(); this class is
	// basically a grid-sampled version of the same style of kernel that InteractionType
	// uses, and indeed, InteractionType now uses SpatialKernel for some of its work.
	// If p_expect_max_density is true, a maximum kernel density is expected and the kernel
	// specification is as it is for setInteractionFunction(); if p_expect_max_density is
	// false, the maximum kernel density is not expected, as for the smooth() method of
	// SpatialMap.
	//
	// The grid sampling is based upon the spatial scale established by a given SpatialMap;
	// the max distance and other kernel parameters are in terms of that scale.
	
	if ((dimensionality_ < 0) || (dimensionality_ > 3))
		EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel dimensionality must be 0, 1, 2, or 3." << EidosTerminate();
	if (max_distance_ <= 0)
		EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel maxDistance must be greater than zero." << EidosTerminate();
	
	// Parse the arguments that define our kernel shape
	if (p_arguments[p_first_kernel_arg]->Type() != EidosValueType::kValueString)
		EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): (internal error) functionType is not a string." << EidosTerminate();
	
	EidosValue_String *functionType_value = (EidosValue_String *)p_arguments[p_first_kernel_arg].get();
	
	const std::string &k_type_string = functionType_value->StringRefAtIndex(0, nullptr);
	SpatialKernelType k_type;
	int expected_k_param_count = 0;
	std::vector<double> k_parameters;
	
	if (k_type_string.compare(gStr_f) == 0)
	{
		k_type = SpatialKernelType::kFixed;
		expected_k_param_count = (p_expect_max_density ? 1 : 0);
		
		// BCH 9/26/2023: Adding this restriction because it is required for the DrawDisplacement_SX() methods.
		// It makes sense – a kernel that doesn't fall off with distance at all shouldn't have infinite extent.
		// For totalOfNeighborStrengths(), for example, this would become simply a count of all interacting
		// individuals across the whole landscape - it is no longer really a spatial query at all.
		if ((dimensionality_ > 0) && std::isinf(max_distance_))
			EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel type 'f' cannot be used unless a finite maximum interaction distance greater than zero has been set." << EidosTerminate();
	}
	else if (k_type_string.compare(gStr_l) == 0)
	{
		k_type = SpatialKernelType::kLinear;
		expected_k_param_count = (p_expect_max_density ? 1 : 0);
		
		if (std::isinf(max_distance_))
			EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel type 'l' cannot be used unless a finite maximum interaction distance greater than zero has been set." << EidosTerminate();
	}
	else if (k_type_string.compare(gStr_e) == 0)
	{
		k_type = SpatialKernelType::kExponential;
		expected_k_param_count = (p_expect_max_density ? 2 : 1);
	}
	else if (k_type_string.compare(gEidosStr_n) == 0)
	{
		k_type = SpatialKernelType::kNormal;
		expected_k_param_count = (p_expect_max_density ? 2 : 1);
	}
	else if (k_type_string.compare(gEidosStr_c) == 0)
	{
		k_type = SpatialKernelType::kCauchy;
		expected_k_param_count = (p_expect_max_density ? 2 : 1);
	}
	else if (k_type_string.compare(gEidosStr_t) == 0)
	{
		k_type = SpatialKernelType::kStudentsT;
		expected_k_param_count = (p_expect_max_density ? 3 : 2);
	}
	else
		EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel functionType '" << k_type_string << "' must be 'f', 'l', 'e', 'n', 'c', or 't'." << EidosTerminate();
	
	if ((dimensionality_ == 0) && (k_type != SpatialKernelType::kFixed))
		EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel functionType 'f' is required for non-spatial interactions." << EidosTerminate();
	
	if ((int)p_arguments.size() - p_first_kernel_arg != 1 + expected_k_param_count)
		EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel functionType '" << k_type << "' requires exactly " << expected_k_param_count << " kernel configuration parameter" << (expected_k_param_count == 1 ? "" : "s") << "." << EidosTerminate();
	
	for (int k_param_index = 0; k_param_index < expected_k_param_count; ++k_param_index)
	{
		EidosValue *k_param_value = p_arguments[1 + k_param_index + p_first_kernel_arg].get();
		EidosValueType k_param_type = k_param_value->Type();
		
		if ((k_param_type != EidosValueType::kValueFloat) && (k_param_type != EidosValueType::kValueInt))
			EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): the parameters for this spatial kernel type must be numeric (integer or float)." << EidosTerminate();
		
		k_parameters.emplace_back(k_param_value->FloatAtIndex(0, nullptr));
	}
	
	// Internally, we always have a max kernel density.  If one was not expected from the arguments,
	// we insert a value of 1.0 for the max kernel density.
	if (!p_expect_max_density)
		k_parameters.insert(k_parameters.begin(), 1.0);
	
	// Bounds-check the IF parameters in the cases where there is a hard bound
	// NOLINTBEGIN(*-branch-clone) : intentional consecutive branches
	switch (k_type)
	{
		case SpatialKernelType::kFixed:
			// no limits on fixed IFs; doesn't make much sense to use 0.0, but it's not illegal
			break;
		case SpatialKernelType::kLinear:
			// no limits on linear IFs; doesn't make much sense to use 0.0, but it's not illegal
			break;
		case SpatialKernelType::kExponential:
			// no limits on exponential IFs; a shape of 0.0 doesn't make much sense, but it's not illegal
			break;
		case SpatialKernelType::kNormal:
			// no limits on the maximum strength (although 0.0 doesn't make much sense); sd must be >= 0
			if (k_parameters[1] < 0.0)
				EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel type 'n' must have a standard deviation parameter >= 0." << EidosTerminate();
			break;
		case SpatialKernelType::kCauchy:
			// no limits on the maximum strength (although 0.0 doesn't make much sense); scale must be > 0
			if (k_parameters[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel type 'c' must have a scale parameter > 0." << EidosTerminate();
			break;
		case SpatialKernelType::kStudentsT:
			// nu can range from -inf to +inf but must be greater than the dimensionality minus one; scale (sigma) must be >= 0
			if (k_parameters[1] <= p_dimensionality - 1)
				EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel type 't' must have a degrees of freedom parameter that is greater than the kernel dimensionality minus one." << EidosTerminate();
			if (k_parameters[2] < 0.0)
				EIDOS_TERMINATION << "ERROR (SpatialKernel::SpatialKernel): spatial kernel type 't' must have a scale parameter >= 0." << EidosTerminate();
			break;
	}
	// NOLINTEND(*-branch-clone)
	
	// Everything seems to be in order, so replace our kernel info with the new info
	kernel_type_ = k_type;
	kernel_param1_ = ((k_parameters.size() >= 1) ? k_parameters[0] : 0.0);
	kernel_param2_ = ((k_parameters.size() >= 2) ? k_parameters[1] : 0.0);
	kernel_param3_ = ((k_parameters.size() >= 3) ? k_parameters[2] : 0.0);
	n_2param2sq_ = (kernel_type_ == SpatialKernelType::kNormal) ? (2.0 * kernel_param2_ * kernel_param2_) : 0.0;
}

SpatialKernel::~SpatialKernel(void)
{
	if (values_)
		free(values_);
}

void SpatialKernel::CalculateGridValues(SpatialMap &p_map)
{
	if ((dimensionality_ < 1) || (dimensionality_ > 3))
		EIDOS_TERMINATION << "ERROR (SpatialKernel::CalculateGridValues): grid values can only be calculated for kernels with dimensionality of 1, 2, or 3." << EidosTerminate(nullptr);
	if ((max_distance_ <= 0.0) || (!std::isfinite(max_distance_)))
		EIDOS_TERMINATION << "ERROR (SpatialKernel::CalculateGridValues): grid values can only be calculated for kernels with a maxDistance that is positive and finite." << EidosTerminate(nullptr);
	
	// Derive our spatial scale from the given spatial map, which provides a correspondence between
	// spatial bounds and pixel sizes; after this, we do not use the SpatialMap, so these scales
	// could instead be passed in.
	double spatial_size_a = (p_map.bounds_a1_ - p_map.bounds_a0_);
	double spatial_size_b = (dimensionality_ >= 2) ? (p_map.bounds_b1_ - p_map.bounds_b0_) : 0.0;
	double spatial_size_c = (dimensionality_ >= 3) ? (p_map.bounds_c1_ - p_map.bounds_c0_) : 0.0;
	
	pixels_to_spatial_a_ = (spatial_size_a / (p_map.grid_size_[0] - 1));
	pixels_to_spatial_b_ = (dimensionality_ >= 2) ? (spatial_size_b / (p_map.grid_size_[1] - 1)) : 0.0;
	pixels_to_spatial_c_ = (dimensionality_ >= 3) ? (spatial_size_c / (p_map.grid_size_[2] - 1)) : 0.0;
	
	dim[0] = 0;
	dim[1] = 0;
	dim[2] = 0;
	
	int64_t values_size = 1;
	
	double pixelsize_d = (max_distance_ * 2) / pixels_to_spatial_a_;	// convert spatial to pixels
	int64_t pixelsize = (int64_t)ceil(pixelsize_d);
	if (pixelsize % 2 == 0)											// round up to an odd integer
		pixelsize++;
	dim[0] = pixelsize;
	values_size *= pixelsize;
	
	if (dimensionality_ >= 2)
	{
		pixelsize_d = (max_distance_ * 2) / pixels_to_spatial_b_;	// convert spatial to pixels
		pixelsize = (int64_t)ceil(pixelsize_d);
		if (pixelsize % 2 == 0)										// round up to an odd integer
			pixelsize++;
		dim[1] = pixelsize;
		values_size *= pixelsize;
	}
	
	if (dimensionality_ >= 3)
	{
		pixelsize_d = (max_distance_ * 2) / pixels_to_spatial_c_;	// convert spatial to pixels
		pixelsize = (int64_t)ceil(pixelsize_d);
		if (pixelsize % 2 == 0)										// round up to an odd integer
			pixelsize++;
		dim[2] = pixelsize;
		values_size *= pixelsize;
	}
	
	// Allocate our values buffer
	values_ = (double *)malloc(values_size * sizeof(double));
	
	if (!values_)
		EIDOS_TERMINATION << "ERROR (SpatialKernel::CalculateGridValues): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// Set our values
	switch (dimensionality_)
	{
		case 1:
		{
			int64_t kernel_offset_a = dim[0] / 2;	// rounds down
			
			// FIXME: TO BE PARALLELIZED
			for (int64_t a = 0; a < dim[0]; ++a)
			{
				double distance = (a - kernel_offset_a) * pixels_to_spatial_a_;
				double density = (distance > max_distance_) ? 0.0 : DensityForDistance(distance);
				
				values_[a] = density;
			}
			break;
		}
		case 2:
		{
			int64_t kernel_offset_a = dim[0] / 2;	// rounds down
			int64_t kernel_offset_b = dim[1] / 2;	// rounds down
			
			// FIXME: TO BE PARALLELIZED
			for (int64_t a = 0; a < dim[0]; ++a)
			{
				double dist_a = (a - kernel_offset_a) * pixels_to_spatial_a_;
				double dist_a_sq = dist_a * dist_a;
				
				for (int64_t b = 0; b < dim[1]; ++b)
				{
					double dist_b = (b - kernel_offset_b) * pixels_to_spatial_b_;
					double dist_b_sq = dist_b * dist_b;
					double distance = sqrt(dist_a_sq + dist_b_sq);
					
					double density = (distance > max_distance_) ? 0.0 : DensityForDistance(distance);
					
					values_[a + b * dim[0]] = density;
				}
			}
			break;
		}
		case 3:
		{
			int64_t kernel_offset_a = dim[0] / 2;	// rounds down
			int64_t kernel_offset_b = dim[1] / 2;	// rounds down
			int64_t kernel_offset_c = dim[2] / 2;	// rounds down
			
			// FIXME: TO BE PARALLELIZED
			for (int64_t a = 0; a < dim[0]; ++a)
			{
				double dist_a = (a - kernel_offset_a) * pixels_to_spatial_a_;
				double dist_a_sq = dist_a * dist_a;
				
				for (int64_t b = 0; b < dim[1]; ++b)
				{
					double dist_b = (b - kernel_offset_b) * pixels_to_spatial_b_;
					double dist_b_sq = dist_b * dist_b;
					
					for (int64_t c = 0; c < dim[2]; ++c)
					{
						double dist_c = (c - kernel_offset_c) * pixels_to_spatial_c_;
						double dist_c_sq = dist_c * dist_c;
						double distance = sqrt(dist_a_sq + dist_b_sq + dist_c_sq);
						double density = (distance > max_distance_) ? 0.0 : DensityForDistance(distance);
						
						values_[a + b * dim[0] + c * dim[0] * dim[1]] = density;
					}
				}
			}
			break;
		}
		default: break;
	}
}

double SpatialKernel::DensityForDistance(double p_distance)
{
	// SEE ALSO: InteractionType::CalculateStrengthNoCallbacks(), which is parallel to this
	switch (kernel_type_)
	{
		case SpatialKernelType::kFixed:
			return (kernel_param1_);																		// fmax
		case SpatialKernelType::kLinear:
			return (kernel_param1_ * (1.0 - p_distance / max_distance_));									// fmax * (1 − d/dmax)
		case SpatialKernelType::kExponential:
			return (kernel_param1_ * exp(-kernel_param2_ * p_distance));										// fmax * exp(−λd)
		case SpatialKernelType::kNormal:
			return (kernel_param1_ * exp(-(p_distance * p_distance) / n_2param2sq_));						// fmax * exp(−d^2/2σ^2)
		case SpatialKernelType::kCauchy:
		{
			double temp = p_distance / kernel_param2_;
			return (kernel_param1_ / (1.0 + temp * temp));													// fmax / (1+(d/λ)^2)
		}
		case SpatialKernelType::kStudentsT:
			return SpatialKernel::tdist(p_distance, kernel_param1_, kernel_param2_, kernel_param3_);		// fmax / (1+(d/t)^2/n)^(−(ν+1)/2)
	}
	EIDOS_TERMINATION << "ERROR (SpatialKernel::DensityForDistance): (internal error) unexpected SpatialKernelType value." << EidosTerminate();
}

void SpatialKernel::DrawDisplacement_S1(double *displacement)
{
	// Draw a displacement from the kernel center, weighted by kernel density
	// Note that we could be going either plus or minus from the center
	Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
	gsl_rng *rng = rng_state->gsl_rng_;
	
	switch (kernel_type_)
	{
		case SpatialKernelType::kFixed:
		{
			displacement[0] = Eidos_rng_uniform(rng) * 2 * max_distance_ - max_distance_;
			return;
		}
		case SpatialKernelType::kLinear:
		{
			double d = (1 - sqrt(Eidos_rng_uniform(rng))) * max_distance_;
			
			displacement[0] = (Eidos_RandomBool(rng_state) ? d : -d);
			return;
		}
		case SpatialKernelType::kExponential:
		{
			double d;
			
			do {
				d = gsl_ran_exponential(rng, 1.0 / kernel_param2_);
			} while (d > max_distance_);
			
			displacement[0] = (Eidos_RandomBool(rng_state) ? d : -d);
			return;
		}
		case SpatialKernelType::kNormal:
		{
			double d;
			
			do {
				d = gsl_ran_gaussian(rng, kernel_param2_);
			} while (d > max_distance_);
			
			displacement[0] = d;
			return;
		}
		case SpatialKernelType::kStudentsT:
		{
			double d;
			
			do {
				d = gsl_ran_tdist(rng, kernel_param2_) * kernel_param3_;
			} while (d > max_distance_);
			
			displacement[0] = d;
			return;
		}
		default:
		{
			// Other distributions are of unclear utility, since draws may cluster at the max distance; this is
			// particularly bad for the Cauchy, because the area under it out to infinity is infinite for D>1
			EIDOS_TERMINATION << "ERROR (SpatialKernel::DrawDisplacement_S1): kernel type not supported." << EidosTerminate();
		}
	}
}

void SpatialKernel::DrawDisplacement_S2(double *displacement)
{
	// Draw a displacement from the kernel center, weighted by kernel density
	// Note that we could be going in any direction from the center
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	switch (kernel_type_)
	{
		case SpatialKernelType::kFixed:
		{
			double theta = Eidos_rng_uniform(rng) * 2 * M_PI;
			double d = sqrt(Eidos_rng_uniform(rng)) * max_distance_;
			displacement[0] = cos(theta) * d;
			displacement[1] = sin(theta) * d;
			return;
		}
		case SpatialKernelType::kLinear:
		{
			double theta = Eidos_rng_uniform(rng) * 2 * M_PI;
			double d = gsl_ran_beta(rng, 2.0, 2.0) * max_distance_;
			displacement[0] = cos(theta) * d;
			displacement[1] = sin(theta) * d;
			return;
		}
		case SpatialKernelType::kExponential:
		{
			double d;
			
			do {
				d = gsl_ran_gamma(rng, 2.0, 1.0 / kernel_param2_);
			} while (d > max_distance_);
			
			double theta = Eidos_rng_uniform(rng) * 2 * M_PI;
			
			displacement[0] = cos(theta) * d;
			displacement[1] = sin(theta) * d;
			return;
		}
		case SpatialKernelType::kNormal:
		{
			double d1, d2;
			
			do {
				d1 = gsl_ran_gaussian(rng, kernel_param2_);
				d2 = gsl_ran_gaussian(rng, kernel_param2_);
			} while (sqrt(d1*d1 + d2*d2) > max_distance_);
			
			displacement[0] = d1;
			displacement[1] = d2;
			return;
		}
		case SpatialKernelType::kStudentsT:
		{
			// df (nu) is kernel_param2_, scale is kernel_param3_
			double d;
			
			do {
				double x = 0.5 + abs(Eidos_rng_uniform(rng) - 0.5);
				d = sqrt(std::max(0.0, kernel_param2_ * (pow(2.0 - 2.0 * x, -2.0 / (kernel_param2_ - 1.0)) - 1.0))) * kernel_param3_;
			} while (d > max_distance_);
			
			double theta = Eidos_rng_uniform(rng) * 2 * M_PI;
			
			displacement[0] = cos(theta) * d;
			displacement[1] = sin(theta) * d;
			return;
		}
		default:
		{
			// Other distributions are of unclear utility, since draws may cluster at the max distance; this is
			// particularly bad for the Cauchy, because the area under it out to infinity is infinite for D>1
			EIDOS_TERMINATION << "ERROR (SpatialKernel::DrawDisplacement_S2): kernel type not supported." << EidosTerminate();
		}
	}
}

void SpatialKernel::DrawDisplacement_S3(double *displacement)
{
	// Draw a displacement from the kernel center, weighted by kernel density
	// Note that we could be going in any direction from the center
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	switch (kernel_type_)
	{
		case SpatialKernelType::kFixed:
		{
			double dx = gsl_ran_gaussian(rng, 1.0);
			double dy = gsl_ran_gaussian(rng, 1.0);
			double dz = gsl_ran_gaussian(rng, 1.0);
			double sphere_dist = sqrt(dx*dx + dy*dy + dz*dz);
			double d = pow(Eidos_rng_uniform(rng), 1/3.0) * max_distance_;
			
			displacement[0] = dx * d / sphere_dist;
			displacement[1] = dy * d / sphere_dist;
			displacement[2] = dz * d / sphere_dist;
			return;
		}
		case SpatialKernelType::kLinear:
		{
			double dx = gsl_ran_gaussian(rng, 1.0);
			double dy = gsl_ran_gaussian(rng, 1.0);
			double dz = gsl_ran_gaussian(rng, 1.0);
			double sphere_dist = sqrt(dx*dx + dy*dy + dz*dz);
			double d = gsl_ran_beta(rng, 3.0, 2.0) * max_distance_;
			
			displacement[0] = dx * d / sphere_dist;
			displacement[1] = dy * d / sphere_dist;
			displacement[2] = dz * d / sphere_dist;
			return;
		}
		case SpatialKernelType::kExponential:
		{
			double dx = gsl_ran_gaussian(rng, 1.0);
			double dy = gsl_ran_gaussian(rng, 1.0);
			double dz = gsl_ran_gaussian(rng, 1.0);
			double sphere_dist = sqrt(dx*dx + dy*dy + dz*dz);
			double d;
			
			do {
				d = gsl_ran_gamma(rng, 3.0, 1.0 / kernel_param2_);
			} while (d > max_distance_);
			
			displacement[0] = dx * d / sphere_dist;
			displacement[1] = dy * d / sphere_dist;
			displacement[2] = dz * d / sphere_dist;
			return;
		}
		case SpatialKernelType::kNormal:
		{
			double d1, d2, d3;
			
			do {
				d1 = gsl_ran_gaussian(rng, kernel_param2_);
				d2 = gsl_ran_gaussian(rng, kernel_param2_);
				d3 = gsl_ran_gaussian(rng, kernel_param2_);
			} while (sqrt(d1*d1 + d2*d2 + d3*d3) > max_distance_);
			
			displacement[0] = d1;
			displacement[1] = d2;
			displacement[2] = d3;
			return;
		}
		case SpatialKernelType::kStudentsT:
			// FIXME: punting for now.  Peter says: "That involves an integral I don't know how to do. If you really want to do it, you could always do it numerically (even, pre-compute the values - it's just a 1D integral that we understand pretty well...)"
		default:
		{
			// Other distributions are of unclear utility, since draws may cluster at the max distance; this is
			// particularly bad for the Cauchy, because the area under it out to infinity is infinite for D>1
			EIDOS_TERMINATION << "ERROR (SpatialKernel::DrawDisplacement_S3): kernel type not supported." << EidosTerminate();
		}
	}
}

std::ostream& operator<<(std::ostream& p_out, SpatialKernel &p_kernel)
{
	std::cout << "Kernel with dimensionality_ == " << p_kernel.dimensionality_ << ":" << std::endl;
	std::cout << "   max_distance_ == " << p_kernel.max_distance_ << std::endl;
	std::cout << "   kernel_type_ == \"" << p_kernel.kernel_type_ << "\"" << std::endl;
	std::cout << "   kernel_param1_ == " << p_kernel.kernel_param1_ << std::endl;
	std::cout << "   kernel_param2_ == " << p_kernel.kernel_param2_ << std::endl;
	std::cout << "   n_2param2sq_ == " << p_kernel.n_2param2sq_ << std::endl;
	std::cout << "   dim[3] == {" << p_kernel.dim[0] << ", " << p_kernel.dim[1] << ", " << p_kernel.dim[2] << "}" << std::endl;
	
	if (p_kernel.values_)
	{
		std::cout << "   pixels_to_spatial_a_ == " << p_kernel.pixels_to_spatial_a_ << std::endl;
		std::cout << "   pixels_to_spatial_b_ == " << p_kernel.pixels_to_spatial_b_ << std::endl;
		std::cout << "   pixels_to_spatial_c_ == " << p_kernel.pixels_to_spatial_c_ << std::endl;
	}
	
	std::cout << "   values ==";
	
	switch (p_kernel.dimensionality_)
	{
		case 1:										// NOLINT(*-branch-clone) : intentional duplicate branches
			// unimplemented
			break;
		case 2:
			for (int b = 0; b < p_kernel.dim[1]; b++)
			{
				std::cout << std::endl << "      ";
				
				for (int a = 0; a < p_kernel.dim[0]; a++)
				{
					std::ostringstream os;
					
					os.precision(3);
					os << std::fixed;
					os << p_kernel.values_[a + b * p_kernel.dim[0]];
					
					std::cout << os.str() << " ";
				}
			}
			break;
		case 3:										// NOLINT(*-branch-clone) : intentional duplicate branches
			// unimplemented
			break;
		default: break;
	}
	std::cout << std::endl;
	
	return p_out;
}








