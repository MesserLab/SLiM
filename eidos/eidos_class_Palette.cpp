//
//  eidos_class_Palette.cpp
//  Eidos
//
//  Created by Ben Haller on 3/8/26.
//  Copyright (c) 2026 Benjamin C. Haller.  All rights reserved.
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


#include "eidos_class_Palette.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_globals.h"

#include <algorithm>
#include <string>
#include <vector>


PaletteTransition PaletteTransitionFromString(const std::string string)
{
	if (string == "step")					return PaletteTransition::kStep;
	if (string == "linear")					return PaletteTransition::kLinear;
	if (string == "sigmoid")				return PaletteTransition::kSigmoid;
	if (string == "inverseSigmoid")			return PaletteTransition::kInverseSigmoid;
	if (string == "easeInSqrt")				return PaletteTransition::kEaseInSqrt;
	if (string == "easeInSquare")			return PaletteTransition::kEaseInSquare;
	if (string == "easeOutSqrt")			return PaletteTransition::kEaseOutSqrt;
	if (string == "easeOutSquare")			return PaletteTransition::kEaseOutSquare;
	
	EIDOS_TERMINATION << "ERROR (PaletteTransitionFromString): palette transition values must be one of 'step', 'linear', 'sigmoid', 'easeInSqrt', 'easeInSquare', 'easeOutSqrt', or 'easeOutSquare' ('" << string << "' supplied)." << EidosTerminate();
}

PaletteBlend PaletteBlendFromString(const std::string string)
{
	if (string == "rgb")					return PaletteBlend::kRGB;
	if (string == "hsvClockwise")			return PaletteBlend::kHSVClockwise;
	if (string == "hsvCounterclockwise")	return PaletteBlend::kHSVCounterclockwise;
	if (string == "hsvShortest")			return PaletteBlend::kHSVShortest;
	if (string == "hsvLongest")				return PaletteBlend::kHSVLongest;
	
	EIDOS_TERMINATION << "ERROR (PaletteBlendFromString): palette blend values must be one of 'rgb', 'hsvClockwise', 'hsvCounterclockwise', 'hsvShortest', or 'hsvLongest' ('" << string << "' supplied)." << EidosTerminate();
}

//
//	EidosPalette
//
#pragma mark -
#pragma mark EidosPalette
#pragma mark -

EidosPalette::EidosPalette(double value, double r, double g, double b)
{
#if DEBUG
	// the caller should guarantee these limits
	if (!std::isfinite(value))
		EIDOS_TERMINATION << "ERROR (EidosPalette::EidosPalette): (internal error) node values must be finite." << EidosTerminate();
	if (((r < 0.0) || (r > 1.0)) || ((g < 0.0) || (g > 1.0)) || ((b < 0.0) || (b > 1.0)))
		EIDOS_TERMINATION << "ERROR (EidosPalette::EidosPalette): (internal error) RGB values must be in [0, 1]." << EidosTerminate();
#endif
	
	range_start_ = value;
	range_end_ = value;
	
	// emplace the first node directly, since AddNode() would bounds-check us
	nodes_.emplace_back(value, r, g, b, PaletteTransition::kUndefined, PaletteBlend::kUndefined);
}

EidosPalette::EidosPalette(std::vector<double> &&values, std::vector<std::string> &&colors, PaletteTransition transition, PaletteBlend blend)
{
	// this constructor is used internally for constructing the default SLiMgui palettes
	int value_count = (int)values.size();
	
	for (int value_index = 0; value_index < value_count; value_index++)
	{
		double value = values[value_index];
		const std::string &color_string = colors[value_index];
		double r, g, b;
		
		Eidos_GetColorComponents(color_string, &r, &g, &b);
		
		if (value_index == 0)
		{
			range_start_ = value;
			range_end_ = value;
			
			// emplace the first node directly, since AddNode() would bounds-check us
			nodes_.emplace_back(value, r, g, b, PaletteTransition::kUndefined, PaletteBlend::kUndefined);
		}
		else
		{
			AddNode(value, r, g, b, transition, blend);
		}
	}
}

