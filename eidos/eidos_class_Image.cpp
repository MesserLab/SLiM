//
//  eidos_class_Image.cpp
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


#include "eidos_class_Image.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_globals.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <algorithm>
#include <string>
#include <vector>

#include "lodepng.h"


//
//	EidosImage
//
#pragma mark -
#pragma mark EidosImage
#pragma mark -

EidosImage::EidosImage(const std::string &p_file_path)
{
	if (!Eidos_string_hasSuffix(p_file_path, ".png") && !Eidos_string_hasSuffix(p_file_path, ".PNG"))
		EIDOS_TERMINATION << "ERROR (EidosImage::EidosImage): only PNG files are supported; a .png or .PNG filename extension must be present" << EidosTerminate(nullptr);
	
	file_path_ = p_file_path;	// remember the path we were given, in case the user wants it back
	
	std::string resolved_path = Eidos_ResolvedPath(p_file_path);
	const char *file_path = resolved_path.c_str();
	std::vector<unsigned char> png_data;
	unsigned width, height;
	unsigned error;
	
	error = lodepng::load_file(png_data, file_path);
	
	if (error)
		EIDOS_TERMINATION << "ERROR (EidosImage::EidosImage): lodepng::load_file error " << error << " : " << lodepng_error_text(error) << EidosTerminate(nullptr);
	
	is_grayscale_ = true;
	error = lodepng::decode(pixels_, width, height, png_data, LodePNGColorType::LCT_GREY, 8);
	
	if (error)
	{
		if (error != 62)	// "conversion from color to grayscale not supported"
		{
			EIDOS_TERMINATION << "ERROR (EidosImage::EidosImage): lodepng::decode error " << error << " : " << lodepng_error_text(error) << EidosTerminate(nullptr);
		}
		
		// Apparently the image is not grayscale; let's try RGB (no alpha!)
		pixels_.clear();
		is_grayscale_ = false;
		error = lodepng::decode(pixels_, width, height, png_data, LodePNGColorType::LCT_RGB, 8);
		
		if (error)
			EIDOS_TERMINATION << "ERROR (EidosImage::EidosImage): lodepng::decode error " << error << " : " << lodepng_error_text(error) << EidosTerminate(nullptr);
	}
	
	// We reach this point if we've had a successful load, either grayscale or RGB
	width_ = width;
	height_ = height;
}

EidosImage::EidosImage(int64_t p_width, int64_t p_height, bool p_grayscale) : width_(p_width), height_(p_height), is_grayscale_(p_grayscale)
{
	if ((width_ <= 0) || (width_ > 100000) || (height_ <= 0) || (height_ > 100000))
		EIDOS_TERMINATION << "ERROR (EidosImage::EidosImage): (internal error) image width and height must be in [1, 100000]." << EidosTerminate();
	
	// immediately allocate our pixel buffer at the appropriate size
	pixels_.resize(width_ * height_ * (is_grayscale_ ? 1 : 3));
}

EidosImage::~EidosImage(void)
{
}

const EidosClass *EidosImage::Class(void) const
{
	return gEidosImage_Class;
}

void EidosImage::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName();	// standard EidosObject behavior (not Dictionary behavior)
}

void EidosImage::GetChannelMetrics(Channel p_channel, int64_t &p_pixel_stride, int64_t &p_pixel_suboffset)
{
	switch (p_channel)
	{
		case Channel::kRedChannel:		p_pixel_stride = 3; p_pixel_suboffset = 0; break;
		case Channel::kGreenChannel:	p_pixel_stride = 3; p_pixel_suboffset = 1; break;
		case Channel::kBlueChannel:		p_pixel_stride = 3; p_pixel_suboffset = 2; break;
		case Channel::kGrayChannel:		p_pixel_stride = 1; p_pixel_suboffset = 0; break;
	}
}

EidosValue_SP EidosImage::ValueForIntegerChannel(EidosValue_SP &p_channel_cache, Channel p_channel)
{
	if (!is_grayscale_ && (p_channel == kGrayChannel))
		EIDOS_TERMINATION << "ERROR (EidosImage::ValueForIntegerChannel): grayscale channel requested from a non-grayscale image" << EidosTerminate(nullptr);
	if (is_grayscale_ && (p_channel != kGrayChannel))
		EIDOS_TERMINATION << "ERROR (EidosImage::ValueForIntegerChannel): RGB channel requested from a grayscale image" << EidosTerminate(nullptr);
	
	if (p_channel_cache)
		return p_channel_cache;
	
	int64_t pixel_stride = 0, pixel_suboffset = 0;
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(height_ * width_);
	p_channel_cache = EidosValue_SP(int_result);
	
	GetChannelMetrics(p_channel, pixel_stride, pixel_suboffset);
	
	// translate the data from by-row to by-column, to match the in-memory format of matrices in Eidos
	for (int64_t y = 0; y < height_; ++y)
		for (int64_t x = 0; x < width_; ++x)
			int_result->set_int_no_check(pixels_[(x + y * width_) * pixel_stride + pixel_suboffset], x * height_ + y);
	
	const int64_t dim_buf[2] = {height_, width_};
	
	int_result->SetDimensions(2, dim_buf);
	
	return p_channel_cache;
}

