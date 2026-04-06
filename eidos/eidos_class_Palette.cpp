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

static int CountFromColorValue(EidosValue *p_color_value, bool p_singleton_required)
{
	// for a color_value that is either float RGB triplets or string colors, this returns the number of colors represented, or raises
	int count = p_color_value->Count();
	
	if (p_color_value->Type() == EidosValueType::kValueString)
	{
		if (p_singleton_required && (count != 1))
			EIDOS_TERMINATION << "ERROR (CountFromColorValue): a single color is required." << EidosTerminate();
		
		return count;
	}
	else if (p_color_value->Type() == EidosValueType::kValueFloat)
	{
		if (count % 3 != 0)
			EIDOS_TERMINATION << "ERROR (CountFromColorValue): the length of RGB color values must be a multiple of three (an even number of RGB triplets)." << EidosTerminate();
		if (p_singleton_required && (count != 3))
			EIDOS_TERMINATION << "ERROR (CountFromColorValue): a single color is required." << EidosTerminate();
		
		return count / 3;
	}
	else
		EIDOS_TERMINATION << "ERROR (CountFromColorValue): a color string or an RGB color value is required." << EidosTerminate();
}

static void ColorFromColorValue(EidosValue *p_color_value, int p_value_index, double *p_r, double *p_g, double *p_b, bool p_singleton_required)
{
	// for a color_value that is either float RGB triplets or string colors, this returns the color at a given index
	int count = CountFromColorValue(p_color_value, p_singleton_required);		// this will error-check the value for us
	
	if ((p_value_index < 0) || (p_value_index >= count))
		EIDOS_TERMINATION << "ERROR (ColorFromColorValue): (internal error) color value index " << p_value_index << " out of range." << EidosTerminate();
	
	// for a color_value that is either float RGB triplets or string colors, this returns the color at the given index, or raises
	if (p_color_value->Type() == EidosValueType::kValueString)
	{
		const std::string &color_string = ((EidosValue_String *)p_color_value)->StringRefAtIndex_NOCAST(p_value_index, nullptr);
		
		Eidos_GetColorComponents(color_string, p_r, p_g, p_b);
	}
	else // (color_value->Type() == EidosValueType::kValueFloat)
	{
		double r = p_color_value->FloatAtIndex_NOCAST(p_value_index * 3 + 0, nullptr);
		double g = p_color_value->FloatAtIndex_NOCAST(p_value_index * 3 + 1, nullptr);
		double b = p_color_value->FloatAtIndex_NOCAST(p_value_index * 3 + 2, nullptr);
		
		if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
			EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires red, green, and blue values to be finite." << EidosTerminate();
		if (((r < 0.0) || (r > 1.0)) || ((g < 0.0) || (g > 1.0)) || ((b < 0.0) || (b > 1.0)))
			EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires red, green, and blue values to be in [0, 1]." << EidosTerminate();
		
		*p_r = r;
		*p_g = g;
		*p_b = b;
	}
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

EidosPalette *EidosPalette::SetFixedValue(double value, float p_red, float p_green, float p_blue)
{
	// a fixed value at the given value, with the given color; this is used when constructing
	// a palette based upon another palette so that the fixed color doesn't drift at all
#if DEBUG
	if (!std::isfinite(value))
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) palette values must be finite." << EidosTerminate();
	if ((value <= range_start_) || (value >= range_end_))
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) a fixed value must be in the interior of the palette's range." << EidosTerminate();
	if (!std::isfinite(p_red) || !std::isfinite(p_green) || !std::isfinite(p_blue))
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) the RGB values must be finite." << EidosTerminate();
	if ((p_red < (float)0.0) || (p_red > (float)1.0) || (p_green < (float)0.0) || (p_green > (float)1.0) || (p_blue < (float)0.0) || (p_blue > (float)1.0))
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) the RGB values must be in [0, 1]." << EidosTerminate();
#endif
	
	if (immutable_)
		EIDOS_TERMINATION << "ERROR (EidosPalette::SetFixedValue): (internal error) the palette cannot be changed, because it is immutable." << EidosTerminate();
	
	fixed_value_ = value;
	fixed_r_ = p_red;
	fixed_g_ = p_green;
	fixed_b_ = p_blue;
	
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
	
	if (raw_fraction < 0.0)
		raw_fraction = 0.0;
	if (raw_fraction > 1.0)
		raw_fraction = 1.0;
	
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
	
	if (transformed_fraction < 0.0)
		transformed_fraction = 0.0;
	if (transformed_fraction > 1.0)
		transformed_fraction = 1.0;
	
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
	PaletteNode *end_node_ptr = node_ptr + nodes_.size();
	PaletteNode *previous_node_ptr = node_ptr++;
	
	// advance to the correct pair of palette nodes, between which the input value lies
	while (value > node_ptr->value_)
	{
		// don't overrun the last pair of nodes
		if (node_ptr + 1 >= end_node_ptr)
			break;
		
		previous_node_ptr = node_ptr++;
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
	PaletteNode *end_node_ptr = node_ptr + nodes_.size();
	PaletteNode *previous_node_ptr = node_ptr++;
	
	//std::cout << "_GenerateColorCache() : range_start_ " << range_start_ << " range_end_ " << range_end_ << std::endl;
	
	for (int cache_index = 0; cache_index < cached_colors_count_; ++cache_index)
	{
		// calculate the input value corresponding to this cache index
		double value = range_start_ + (range_end_ - range_start_) * (cache_index / (double)(cached_colors_count_ - 1));
		
		// advance to the correct pair of palette nodes, between which the input value lies
		while (value > node_ptr->value_)
		{
			// don't overrun the last pair of nodes
			if (node_ptr + 1 >= end_node_ptr)
				break;
			
			previous_node_ptr = node_ptr++;
		}
		
		_CalculateColorForValueGivenNodes(value, cache_ptr, cache_ptr + 1, cache_ptr + 2, node_ptr, previous_node_ptr);
		cache_ptr += 3;
		
		//std::cout << "   _GenerateColorCache() : cache_index " << cache_index << " of " << cached_colors_count_ << ", value " << value << " mapped to RGB {" << *(cache_ptr - 3) << ", " << *(cache_ptr - 2) << ", " << *(cache_ptr - 1) << "}" << std::endl;
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
	
	if (CountFromColorValue(color_value, /* p_singleton_required */ false) != value_count)
		EIDOS_TERMINATION << "ERROR (EidosPalette::ExecuteMethod_addNode): addNode() requires either one color string per value, or one float RGB triplet per value." << EidosTerminate();
	
	for (int value_index = 0; value_index < value_count; value_index++)
	{
		double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
		PaletteTransition transition;
		PaletteBlend blend;
		double r, g, b;
		
		if (transition_singleton)
			transition = singleton_transition;
		else
			transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index, nullptr));
		
		if (blend_singleton)
			blend = singleton_blend;
		else
			blend = PaletteBlendFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(value_index, nullptr));
		
		ColorFromColorValue(color_value, value_index, &r, &g, &b, /* p_singleton_required */ false);
		AddNode(value, r, g, b, transition, blend);
	}
	
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
//		variant 1: numeric$ value, fs color
//		variant 2: numeric range, fs colors)
//		variant 3: numeric values, fs colors, [string transition = "linear"], [string blend = "hsvShortest"]
//		variant 4: string$ colors, numeric range, [Nif sourceRange = NULL]
//		variant 5: object<Palette>$ palette, float rescaledRange, [Nf existingRange = NULL], [logical$ fullPalette = T]
//
// Note that Eidos does NOT type-check variants for us; it checks all variants against the ellipsis in the
// base signature.  We therefore have to check very carefully below to detect illegal calling patterns.
//
static EidosValue_SP Eidos_Instantiate_EidosPalette(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosPalette *objectElement = nullptr;
	
	if ((p_arguments.size() == 2) &&
		((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) && (p_arguments[0]->Count() == 1) &&
		((p_arguments[1]->Type() == EidosValueType::kValueFloat) || (p_arguments[1]->Type() == EidosValueType::kValueString)))
	{
		// Variant 1: start value and start color; this works with addNode()
		//
		//		(object<Palette>$)Palette(numeric$ value, fs color)
		EidosValue *value_value = p_arguments[0].get();
		EidosValue *color_value = p_arguments[1].get();
		
		double value = value_value->FloatAtIndex_CAST(0, nullptr);
		double r, g, b;
		
		if (!std::isfinite(value))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a singleton value, the Palette() constructor requires the value to be finite." << EidosTerminate();
		
		ColorFromColorValue(color_value, 0, &r, &g, &b, true);
		
		objectElement = new EidosPalette(value, r, g, b);
	}
	else if ((p_arguments.size() == 2) &&
			 ((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) &&
			 ((p_arguments[1]->Type() == EidosValueType::kValueFloat) || (p_arguments[1]->Type() == EidosValueType::kValueString)))
	{
		// Variant 2: overall range, and a bunch of colors
		//
		//		(object<Palette>$)Palette(numeric range, fs colors)
		EidosValue *range_value = p_arguments[0].get();
		EidosValue *color_value = p_arguments[1].get();
		
		if (p_arguments[0]->Count() != 2)
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): range must be a vector of length two, containing the start and end of the range." << EidosTerminate();
		
		double start_value = range_value->FloatAtIndex_CAST(0, nullptr);
		double end_value = range_value->FloatAtIndex_CAST(1, nullptr);
		
		if (!std::isfinite(start_value) || !std::isfinite(end_value) || (start_value >= end_value))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a value range, the start and end values must both be finite, and the start value must be less than the end value." << EidosTerminate();
		
		// we subdivide the value range into equal-length segments and distribute the given colors across it
		int color_count = CountFromColorValue(color_value, /* p_singleton_required */ false);
		
		if (color_count < 2)
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a value range, at least two colors must be specified." << EidosTerminate();
		
		for (int color_index = 0; color_index < color_count; ++color_index)
		{
			double value = start_value + color_index * ((end_value - start_value) / (color_count - 1));
			double r, g, b;
			
			ColorFromColorValue(color_value, color_index, &r, &g, &b, /* p_singleton_required */ false);
			
			if (color_index == 0)
				objectElement = new EidosPalette(value, r, g, b);
			else
				objectElement->AddNode(value, r, g, b, PaletteTransition::kLinear, PaletteBlend::kHSVShortest);
		}
	}
	else if ((p_arguments.size() == 4) &&
			 ((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) &&
			 ((p_arguments[1]->Type() == EidosValueType::kValueFloat) || (p_arguments[1]->Type() == EidosValueType::kValueString)) &&
			 (p_arguments[2]->Type() == EidosValueType::kValueString) &&
			 (p_arguments[3]->Type() == EidosValueType::kValueString))
	{
		// Variant 3: values, colors, transition, and blend
		//
		//		(object<Palette>$)Palette(numeric value, fs color, [s transition = "linear"], [s blend = "hsvShortest"])
		EidosValue *value_value = p_arguments[0].get();
		EidosValue *color_value = p_arguments[1].get();
		EidosValue *transition_value = p_arguments[2].get();
		EidosValue *blend_value = p_arguments[3].get();
		
		int value_count = value_value->Count();
		
		// transition
		bool transition_singleton;
		PaletteTransition singleton_transition = PaletteTransition::kUndefined;
		int transition_count = transition_value->Count();
		
		if (transition_count == 1)
			transition_singleton = true;
		else if (transition_count == value_count - 1)
			transition_singleton = false;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires transition to be singleton, or one less than the length of value." << EidosTerminate();
		
		if (transition_singleton)
			singleton_transition = PaletteTransitionFromString(((EidosValue_String *)transition_value)->StringRefAtIndex_NOCAST(0, nullptr));
		
		// blend
		bool blend_singleton;
		PaletteBlend singleton_blend = PaletteBlend::kUndefined;
		int blend_count = blend_value->Count();
		
		if (blend_count == 1)
			blend_singleton = true;
		else if (blend_count == value_count - 1)
			blend_singleton = false;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires blend to be singleton, or one less than the length of value." << EidosTerminate();
		
		if (blend_singleton)
			singleton_blend = PaletteBlendFromString(((EidosValue_String *)blend_value)->StringRefAtIndex_NOCAST(0, nullptr));
		
		if (CountFromColorValue(color_value, /* p_singleton_required */ false) != value_count)
			EIDOS_TERMINATION << "ERROR (EidosPalette::Eidos_Instantiate_EidosPalette): the Palette() constructor requires either one color string per value, or one float RGB triplet per value." << EidosTerminate();
		
		for (int value_index = 0; value_index < value_count; value_index++)
		{
			double value = value_value->FloatAtIndex_CAST(value_index, nullptr);
			double r, g, b;
			
			ColorFromColorValue(color_value, value_index, &r, &g, &b, /* p_singleton_required */ false);
			
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
	else if ((p_arguments.size() == 3) &&
			 (p_arguments[0]->Type() == EidosValueType::kValueString) && (p_arguments[0]->Count() == 1) &&
			 ((p_arguments[1]->Type() == EidosValueType::kValueInt) || (p_arguments[1]->Type() == EidosValueType::kValueFloat)) &&
			 ((p_arguments[2]->Type() == EidosValueType::kValueInt) || (p_arguments[2]->Type() == EidosValueType::kValueFloat) || (p_arguments[2]->Type() == EidosValueType::kValueNULL)))
	{
		// variant 4: from an Eidos color palette (as from colors()), with a range and a source range
		//
		//		(object<Palette>$)Palette(string$ colors, numeric range, [Nif sourceRange = NULL])
		EidosValue_String *colors_value = (EidosValue_String *)p_arguments[0].get();
		EidosValue *range_value = p_arguments[1].get();
		EidosValue *sourceRange_value = p_arguments[2].get();
		
		if (p_arguments[1]->Count() == 2)
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): range must be a vector of length two, containing the start and end of the range." << EidosTerminate();
		if ((p_arguments[2]->Type() != EidosValueType::kValueNULL) && (p_arguments[2]->Count() == 2))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): sourceRange must be a vector of length two, containing the start and end of the source range (or it may be NULL)." << EidosTerminate();
		
		const std::string &colors = colors_value->StringRefAtIndex_NOCAST(0, nullptr);
		EidosColorPalette palette = EidosColorPaletteForName(colors);
		
		if (palette == EidosColorPalette::kPalette_INVALID)
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): unrecognized color palette name in Palette()." << EidosTerminate(nullptr);
		
		double start_value = range_value->FloatAtIndex_CAST(0, nullptr);
		double end_value = range_value->FloatAtIndex_CAST(1, nullptr);
		
		if (!std::isfinite(start_value) || !std::isfinite(end_value) || (start_value >= end_value))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): with a value range, the start and end values must both be finite, and the start value must be less than the end value." << EidosTerminate();
		
		double source_start = 0.0, source_end = 1.0;
		
		if (sourceRange_value->Type() != EidosValueType::kValueNULL)
		{
			source_start = sourceRange_value->FloatAtIndex_CAST(0, nullptr);
			source_end = sourceRange_value->FloatAtIndex_CAST(1, nullptr);
		}
		
		int color_count = 256;	// matches the number of steps in many of the palettes in eidos_tinycolormap
		
		for (int color_index = 0; color_index < color_count; ++color_index)
		{
			double value = start_value + color_index * ((end_value - start_value) / (color_count - 1));
			double sourceValue = source_start + color_index * ((source_end - source_start) / (color_count - 1));
			double r, g, b;
			
			EidosColorPaletteLookup(sourceValue, palette, r, g, b);
			
			if (color_index == 0)
				objectElement = new EidosPalette(value, r, g, b);
			else
				objectElement->AddNode(value, r, g, b, PaletteTransition::kLinear, PaletteBlend::kHSVShortest);
		}
	}
	else if ((p_arguments.size() == 4) &&
			 (p_arguments[0]->Type() == EidosValueType::kValueObject))
	{
		// variant 5: from an existing Palette object, using its full range or a subrange, and rescaling/shifting it
		//
		//		(object<Palette>$)Palette(object<Palette>$ palette, float rescaledRange, [Nf existingRange = NULL], [logical$ fullPalette = T])
		EidosValue *palette_value = p_arguments[0].get();
		EidosValue *rescaledRange_value = p_arguments[1].get();
		EidosValue *existingRange_value = p_arguments[2].get();
		EidosValue *fullPalette_value = p_arguments[3].get();
		
		EidosPalette *existingPalette = dynamic_cast<EidosPalette *>(palette_value->ObjectElementAtIndex_NOCAST(0, nullptr));
		double existingFull_start, existingFull_end;
		
		existingPalette->Range(&existingFull_start, &existingFull_end);
		
		// get the rescaled range for the new palette
		double rescaledRange_start = rescaledRange_value->FloatAtIndex_NOCAST(0, nullptr);
		double rescaledRange_end = rescaledRange_value->FloatAtIndex_NOCAST(1, nullptr);
		
		if (!std::isfinite(rescaledRange_start) || !std::isfinite(rescaledRange_end) || (rescaledRange_start > rescaledRange_end))
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): for Palette() variant 5, rescaledRange must be a two-element float vector, [start, end], and start and end must be finite values with start < end." << EidosTerminate();
		
		// get the existing range in the existing palette
		bool existingRange_is_NULL;
		double existingRange_start, existingRange_end;
		
		if (existingRange_value->Type() == EidosValueType::kValueNULL)
		{
			existingRange_is_NULL = true;
			existingPalette->Range(&existingRange_start, &existingRange_end);
		}
		else
		{
			existingRange_is_NULL = false;
			existingRange_start = existingRange_value->FloatAtIndex_NOCAST(0, nullptr);
			existingRange_end = existingRange_value->FloatAtIndex_NOCAST(1, nullptr);
			
			if (!std::isfinite(existingRange_start) || !std::isfinite(existingRange_end) || (rescaledRange_start == rescaledRange_end))
				EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): for Palette() variant 5, existingRange must be a two-element float vector, [start, end], and start and end must be finite values with start != end." << EidosTerminate();
			
			if ((existingRange_start == existingFull_start) && (existingRange_end == existingFull_end))
			{
				// the specified range is the full range of the existing palette, so it's as if NULL was passed
				existingRange_is_NULL = true;
			}
		}
		
		// get fullPalette, and if it is T and we're using a subrange of the existing palette, calculate the true ranges
		bool fullPalette = fullPalette_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		// to get from the existing subrange to the rescaled subrange involves a translation and a scaling;
		// we figure that out, so we can map any position on the existing palette to a rescaled position
		double existing_subrange_width = existingRange_end - existingRange_start;
		double rescaled_subrange_width = rescaledRange_end - rescaledRange_start;
		
		double translation_to_zero = -existingRange_start;
		double rescaling_factor = rescaled_subrange_width / existing_subrange_width;
		double translation_to_rescaled_start = rescaledRange_start;
		
		if (fullPalette && !existingRange_is_NULL)
		{
			// use our remapping to remap the full existing range to the full rescaled range
			double remapped_existingFull_start = (existingFull_start + translation_to_zero) * rescaling_factor + translation_to_rescaled_start;
			double remapped_existingFull_end = (existingFull_end + translation_to_zero) * rescaling_factor + translation_to_rescaled_start;
			
			rescaledRange_start = remapped_existingFull_start;
			rescaledRange_end = remapped_existingFull_end;
			
			// and adopt the extended existing range; that is the range we will iterate over
			existingRange_start = existingFull_start;
			existingRange_end = existingFull_end;
			
			// the above code will result in a flipped rescaled range if the supplied existing range
			// was flipped; sort that out by flipping both the rescaled range and the existing range,
			// which is equivalent and lets us build the new palette from start to end as expected
			if (rescaledRange_start > rescaledRange_end)
			{
				std::swap(rescaledRange_start, rescaledRange_end);
				std::swap(existingRange_start, existingRange_end);
			}
		}
		
		// now generate the new palette
		int color_count = 1024;	// arbitrary, but hopefully high enough to capture the details of the source palette even if it is complex
		
		for (int color_index = 0; color_index < color_count; ++color_index)
		{
			double r, g, b;
			double existingValue = existingRange_start + (existingRange_end - existingRange_start) * (color_index / (double)(color_count - 1));
			double rescaledValue = rescaledRange_start + (rescaledRange_end - rescaledRange_start) * (color_index / (double)(color_count - 1));
			
			existingPalette->ColorForValue(existingValue, &r, &g, &b);
			
			if (color_index == 0)
				objectElement = new EidosPalette(rescaledValue, r, g, b);
			else
				objectElement->AddNode(rescaledValue, r, g, b, PaletteTransition::kLinear, PaletteBlend::kHSVShortest);
		}
		
		// transfer the fixed point of the existing palette, if it is within range
		double fixed_value;
		float fixed_r, fixed_g, fixed_b;
		
		existingPalette->GetFixedValue(&fixed_value, &fixed_r, &fixed_g, &fixed_b);
		
		if (!std::isnan(fixed_value))
		{
			// use our remapping to remap the fixed value to the rescaled coordinate system
			double rescaled_fixed_value = (fixed_value + translation_to_zero) * rescaling_factor + translation_to_rescaled_start;
			
			objectElement->SetFixedValue(rescaled_fixed_value, fixed_r, fixed_g, fixed_b);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosPalette): the Palette() constructor requires the arguments passed to conform to one of five specific variants (see the documentation); these arguments were not recognized as one of those variants." << EidosTerminate();
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
			EidosFunctionSignature *variant3 = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddNumeric("values")->AddFloatString("colors")->AddString_O("transition", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("linear")))->AddString_O("blend", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("hsvShortest")));
			EidosFunctionSignature *variant4 = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddString_S("colors")->AddNumeric("range")->AddNumeric_ON("sourceRange", gStaticEidosValueNULL);
			EidosFunctionSignature *variant5 = (EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Palette, Eidos_Instantiate_EidosPalette, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class))->AddObject_S("palette", gEidosPalette_Class)->AddFloat("rescaledRange")->AddFloat_ON("existingRange", gStaticEidosValueNULL)->AddLogical_OS("fullPalette", gStaticEidosValue_LogicalT);
			
			// ownership of these objects is taken from us
			ellipsisSignature->AddEllipsisVariant(variant1, "a single node");
			ellipsisSignature->AddEllipsisVariant(variant2, "a range and colors");
			ellipsisSignature->AddEllipsisVariant(variant3, "values and colors");
			ellipsisSignature->AddEllipsisVariant(variant4, "from a colors() palette");
			ellipsisSignature->AddEllipsisVariant(variant5, "from a Palette object");
			
			functions->emplace_back(ellipsisSignature);
		}
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}


































