EidosPalette::EidosPalette(EidosValue *p_range, EidosValue *p_colors, const std::string &p_code_name, const std::string &p_eidos_name)
{
	// this constructor is used internally for defineSpatialMap() and similar; the two EidosValues passed
	// in should conform to [if valueRange = NULL], [s colors = NULL] (the NULL case chould be handled by
	// the caller, since that means that a Palette object is not being requested).
	EidosValueType range_type = p_range->Type();
	EidosValueType colors_type = p_colors->Type();
	int range_count = p_range->Count();
	int colors_count = p_colors->Count();
	
	if ((range_type != EidosValueType::kValueInt) && (range_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (" << p_code_name << "): (internal error) " << p_eidos_name << " valueRange must be type integer or float." << EidosTerminate();
	if (range_count != 2)
			EIDOS_TERMINATION << "ERROR (" << p_code_name << "): " << p_eidos_name << " valueRange must be exactly length 2 (giving the min and max value for the palette)." << EidosTerminate();
	
	if (colors_type != EidosValueType::kValueString)
		EIDOS_TERMINATION << "ERROR (" << p_code_name << "): (internal error) " << p_eidos_name << " colors must be type string." << EidosTerminate();
	if (colors_count < 2)
		EIDOS_TERMINATION << "ERROR (" << p_code_name << "): " << p_eidos_name << " colors must be of length >= 2." << EidosTerminate();
	
	// valueRange and colors were provided, so use them for coloring
	double range_min_ = p_range->NumericAtIndex_NOCAST(0, nullptr);
	double range_max_ = p_range->NumericAtIndex_NOCAST(1, nullptr);
	
	if (!std::isfinite(range_min_) || !std::isfinite(range_max_) || (range_min_ > range_max_))
		EIDOS_TERMINATION << "ERROR (" << p_code_name << "): " << p_eidos_name << " valueRange must be finite, and min <= max is required." << EidosTerminate();
	
	const std::string *colors_vec_ptr = p_colors->StringData();
	
	for (int colors_index = 0; colors_index < colors_count; ++colors_index)
	{
		double value = range_min_ + (range_max_ - range_min_) * (colors_index / (double)(colors_count - 1));
		double r, g, b;
		
		Eidos_GetColorComponents(colors_vec_ptr[colors_index], &r, &g, &b);
		
		if (colors_index == 0)
		{
			range_start_ = value;
			range_end_ = value;
			
			// emplace the first node directly, since AddNode() would bounds-check us
			nodes_.emplace_back(value, r, g, b, PaletteTransition::kUndefined, PaletteBlend::kUndefined);
		}
		else
		{
			AddNode(value, r, g, b, PaletteTransition::kLinear, PaletteBlend::kRGB);
		}
	}
}

EidosPalette::~EidosPalette(void)
{
	if (cached_colors_)
	{
		free(cached_colors_);
		cached_colors_ = nullptr;
	}
	
#if defined(SLIMGUI)
	if (image_)
	{
		if (image_deleter_)
			image_deleter_(image_);
		else
			std::cout << "Missing Palette image_deleter_; leaking memory" << std::endl;
	}
#endif
}

EidosPalette *EidosPalette::AddNode(double value, double r, double g, double b, PaletteTransition transition, PaletteBlend blend)
{
	// add a node at the given value with the given color, transition, and blend
#if DEBUG
	if (!std::isfinite(value))
		EIDOS_TERMINATION << "ERROR (EidosPalette::AddNode): (internal error) node values must be finite." << EidosTerminate();
	if (((r < 0.0) || (r > 1.0)) || ((g < 0.0) || (g > 1.0)) || ((b < 0.0) || (b > 1.0)))
		EIDOS_TERMINATION << "ERROR (EidosPalette::AddNode): (internal error) RGB values must be in [0, 1]." << EidosTerminate();
	if ((transition < PaletteTransition::kStep) || (transition > PaletteTransition::kEaseOutSquare))
		EIDOS_TERMINATION << "ERROR (EidosPalette::AddNode): (internal error) transition out of range." << EidosTerminate();
	if ((blend < PaletteBlend::kRGB) || (blend > PaletteBlend::kHSVLongest))
		EIDOS_TERMINATION << "ERROR (EidosPalette::AddNode): (internal error) blend out of range." << EidosTerminate();
#endif
	
	if (immutable_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::AddNode): (internal error) the palette cannot be changed, because it is immutable." << EidosTerminate();
	
	if (value <= range_end_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::AddNode): (internal error) nodes must be added in strictly ascending order." << EidosTerminate();
	
	range_end_ = value;
	nodes_.emplace_back(value, r, g, b, transition, blend);
	
	_InvalidateColorCache();
	
	return this;
}

EidosPalette *EidosPalette::SetFixedValue(double value)
{
	// add a fixed value at the given value, cached and returned without using the cache;
	// this allows us to guarantee that we return yellow at the neutral point
#if DEBUG
	if (!std::isfinite(value))
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) palette values must be finite." << EidosTerminate();
	if ((value <= range_start_) || (value >= range_end_))
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) a fixed value must be in the interior of the palette's range." << EidosTerminate();
#endif
	
	if (immutable_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) the palette cannot be changed, because it is immutable." << EidosTerminate();
	
	// calculate the color at the fixed value; it cannot be different from what the palette would normally produce
	float r, g, b;
	
	CalculateColorForValue(value, &r, &g, &b);
	
	fixed_value_ = value;
	fixed_r_ = r;
	fixed_g_ = g;
	fixed_b_ = b;
	
	// invalidation is not needed since the fixed value overrides the cache
	//_InvalidateColorCache();
	
	return this;
}