EidosValue_SP EidosImage::ValueForFloatChannel(EidosValue_SP &p_channel_cache, Channel p_channel)
{
	if (!is_grayscale_ && (p_channel == kGrayChannel))
		EIDOS_TERMINATION << "ERROR (EidosImage::ValueForFloatChannel): grayscale channel requested from a non-grayscale image" << EidosTerminate(nullptr);
	if (is_grayscale_ && (p_channel != kGrayChannel))
		EIDOS_TERMINATION << "ERROR (EidosImage::ValueForFloatChannel): RGB channel requested from a grayscale image" << EidosTerminate(nullptr);
	
	if (p_channel_cache)
		return p_channel_cache;
	
	int64_t pixel_stride = 0, pixel_suboffset = 0;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(height_ * width_);
	p_channel_cache = EidosValue_SP(float_result);
	
	GetChannelMetrics(p_channel, pixel_stride, pixel_suboffset);
	
	// translate the data from by-row to by-column, to match the in-memory format of matrices in Eidos
	for (int64_t y = 0; y < height_; ++y)
		for (int64_t x = 0; x < width_; ++x)
			float_result->set_float_no_check(pixels_[(x + y * width_) * pixel_stride + pixel_suboffset] / 255.0, x * height_ + y);
	
	const int64_t dim_buf[2] = {height_, width_};
	
	float_result->SetDimensions(2, dim_buf);
	
	return p_channel_cache;
}

EidosValue_SP EidosImage::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gEidosID_width:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(width_));
		case gEidosID_height:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(height_));
		case gEidosID_bitsPerChannel:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8));	// only 8 is supported for now, but this is for future expansion
		case gEidosID_isGrayscale:
			return (is_grayscale_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gEidosID_integerR:
			return ValueForIntegerChannel(int_red_values_, EidosImage::kRedChannel);
		case gEidosID_integerG:
			return ValueForIntegerChannel(int_green_values_, EidosImage::kGreenChannel);
		case gEidosID_integerB:
			return ValueForIntegerChannel(int_blue_values_, EidosImage::kBlueChannel);
		case gEidosID_integerK:
			return ValueForIntegerChannel(int_gray_values_, EidosImage::kGrayChannel);
		case gEidosID_floatR:
			return ValueForFloatChannel(float_red_values_, EidosImage::kRedChannel);
		case gEidosID_floatG:
			return ValueForFloatChannel(float_green_values_, EidosImage::kGreenChannel);
		case gEidosID_floatB:
			return ValueForFloatChannel(float_blue_values_, EidosImage::kBlueChannel);
		case gEidosID_floatK:
			return ValueForFloatChannel(float_gray_values_, EidosImage::kGrayChannel);
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue_SP EidosImage::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gEidosID_write:					return ExecuteMethod_write(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)write(string$ filePath)
//
EidosValue_SP EidosImage::ExecuteMethod_write(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	
	std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex(0, nullptr));
	
	unsigned error;
	
	if (is_grayscale_)
		error = lodepng::encode(outfile_path, pixels_, (unsigned)width_, (unsigned)height_, LodePNGColorType::LCT_GREY, 8);	// K channel, 8 bits per channel
	else
		error = lodepng::encode(outfile_path, pixels_, (unsigned)width_, (unsigned)height_, LodePNGColorType::LCT_RGB, 8);	// RGB channels, 8 bits per channel
	
	if (error)
		EIDOS_TERMINATION << "ERROR (EidosImage::ExecuteMethod_write): write() could not write to " << outfile_path << " (encoder error " << error << ": " << lodepng_error_text(error) << ")." << EidosTerminate();
	
	return gStaticEidosValueVOID;
}


