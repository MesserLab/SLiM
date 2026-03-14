//
//  eidos_class_Palette.h
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

/*
 
 The class Palette provides a mapping from a continuous floating-point value to a corresponding color
 
 */

#ifndef __Eidos__eidos_class_palette__
#define __Eidos__eidos_class_palette__


#include "eidos_value.h"


class EidosPalette_Class;
extern EidosPalette_Class *gEidosPalette_Class;

// This enum represents a style of transition from one color to the next
enum class PaletteTransition {
	kUndefined = -1,
	kStep = 0,
	kLinear,
	kSigmoid,
	kInverseSigmoid,
	kEaseInSqrt,
	kEaseInSquare,
	kEaseOutSqrt,
	kEaseOutSquare
};

PaletteTransition PaletteTransitionFromString(const std::string string);

// This enum represents a way of producing intermediate colors between two endpoint colors
enum class PaletteBlend {
	kUndefined = -1,
	kRGB = 0,
	kHSVClockwise,
	kHSVCounterclockwise,
	kHSVShortest,
	kHSVLongest
};

PaletteBlend PaletteBlendFromString(const std::string string);

// This struct represents one node in the color scheme sequence, telling Palette how to move
// from one color to the next.  Note the transition info for the first node is not used;
// only its position and color are used.
typedef struct _PaletteNode
{
	double value_;
	double r_, g_, b_;
	double h_, s_, v_;					// cached from r_, g_, b_
	PaletteTransition transition_;
	PaletteBlend blend_;
	
	_PaletteNode(double value, double r, double g, double b, PaletteTransition transition, PaletteBlend blend) :
		value_(value), r_(r), g_(g), b_(b), transition_(transition), blend_(blend)
	{
		// cache HSV values up front, to avoid calculating them multiple times later
		Eidos_RGB2HSV(r, g, b, &h_, &s_, &v_);
	};
} PaletteNode;

class EidosPalette : public EidosDictionaryRetained
{
private:
	typedef EidosDictionaryRetained super;
	
protected:
	double range_start_, range_end_;
	std::vector<PaletteNode> nodes_;
	
	// for speed, we keep a cache of precalculated values that is large enough that we don't need to interpolate
	// each cache entry consists of an RGB triplet of floats; the cache size is cached_colors_count_ * 3 floats
	int cached_colors_count_ = -1;
	float *cached_colors_ = nullptr;	// OWNED POINTER; float is used for final values since the cache is big
	
	void _CalculateColorForValueGivenNodes(double value, float *r, float *g, float *b, PaletteNode *node_ptr, PaletteNode *previous_node_ptr);
	
	void _GenerateColorCache(void);
	void _InvalidateColorCache(void);
	inline __attribute__((always_inline)) bool _ColorCacheIsInvalid(void) { return (cached_colors_count_ == -1); }
	
public:
#if defined(SLIMGUI)
	// This cache is for Plot's image() method
	void *image_ = nullptr;							// OWNED POINTER: a QImage* used by Plot
	void (*image_deleter_)(void *ptr) = nullptr;	// a deleter function for image_ since we don't reference Qt
#endif
	
	EidosPalette(const EidosPalette &p_original) = delete;	// no copy-construct
	EidosPalette& operator=(const EidosPalette&) = delete;	// no copying
	
	EidosPalette(double value, double r, double g, double b);
	EidosPalette(std::vector<double> &&values, std::vector<std::string> &&colors, PaletteTransition transition, PaletteBlend blend);
	virtual ~EidosPalette(void) override;
	
	void AddNode(double value, double r, double g, double b, PaletteTransition transition, PaletteBlend blend);
	
	inline __attribute__((always_inline)) void Range(double *start, double *end) { *start = range_start_; *end = range_end_; }
	
	void CalculateColorForValue(double value, float *r, float *g, float *b);	// doesn't use the cache; slower
	
	inline __attribute__((always_inline)) void ColorForValue(double value, float *r, float *g, float *b)
	{
		if (_ColorCacheIsInvalid())
			_GenerateColorCache();
		
		int color_index;
		
		if (value <= range_start_)
			color_index = 0;
		else if (value >= range_end_)
			color_index = cached_colors_count_ - 1;
		else
			color_index = (int)std::round((cached_colors_count_ - 1) * (value - range_start_) / (range_end_ - range_start_));
        
		*r = cached_colors_[color_index * 3 + 0];
		*g = cached_colors_[color_index * 3 + 1];
		*b = cached_colors_[color_index * 3 + 2];
	}
	inline __attribute__((always_inline)) void ColorForValue(double value, double *r, double *g, double *b)
	{
		if (_ColorCacheIsInvalid())
			_GenerateColorCache();
		
		int color_index;
		
		if (value <= range_start_)
			color_index = 0;
		else if (value >= range_end_)
			color_index = cached_colors_count_ - 1;
		else
            color_index = (int)std::round((cached_colors_count_ - 1) * (value - range_start_) / (range_end_ - range_start_));
        
		*r = (double)cached_colors_[color_index * 3 + 0];
		*g = (double)cached_colors_[color_index * 3 + 1];
		*b = (double)cached_colors_[color_index * 3 + 2];
	}
	
	inline __attribute__((always_inline)) void GetSpectrum(float **spectrum_ptr, int *spectrum_count)
	{
		if (_ColorCacheIsInvalid())
			_GenerateColorCache();
		
		if (spectrum_ptr)
			*spectrum_ptr = cached_colors_;
		if (spectrum_count)
			*spectrum_count = cached_colors_count_;
	}
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_addNode(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_colorForValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class EidosPalette_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	EidosPalette_Class(const EidosPalette_Class &p_original) = delete;	// no copy-construct
	EidosPalette_Class& operator=(const EidosPalette_Class&) = delete;	// no copying
	inline EidosPalette_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual std::vector<EidosPropertySignature_CSP> *Properties_MUTABLE(void) const override;	// use Properties() instead
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
};


#endif /* defined(__Eidos__eidos_class_palette__) */





































