bool EidosPalette::GetFixedValue(double *p_fixed_value, float *p_red, float *p_green, float *p_blue)
{
	if (std::isnan(fixed_value_))
		return false;
	
	*p_fixed_value = fixed_value_;
	*p_red = fixed_r_;
	*p_green = fixed_g_;
	*p_blue = fixed_b_;
	
	return true;
}

void EidosPalette::_CalculateColorForValueGivenNodes(double value, float *p_r, float *p_g, float *p_b, PaletteNode *node_ptr, PaletteNode *previous_node_ptr)
{
	// calculate the raw fraction in [0, 1] where the input value lies between the two nodes
	double ramp_start = (double)previous_node_ptr->value_;
	double ramp_end = (double)node_ptr->value_;
	double raw_fraction = ((double)value - ramp_start) / (ramp_end - ramp_start);
	
	// calculate the transformed fraction in [0, 1], using the chosen transition
	double transformed_fraction;
	
	switch (node_ptr->transition_)
	{
		case PaletteTransition::kStep:
		{
			// step from 0.0 to 1.0 at the midpoint between the nodes
			transformed_fraction = (raw_fraction < 0.5) ? 0.0 : 1.0;
			break;
		}
		case PaletteTransition::kLinear:
		{
			// linear from 0.0 to 1.0 between the nodes
			transformed_fraction = raw_fraction;
			break;
		}
			
			// sigmoid curves from 0.0 to 1.0 between the nodes
			// sigmoid is gradual at the start and end, more rapid in the center (like logistic)
			// inverse sigmoid is rapid at the start and end, slower in the center (like probit/logit)
			// see https://blog.demofox.org/2012/09/24/bias-and-gain-are-your-friend/ for this math
		case PaletteTransition::kSigmoid:
		{
			const double gain = 0.25;
			
			if (raw_fraction < 0.5)
			{
				double x = raw_fraction * 2.0;
				double bias = gain;
				transformed_fraction = ((x / ((((1.0 / bias) - 2.0) * (1.0 - x)) + 1.0)) / 2.0);
			}
			else
			{
				double x = raw_fraction * 2.0 - 1.0;
				double bias = 1.0 - gain;
				transformed_fraction = ((x / ((((1.0 / bias) - 2.0) * (1.0 - x)) + 1.0)) / 2.0 + 0.5);
			}
			break;
		}
		case PaletteTransition::kInverseSigmoid:
		{
			const double gain = 0.75;
			
			if (raw_fraction < 0.5)
			{
				double x = raw_fraction * 2.0;
				double bias = gain;
				transformed_fraction = ((x / ((((1.0 / bias) - 2.0) * (1.0 - x)) + 1.0)) / 2.0);
			}
			else
			{
				double x = raw_fraction * 2.0 - 1.0;
				double bias = 1.0 - gain;
				transformed_fraction = ((x / ((((1.0 / bias) - 2.0) * (1.0 - x)) + 1.0)) / 2.0 + 0.5);
			}
			break;
		}
			
			// "ease in" means the slope is more gradual at the beginning
			// "ease out" means the slope is more gradual at the end
			// see https://github.com/maptiler/maptiler-sdk-js/blob/main/colorramp.md
		case PaletteTransition::kEaseInSqrt:
		{
			transformed_fraction = 1.0 - sqrt(1.0 - raw_fraction);
			break;
		}
		case PaletteTransition::kEaseInSquare:
		{
			transformed_fraction = raw_fraction * raw_fraction;
			break;
		}
		case PaletteTransition::kEaseOutSqrt:
		{
			transformed_fraction = std::sqrt(raw_fraction);
			break;
		}
		case PaletteTransition::kEaseOutSquare:
		{
			transformed_fraction = 1.0 - (1.0 - raw_fraction) * (1.0 - raw_fraction);
			break;
		}
			
		default:
			EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): (internal error) unrecognized PaletteTransition value." << EidosTerminate(nullptr);
	}
	
	// calculate a mix of the colors of the two nodes
	PaletteBlend blend = node_ptr->blend_;
	
	if (blend == PaletteBlend::kRGB)
	{
		// simple weighted average in RGB space
		*p_r = (float)(node_ptr->r_ * transformed_fraction + previous_node_ptr->r_ * (1.0 - transformed_fraction));
		*p_g = (float)(node_ptr->g_ * transformed_fraction + previous_node_ptr->g_ * (1.0 - transformed_fraction));
		*p_b = (float)(node_ptr->b_ * transformed_fraction + previous_node_ptr->b_ * (1.0 - transformed_fraction));
	}
	else if ((blend == PaletteBlend::kHSVLongest) ||
			 (blend == PaletteBlend::kHSVShortest) ||
			 (blend == PaletteBlend::kHSVClockwise) ||
			 (blend == PaletteBlend::kHSVCounterclockwise))
	{
		// blends in HSV space
		double h1 = previous_node_ptr->h_, s1 = previous_node_ptr->s_, v1 = previous_node_ptr->v_;
		double h2 = node_ptr->h_, s2 = node_ptr->s_, v2 = node_ptr->v_;
		bool clockwise;
		
		if (h1 != h2)
		{
			switch (blend)
			{
				case PaletteBlend::kHSVLongest:
				{
					// choose the longer option, clockwise or counterclockwise
					if (h2 > h1)
						clockwise = (h2 - h1 > 0.5) ? false : true;
					else
						clockwise = (h1 - h2 > 0.5) ? true : false;
					break;
				}
				case PaletteBlend::kHSVShortest:
				{
					// choose the shorter option, clockwise or counterclockwise
					if (h2 > h1)
						clockwise = (h2 - h1 > 0.5) ? true : false;
					else
						clockwise = (h1 - h2 > 0.5) ? false : true;
					break;
				}
				case PaletteBlend::kHSVClockwise:
				{
					clockwise = true;
					break;
				}
				case PaletteBlend::kHSVCounterclockwise:
				{
					clockwise = false;
					break;
				}
					
				default:
					EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): (internal error) unrecognized PaletteBlend value." << EidosTerminate(nullptr);
			}
			
			if (clockwise && (h2 > h1))
				h1 += 1.0;
			if (!clockwise && (h1 > h2))
				h2 += 1.0;
		}
		
		// now interpolate in HSV space, handle the periodic boundary in hue, and convert to RGB at the end
		double h = h2 * transformed_fraction + h1 * (1.0 - transformed_fraction);
		double s = s2 * transformed_fraction + s1 * (1.0 - transformed_fraction);
		double v = v2 * transformed_fraction + v1 * (1.0 - transformed_fraction);
		
		if (h > 1.0)
			h -= 1.0;
		
		double r, g, b;
		
		Eidos_HSV2RGB(h, s, v, &r, &g, &b);
		
		*p_r = (float)r;
		*p_g = (float)g;
		*p_b = (float)b;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): (internal error) unrecognized PaletteBlend value." << EidosTerminate(nullptr);
	}
}