//
//	Object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	(object<Image>$)Image(...)
static EidosValue_SP Eidos_Instantiate_EidosImage(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosImage *objectElement = nullptr;
	
	if ((p_arguments.size() == 1) && (p_arguments[0]->Type() == EidosValueType::kValueString) && (p_arguments[0]->Count() == 1))
	{
		EidosValue_String *filePath_value = (EidosValue_String *)p_arguments[0].get();
		objectElement = new EidosImage(filePath_value->StringRefAtIndex(0, nullptr));
	}
	else if ((p_arguments.size() == 1) &&
			 ((p_arguments[0]->Type() == EidosValueType::kValueInt) || (p_arguments[0]->Type() == EidosValueType::kValueFloat)) &&
			 (p_arguments[0]->Count() >= 1))
	{
		// test: x = matrix(c(255, 255, 255, 0, 0, rep(0, 10)), nrow=3, ncol=5, byrow=T); y = Image(x); y.write("~/Desktop/test.png");
		// test: x = matrix(c(1.0, 1, 1, 0, 0, rep(0, 10)), nrow=3, ncol=5, byrow=T); y = Image(x); y.write("~/Desktop/test.png");
		EidosValue *numeric_value = p_arguments[0].get();
		
		if (numeric_value->DimensionCount() != 2)
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): Image(), when passed a numeric vector, requires that vector to be a matrix." << EidosTerminate();
		
		int64_t height = numeric_value->Dimensions()[0];	// height in pixels == number of rows
		int64_t width = numeric_value->Dimensions()[1];		// width in pixels == number of columns
		
		if (numeric_value->Type() == EidosValueType::kValueInt)
		{
			if (numeric_value->Count() == 1)
			{
				// singleton case
				objectElement = new EidosImage(1, 1, true);
				
				unsigned char *image_data = objectElement->Data();
				int64_t int_value = numeric_value->IntAtIndex(0, nullptr);
				
				if ((int_value < 0) || (int_value > 255))
					EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): Image(), when passed an integer vector, requires values to be in [0, 255]." << EidosTerminate();
				
				*image_data = (unsigned char)int_value;
			}
			else
			{
				// vector case, fast access
				objectElement = new EidosImage(width, height, true);
				
				unsigned char *image_data = objectElement->Data();
				EidosValue_Int_vector *int_values = (EidosValue_Int_vector *)p_arguments[0].get();
				const int64_t *int_data = int_values->data();
				
				// translate the data from by-column to by-row, to match the in-memory format of images
				for (int64_t y = 0; y < height; ++y)
				{
					for (int64_t x = 0; x < width; ++x)
					{
						int64_t int_value = *(int_data + y + x * height);
						
						if ((int_value < 0) || (int_value > 255))
							EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): Image(), when passed an integer vector, requires values to be in [0, 255]." << EidosTerminate();
						
						*(image_data + x + y * width) = (unsigned char)int_value;
					}
				}
			}
		}
		else if (numeric_value->Type() == EidosValueType::kValueFloat)
		{
			if (numeric_value->Count() == 1)
			{
				// singleton case
				objectElement = new EidosImage(1, 1, true);
				
				unsigned char *image_data = objectElement->Data();
				double float_value = numeric_value->FloatAtIndex(0, nullptr);
				
				if ((float_value < 0.0) || (float_value > 1.0))
					EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): Image(), when passed a float vector, requires values to be in [0.0, 1.0]." << EidosTerminate();
				
				int int_value = (int)round(float_value * 255.0);
				*image_data = (unsigned char)int_value;
			}
			else
			{
				// vector case, fast access
				objectElement = new EidosImage(width, height, true);
				
				unsigned char *image_data = objectElement->Data();
				EidosValue_Float_vector *float_values = (EidosValue_Float_vector *)p_arguments[0].get();
				const double *float_data = float_values->data();
				
				// translate the data from by-column to by-row, to match the in-memory format of images
				for (int64_t y = 0; y < height; ++y)
				{
					for (int64_t x = 0; x < width; ++x)
					{
						double float_value = *(float_data + y + x * height);
						
						if ((float_value < 0.0) || (float_value > 1.0))
							EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): Image(), when passed a float vector, requires values to be in [0.0, 1.0]." << EidosTerminate();
						
						int int_value = (int)round(float_value * 255.0);
						*(image_data + x + y * width) = (unsigned char)int_value;
					}
				}
			}
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): (internal error) unexpected type for numeric_value." << EidosTerminate();
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_Instantiate_EidosImage): the Image() constructor requires either a singleton string (a file path) or a numeric vector (a matrix of pixel values)." << EidosTerminate();
	}
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosImage_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}


//
//	EidosImage_Class
//
#pragma mark -
#pragma mark EidosImage_Class
#pragma mark -

EidosClass *gEidosImage_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *EidosImage_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosImage_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_width,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_height,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_isGrayscale,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_bitsPerChannel,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_integerR,			true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_integerG,			true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_integerB,			true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_integerK,			true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_floatR,			true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_floatG,			true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_floatB,			true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_floatK,			true,	kEidosValueMaskFloat)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosImage_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosImage_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_write, kEidosValueMaskVOID))->AddString_S(gEidosStr_filePath));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *EidosImage_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosImage_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Image, Eidos_Instantiate_EidosImage, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosImage_Class))->AddEllipsis());
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}


































































