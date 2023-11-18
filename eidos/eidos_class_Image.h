//
//  eidos_class_Image.h
//  Eidos
//
//  Created by Ben Haller on 10/8/20.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
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
 
 The class EidosImage provides a simple RGB image object based on lodepng
 
 */

#ifndef __Eidos__eidos_class_image__
#define __Eidos__eidos_class_image__

#include "eidos_value.h"


extern EidosClass *gEidosImage_Class;


class EidosImage : public EidosDictionaryRetained
{
private:
	typedef EidosDictionaryRetained super;

private:
	enum Channel {
		kRedChannel = 0,
		kGreenChannel,
		kBlueChannel,
		kGrayChannel
	};
	
	// image data from LodePNG; 8-bit grayscale if possible, otherwise 24-bit RGB
	std::string file_path_;
	std::vector<unsigned char> pixels_;
	int64_t width_ = 0, height_ = 0;
	bool is_grayscale_ = false;
	
	// cached channel data as EidosValues; these are constructed lazily
	EidosValue_SP int_red_values_, int_green_values_, int_blue_values_, int_gray_values_;
	EidosValue_SP float_red_values_, float_green_values_, float_blue_values_, float_gray_values_;
	
	void GetChannelMetrics(Channel p_channel, int64_t &p_pixel_stride, int64_t &p_pixel_suboffset);
	EidosValue_SP ValueForIntegerChannel(EidosValue_SP &p_channel_cache, Channel p_channel);
	EidosValue_SP ValueForFloatChannel(EidosValue_SP &p_channel_cache, Channel p_channel);
	
public:
	EidosImage(const EidosImage &p_original) = delete;	// no copy-construct
	EidosImage& operator=(const EidosImage&) = delete;	// no copying
	
	explicit EidosImage(const std::string &p_file_path);
	EidosImage(int64_t p_width, int64_t p_height, bool p_grayscale);
	virtual ~EidosImage(void) override;
	
	inline int64_t Width(void) { return width_; }
	inline int64_t Height(void) { return height_; }
	inline unsigned char *Data(void) { return pixels_.data(); }
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_write(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class EidosImage_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	EidosImage_Class(const EidosImage_Class &p_original) = delete;	// no copy-construct
	EidosImage_Class& operator=(const EidosImage_Class&) = delete;	// no copying
	inline EidosImage_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
};


#endif /* defined(__Eidos__eidos_class_image__) */





































