void EidosPalette::CalculateColorForValue(double value, float *r, float *g, float *b)
{
	PaletteNode *node_ptr = nodes_.data();
#if DEBUG
	PaletteNode *end_node_ptr = node_ptr + nodes_.size();
#endif
	PaletteNode *previous_node_ptr = node_ptr++;
	
	// advance to the correct pair of palette nodes, between which the input value lies
	while (value > node_ptr->value_)
	{
		previous_node_ptr = node_ptr++;
		
#if DEBUG
		if (node_ptr >= end_node_ptr)
			EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): (internal error) ran out of palette nodes." << EidosTerminate(nullptr);
#endif
	}
	
	_CalculateColorForValueGivenNodes(value, r, g, b, node_ptr, previous_node_ptr);
}

void EidosPalette::_GenerateColorCache(void)
{
	// this generates a cache of generated colors at a high enough resolution that interpolation is not needed
#if DEBUG
	if (!_ColorCacheIsInvalid())
		EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): (internal error) _GenerateColorCache() called with a cache that is not invalid." << EidosTerminate();
#endif
	
	cached_colors_count_ = 4096;	// this makes a ~48K table, which doesn't seem unreasonable.
	
	if (!cached_colors_)
		cached_colors_ = (float *)malloc(cached_colors_count_ * sizeof(float) * 3);
	
	if (!cached_colors_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// we walk forward through nodes as we fill the cache, taking advantage of the sorted order of the nodes
	// the first node defines the start of the color ramp; the next node defines the end of the color ramp
	float *cache_ptr = cached_colors_;
	PaletteNode *node_ptr = nodes_.data();
#if DEBUG
	PaletteNode *end_node_ptr = node_ptr + nodes_.size();
#endif
	PaletteNode *previous_node_ptr = node_ptr++;
	
	for (int cache_index = 0; cache_index < cached_colors_count_; ++cache_index)
	{
		// calculate the input value corresponding to this cache index
		double value = range_start_ + (range_end_ - range_start_) * (cache_index / (double)(cached_colors_count_ - 1));
		
		// advance to the correct pair of palette nodes, between which the input value lies
		while (value > node_ptr->value_)
		{
			previous_node_ptr = node_ptr++;
			
#if DEBUG
			if (node_ptr >= end_node_ptr)
				EIDOS_TERMINATION << "ERROR (EidosPalette::_GenerateColorCache): (internal error) ran out of palette nodes." << EidosTerminate(nullptr);
#endif
		}
		
		_CalculateColorForValueGivenNodes(value, cache_ptr, cache_ptr + 1, cache_ptr + 2, node_ptr, previous_node_ptr);
		cache_ptr += 3;
		
		//std::cout << "_GenerateColorCache() : cache_index " << cache_index << " of " << cached_colors_count_ << ", value " << value << " mapped to RGB {" << *(cache_ptr - 3) << ", " << *(cache_ptr - 2) << ", " << *(cache_ptr - 1) << "}" << std::endl;
	}
}

