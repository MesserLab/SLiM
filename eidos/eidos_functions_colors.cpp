//
//  eidos_functions_color.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15; split from eidos_functions.cpp 09/26/2022
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#include "eidos_functions.h"

#include <string>
#include <vector>


// ************************************************************************************
//
//	color manipulation functions
//
#pragma mark -
#pragma mark Color manipulation functions
#pragma mark -


//	(string)cmColors(integer$ n)
//	DEPRECATED IN SLIM 3.5; use colors()
EidosValue_SP Eidos_ExecuteFunction_cmColors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	char hex_chars[8];
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cmColors): cmColors() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double red, green, blue;
		
		Eidos_ColorPaletteLookup(fraction, EidosColorPalette::kPalette_cm, red, green, blue);
		Eidos_GetColorString(red, green, blue, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(string)colors(numeric x, string$ name)
EidosValue_SP Eidos_ExecuteFunction_colors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue_String *name_value = (EidosValue_String *)p_arguments[1].get();
	const std::string &name = name_value->StringRefAtIndex(0, nullptr);
	EidosColorPalette palette;
	char hex_chars[8];
	
	if (name == "cm")				palette = EidosColorPalette::kPalette_cm;
	else if (name == "heat")		palette = EidosColorPalette::kPalette_heat;
	else if (name == "terrain")		palette = EidosColorPalette::kPalette_terrain;
	else if (name == "parula")		palette = EidosColorPalette::kPalette_parula;
	else if (name == "hot")			palette = EidosColorPalette::kPalette_hot;
	else if (name == "jet")			palette = EidosColorPalette::kPalette_jet;
	else if (name == "turbo")		palette = EidosColorPalette::kPalette_turbo;
	else if (name == "gray")		palette = EidosColorPalette::kPalette_gray;
	else if (name == "magma")		palette = EidosColorPalette::kPalette_magma;
	else if (name == "inferno")		palette = EidosColorPalette::kPalette_inferno;
	else if (name == "plasma")		palette = EidosColorPalette::kPalette_plasma;
	else if (name == "viridis")		palette = EidosColorPalette::kPalette_viridis;
	else if (name == "cividis")		palette = EidosColorPalette::kPalette_cividis;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_colors): unrecognized color palette name in colors()." << EidosTerminate(nullptr);
	
	if (x_value->Type() == EidosValueType::kValueInt)
	{
		if (x_value->Count() != 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_colors): colors() requires an integer x parameter value to be singleton (the number of colors to generate)." << EidosTerminate(nullptr);
		
		int64_t x = x_value->IntAtIndex(0, nullptr);
		if ((x < 0) || (x > 100000))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_colors): colors() requires 0 <= x <= 100000." << EidosTerminate(nullptr);
		
		int color_count = (int)x;
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
			double red, green, blue;
			
			Eidos_ColorPaletteLookup(fraction, palette, red, green, blue);
			
			Eidos_GetColorString(red, green, blue, hex_chars);
			string_result->PushString(std::string(hex_chars));
		}
	}
	else if (x_value->Type() == EidosValueType::kValueFloat)
	{
		int color_count = x_value->Count();
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			double fraction = x_value->FloatAtIndex(value_index, nullptr);
			double red, green, blue;
			
			Eidos_ColorPaletteLookup(fraction, palette, red, green, blue);
			
			Eidos_GetColorString(red, green, blue, hex_chars);
			string_result->PushString(std::string(hex_chars));
		}
	}
	
	return result_SP;
}

//	(float)color2rgb(string color)
EidosValue_SP Eidos_ExecuteFunction_color2rgb(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *color_value = (EidosValue_String *)p_arguments[0].get();
	int color_count = color_value->Count();
	float r, g, b;
	
	if (color_count == 1)
	{
		Eidos_GetColorComponents(color_value->StringRefAtIndex(0, nullptr), &r, &g, &b);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{r, g, b});
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((size_t)color_count * 3);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			Eidos_GetColorComponents(color_value->StringRefAtIndex(value_index, nullptr), &r, &g, &b);
			float_result->set_float_no_check(r, value_index);
			float_result->set_float_no_check(g, value_index + color_count);
			float_result->set_float_no_check(b, value_index + color_count + color_count);
		}
		
		const int64_t dim_buf[2] = {color_count, 3};
		
		result_SP->SetDimensions(2, dim_buf);
	}
	
	return result_SP;
}

