//
//  QtSLiM_Plot.cpp
//  SLiM
//
//  Created by Ben Haller on 1/30/2024.
//  Copyright (c) 2024-2025 Benjamin C. Haller.  All rights reserved.
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


#include "QtSLiM_Plot.h"
#include "QtSLiMGraphView_CustomPlot.h"

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_class_Image.h"

#include "spatial_map.h"

#include <unistd.h>

#include <string>
#include <algorithm>
#include <vector>
#include <limits>


#pragma mark -
#pragma mark Plot
#pragma mark -

Plot::Plot(const std::string &title, QtSLiMGraphView_CustomPlot *p_plotview) :
    title_(title), plotview_(p_plotview)
{
}

Plot::~Plot(void)
{
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Plot::Class(void) const
{
	return gSLiM_Plot_Class;
}

void Plot::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP Plot::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		// constants
        case gID_title:
        {
            return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(title_));
        }
        
        // variables
		
		// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void Plot::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Plot::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
        case gID_abline:                return ExecuteMethod_abline(p_method_id, p_arguments, p_interpreter);
        case gID_addLegend:             return ExecuteMethod_addLegend(p_method_id, p_arguments, p_interpreter);
        case gID_axis:                  return ExecuteMethod_axis(p_method_id, p_arguments, p_interpreter);
        case gID_image:                 return ExecuteMethod_image(p_method_id, p_arguments, p_interpreter);
        case gID_legendLineEntry:       return ExecuteMethod_legendLineEntry(p_method_id, p_arguments, p_interpreter);
        case gID_legendPointEntry:      return ExecuteMethod_legendPointEntry(p_method_id, p_arguments, p_interpreter);
        case gID_legendSwatchEntry:     return ExecuteMethod_legendSwatchEntry(p_method_id, p_arguments, p_interpreter);
        case gID_legendTitleEntry:      return ExecuteMethod_legendTitleEntry(p_method_id, p_arguments, p_interpreter);
        case gID_lines:					return ExecuteMethod_lines(p_method_id, p_arguments, p_interpreter);
        case gID_matrix:                return ExecuteMethod_matrix(p_method_id, p_arguments, p_interpreter);
        case gID_points:				return ExecuteMethod_points(p_method_id, p_arguments, p_interpreter);
        case gID_rects:					return ExecuteMethod_rects(p_method_id, p_arguments, p_interpreter);
        case gID_segments:				return ExecuteMethod_segments(p_method_id, p_arguments, p_interpreter);
        case gID_setBorderless:			return ExecuteMethod_setBorderless(p_method_id, p_arguments, p_interpreter);
        case gID_text:					return ExecuteMethod_text(p_method_id, p_arguments, p_interpreter);
        case gEidosID_write:			return ExecuteMethod_write(p_method_id, p_arguments, p_interpreter);
        default:                        return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)abline([Nif a = NULL], [Nif b = NULL], [Nif h = NULL], [Nif v = NULL], [string color = "red"], [numeric lwd = 1.0], [float alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_abline(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *a_value = p_arguments[0].get();
    EidosValue *b_value = p_arguments[1].get();
    EidosValue *h_value = p_arguments[2].get();
    EidosValue *v_value = p_arguments[3].get();
    EidosValue *color_value = p_arguments[4].get();
    EidosValue *lwd_value = p_arguments[5].get();
    EidosValue *alpha_value = p_arguments[6].get();
    double *a = nullptr, *b = nullptr, *h = nullptr, *v = nullptr;
    int line_count;
    
    if ((a_value->Type() != EidosValueType::kValueNULL) && (b_value->Type() != EidosValueType::kValueNULL) && (h_value->Type() == EidosValueType::kValueNULL) && (v_value->Type() == EidosValueType::kValueNULL))
    {
        // a and b
        int acount = a_value->Count();
        int bcount = b_value->Count();
        
        if (acount == bcount)   line_count = acount;
        else if (acount == 1)   line_count = bcount;
        else if (bcount == 1)   line_count = acount;
        else
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires a and b to be the same length, or one of them to be singleton." << EidosTerminate();
        
        if (line_count == 0)
            return gStaticEidosValueVOID;
        
        a = (double *)malloc(line_count * sizeof(double));
        b = (double *)malloc(line_count * sizeof(double));
        
        if (!a || !b)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
        
        for (int index = 0; index < line_count; ++index)
        {
            a[index] = a_value->NumericAtIndex_NOCAST(index % acount, nullptr);
            b[index] = b_value->NumericAtIndex_NOCAST(index % bcount, nullptr);
        }
    }
    else if ((a_value->Type() == EidosValueType::kValueNULL) && (b_value->Type() == EidosValueType::kValueNULL) && (h_value->Type() != EidosValueType::kValueNULL) && (v_value->Type() == EidosValueType::kValueNULL))
    {
        // h
        line_count = h_value->Count();
        
        if (line_count == 0)
            return gStaticEidosValueVOID;
        
        h = (double *)malloc(line_count * sizeof(double));
        
        if (!h)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
        
        for (int index = 0; index < line_count; ++index)
            h[index] = h_value->NumericAtIndex_NOCAST(index, nullptr);
    }
    else if ((a_value->Type() == EidosValueType::kValueNULL) && (b_value->Type() == EidosValueType::kValueNULL) && (h_value->Type() == EidosValueType::kValueNULL) && (v_value->Type() != EidosValueType::kValueNULL))
    {
        // v
        line_count = v_value->Count();
        
        if (line_count == 0)
            return gStaticEidosValueVOID;
        
        v = (double *)malloc(line_count * sizeof(double));
        
        if (!v)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
        
        for (int index = 0; index < line_count; ++index)
            v[index] = v_value->NumericAtIndex_NOCAST(index, nullptr);
    }
    else
    {
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires one of three usage modes: (1) a and b are non-NULL while h and v are NULL; (2) a, b, and v are NULL while h is non-NULL; or (3) a, b, and h are NULL while v is non-NULL." << EidosTerminate(nullptr);
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != line_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires color to match the number of lines, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // lwd
    std::vector<double> *lwds = new std::vector<double>;
    int lwd_count = lwd_value->Count();
    
    if ((lwd_count != 1) && (lwd_count != line_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires lwd to match the number of lines, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    // alpha
    std::vector<double> *alphas = new std::vector<double>;
    int alpha_count = alpha_value->Count();
    
    if ((alpha_count != 1) && (alpha_count != line_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires alpha to match the number of lines, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < alpha_count; ++index)
    {
        double alpha = alpha_value->FloatAtIndex_NOCAST(index, nullptr);
        
        if ((alpha < 0.0) || (alpha > 1.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_abline): abline() requires the elements of alpha to be in [0, 1]." << EidosTerminate(nullptr);
        
        alphas->push_back(alpha);
    }
    
    plotview_->addABLineData(a, b, h, v, line_count, colors, alphas, lwds);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)addLegend([Ns$ position = NULL], [Ni$ inset = NULL], [Nif$ labelSize = NULL], [Nif$ lineHeight = NULL], [Nif$ graphicsWidth = NULL], [Nif$ exteriorMargin = NULL], [Nif$ interiorMargin = NULL])
//
EidosValue_SP Plot::ExecuteMethod_addLegend(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *position_value = p_arguments[0].get();
    EidosValue *inset_value = p_arguments[1].get();
    EidosValue *labelSize_value = p_arguments[2].get();
    EidosValue *lineHeight_value = p_arguments[3].get();
    EidosValue *graphicsWidth_value = p_arguments[4].get();
    EidosValue *exteriorMargin_value = p_arguments[5].get();
    EidosValue *interiorMargin_value = p_arguments[6].get();
    
    // position
    QtSLiM_LegendPosition position;
    
    if (position_value->Type() == EidosValueType::kValueNULL)
    {
        position = QtSLiM_LegendPosition::kUnconfigured;
    }
    else
    {
        QString positionString  = QString::fromStdString(position_value->StringAtIndex_NOCAST(0, nullptr));
        
        if (positionString == "topLeft")
            position = QtSLiM_LegendPosition::kTopLeft;
        else if (positionString == "topRight")
            position = QtSLiM_LegendPosition::kTopRight;
        else if (positionString == "bottomLeft")
            position = QtSLiM_LegendPosition::kBottomLeft;
        else if (positionString == "bottomRight")
            position = QtSLiM_LegendPosition::kBottomRight;
        else
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires position to be 'topLeft', 'topRight', 'bottomLeft', or 'bottomRight' (or NULL)." << EidosTerminate(nullptr);
    }
    
    // inset
    int inset;
    
    if (inset_value->Type() == EidosValueType::kValueNULL)
        inset = -1;
    else
    {
        inset = inset_value->IntAtIndex_NOCAST(0, nullptr);
        
        if ((inset < 0) || (inset > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires inset to be in [0, 50]." << EidosTerminate(nullptr);
    }
    
    // labelSize
    double labelSize;
    
    if (labelSize_value->Type() == EidosValueType::kValueNULL)
        labelSize = -1;
    else
    {
        labelSize = labelSize_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(labelSize) || (labelSize < 5) || (labelSize > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires labelSize to be in [5, 50]." << EidosTerminate(nullptr);
    }
    
    // lineHeight
    double lineHeight;
    
    if (lineHeight_value->Type() == EidosValueType::kValueNULL)
        lineHeight = -1;
    else
    {
        lineHeight = lineHeight_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(lineHeight) || (lineHeight < 5) || (lineHeight > 100))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires lineHeight to be in [5, 100]." << EidosTerminate(nullptr);
    }
    
    // graphicsWidth
    double graphicsWidth;
    
    if (graphicsWidth_value->Type() == EidosValueType::kValueNULL)
        graphicsWidth = -1;
    else
    {
        graphicsWidth = graphicsWidth_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(graphicsWidth) || (graphicsWidth < 5) || (graphicsWidth > 100))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires graphicsWidth to be in [5, 100]." << EidosTerminate(nullptr);
    }
    
    // exteriorMargin
    double exteriorMargin;
    
    if (exteriorMargin_value->Type() == EidosValueType::kValueNULL)
        exteriorMargin = -1;
    else
    {
        exteriorMargin = exteriorMargin_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(exteriorMargin) || (exteriorMargin < 0) || (exteriorMargin > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires exteriorMargin to be in [0, 50]." << EidosTerminate(nullptr);
    }
    
    // interiorMargin
    double interiorMargin;
    
    if (interiorMargin_value->Type() == EidosValueType::kValueNULL)
        interiorMargin = -1;
    else
    {
        interiorMargin = interiorMargin_value->NumericAtIndex_NOCAST(0, nullptr);
        
        if (!std::isfinite(interiorMargin) || (interiorMargin < 0) || (interiorMargin > 50))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() requires interiorMargin to be in [0, 50]." << EidosTerminate(nullptr);
    }
    
    if (plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_addLegend): addLegend() has already been called for this plot, and should only be called once." << EidosTerminate(nullptr);
    
    plotview_->addLegend(position, inset, labelSize, lineHeight, graphicsWidth, exteriorMargin, interiorMargin);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)axis(integer$ side, [Nif at = NULL], [ls labels = T])
//
EidosValue_SP Plot::ExecuteMethod_axis(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *side_value = p_arguments[0].get();
    EidosValue *at_value = p_arguments[1].get();
    EidosValue *labels_value = p_arguments[2].get();
    
    // side
    int side = (int)side_value->IntAtIndex_NOCAST(0, nullptr);
    
    if ((side != 1) && (side != 2))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires side to be 1 (for the x axis) or 2 (for the y axis)." << EidosTerminate(nullptr);
    
    // at
    std::vector<double> *at = nullptr;
    int at_length = 0;
    double last_at = -std::numeric_limits<double>::infinity();
    
    if (at_value->Type() != EidosValueType::kValueNULL)
    {
        at_length = at_value->Count();
        at = new std::vector<double>;
        
        for (int index = 0; index < at_length; ++index)
        {
            double pos = at_value->NumericAtIndex_NOCAST(index, nullptr);
            
            if (!std::isfinite(pos))
                EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires the elements of at to be finite." << EidosTerminate(nullptr);
            
            if (pos <= last_at)
                EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires the elements of at to be in sorted (increasing) order." << EidosTerminate(nullptr);
            last_at = pos;
            
            at->push_back(pos);
        }
    }
    
    // labels
    std::vector<QString> *labels;
    int labels_type;
    
    if (labels_value->Type() == EidosValueType::kValueLogical)
    {
        // labels can be T, F, or a vector of type string; we need a separate flag to differentiate those cases
        // T is 1, F is 0, and the string vector case is 2
        labels = nullptr;
        labels_type = (int)labels_value->LogicalAtIndex_NOCAST(0, nullptr);
    }
    else
    {
        if (at_value->Type() == EidosValueType::kValueNULL)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires that when at is NULL, labels be T or F; a vector of labels cannot be supplied without corresponding positions." << EidosTerminate(nullptr);
        
        const std::string *string_data = labels_value->StringData();
        int labels_length = labels_value->Count();
        
        if (labels_length != at_length)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_axis): axis() requires that labels be the same length as at (if labels is not T or F), to supply a label for each corresponding position." << EidosTerminate(nullptr);
        
        labels = new std::vector<QString>;
        labels_type = 2;
        
        for (int index = 0; index < labels_length; ++index)
            labels->emplace_back(QString::fromStdString(string_data[index]));
    }
    
    plotview_->setAxisConfiguration(side, at, labels_type, labels);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)image(object$ image, numeric$ x1, numeric$ y1, numeric$ x2, numeric$ y2, [logical$ flipped = F], [float$ alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_image(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *image_value = p_arguments[0].get();
    EidosValue *x1_value = p_arguments[1].get();
    EidosValue *y1_value = p_arguments[2].get();
    EidosValue *x2_value = p_arguments[3].get();
    EidosValue *y2_value = p_arguments[4].get();
    EidosValue *flipped_value = p_arguments[5].get();
    EidosValue *alpha_value = p_arguments[6].get();
    
    // flipped -- handled out of order because we need this to process image
    bool flipped = flipped_value->LogicalAtIndex_NOCAST(0, nullptr);
    
    // image
    EidosObject *image_object = image_value->ObjectElementAtIndex_NOCAST(0, nullptr);
    QImage image;
    
    if (image_object->Class() == gSLiM_SpatialMap_Class)
    {
        // if image is a SpatialImage, plot the map's values; it must be a singleton and have 2D spatiality
        if (image_value->Count() != 1)
            EIDOS_TERMINATION << "ERROR (Plot::image): a SpatialMap value passed to image() must be a singleton." << EidosTerminate(nullptr);
        
        SpatialMap *spatial_map = (SpatialMap *)image_object;
        
        if (spatial_map->image_ && (spatial_map->image_flipped_ == flipped))
        {
            // we have a cached QImage for this SpatialMap, and it matches our flipped flag
            image = *(QImage *)spatial_map->image_;  // make a copy with implicit sharing
        }
        else
        {
            // cache the spatial map's image if it doesn't already have a matching cache; first delete any existing (unmatching) cache
            if (spatial_map->image_)
            {
                if (spatial_map->image_deleter_)
                    spatial_map->image_deleter_(spatial_map->image_);
                else
                    std::cout << "Missing SpatialMap image_deleter_; leaking memory" << std::endl;
                
                spatial_map->image_ = nullptr;
                spatial_map->image_deleter_ = nullptr;
            }
            
            if (spatial_map->spatiality_ != 2)
                EIDOS_TERMINATION << "ERROR (Plot::image): image() only supports plotting of spatial maps of spatiality 2 ('xy', 'xz', or 'yz' maps)." << EidosTerminate(nullptr);
            
            int64_t image_width = spatial_map->grid_size_[0];
            int64_t image_height = spatial_map->grid_size_[1];
            
            if ((image_width <= 1) || (image_height <= 1))
                EIDOS_TERMINATION << "ERROR (Plot::image): image() requires the plotted spatial map to be 2x2 or larger in its grid dimensions." << EidosTerminate(nullptr);
            
            // the edge rows/columns get one pixel; interior rows/edges get two pixels
            // this gives us an image that correctly matches the spatial map in SLiM
            image_width = image_width * 2 - 2;
            image_height = image_height * 2 - 2;
            
            // make the image buffer to be used by QtSLiMGraphView_CustomPlot; note that it takes ownership of image_data and frees it for us
            const int bytes_per_pixel = 3;  // RGB888 format
            uint8_t *image_data = (uint8_t *)malloc(image_width * image_height * bytes_per_pixel * sizeof(uint8_t));
            
            // we never interpolate map values, since the correct resolution is ambiguous; we're generating vector graphics here
            // FIXME: maybe the spatial map display in the individuals view should turn off interpolation too; let the user interpolate the map if they want interpolated display
            spatial_map->FillRGBBuffer(image_data, image_width, image_height, flipped, /* no_interpolation */ false);
            
            QImage *cached_image = new QImage(image_data, image_width, image_height, image_width * bytes_per_pixel, QImage::Format_RGB888, free, image_data);
            
            // We give the cached image to the SpatialMap object.  Since it doesn't build against Qt, we give it a deletor function.
            spatial_map->image_ = cached_image;
            spatial_map->image_flipped_ = flipped;
            spatial_map->image_deleter_ = Eidos_Deleter<QImage>;
            
            image = *cached_image;  // make a copy with implicit sharing
        }
    }
    else if (image_object->Class() == gEidosImage_Class)
    {
        // if image is an Image, plot the image's values; it must be a singleton
        if (image_value->Count() != 1)
            EIDOS_TERMINATION << "ERROR (Plot::image): an Image value passed to image() must be a singleton." << EidosTerminate(nullptr);
        
        EidosImage *eidos_image = (EidosImage *)image_object;
        
        if (eidos_image->image_ && (eidos_image->image_flipped_ == flipped))
        {
            // we have a cached QImage for this Image, and it matches our flipped flag
            image = *(QImage *)eidos_image->image_;  // make a copy with implicit sharing
        }
        else
        {
            // cache the Image's image if it doesn't already have a matching cache; first delete any existing (unmatching) cache
            if (eidos_image->image_)
            {
                if (eidos_image->image_deleter_)
                    eidos_image->image_deleter_(eidos_image->image_);
                else
                    std::cout << "Missing Image image_deleter_; leaking memory" << std::endl;
                
                eidos_image->image_ = nullptr;
                eidos_image->image_deleter_ = nullptr;
            }
            
            int64_t image_width = eidos_image->width_;
            int64_t image_height = eidos_image->height_;
            
            // make the image buffer to be used by QtSLiMGraphView_CustomPlot; note that it takes ownership of image_data and frees it for us
            const int bytes_per_pixel = (eidos_image->is_grayscale_ ? 1 : 3);  // Grayscale8 or RGB888 format
            uint8_t *image_data = (uint8_t *)malloc(image_width * image_height * bytes_per_pixel * sizeof(uint8_t));
            
            // optionally flip the rows of the image buffer so it displays in the orientation we want; this could be an option
            // note that here flipping is done by default, so flipped=true turns off this flip!
            if (!flipped)
            {
                for (int y = 0; y < image_height; ++y)
                    memcpy(image_data + y * (image_width * bytes_per_pixel * sizeof(uint8_t)),
                           eidos_image->pixels_.data() + (image_height - y - 1) * (image_width * bytes_per_pixel * sizeof(uint8_t)),
                           image_width * bytes_per_pixel * sizeof(uint8_t));
            }
            else
            {
                memcpy(image_data, eidos_image->pixels_.data(), image_width * image_height * bytes_per_pixel * sizeof(uint8_t));
            }
            
            QImage *cached_image = new QImage(image_data, image_width, image_height, image_width * bytes_per_pixel,
                                              (eidos_image->is_grayscale_ ? QImage::Format_Grayscale8 : QImage::Format_RGB888), free, image_data);
            
            // We give the cached image to the EidosImage object.  Since it doesn't build against Qt, we give it a deletor function.
            eidos_image->image_ = cached_image;
            eidos_image->image_flipped_ = flipped;
            eidos_image->image_deleter_ = Eidos_Deleter<QImage>;
            
            image = *cached_image;  // make a copy with implicit sharing
        }
    }
    else
    {
        EIDOS_TERMINATION << "ERROR (Plot::image): unsupported object class in image(); image must be of class SpatialMap or class Image." << EidosTerminate(nullptr);
    }
    
    // x and y
    double *x = (double *)malloc(2 * sizeof(double));
    double *y = (double *)malloc(2 * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::image): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    x[0] = x1_value->NumericAtIndex_NOCAST(0, nullptr);
    y[0] = y1_value->NumericAtIndex_NOCAST(0, nullptr);
    x[1] = x2_value->NumericAtIndex_NOCAST(0, nullptr);
    y[1] = y2_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if (x[0] > x[1])
        EIDOS_TERMINATION << "ERROR (Plot::image): image() requires x1 <= x2." << EidosTerminate(nullptr);
    if (y[0] > y[1])
        EIDOS_TERMINATION << "ERROR (Plot::image): image() requires y1 <= y2." << EidosTerminate(nullptr);
    
    // alpha
    double alpha = alpha_value->FloatAtIndex_NOCAST(0, nullptr);
    
    if ((alpha < 0.0) || (alpha > 1.0))
        EIDOS_TERMINATION << "ERROR (Plot::image): image() requires the image alpha to be in [0, 1]." << EidosTerminate(nullptr);
    
    std::vector<double> *imageAlphas = new std::vector<double>;  // we only take a singleton alpha, but the API expects a buffer
    
    imageAlphas->push_back(alpha);
    
    plotview_->addImageData(x, y, 2, image, imageAlphas);       // implicitly shares image; takes the x, y, and imageAlphas buffers from us
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendLineEntry(string$ label, [string$ color = "red"], [numeric$ lwd = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_legendLineEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    EidosValue *color_value = p_arguments[1].get();
    EidosValue *lwd_value = p_arguments[2].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (label.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): legendLineEntry() requires a non-empty legend label." << EidosTerminate();
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    QColor color(colorR, colorG, colorB, 255);
    
    // lwd
    double lwd = lwd_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((lwd < 0.0) || (lwd > 100.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): legendLineEntry() requires the line width lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendLineEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendLineEntry(label, color, lwd);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendPointEntry(string$ label, [integer$ symbol = 0], [string$ color = "red"], [string$ border = "black"], [numeric$ lwd = 1.0], [numeric$ size = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_legendPointEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    EidosValue *symbol_value = p_arguments[1].get();
    EidosValue *color_value = p_arguments[2].get();
    EidosValue *border_value = p_arguments[3].get();
    EidosValue *lwd_value = p_arguments[4].get();
    EidosValue *size_value = p_arguments[5].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (label.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires a non-empty legend label." << EidosTerminate();
    
    // symbol
    int symbol = symbol_value->IntAtIndex_NOCAST(0, nullptr);
    
    if (symbol < 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires the elements of symbol to be >= 0." << EidosTerminate();
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    QColor color(colorR, colorG, colorB, 255);
    
    // border
    std::string border_string = border_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t borderR, borderG, borderB;
    
    Eidos_GetColorComponents(border_string, &borderR, &borderG, &borderB);
    
    QColor border(borderR, borderG, borderB, 255);
    
    // lwd
    double lwd = lwd_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((lwd < 0.0) || (lwd > 100.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    // size
    double size = size_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((size <= 0.0) || (size > 1000.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): legendPointEntry() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendPointEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendPointEntry(label, symbol, color, border, lwd, size);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendSwatchEntry(string$ label, [string$ color = "red"])
//
EidosValue_SP Plot::ExecuteMethod_legendSwatchEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    EidosValue *color_value = p_arguments[1].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (label.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendSwatchEntry): legendSwatchEntry() requires a non-empty legend label." << EidosTerminate();
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    QColor color(colorR, colorG, colorB, 255);
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendSwatchEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendSwatchEntry(label, color);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)legendTitleEntry(string$ label)
//
EidosValue_SP Plot::ExecuteMethod_legendTitleEntry(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *label_value = p_arguments[0].get();
    
    // label
    QString label = QString::fromStdString(label_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (!plotview_->legendAdded())
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_legendTitleEntry): addLegend() must be called before adding legend entries." << EidosTerminate(nullptr);
    
    plotview_->addLegendTitleEntry(label);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)lines(numeric x, numeric y, [string$ color = "red"], [numeric$ lwd = 1.0], [float$ alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_lines(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *x_value = p_arguments[0].get();
    EidosValue *y_value = p_arguments[1].get();
    EidosValue *color_value = p_arguments[2].get();
    EidosValue *lwd_value = p_arguments[3].get();
    EidosValue *alpha_value = p_arguments[4].get();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    
    if (xcount != ycount)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): lines() requires x and y to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x, x_value->FloatData(), xcount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x_value->IntData();
        
        for (int index = 0; index < xcount; ++index)
            x[index] = int_data[index];
    }
    
    if (y_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y, y_value->FloatData(), ycount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y_value->IntData();
        
        for (int index = 0; index < ycount; ++index)
            y[index] = int_data[index];
    }
    
    // color
    std::string color_string = color_value->StringAtIndex_NOCAST(0, nullptr);
    uint8_t colorR, colorG, colorB;
    
    Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
    
    std::vector<QColor> *colors = new std::vector<QColor>;
    
    colors->emplace_back(colorR, colorG, colorB, 255);
    
    // lwd
    double lwd = lwd_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((lwd < 0.0) || (lwd > 100.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): lines() requires the line width lwd to be in [0, 100]." << EidosTerminate(nullptr);
    
    std::vector<double> *lineWidths = new std::vector<double>;  // we only take a singleton width, but the API expects a buffer
    
    lineWidths->push_back(lwd);
    
    // alpha
    double alpha = alpha_value->FloatAtIndex_NOCAST(0, nullptr);
    
    if ((alpha < 0.0) || (alpha > 1.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_lines): lines() requires the line width alpha to be in [0, 1]." << EidosTerminate(nullptr);
    
    std::vector<double> *lineAlphas = new std::vector<double>;  // we only take a singleton alpha, but the API expects a buffer
    
    lineAlphas->push_back(alpha);
    
    plotview_->addLineData(x, y, xcount, colors, lineAlphas, lineWidths);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)matrix(object$ image, numeric$ x1, numeric$ y1, numeric$ x2, numeric$ y2, [logical$ flipped = F],
//                                          [Nif valueRange = NULL], [Ns$ colors = NULL], [float$ alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_matrix(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *matrix_value = p_arguments[0].get();
    EidosValue *x1_value = p_arguments[1].get();
    EidosValue *y1_value = p_arguments[2].get();
    EidosValue *x2_value = p_arguments[3].get();
    EidosValue *y2_value = p_arguments[4].get();
    EidosValue *flipped_value = p_arguments[5].get();
    EidosValue *valueRange_value = p_arguments[6].get();
    EidosValue *colors_value = p_arguments[7].get();
    EidosValue *alpha_value = p_arguments[8].get();
    
    // flipped
    bool flipped = flipped_value->LogicalAtIndex_NOCAST(0, nullptr);
    
    // valueRange
    double range_min = 0.0, range_max = 0.0;
    
    if (valueRange_value->Type() == EidosValueType::kValueNULL)
    {
        range_min = 0.0;
        range_max = 1.0;
    }
    else
    {
        if (valueRange_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (Plot::matrix): matrix() requires valueRange to be a vector of length 2 providing a data range, or NULL to use the default data range of [0, 1]." << EidosTerminate(nullptr);
        
        range_min = valueRange_value->NumericAtIndex_NOCAST(0, nullptr);
        range_max = valueRange_value->NumericAtIndex_NOCAST(1, nullptr);
        
        if (!std::isfinite(range_min) || !std::isfinite(range_max) || (range_min == range_max))
            EIDOS_TERMINATION << "ERROR (Plot::matrix): matrix() requires valueRange to contain finite, unequal values that define a data range." << EidosTerminate(nullptr);
    }
    
    // colors
    std::string colors_name;
    
    if (colors_value->Type() == EidosValueType::kValueNULL)
    {
        colors_name = "gray";
        std::swap(range_min, range_max);
    }
    else
    {
        colors_name = colors_value->StringAtIndex_NOCAST(0, nullptr);
    }
    
    EidosColorPalette palette = Eidos_PaletteForName(colors_name);
    
    if (palette == EidosColorPalette::kPalette_INVALID)
        EIDOS_TERMINATION << "ERROR (Plot::matrix): unrecognized color palette name in matrix()." << EidosTerminate(nullptr);
    
    // figure out the rescaling factors in effect; note that colors=NULL might have reversed the rescaling range
    bool rescaling = false;
    double rescale_offset = 0.0;
    double rescale_scaling = 1.0;
    
    if ((range_min != 0.0) || (range_max != 1.0))
    {
        rescaling = true;
        rescale_offset = -range_min;
        rescale_scaling = 1.0 / (range_max - range_min);
    }
    
    // matrix
    if ((matrix_value->DimensionCount() != 2))
        EIDOS_TERMINATION << "ERROR (Plot::matrix): matrix() requires that parameter matrix is actually a matrix (not a vector or array)." << EidosTerminate(nullptr);
    
    const int64_t *dims = matrix_value->Dimensions();
    int64_t matrix_rows = dims[0];
    int64_t matrix_columns = dims[1];
    
    if ((matrix_value->DimensionCount() != 2) || (matrix_rows < 1) || (matrix_columns < 1))
        EIDOS_TERMINATION << "ERROR (Plot::matrix): matrix() requires a matrix that is at least 1x1 in its dimensions." << EidosTerminate(nullptr);
    
    // make the image buffer to be used by QtSLiMGraphView_CustomPlot; note that it takes ownership of image_data and frees it for us
    int64_t image_width = matrix_columns;
    int64_t image_height = matrix_rows;
    const int bytes_per_pixel = 3;  // RGB888 format
    uint8_t *image_data = (uint8_t *)malloc(matrix_rows * matrix_columns * bytes_per_pixel * sizeof(uint8_t));
    uint8_t *image_data_ptr = image_data;
    
    // optionally flip the rows of the image buffer so it displays in the orientation we want; this could be an option
    // note that here flipping is done by default, so flipped=true turns off this flip!
    if (matrix_value->Type() == EidosValueType::kValueInt)
    {
        const int64_t *matrix_data = matrix_value->IntData();
        
        for (int64_t matrix_row = 0; matrix_row < matrix_rows; ++matrix_row)
        {
            int64_t matrix_row_to_read = (flipped ? matrix_row : (matrix_rows - 1) - matrix_row);
            
            for (int64_t matrix_column = 0; matrix_column < matrix_columns; ++matrix_column)
            {
                double original_value = matrix_data[matrix_row_to_read + matrix_column * matrix_rows];
                double value = original_value;
                
                if (rescaling)
                    value = (value + rescale_offset) * rescale_scaling;
                
                if (value < 0.0)
                    value = 0.0;
                if (value > 1.0)
                    value = 1.0;
                
                double red, green, blue;
                
                Eidos_ColorPaletteLookup(value, palette, red, green, blue);
                
                uint8_t r_i = round(red * 255.0);
                uint8_t g_i = round(green * 255.0);
                uint8_t b_i = round(blue * 255.0);
                
                *(image_data_ptr++) = r_i;
                *(image_data_ptr++) = g_i;
                *(image_data_ptr++) = b_i;
                
                //std::cout << "original_value " << original_value << " rescaled to value " << value << ", r " << (uint32_t)r_i << ", g " << (uint32_t)g_i << ", b " << (uint32_t)b_i << std::endl;
            }
        }
    }
    else
    {
        const double *matrix_data = matrix_value->FloatData();
        
        for (int64_t matrix_row = 0; matrix_row < matrix_rows; ++matrix_row)
        {
            int64_t matrix_row_to_read = (flipped ? matrix_row : (matrix_rows - 1) - matrix_row);
            
            for (int64_t matrix_column = 0; matrix_column < matrix_columns; ++matrix_column)
            {
                double original_value = matrix_data[matrix_row_to_read + matrix_column * matrix_rows];
                double value = original_value;
                
                if (rescaling)
                    value = (value + rescale_offset) * rescale_scaling;
                
                if (value < 0.0)
                    value = 0.0;
                if (value > 1.0)
                    value = 1.0;
                
                double red, green, blue;
                
                Eidos_ColorPaletteLookup(value, palette, red, green, blue);
                
                uint8_t r_i = round(red * 255.0);
                uint8_t g_i = round(green * 255.0);
                uint8_t b_i = round(blue * 255.0);
                
                *(image_data_ptr++) = r_i;
                *(image_data_ptr++) = g_i;
                *(image_data_ptr++) = b_i;
                
                //std::cout << "original_value " << original_value << " rescaled to value " << value << ", r " << (uint32_t)r_i << ", g " << (uint32_t)g_i << ", b " << (uint32_t)b_i << std::endl;
            }
        }
    }
    
    QImage image(image_data, image_width, image_height, image_width * bytes_per_pixel, QImage::Format_RGB888, free, image_data);
    
    // x and y
    double *x = (double *)malloc(2 * sizeof(double));
    double *y = (double *)malloc(2 * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::image): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    x[0] = x1_value->NumericAtIndex_NOCAST(0, nullptr);
    y[0] = y1_value->NumericAtIndex_NOCAST(0, nullptr);
    x[1] = x2_value->NumericAtIndex_NOCAST(0, nullptr);
    y[1] = y2_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if (x[0] > x[1])
        EIDOS_TERMINATION << "ERROR (Plot::image): image() requires x1 <= x2." << EidosTerminate(nullptr);
    if (y[0] > y[1])
        EIDOS_TERMINATION << "ERROR (Plot::image): image() requires y1 <= y2." << EidosTerminate(nullptr);
    
    // alpha
    double alpha = alpha_value->FloatAtIndex_NOCAST(0, nullptr);
    
    if ((alpha < 0.0) || (alpha > 1.0))
        EIDOS_TERMINATION << "ERROR (Plot::image): image() requires the image alpha to be in [0, 1]." << EidosTerminate(nullptr);
    
    std::vector<double> *imageAlphas = new std::vector<double>;  // we only take a singleton alpha, but the API expects a buffer
    
    imageAlphas->push_back(alpha);
    
    plotview_->addImageData(x, y, 2, image, imageAlphas);       // implicitly shares image; takes the x, y, and imageAlphas buffers from us
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)points(numeric x, numeric y, [integer symbol = 0], [string color = "red"], [string border = "black"], [numeric lwd = 1.0], [numeric size = 1.0], [float alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_points(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *x_value = p_arguments[0].get();
    EidosValue *y_value = p_arguments[1].get();
    EidosValue *symbol_value = p_arguments[2].get();
    EidosValue *color_value = p_arguments[3].get();
    EidosValue *border_value = p_arguments[4].get();
    EidosValue *lwd_value = p_arguments[5].get();
    EidosValue *size_value = p_arguments[6].get();
    EidosValue *alpha_value = p_arguments[7].get();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    
    if (xcount != ycount)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires x and y to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x, x_value->FloatData(), xcount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x_value->IntData();
        
        for (int index = 0; index < xcount; ++index)
            x[index] = int_data[index];
    }
    
    if (y_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y, y_value->FloatData(), ycount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y_value->IntData();
        
        for (int index = 0; index < ycount; ++index)
            y[index] = int_data[index];
    }
    
    // symbol
    std::vector<int> *symbols = new std::vector<int>;
    int symbol_count = symbol_value->Count();
    
    if ((symbol_count != 1) && (symbol_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires symbol to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < symbol_count; ++index)
    {
        int symbol = symbol_value->IntAtIndex_NOCAST(index, nullptr);
        
        if (symbol < 0)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of symbol to be >= 0." << EidosTerminate();
        
        symbols->push_back(symbol);
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires color to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // border
    std::vector<QColor> *borders = new std::vector<QColor>;
    int border_count = border_value->Count();
    
    if ((border_count != 1) && (border_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires border to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < border_count; ++index)
    {
        std::string border_string = border_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t borderR, borderG, borderB;
        
        Eidos_GetColorComponents(border_string, &borderR, &borderG, &borderB);
        
        borders->emplace_back(borderR, borderG, borderB, 255);
    }
    
    // lwd
    std::vector<double> *lwds = new std::vector<double>;
    int lwd_count = lwd_value->Count();
    
    if ((lwd_count != 1) && (lwd_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires lwd to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    // size
    std::vector<double> *sizes = new std::vector<double>;
    int size_count = size_value->Count();
    
    if ((size_count != 1) && (size_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires size to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < size_count; ++index)
    {
        double size = size_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((size <= 0.0) || (size > 1000.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
        
        sizes->push_back(size);
    }
    
    // alpha
    std::vector<double> *alphas = new std::vector<double>;
    int alpha_count = alpha_value->Count();
    
    if ((alpha_count != 1) && (alpha_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires alpha to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < alpha_count; ++index)
    {
        double alpha = alpha_value->FloatAtIndex_NOCAST(index, nullptr);
        
        if ((alpha < 0.0) || (alpha > 1.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires the elements of alpha to be in [0, 1]." << EidosTerminate(nullptr);
        
        alphas->push_back(alpha);
    }
    
    plotview_->addPointData(x, y, xcount, symbols, colors, borders, alphas, lwds, sizes);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)rects(numeric x1, numeric y1, numeric x2, numeric y2, [string color = "red"], [string border = "black"], [numeric lwd = 1.0], [float alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_rects(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
    EidosValue *x1_value = p_arguments[0].get();
    EidosValue *y1_value = p_arguments[1].get();
    EidosValue *x2_value = p_arguments[2].get();
    EidosValue *y2_value = p_arguments[3].get();
    EidosValue *color_value = p_arguments[4].get();
    EidosValue *border_value = p_arguments[5].get();
    EidosValue *lwd_value = p_arguments[6].get();
    EidosValue *alpha_value = p_arguments[7].get();
    
    // x1, y1, x2, y2
    int segment_count = x1_value->Count();
    int y1count = y1_value->Count();
    int x2count = x2_value->Count();
    int y2count = y2_value->Count();
    
    if ((segment_count != y1count) || (segment_count != x2count) || (segment_count != y2count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires x1, y1, x2, and y2 to be the same length." << EidosTerminate();
    
    double *x1 = (double *)malloc(segment_count * sizeof(double));
    double *y1 = (double *)malloc(segment_count * sizeof(double));
    double *x2 = (double *)malloc(segment_count * sizeof(double));
    double *y2 = (double *)malloc(segment_count * sizeof(double));
    
    if (!x1 || !y1 || !x2 || !y2)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x1_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x1, x1_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x1_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            x1[index] = int_data[index];
    }
    
    if (y1_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y1, y1_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y1_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            y1[index] = int_data[index];
    }
    
    if (x2_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x2, x2_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x2_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            x2[index] = int_data[index];
    }
    
    if (y2_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y2, y2_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y2_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            y2[index] = int_data[index];
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires color to match the length of x1/y1/x2/y2, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        
        if (color_string == "none")
        {
            // "none" is a special named color provided by rects() to say you don't want a frame or fill
            colors->emplace_back(Qt::transparent);
        }
        else
        {
            uint8_t colorR, colorG, colorB;
            
            Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
            
            colors->emplace_back(colorR, colorG, colorB, 255);
        }
    }
    
    // border
    std::vector<QColor> *borders = new std::vector<QColor>;
    int border_count = border_value->Count();
    
    if ((border_count != 1) && (border_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_points): points() requires border to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < border_count; ++index)
    {
        std::string border_string = border_value->StringAtIndex_NOCAST(index, nullptr);
        
        if (border_string == "none")
        {
            // "none" is a special named color provided by rects() to say you don't want a frame or fill
            borders->emplace_back(Qt::transparent);
        }
        else
        {
            uint8_t borderR, borderG, borderB;
            
            Eidos_GetColorComponents(border_string, &borderR, &borderG, &borderB);
            
            borders->emplace_back(borderR, borderG, borderB, 255);
        }
    }
    
    // lwd
    std::vector<double> *lwds = new std::vector<double>;
    int lwd_count = lwd_value->Count();
    
    if ((lwd_count != 1) && (lwd_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires lwd to match the length of x1/y1/x2/y2, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    // alpha
    std::vector<double> *alphas = new std::vector<double>;
    int alpha_count = alpha_value->Count();
    
    if ((alpha_count != 1) && (alpha_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires alpha to match the length of x1/y1/x2/y2, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < alpha_count; ++index)
    {
        double alpha = alpha_value->FloatAtIndex_NOCAST(index, nullptr);
        
        if ((alpha < 0.0) || (alpha > 1.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires the elements of alpha to be in [0, 1]." << EidosTerminate(nullptr);
        
        alphas->push_back(alpha);
    }
    
    plotview_->addRectData(x1, y1, x2, y2, segment_count, colors, borders, alphas, lwds);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)segments(numeric x1, numeric y1, numeric x2, numeric y2, [string color = "red"], [numeric lwd = 1.0], [float alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_segments(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
    EidosValue *x1_value = p_arguments[0].get();
    EidosValue *y1_value = p_arguments[1].get();
    EidosValue *x2_value = p_arguments[2].get();
    EidosValue *y2_value = p_arguments[3].get();
    EidosValue *color_value = p_arguments[4].get();
    EidosValue *lwd_value = p_arguments[5].get();
    EidosValue *alpha_value = p_arguments[6].get();
    
    // x1, y1, x2, y2
    int segment_count = x1_value->Count();
    int y1count = y1_value->Count();
    int x2count = x2_value->Count();
    int y2count = y2_value->Count();
    
    if ((segment_count != y1count) || (segment_count != x2count) || (segment_count != y2count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires x1, y1, x2, and y2 to be the same length." << EidosTerminate();
    
    double *x1 = (double *)malloc(segment_count * sizeof(double));
    double *y1 = (double *)malloc(segment_count * sizeof(double));
    double *x2 = (double *)malloc(segment_count * sizeof(double));
    double *y2 = (double *)malloc(segment_count * sizeof(double));
    
    if (!x1 || !y1 || !x2 || !y2)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x1_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x1, x1_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x1_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            x1[index] = int_data[index];
    }
    
    if (y1_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y1, y1_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y1_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            y1[index] = int_data[index];
    }
    
    if (x2_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x2, x2_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x2_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            x2[index] = int_data[index];
    }
    
    if (y2_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y2, y2_value->FloatData(), segment_count * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y2_value->IntData();
        
        for (int index = 0; index < segment_count; ++index)
            y2[index] = int_data[index];
    }
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires color to match the length of x1/y1/x2/y2, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // lwd
    std::vector<double> *lwds = new std::vector<double>;
    int lwd_count = lwd_value->Count();
    
    if ((lwd_count != 1) && (lwd_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires lwd to match the length of x1/y1/x2/y2, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < lwd_count; ++index)
    {
        double lwd = lwd_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((lwd < 0.0) || (lwd > 100.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires the elements of lwd to be in [0, 100]." << EidosTerminate(nullptr);
        
        lwds->push_back(lwd);
    }
    
    // alpha
    std::vector<double> *alphas = new std::vector<double>;
    int alpha_count = alpha_value->Count();
    
    if ((alpha_count != 1) && (alpha_count != segment_count))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires alpha to match the length of x1/y1/x2/y2, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < alpha_count; ++index)
    {
        double alpha = alpha_value->FloatAtIndex_NOCAST(index, nullptr);
        
        if ((alpha < 0.0) || (alpha > 1.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_segments): segments() requires the elements of alpha to be in [0, 1]." << EidosTerminate(nullptr);
        
        alphas->push_back(alpha);
    }
    
    plotview_->addSegmentData(x1, y1, x2, y2, segment_count, colors, alphas, lwds);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)setBorderless([numeric marginLeft = 0.0], [numeric marginTop = 0.0], [numeric marginRight = 0.0], [numeric marginBottom = 0.0])
//
EidosValue_SP Plot::ExecuteMethod_setBorderless(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
    EidosValue *marginLeft_value = p_arguments[0].get();
    EidosValue *marginTop_value = p_arguments[1].get();
    EidosValue *marginRight_value = p_arguments[2].get();
    EidosValue *marginBottom_value = p_arguments[3].get();
    
    double marginLeft = marginLeft_value->NumericAtIndex_NOCAST(0, nullptr);
    double marginTop = marginTop_value->NumericAtIndex_NOCAST(0, nullptr);
    double marginRight = marginRight_value->NumericAtIndex_NOCAST(0, nullptr);
    double marginBottom = marginBottom_value->NumericAtIndex_NOCAST(0, nullptr);
    
    if ((marginLeft < 0.0) || (marginTop < 0.0) || (marginRight < 0.0) || (marginBottom < 0.0))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_setBorderless): setBorderless() requires all margins to be >= 0." << EidosTerminate(nullptr);
    
    plotview_->setBorderless(true, marginLeft, marginTop, marginRight, marginBottom);
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)text(numeric x, numeric y, string labels, [string color = "black"], [numeric size = 10.0], [Nif adj = NULL], [float alpha = 1.0])
//
EidosValue_SP Plot::ExecuteMethod_text(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *x_value = p_arguments[0].get();
    EidosValue *y_value = p_arguments[1].get();
    EidosValue *labels_value = p_arguments[2].get();
    EidosValue *color_value = p_arguments[3].get();
    EidosValue *size_value = p_arguments[4].get();
    EidosValue *adj_value = p_arguments[5].get();
    EidosValue *alpha_value = p_arguments[6].get();
    
    // x and y
    int xcount = x_value->Count();
    int ycount = y_value->Count();
    int labelscount = labels_value->Count();
    
    if ((xcount != ycount) || (xcount != labelscount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires x, y, and labels to be the same length." << EidosTerminate();
    
    double *x = (double *)malloc(xcount * sizeof(double));
    double *y = (double *)malloc(ycount * sizeof(double));
    
    if (!x || !y)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
    
    if (x_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(x, x_value->FloatData(), xcount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = x_value->IntData();
        
        for (int index = 0; index < xcount; ++index)
            x[index] = int_data[index];
    }
    
    if (y_value->Type() == EidosValueType::kValueFloat)
    {
        memcpy(y, y_value->FloatData(), ycount * sizeof(double));
    }
    else
    {
        const int64_t *int_data = y_value->IntData();
        
        for (int index = 0; index < ycount; ++index)
            y[index] = int_data[index];
    }
    
    // labels
    std::vector<QString> *labels = new std::vector<QString>;
    const std::string *string_data = labels_value->StringData();
    
    for (int index = 0; index < labelscount; ++index)
        labels->emplace_back(QString::fromStdString(string_data[index]));
    
    // color
    std::vector<QColor> *colors = new std::vector<QColor>;
    int color_count = color_value->Count();
    
    if ((color_count != 1) && (color_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires color to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < color_count; ++index)
    {
        std::string color_string = color_value->StringAtIndex_NOCAST(index, nullptr);
        uint8_t colorR, colorG, colorB;
        
        Eidos_GetColorComponents(color_string, &colorR, &colorG, &colorB);
        
        colors->emplace_back(colorR, colorG, colorB, 255);
    }
    
    // size
    std::vector<double> *sizes = new std::vector<double>;
    int size_count = size_value->Count();
    
    if ((size_count != 1) && (size_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires size to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < size_count; ++index)
    {
        double size = size_value->NumericAtIndex_NOCAST(index, nullptr);
        
        if ((size <= 0.0) || (size > 1000.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires the elements of size to be in (0, 1000]." << EidosTerminate(nullptr);
        
        sizes->push_back(size);
    }
    
    // adj
    double adj[2] = {0.5, 0.5};
    
    if (adj_value->Type() != EidosValueType::kValueNULL)
    {
        if (adj_value->Count() != 2)
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires adj to be a numeric vector of length 2, or NULL." << EidosTerminate();
        
        adj[0] = adj_value->NumericAtIndex_NOCAST(0, nullptr);
        adj[1] = adj_value->NumericAtIndex_NOCAST(1, nullptr);
    }
    
    // alpha
    std::vector<double> *alphas = new std::vector<double>;
    int alpha_count = alpha_value->Count();
    
    if ((alpha_count != 1) && (alpha_count != xcount))
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires alpha to match the length of x and y, or be singleton." << EidosTerminate();
    
    for (int index = 0; index < alpha_count; ++index)
    {
        double alpha = alpha_value->FloatAtIndex_NOCAST(index, nullptr);
        
        if ((alpha < 0.0) || (alpha > 1.0))
            EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_text): text() requires the elements of alpha to be in [0, 1]." << EidosTerminate(nullptr);
        
        alphas->push_back(alpha);
    }
    
    plotview_->addTextData(x, y, labels, xcount, colors, alphas, sizes, adj);       // takes ownership of buffers
    
    return gStaticEidosValueVOID;
}

//	*********************	– (void)write(string$ filePath)
//
EidosValue_SP Plot::ExecuteMethod_write(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
    EidosValue *filePath_value = p_arguments[0].get();
    
    std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
    
    if (outfile_path.length() == 0)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_write): write() requires a non-empty path." << EidosTerminate();
    
    QString qpath = QString::fromStdString(outfile_path);
    bool success = plotview_->writeToFile(qpath);
    
    if (!success)
        EIDOS_TERMINATION << "ERROR (Plot::ExecuteMethod_write): write() could not write to " << outfile_path << "; check the permissions of the enclosing directory." << EidosTerminate();
    
    return gStaticEidosValueVOID;
}


//
//	Plot_Class
//
#pragma mark -
#pragma mark Plot_Class
#pragma mark -

EidosClass *gSLiM_Plot_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Plot_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
        
        properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_title, true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
        
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Plot_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
        
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_abline, kEidosValueMaskVOID))
                                  ->AddNumeric_ON("a", gStaticEidosValueNULL)->AddNumeric_ON("b", gStaticEidosValueNULL)
                                  ->AddNumeric_ON("h", gStaticEidosValueNULL)->AddNumeric_ON("v", gStaticEidosValueNULL)
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_addLegend, kEidosValueMaskVOID))
                                  ->AddString_OSN("position", gStaticEidosValueNULL)->AddInt_OSN("inset", gStaticEidosValueNULL)
                                  ->AddNumeric_OSN("labelSize", gStaticEidosValueNULL)->AddNumeric_OSN("lineHeight", gStaticEidosValueNULL)
                                  ->AddNumeric_OSN("graphicsWidth", gStaticEidosValueNULL)->AddNumeric_OSN("exteriorMargin", gStaticEidosValueNULL)
                                  ->AddNumeric_OSN("interiorMargin", gStaticEidosValueNULL)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_axis, kEidosValueMaskVOID))
                                  ->AddInt_S("side")->AddNumeric_ON("at", gStaticEidosValueNULL)
                                  ->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional, "labels", nullptr, gStaticEidosValue_LogicalT)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_image, kEidosValueMaskVOID))
                                  ->AddObject_S(gStr_image, nullptr)->AddNumeric_S("x1")->AddNumeric_S("y1")->AddNumeric_S("x2")->AddNumeric_S("y2")
                                  ->AddLogical_OS("flipped", gStaticEidosValue_LogicalF)->AddFloat_OS("alpha", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendLineEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendPointEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")->AddInt_OS("symbol", gStaticEidosValue_Integer0)
                                  ->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddString_OS("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)->AddNumeric_OS("size", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendSwatchEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_legendTitleEntry, kEidosValueMaskVOID))
                                  ->AddString_S("label")));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_lines, kEidosValueMaskVOID))
                                  ->AddNumeric("x")->AddNumeric("y")->AddString_OS("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_OS("lwd", gStaticEidosValue_Float1)->AddFloat_OS("alpha", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_matrix, kEidosValueMaskVOID))
                                  ->AddNumeric("matrix")->AddNumeric_S("x1")->AddNumeric_S("y1")->AddNumeric_S("x2")->AddNumeric_S("y2")
                                  ->AddLogical_OS("flipped", gStaticEidosValue_LogicalF)->AddNumeric_ON("valueRange", gStaticEidosValueNULL)
                                  ->AddString_OSN("colors", gStaticEidosValueNULL)->AddFloat_OS("alpha", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_points, kEidosValueMaskVOID))
                                  ->AddNumeric("x")->AddNumeric("y")->AddInt_O("symbol", gStaticEidosValue_Integer0)
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddString_O("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddNumeric_O("size", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1)));
        methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_rects, kEidosValueMaskVOID))
                                  ->AddNumeric("x1")->AddNumeric("y1")->AddNumeric("x2")->AddNumeric("y2")
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddString_O("border", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1));
        methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_segments, kEidosValueMaskVOID))
                                  ->AddNumeric("x1")->AddNumeric("y1")->AddNumeric("x2")->AddNumeric("y2")
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("red")))
                                  ->AddNumeric_O("lwd", gStaticEidosValue_Float1)->AddFloat_O("alpha", gStaticEidosValue_Float1));
        methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setBorderless, kEidosValueMaskVOID))
                                  ->AddNumeric_OS("marginLeft", gStaticEidosValue_Float0)->AddNumeric_OS("marginTop", gStaticEidosValue_Float0)
                                  ->AddNumeric_OS("marginRight", gStaticEidosValue_Float0)->AddNumeric_OS("marginBottom", gStaticEidosValue_Float0));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gStr_text, kEidosValueMaskVOID))
                                  ->AddNumeric("x")->AddNumeric("y")->AddString("labels")
                                  ->AddString_O("color", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("black")))
                                  ->AddNumeric_O("size", EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(10)))
                                  ->AddNumeric_ON("adj", gStaticEidosValueNULL)->AddFloat_O("alpha", gStaticEidosValue_Float1)));
        methods->emplace_back(static_cast<EidosInstanceMethodSignature *>((new EidosInstanceMethodSignature(gEidosStr_write, kEidosValueMaskVOID))
                                  ->AddString_S(gEidosStr_filePath)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
