void EidosPalette::_InvalidateColorCache(void)
{
	cached_colors_count_ = -1;
	
#if defined(SLIMGUI)
	// Force a display image recache in SLiMgui
	if (image_)
	{
		if (image_deleter_)
			image_deleter_(image_);
		else
			std::cout << "Missing Palette image_deleter_; leaking memory" << std::endl;
		
		image_ = nullptr;
		image_deleter_ = nullptr;
	}
#endif
}

const EidosClass *EidosPalette::Class(void) const
{
	return gEidosPalette_Class;
}

void EidosPalette::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP EidosPalette::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gEidosID_range:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{range_start_, range_end_});
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue_SP EidosPalette::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gEidosID_addNode:					return ExecuteMethod_addNode(p_method_id, p_arguments, p_interpreter);
		case gEidosID_colorForValue:			return ExecuteMethod_colorForValue(p_method_id, p_arguments, p_interpreter);
		case gEidosID_setFixedPoint:			return ExecuteMethod_setFixedPoint(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (o<Palette>$)addNode(numeric value, fs color, [s transition = "linear"], [s blend = "hsvShortest"])
//
EidosValue_SP EidosPalette::ExecuteMethod_addNode(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (immutable_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() cannot modify this palette because it is immutable; make a new palette instead." << EidosTerminate();
	
	EidosValue *value_value = p_arguments[0].get();
	EidosValue *color_value = p_arguments[1].get();
	EidosValue *transition_value = p_arguments[2].get();
	EidosValue *blend_value = p_arguments[3].get();
	
	int value_count = value_value->Count();
	int color_count = color_value->Count();
	
	// transition
	int transition_count = transition_value->Count();
	bool transition_singleton;
	
	if (transition_count == 1)
		transition_singleton = true;
	else if (transition_count == value_count)
		transition_singleton = false;
	else
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires transition to be singleton, or the same length as value." << EidosTerminate();
	
	PaletteTransition singleton_transition = PaletteTransition::kUndefined;
	
	if (transition_singleton)
		singleton_transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(0, nullptr));
	
	// blend
	int blend_count = blend_value->Count();
	bool blend_singleton;
	
	if (blend_count == 1)
		blend_singleton = true;
	else if (blend_count == value_count)
		blend_singleton = false;
	else
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires blend to be singleton, or the same length as value." << EidosTerminate();
	
	PaletteBlend singleton_blend = PaletteBlend::kUndefined;
	
	if (blend_singleton)
		singleton_blend = PaletteBlendFromString(((EidosValue_String *)blend_value)->StringRefAtIndex_NOCAST(0, nullptr));
	
	if ((color_value->Type() == EidosValueType::kValueString) && (color_count == value_count))
	{
		// one color string per value
		for (int value_index = 0; value_index < value_count; value_index++)
		{
			double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
			const std::string &color_string = ((EidosValue_String *)color_value)->StringRefAtIndex_NOCAST(value_index, nullptr);
			double r, g, b;
			PaletteTransition transition;
			PaletteBlend blend;
			
			if (transition_singleton)
				transition = singleton_transition;
			else
				transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index, nullptr));
			
			if (blend_singleton)
				blend = singleton_blend;
			else
				blend = PaletteBlendFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index, nullptr));
			
			Eidos_GetColorComponents(color_string, &r, &g, &b);
			
			AddNode(value, r, g, b, transition, blend);
		}
	}
	else if ((color_value->Type() == EidosValueType::kValueFloat) && (color_count == value_count * 3))
	{
		// one float RGB triplet per value
		for (int value_index = 0; value_index < value_count; value_index++)
		{
			double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
			double r, g, b;
			PaletteTransition transition;
			PaletteBlend blend;
			
			r = color_value->FloatAtIndex_NOCAST(value_index * 3 + 0, nullptr);
			g = color_value->FloatAtIndex_NOCAST(value_index * 3 + 1, nullptr);
			b = color_value->FloatAtIndex_NOCAST(value_index * 3 + 2, nullptr);
			
			if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
				EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires red, green, and blue values to be finite." << EidosTerminate();
			if (((r < 0.0) || (r > 1.0)) || ((g < 0.0) || (g > 1.0)) || ((b < 0.0) || (b > 1.0)))
				EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires red, green, and blue values to be in [0, 1]." << EidosTerminate();
			
			if (transition_singleton)
				transition = singleton_transition;
			else
				transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index, nullptr));
			
			if (blend_singleton)
				blend = singleton_blend;
			else
				blend = PaletteBlendFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index, nullptr));
			
			AddNode(value, r, g, b, transition, blend);
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires either one color string per value, or one float RGB triplet per value." << EidosTerminate();
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gEidosPalette_Class));
}