//	(string)heatColors(integer$ n)
//	DEPRECATED IN SLIM 3.5; use colors()
EidosValue_SP Eidos_ExecuteFunction_heatColors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	char hex_chars[8];
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_heatColors): heatColors() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double red, green, blue;
		
		Eidos_ColorPaletteLookup(fraction, EidosColorPalette::kPalette_heat, red, green, blue);
		Eidos_GetColorString(red, green, blue, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(float)hsv2rgb(float hsv)
EidosValue_SP Eidos_ExecuteFunction_hsv2rgb(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *hsv_value = p_arguments[0].get();
	int hsv_count = hsv_value->Count();
	
	if (((hsv_value->DimensionCount() != 1) || (hsv_count != 3)) &&
		((hsv_value->DimensionCount() != 2) || (hsv_value->Dimensions()[1] != 3)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_hsv2rgb): in function hsv2rgb(), hsv must contain exactly three elements, or be a matrix with exactly three columns." << EidosTerminate(nullptr);
	
	int color_count = hsv_count / 3;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((size_t)color_count * 3);
	result_SP = EidosValue_SP(float_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double h = hsv_value->FloatAtIndex(value_index, nullptr);
		double s = hsv_value->FloatAtIndex(value_index + color_count, nullptr);
		double v = hsv_value->FloatAtIndex(value_index + color_count + color_count, nullptr);
		double r, g, b;
		
		Eidos_HSV2RGB(h, s, v, &r, &g, &b);
		
		float_result->set_float_no_check(r, value_index);
		float_result->set_float_no_check(g, value_index + color_count);
		float_result->set_float_no_check(b, value_index + color_count + color_count);
	}
	
	float_result->CopyDimensionsFromValue(hsv_value);
	
	return result_SP;
}

//	(string)rainbow(integer$ n, [float$ s = 1], [float$ v = 1], [float$ start = 0], [Nf$ end = NULL], [logical$ ccw = T])
EidosValue_SP Eidos_ExecuteFunction_rainbow(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *s_value = p_arguments[1].get();
	EidosValue *v_value = p_arguments[2].get();
	EidosValue *start_value = p_arguments[3].get();
	EidosValue *end_value = p_arguments[4].get();
	EidosValue *ccw_value = p_arguments[5].get();
	
	int64_t n = n_value->IntAtIndex(0, nullptr);
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	double s = s_value->FloatAtIndex(0, nullptr);
	
	if ((s < 0.0) || (s > 1.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV saturation s to be in the interval [0.0, 1.0]." << EidosTerminate(nullptr);
	
	double v = v_value->FloatAtIndex(0, nullptr);
	
	if ((v < 0.0) || (v > 1.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV value v to be in the interval [0.0, 1.0]." << EidosTerminate(nullptr);
	
	double start = start_value->FloatAtIndex(0, nullptr);
	
	if ((start < 0.0) || (start > 1.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV hue start to be in the interval [0.0, 1.0]." << EidosTerminate(nullptr);
	
	double end = (end_value->Type() == EidosValueType::kValueNULL) ? ((n-1) / (double)n) : end_value->FloatAtIndex(0, nullptr);
	
	if ((n > 0) && ((end < 0.0) || (end > 1.0)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV hue end to be in the interval [0.0, 1.0], or NULL." << EidosTerminate(nullptr);
	
	if ((n > 1) && (start == end))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires start != end." << EidosTerminate(nullptr);
	
	eidos_logical_t ccw = ccw_value->LogicalAtIndex(0, nullptr);
	
	if (ccw && (end < start))
		end += 1.0;
	else if (!ccw && (end > start))
		start += 1.0;
	
	char hex_chars[8];
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	double r, g, b;
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double w = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double h = start + (end - start) * w;
		
		if (h >= 1.0)
			h -= 1.0;
		
		Eidos_HSV2RGB(h, s, v, &r, &g, &b);
		Eidos_GetColorString(r, g, b, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(string)rgb2color(float rgb)
EidosValue_SP Eidos_ExecuteFunction_rgb2color(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *rgb_value = p_arguments[0].get();
	int rgb_count = rgb_value->Count();
	char hex_chars[8];
	
	if (((rgb_value->DimensionCount() != 1) || (rgb_count != 3)) &&
		((rgb_value->DimensionCount() != 2) || (rgb_value->Dimensions()[1] != 3)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2color): in function rgb2color(), rgb must contain exactly three elements, or be a matrix with exactly three columns." << EidosTerminate(nullptr);
	
	if ((rgb_value->DimensionCount() == 1) && (rgb_count == 3))
	{
		double r = rgb_value->FloatAtIndex(0, nullptr);
		double g = rgb_value->FloatAtIndex(1, nullptr);
		double b = rgb_value->FloatAtIndex(2, nullptr);
		
		if (std::isnan(r) || std::isnan(g) || std::isnan(b))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2color): color component with value NAN is not legal." << EidosTerminate();
		
		Eidos_GetColorString(r, g, b, hex_chars);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(hex_chars)));
	}
	else
	{
		int color_count = rgb_count / 3;
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			double r = rgb_value->FloatAtIndex(value_index, nullptr);
			double g = rgb_value->FloatAtIndex(value_index + color_count, nullptr);
			double b = rgb_value->FloatAtIndex(value_index + color_count + color_count, nullptr);
			
			if (std::isnan(r) || std::isnan(g) || std::isnan(b))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2color): color component with value NAN is not legal." << EidosTerminate();
			
			Eidos_GetColorString(r, g, b, hex_chars);
			
			string_result->PushString(std::string(hex_chars));
		}
	}
	
	return result_SP;
}

//	(float)rgb2hsv(float rgb)
EidosValue_SP Eidos_ExecuteFunction_rgb2hsv(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *rgb_value = p_arguments[0].get();
	int rgb_count = rgb_value->Count();
	
	if (((rgb_value->DimensionCount() != 1) || (rgb_count != 3)) &&
		((rgb_value->DimensionCount() != 2) || (rgb_value->Dimensions()[1] != 3)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2hsv): in function rgb2hsv(), rgb must contain exactly three elements, or be a matrix with exactly three columns." << EidosTerminate(nullptr);
	
	int color_count = rgb_count / 3;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((size_t)color_count * 3);
	result_SP = EidosValue_SP(float_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double r = rgb_value->FloatAtIndex(value_index, nullptr);
		double g = rgb_value->FloatAtIndex(value_index + color_count, nullptr);
		double b = rgb_value->FloatAtIndex(value_index + color_count + color_count, nullptr);
		double h, s, v;
		
		Eidos_RGB2HSV(r, g, b, &h, &s, &v);
		
		float_result->set_float_no_check(h, value_index);
		float_result->set_float_no_check(s, value_index + color_count);
		float_result->set_float_no_check(v, value_index + color_count + color_count);
	}
	
	float_result->CopyDimensionsFromValue(rgb_value);
	
	return result_SP;
}

//	(string)terrainColors(integer$ n)
//	DEPRECATED IN SLIM 3.5; use colors()
EidosValue_SP Eidos_ExecuteFunction_terrainColors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	char hex_chars[8];
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_terrainColors): terrainColors() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double red, green, blue;
		
		Eidos_ColorPaletteLookup(fraction, EidosColorPalette::kPalette_terrain, red, green, blue);
		Eidos_GetColorString(red, green, blue, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

















