//	*********************	– (s)colorForValue(numeric value)
//
EidosValue_SP EidosPalette::ExecuteMethod_colorForValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *value_value = p_arguments[0].get();
	int value_count = value_value->Count();
	
	if (value_count == 0)
		return gStaticEidosValue_String_ZeroVec;
	
	EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(value_count);
	
	for (int value_index = 0; value_index < value_count; ++value_index)
	{
		double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
		
		if (!std::isfinite(value))
			EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_colorForValue): colorForValue() requires all elements of value to be finite (not INF or NAN)." << EidosTerminate();
		
		double r, g, b;
		char hex_chars[8];
		
		ColorForValue(value, &r, &g, &b);
		Eidos_GetColorString(r, g, b, hex_chars);
		
		string_result->PushString(std::string(hex_chars));
	}
	
	return EidosValue_SP(string_result);
}


//	*********************	– (o<Palette>$)setFixedPoint(numeric$ value)
//
EidosValue_SP EidosPalette::ExecuteMethod_setFixedPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (immutable_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_setFixedPoint): setFixedPoint() cannot modify this palette because it is immutable; make a new palette instead." << EidosTerminate();
	
	EidosValue *value_value = p_arguments[0].get();
	double value = value_value->FloatAtIndex_CAST(0, nullptr);
	
	if (!std::isfinite(value))
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_setFixedPoint): setFixedPoint() requires value to be finite." << EidosTerminate();
	if ((value <= range_start_) || (value >= range_end_))
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_setFixedPoint): setFixedPoint() requires value be in the interior of the palette's range." << EidosTerminate();
	
	SetFixedValue(value);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gEidosPalette_Class));
}


//
//	Object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	*********************	(object<Palette>$)Palette(...)
//
//		variant 1: ... conforms to numeric$ value, fs color
//		variant 2: ... conforms to numeric range, fs colors)
//		variant 3: ... conforms to numeric values, fs colors, [string$ transition = "linear"], [string$ blend = "hsvShortest"]
static EidosValue_SP Eidos_Instantiate_EidosPalette(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosPalette *objectElement = nullptr;
	
	if ((p_arguments.size() == 2) &&
		((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) && (p_arguments[0]->Count() == 1) &&
		((p_arguments[1]->Type() == EidosValueType::kValueFloat) || (p_arguments[1]->Type() == EidosValueType::kValueString)))
	{
		//		(object<Palette>$)Palette(numeric$ value, fs color)		// start value and start color; this works with addNode()
		EidosValue *value_value = p_arguments[0].get();
		EidosValue *color_value = p_arguments[1].get();
		
		double value = value_value->FloatAtIndex_CAST(0, nullptr);
		
		if (!std::isfinite(value))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a singleton value, the Palette() constructor requires the value to be finite." << EidosTerminate();
		
		if (color_value->Type() == EidosValueType::kValueFloat)
		{
			// rgb triplet
			if (color_value->Count() != 3)
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires RGB color values to contain exactly three elements." << EidosTerminate();
			
			double r = color_value->FloatAtIndex_NOCAST(0, nullptr);
			double g = color_value->FloatAtIndex_NOCAST(1, nullptr);
			double b = color_value->FloatAtIndex_NOCAST(2, nullptr);
			
			if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires RGB color values to be finite." << EidosTerminate();
			if ((r < 0.0) || (r > 1.0) || (g < 0.0) || (g > 1.0) || (b < 0.0) || (b > 1.0))
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires RGB color values to be in [0, 1]." << EidosTerminate();
			
			objectElement = new EidosPalette(value, r, g, b);
		}
		else // (color_value->Type() == EidosValueType::kValueString)
		{
			// color string
			if (color_value->Count() != 1)
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a singleton value, the Palette() constructor requires a string color values to be singleton also." << EidosTerminate();
			
			double r, g, b;
			
			Eidos_GetColorComponents(((EidosValue_String *)color_value)->StringRefAtIndex_NOCAST(0, nullptr), &r, &g, &b);
			
			objectElement = new EidosPalette(value, r, g, b);
		}
	}
	else if ((p_arguments.size() == 2) &&
			 ((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) && (p_arguments[0]->Count() == 2) &&
			 ((p_arguments[1]->Type() == EidosValueType::kValueFloat) || (p_arguments[1]->Type() == EidosValueType::kValueString)))
	{
		//		(object<Palette>$)Palette(numeric range, fs colors)		// overall range, and a bunch of colors
		EidosValue *range_value = p_arguments[0].get();
		EidosValue *color_value = p_arguments[1].get();
		
		double start_value = range_value->FloatAtIndex_CAST(0, nullptr);
		double end_value = range_value->FloatAtIndex_CAST(1, nullptr);
		
		if (!std::isfinite(start_value) || !std::isfinite(end_value) || (start_value >= end_value))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a value range, the start and end values must both be finite, and the start value must be less than the end value." << EidosTerminate();
		
		// we subdivide the value range into equal-length segments and distribute the given colors across it
		int color_count = color_value->Count();
		
		if (((color_value->Type() == EidosValueType::kValueFloat) && (color_count < 6)) ||
			((color_value->Type() == EidosValueType::kValueString) && (color_count < 2)))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a value range, at least two colors must be specified." << EidosTerminate();
		
		for (int color_index = 0; color_index < color_count; ++color_index)
		{
			double value = start_value + color_index * ((end_value - start_value) / (color_count - 1));
			
			if (color_value->Type() == EidosValueType::kValueFloat)
			{
				// rgb triplet
				double r = color_value->FloatAtIndex_NOCAST(color_index * 3 + 0, nullptr);
				double g = color_value->FloatAtIndex_NOCAST(color_index * 3 + 1, nullptr);
				double b = color_value->FloatAtIndex_NOCAST(color_index * 3 + 2, nullptr);
				
				if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
					EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires RGB color values to be finite." << EidosTerminate();
				if ((r < 0.0) || (r > 1.0) || (g < 0.0) || (g > 1.0) || (b < 0.0) || (b > 1.0))
					EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires RGB color values to be in [0, 1]." << EidosTerminate();
				
				if (color_index == 0)
					objectElement = new EidosPalette(value, r, g, b);
				else
					objectElement->AddNode(value, r, g, b, PaletteTransition::kLinear, PaletteBlend::kHSVShortest);
			}
			else
			{
				double r, g, b;
				
				Eidos_GetColorComponents(((EidosValue_String *)color_value)->StringRefAtIndex_NOCAST(color_index, nullptr), &r, &g, &b);
				
				if (color_index == 0)
					objectElement = new EidosPalette(value, r, g, b);
				else
					objectElement->AddNode(value, r, g, b, PaletteTransition::kLinear, PaletteBlend::kHSVShortest);
			}
		}
	}
	else if ((p_arguments.size() >= 2) && (p_arguments.size() <= 4) &&
			 ((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) &&
			 ((p_arguments[1]->Type() == EidosValueType::kValueFloat) || (p_arguments[1]->Type() == EidosValueType::kValueString)))
	{
		//		(object<Palette>$)Palette(numeric value, fs color, [s transition = "linear"], [s blend = "hsvShortest"])		// values, colors, transitions, and blends
		EidosValue *value_value = p_arguments[0].get();
		EidosValue *color_value = p_arguments[1].get();
		EidosValue *transition_value = (p_arguments.size() >= 3) ? p_arguments[2].get() : nullptr;
		EidosValue *blend_value = (p_arguments.size() >= 4) ? p_arguments[3].get() : nullptr;
		
		int value_count = value_value->Count();
		int color_count = color_value->Count();
		
		// transition
		bool transition_singleton;
		PaletteTransition singleton_transition = PaletteTransition::kUndefined;
		
		if (!transition_value)
		{
			transition_singleton = true;
			singleton_transition = PaletteTransition::kLinear;
		}
		else
		{
			int transition_count = transition_value->Count();
			
			if (transition_count == 1)
				transition_singleton = true;
			else if (transition_count == value_count - 1)
				transition_singleton = false;
			else
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires transition to be singleton, or one less than the length of value." << EidosTerminate();
			
			if (transition_singleton)
				singleton_transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(0, nullptr));
		}
		
		// blend
		bool blend_singleton;
		PaletteBlend singleton_blend = PaletteBlend::kUndefined;
		
		if (!blend_value)
		{
			blend_singleton = true;
			singleton_blend = PaletteBlend::kHSVShortest;
		}
		else
		{
			int blend_count = blend_value->Count();
			
			if (blend_count == 1)
				blend_singleton = true;
			else if (blend_count == value_count - 1)
				blend_singleton = false;
			else
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires blend to be singleton, or one less than the length of value." << EidosTerminate();
			
			if (blend_singleton)
				singleton_blend = PaletteBlendFromString(((EidosValue_String *)blend_value)->StringRefAtIndex_NOCAST(0, nullptr));
		}
		
		if ((color_value->Type() == EidosValueType::kValueString) && (color_count == value_count))
		{
			// one color string per value
			for (int value_index = 0; value_index < value_count; value_index++)
			{
				double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
				const std::string &color_string = ((EidosValue_String *)color_value)->StringRefAtIndex_NOCAST(value_index, nullptr);
				double r, g, b;
				
				Eidos_GetColorComponents(color_string, &r, &g, &b);
				
				if (value_index == 0)
				{
					objectElement = new EidosPalette(value, r, g, b);
				}
				else
				{
					PaletteTransition transition;
					PaletteBlend blend;
					
					if (transition_singleton)
						transition = singleton_transition;
					else
						transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index - 1, nullptr));
					
					if (blend_singleton)
						blend = singleton_blend;
					else
						blend = PaletteBlendFromString(((EidosValue_String *)blend_value)->StringRefAtIndex_NOCAST(value_index - 1, nullptr));
					
					objectElement->AddNode(value, r, g, b, transition, blend);
				}
			}
		}
		else if ((color_value->Type() == EidosValueType::kValueFloat) && (color_count == value_count * 3))
		{
			// one float RGB triplet per value
			for (int value_index = 0; value_index < value_count; value_index++)
			{
				double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
				double r, g, b;
				r = color_value->FloatAtIndex_NOCAST(value_index * 3 + 0, nullptr);
				g = color_value->FloatAtIndex_NOCAST(value_index * 3 + 1, nullptr);
				b = color_value->FloatAtIndex_NOCAST(value_index * 3 + 2, nullptr);
				
				if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
					EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires red, green, and blue values to be finite." << EidosTerminate();
				if (((r < 0.0) || (r > 1.0)) || ((g < 0.0) || (g > 1.0)) || ((b < 0.0) || (b > 1.0)))
					EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires red, green, and blue values to be in [0, 1]." << EidosTerminate();
				
				if (value_index == 0)
				{
					objectElement = new EidosPalette(value, r, g, b);
				}
				else
				{
					PaletteTransition transition;
					PaletteBlend blend;
					
					if (transition_singleton)
						transition = singleton_transition;
					else
						transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index - 1, nullptr));
					
					if (blend_singleton)
						blend = singleton_blend;
					else
						blend = PaletteBlendFromString(((EidosValue_String *)blend_value)->StringRefAtIndex_NOCAST(value_index - 1, nullptr));
					
					objectElement->AddNode(value, r, g, b, transition, blend);
				}
			}
		}
		else
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires either one color string per value, or one float RGB triplet per value." << EidosTerminate();
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires either a singleton start value and a start color, a value range and a vector/matrix of colors, or a vector of values and a vector/matrix of colors with optional transitions and blends." << EidosTerminate();
	}
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(objectElement, gEidosPalette_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}


//
//	EidosPalette_Class
//
#pragma mark -
#pragma mark EidosPalette_Class
#pragma mark -

EidosPalette_Class *gEidosPalette_Class = nullptr;


std::vector<EidosPropertySignature_CSP> *EidosPalette_Class::Properties_MUTABLE(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosPalette_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties_MUTABLE());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_range,				true,	kEidosValueMaskFloat)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosPalette_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosPalette_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_addNode, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddNumeric("value")->AddFloatString("color")->AddString_O("transition", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("linear")))->AddString_O("blend", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("hsvShortest"))));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_colorForValue, kEidosValueMaskString))->AddNumeric("value"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_setFixedPoint, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddNumeric_S("value"));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *EidosPalette_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosPalette_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		// the Palette() constructor has three ellipsis variants
		{
			EidosFunctionSignature *ellipsisSignature = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddEllipsis();
			EidosFunctionSignature *variant1 = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddNumeric_S("value")->AddFloatString("color");
			EidosFunctionSignature *variant2 = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddNumeric("range")->AddFloatString("colors");
			EidosFunctionSignature *variant3 = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddNumeric("values")->AddFloatString("colors")->AddString_OS("transition", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("linear")))->AddString_OS("blend", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("hsvShortest")));
			
			ellipsisSignature->AddEllipsisVariant(variant1)->AddEllipsisVariant(variant2)->
				AddEllipsisVariant(variant3);	// ownership of these objects is taken from us
			functions->emplace_back(ellipsisSignature);
		}
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}


































































