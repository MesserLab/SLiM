//
//  QtSLiMExtras.cpp
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
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


#include "QtSLiMExtras.h"

#include <QTimer>
#include <QTextCharFormat>
#include <QAction>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSizePolicy>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QPaintEvent>
#include <QStyleOption>
#include <QStyle>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPalette>
#include <QApplication>
#include <QDebug>
#include <cmath>

#include <string>
#include <algorithm>

#include "QtSLiMPreferences.h"
#include "QtSLiMAppDelegate.h"

#include "eidos_value.h"


void QtSLiMFrameRect(const QRect &p_rect, const QColor &p_color, QPainter &p_painter)
{
    p_painter.fillRect(QRect(p_rect.left(), p_rect.top(), p_rect.width(), 1), p_color);                                 // top edge
    p_painter.fillRect(QRect(p_rect.left(), p_rect.top() + 1, 1, p_rect.height() - 2), p_color);                        // left edge (without corner pixels)
    p_painter.fillRect(QRect(p_rect.left() + p_rect.width() - 1, p_rect.top() + 1, 1, p_rect.height() - 2), p_color);   // right edge (without corner pixels)
    p_painter.fillRect(QRect(p_rect.left(), p_rect.top() + p_rect.height() - 1, p_rect.width(), 1), p_color);           // bottom edge
}

void QtSLiMFrameRect(const QRectF &p_rect, const QColor &p_color, QPainter &p_painter, double w)
{
    p_painter.fillRect(QRectF(p_rect.left(), p_rect.top(), p_rect.width(), w), p_color);
    p_painter.fillRect(QRectF(p_rect.left(), p_rect.top() + w, w, p_rect.height() - 2*w), p_color);
    p_painter.fillRect(QRectF(p_rect.left() + p_rect.width() - w, p_rect.top() + w, w, p_rect.height() - 2*w), p_color);
    p_painter.fillRect(QRectF(p_rect.left(), p_rect.top() + p_rect.height() - w, p_rect.width(), w), p_color);
}

QColor QtSLiMColorWithWhite(double p_white, double p_alpha)
{
    QColor color;
    color.setRgbF(p_white, p_white, p_white, p_alpha);
    return color;
}

QColor QtSLiMColorWithRGB(double p_red, double p_green, double p_blue, double p_alpha)
{
    QColor color;
    color.setRgbF(p_red, p_green, p_blue, p_alpha);
    return color;
}

QColor QtSLiMColorWithHSV(double p_hue, double p_saturation, double p_value, double p_alpha)
{
    QColor color;
    color.setHsvF(p_hue, p_saturation, p_value, p_alpha);
    return color;
}


bool QtSLiMInDarkMode(void)
{
    // We determine whether we're in dark mode heuristically: if the window background color is darker than 50% gray
    // We don't attempt to cache this value, since it sounds like the change notification for this is buggy
    QColor windowColor = QPalette().color(QPalette::Window);
    double windowBrightness = 0.21 * windowColor.redF() + 0.72 * windowColor.greenF() + 0.07 * windowColor.blueF();
    
    return (windowBrightness < 0.5);
}

QString QtSLiMImagePath(QString baseName, bool highlighted)
{
    bool inDarkMode = QtSLiMInDarkMode();
    
    baseName = (inDarkMode ? ":/buttons_DARK/" : ":/buttons/") + baseName;
    
    if (highlighted)
        baseName += "_H";
    if (inDarkMode)
        baseName += "_DARK";
    baseName += ".png";
    
    return baseName;
}


const double greenBrightness = 0.8;

void RGBForFitness(double value, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor)
{
	// apply the scaling factor
	value = (value - 1.0) * scalingFactor + 1.0;
	
	if (value <= 0.5)
	{
		// value <= 0.5 is a shade of red, going down to black
		*colorRed = static_cast<float>(value * 2.0);
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value >= 2.0)
	{
		// value >= 2.0 is a shade of green, going up to white
		*colorRed = static_cast<float>((value - 2.0) * greenBrightness / value);
		*colorGreen = static_cast<float>(greenBrightness);
		*colorBlue = static_cast<float>((value - 2.0) * greenBrightness / value);
	}
	else if (value <= 1.0)
	{
		// value <= 1.0 (but > 0.5) goes from red (unfit) to yellow (neutral)
		*colorRed = 1.0;
		*colorGreen = static_cast<float>((value - 0.5) * 2.0);
		*colorBlue = 0.0;
	}
	else	// 1.0 < value < 2.0
	{
		// value > 1.0 (but < 2.0) goes from yellow (neutral) to green (fit)
		*colorRed = static_cast<float>(2.0 - value);
		*colorGreen = static_cast<float>(greenBrightness + (1.0 - greenBrightness) * (2.0 - value));
		*colorBlue = 0.0;
	}
}

void RGBForSelectionCoeff(double value, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor)
{
	// apply a scaling factor; this could be user-adjustible since different models have different relevant fitness ranges
	value *= scalingFactor;
	
	// and add 1, just so we can re-use the same code as in RGBForFitness()
	value += 1.0;
	
	if (value <= 0.0)
	{
		// value <= 0.0 is the darkest shade of red we use
		*colorRed = 0.5;
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value <= 0.5)
	{
		// value <= 0.5 is a shade of red, going down toward black
		*colorRed = static_cast<float>(value + 0.5);
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value < 1.0)
	{
		// value <= 1.0 (but > 0.5) goes from red (very unfit) to orange (nearly neutral)
		*colorRed = 1.0;
		*colorGreen = static_cast<float>((value - 0.5) * 1.0);
		*colorBlue = 0.0;
	}
	else if (value == 1.0)
	{
		// exactly neutral mutations are yellow
		*colorRed = 1.0;
		*colorGreen = 1.0;
		*colorBlue = 0.0;
	}
	else if (value <= 1.5)
	{
		// value > 1.0 (but < 1.5) goes from green (nearly neutral) to cyan (fit)
		*colorRed = 0.0;
		*colorGreen = static_cast<float>(greenBrightness);
		*colorBlue = static_cast<float>((value - 1.0) * 2.0);
	}
	else if (value <= 2.0)
	{
		// value > 1.5 (but < 2.0) goes from cyan (fit) to blue (very fit)
		*colorRed = 0.0;
		*colorGreen = static_cast<float>(greenBrightness * ((2.0 - value) * 2.0));
		*colorBlue = 1.0;
	}
	else // (value > 2.0)
	{
		// value > 2.0 is a shade of blue, going up toward white
		*colorRed = static_cast<float>((value - 2.0) * 0.75 / value);
		*colorGreen = static_cast<float>((value - 2.0) * 0.75 / value);
		*colorBlue = 1.0;
	}
}

// A subclass of QLineEdit that selects all its text when it receives keyboard focus
// thanks to https://stackoverflow.com/a/51807268/2752221
QtSLiMGenerationLineEdit::QtSLiMGenerationLineEdit(const QString &contents, QWidget *p_parent) : QLineEdit(contents, p_parent)
{
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, [this]() { _ReconfigureAppearance(); });
    _ReconfigureAppearance();
}

QtSLiMGenerationLineEdit::QtSLiMGenerationLineEdit(QWidget *p_parent) : QLineEdit(p_parent)
{
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, [this]() { _ReconfigureAppearance(); });
    _ReconfigureAppearance();
}

QtSLiMGenerationLineEdit::~QtSLiMGenerationLineEdit() {}

void QtSLiMGenerationLineEdit::focusInEvent(QFocusEvent *p_event)
{
    // First let the base class process the event
    QLineEdit::focusInEvent(p_event);
    
    // Then select the text by a single shot timer, so that everything will
    // be processed before (calling selectAll() directly won't work)
    QTimer::singleShot(0, this, &QLineEdit::selectAll);
}

void QtSLiMGenerationLineEdit::setProgress(double p_progress)
{
    double newProgress = std::min(std::max(p_progress, 0.0), 1.0);
    
    if (newProgress != progress)
    {
        progress = newProgress;
        update();
    }
}

void QtSLiMGenerationLineEdit::_ReconfigureAppearance()
{
    // Eight states, based on three binary flags; but two states never happen in practice
    bool darkMode = QtSLiMInDarkMode();
    bool enabled = isEnabled();
    
    if (darkMode)
    {
        if (enabled)
        {
            if (dimmed)
                setStyleSheet("color: red;  background-color: black");                  // doesn't happen
            else
                setStyleSheet("color: rgb(255, 255, 255);  background-color: black");   // not playing
        }
        else
        {
            if (dimmed)
                setStyleSheet("color: rgb(40, 40, 40);  background-color: black");      // error state (not normally visible)
            else
                setStyleSheet("color: rgb(170, 170, 170);  background-color: black");   // playing
        }
    }
    else
    {
        if (enabled)
        {
            if (dimmed)
                setStyleSheet("color: red;  background-color: white");                  // doesn't happen
            else
                setStyleSheet("color: rgb(0, 0, 0);  background-color: white");         // not playing
        }
        else
        {
            if (dimmed)
                setStyleSheet("color: rgb(192, 192, 192);  background-color: white");   // error state (not normally visible)
            else
                setStyleSheet("color: rgb(120, 120, 120);  background-color: white");   // playing
        }
    }
    
    update();
}

void QtSLiMGenerationLineEdit::setAppearance(bool p_enabled, bool p_dimmed)
{
    if ((isEnabled() != p_enabled) || (dimmed != p_dimmed))
    {
        setEnabled(p_enabled);
        dimmed = p_dimmed;
        
        _ReconfigureAppearance();
    }
}

void QtSLiMGenerationLineEdit::paintEvent(QPaintEvent *p_paintEvent)
{
    // first let super draw
    QLineEdit::paintEvent(p_paintEvent);
    
    // then overlay a progress bar on top, if requested, and if we are not disabled & dimmed (error state)
    bool enabled = isEnabled();
    
    if (!enabled && dimmed)
        return;
    
    if (progress > 0.0)
    {
        bool darkMode = QtSLiMInDarkMode();
        QPainter painter(this);
        QRect bounds = rect().adjusted(2, 2, -2, -2);
        
        bounds.setWidth(round(bounds.width() * progress));
        
        if (darkMode) {
            // lighten the black background to a dark green; text is unaffected since it's light
            painter.setCompositionMode(QPainter::CompositionMode_Lighten);
            painter.fillRect(bounds, QColor(0, 120, 0));
        } else {
            // darken the white background to a light green; text is unaffected since it's dark
            painter.setCompositionMode(QPainter::CompositionMode_Darken);
            painter.fillRect(bounds, QColor(180, 255, 180));
        }
    }
}

void ColorizePropertySignature(const EidosPropertySignature *property_signature, double pointSize, QTextCursor lineCursor)
{
    //
    //	Note this logic is paralleled in the function operator<<(ostream &, const EidosPropertySignature &).
    //	These two should be kept in synch so the user-visible format of signatures is consistent.
    //
    QString docSigString = lineCursor.selectedText();
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    QTextCharFormat ttFormat;
    QFont displayFont(prefs.displayFontPref());
    displayFont.setPointSizeF(pointSize);
    ttFormat.setFont(displayFont);
    lineCursor.setCharFormat(ttFormat);
    
    bool inDarkMode = QtSLiMInDarkMode();
    QTextCharFormat functionAttrs(ttFormat), typeAttrs(ttFormat);
    functionAttrs.setForeground(QBrush(inDarkMode ? QColor(115, 145, 255) : QColor(28, 0, 207)));
    typeAttrs.setForeground(QBrush(inDarkMode ? QColor(90, 210, 90) : QColor(0, 116, 0)));
    
    QTextCursor propertyNameCursor(lineCursor);
    propertyNameCursor.setPosition(lineCursor.anchor(), QTextCursor::MoveAnchor);
    propertyNameCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, static_cast<int>(property_signature->property_name_.length()));
    propertyNameCursor.setCharFormat(functionAttrs);
    
    int nameLength = QString::fromStdString(property_signature->property_name_).length();
    int connectorLength = QString::fromStdString(property_signature->PropertySymbol()).length();
    int typeLength = docSigString.length() - (nameLength + 4 + connectorLength);
    QTextCursor typeCursor(lineCursor);
    typeCursor.setPosition(lineCursor.position(), QTextCursor::MoveAnchor);
    typeCursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
    typeCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, typeLength);
    typeCursor.setCharFormat(typeAttrs);
}

void ColorizeCallSignature(const EidosCallSignature *call_signature, double pointSize, QTextCursor lineCursor)
{
    //
    //	Note this logic is paralleled in the function operator<<(ostream &, const EidosCallSignature &).
    //	These two should be kept in synch so the user-visible format of signatures is consistent.
    //
    QString docSigString = lineCursor.selectedText();
    std::ostringstream ss;
    ss << *call_signature;
    QString callSigString = QString::fromStdString(ss.str());
    
    if (callSigString.endsWith(" <SLiM>") && !docSigString.endsWith(" <SLiM>"))
        callSigString.chop(7);
    
    if (docSigString != callSigString)
    {
        qDebug() << "*** " << ((call_signature->CallPrefix().length() > 0) ? "method" : "function") << "signature mismatch:\nold:" << docSigString << "\nnew:" << callSigString;
        return;
    }
    
    // the signature conforms to expectations, so we can colorize it
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    QTextCharFormat ttFormat;
    QFont displayFont(prefs.displayFontPref());
    displayFont.setPointSizeF(pointSize);
    ttFormat.setFont(displayFont);
    lineCursor.setCharFormat(ttFormat);
    
    bool inDarkMode = QtSLiMInDarkMode();
    QTextCharFormat typeAttrs(ttFormat), functionAttrs(ttFormat), paramAttrs(ttFormat);
    typeAttrs.setForeground(QBrush(inDarkMode ? QColor(115, 145, 255) : QColor(28, 0, 207)));
    functionAttrs.setForeground(QBrush(inDarkMode ? QColor(90, 210, 90) : QColor(0, 116, 0)));
    paramAttrs.setForeground(QBrush(inDarkMode ? QColor(220, 83, 185) : QColor(170, 13, 145)));
    
    int prefix_string_len = QString::fromStdString(call_signature->CallPrefix()).length();
    int return_type_string_len = QString::fromStdString(StringForEidosValueMask(call_signature->return_mask_, call_signature->return_class_, "", nullptr)).length();
    int function_name_string_len = QString::fromStdString(call_signature->call_name_).length();
    
    // colorize return type
    QTextCursor scanCursor(lineCursor);
    scanCursor.setPosition(lineCursor.anchor() + prefix_string_len + 1, QTextCursor::MoveAnchor);
    scanCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, return_type_string_len);
    scanCursor.setCharFormat(typeAttrs);
    
    // colorize call name
    scanCursor.setPosition(scanCursor.position() + 1, QTextCursor::MoveAnchor);
    scanCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, function_name_string_len);
    scanCursor.setCharFormat(functionAttrs);
    
    scanCursor.setPosition(scanCursor.position() + 1, QTextCursor::MoveAnchor);
    
    // colorize arguments
    size_t arg_mask_count = call_signature->arg_masks_.size();
    
    if (arg_mask_count == 0)
    {
        // colorize "void"
        scanCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
        scanCursor.setCharFormat(typeAttrs);
    }
    else
    {
        for (size_t arg_index = 0; arg_index < arg_mask_count; ++arg_index)
        {
            EidosValueMask type_mask = call_signature->arg_masks_[arg_index];
            const std::string &arg_name = call_signature->arg_names_[arg_index];
            const EidosClass *arg_obj_class = call_signature->arg_classes_[arg_index];
            EidosValue_SP arg_default = call_signature->arg_defaults_[arg_index];
            
            // skip private arguments
            if ((arg_name.length() >= 1) && (arg_name[0] == '_'))
                continue;
            
            scanCursor.setPosition(scanCursor.position() + ((arg_index > 0) ? 2 : 0), QTextCursor::MoveAnchor);     // ", "
            
            //
            //	Note this logic is paralleled in the function StringForEidosValueMask().
            //	These two should be kept in synch so the user-visible format of signatures is consistent.
            //
            if (arg_name == gEidosStr_ELLIPSIS)
            {
                scanCursor.setPosition(scanCursor.position() + 3, QTextCursor::MoveAnchor);     // "..."
                continue;
            }
            
            bool is_optional = !!(type_mask & kEidosValueMaskOptional);
            bool requires_singleton = !!(type_mask & kEidosValueMaskSingleton);
            EidosValueMask stripped_mask = type_mask & kEidosValueMaskFlagStrip;
            int typeLength = 0;
            
            if (is_optional)
                scanCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);     // "["
            
            if (stripped_mask == kEidosValueMaskNone)               typeLength = 1;     // "?"
            else if (stripped_mask == kEidosValueMaskAny)           typeLength = 1;     // "*"
            else if (stripped_mask == kEidosValueMaskAnyBase)       typeLength = 1;     // "+"
            else if (stripped_mask == kEidosValueMaskVOID)          typeLength = 4;     // "void"
            else if (stripped_mask == kEidosValueMaskNULL)          typeLength = 4;     // "NULL"
            else if (stripped_mask == kEidosValueMaskLogical)       typeLength = 7;     // "logical"
            else if (stripped_mask == kEidosValueMaskString)        typeLength = 6;     // "string"
            else if (stripped_mask == kEidosValueMaskInt)           typeLength = 7;     // "integer"
            else if (stripped_mask == kEidosValueMaskFloat)         typeLength = 5;     // "float"
            else if (stripped_mask == kEidosValueMaskObject)        typeLength = 6;     // "object"
            else if (stripped_mask == kEidosValueMaskNumeric)       typeLength = 7;     // "numeric"
            else
            {
                if (stripped_mask & kEidosValueMaskVOID)            typeLength++;     // "v"
                if (stripped_mask & kEidosValueMaskNULL)            typeLength++;     // "N"
                if (stripped_mask & kEidosValueMaskLogical)         typeLength++;     // "l"
                if (stripped_mask & kEidosValueMaskInt)             typeLength++;     // "i"
                if (stripped_mask & kEidosValueMaskFloat)           typeLength++;     // "f"
                if (stripped_mask & kEidosValueMaskString)          typeLength++;     // "s"
                if (stripped_mask & kEidosValueMaskObject)          typeLength++;     // "o"
            }
            
            if (arg_obj_class && (stripped_mask & kEidosValueMaskObject))
            {
                int obj_type_name_len = QString::fromStdString(arg_obj_class->ClassName()).length();
                
                typeLength += (obj_type_name_len + 2);  // "<" obj_type_name ">"
            }
            
            if (requires_singleton)
                typeLength++;     // "$"
            
            scanCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, typeLength);
            scanCursor.setCharFormat(typeAttrs);
            scanCursor.setPosition(scanCursor.position(), QTextCursor::MoveAnchor);
            
            if (arg_name.length() > 0)
            {
                scanCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);    // " "
                scanCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, QString::fromStdString(arg_name).length());
                scanCursor.setCharFormat(paramAttrs);
                scanCursor.setPosition(scanCursor.position(), QTextCursor::MoveAnchor);
            }
            
            if (is_optional)
            {
                if (arg_default && (arg_default != gStaticEidosValueNULLInvisible.get()))
                {
                    scanCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 3);    // " = "
                    
                    std::ostringstream default_string_stream;
                    
                    arg_default->Print(default_string_stream);
                    
                    int default_string_len = QString::fromStdString(default_string_stream.str()).length();
                    
                    scanCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, default_string_len);
                }
                
                scanCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);    // "]"
            }
        }
    }
}


// A subclass of QHBoxLayout specifically designed to lay out the play controls in the main window

QtSLiMPlayControlsLayout::~QtSLiMPlayControlsLayout()
{
}

QSize QtSLiMPlayControlsLayout::sizeHint() const
{
    QSize size(0,0);
    int n = count();
    
    for (int i = 0; i < n; ++i)
    {
        if (i == 2)
            continue;     // the profile button takes no space
        
        QLayoutItem *layoutItem = itemAt(i);
        QSize itemSizeHint = layoutItem->sizeHint();
        
        size.rwidth() += itemSizeHint.width();
        size.rheight() = std::max(size.height(), itemSizeHint.height());
    }
    
    size.rwidth() += (n - 2) * spacing();   // -2 because we exclude spacing for the profile button
    
    return size;
}

QSize QtSLiMPlayControlsLayout::minimumSize() const
{
    QSize size(0,0);
    int n = count();
    
    for (int i = 0; i < n; ++i)
    {
        if (i == 2)
            continue;     // the profile button takes no space
        
        QLayoutItem *layoutItem = itemAt(i);
        QSize itemMinimumSize = layoutItem->minimumSize();
        
        size.rwidth() += itemMinimumSize.width();
        size.rheight() = std::max(size.height(), itemMinimumSize.height());
    }
    
    size.rwidth() += (n - 2) * spacing();   // -2 because we exclude spacing for the profile button
    
    return size;
}

void QtSLiMPlayControlsLayout::setGeometry(const QRect &rect)
{
    QHBoxLayout::setGeometry(rect);
    
    int n = count();
    int position = rect.x();
    QRect playButtonRect;
    
    for (int i = 0; i < n; ++i)
    {
        if (i == 2)
            continue;     // the profile button takes no space
        
        QLayoutItem *layoutItem = itemAt(i);
        QSize itemSizeHint = layoutItem->sizeHint();
        QRect geom(position, rect.y(), itemSizeHint.width(), itemSizeHint.height());
        
        layoutItem->setGeometry(geom);
        position += itemSizeHint.width() + spacing();
        
        if (i == 1)
            playButtonRect = geom;
    }
    
    // position the profile button; the button must lie inside the bounds of the parent widget due to clipping
    QLayoutItem *profileButton = itemAt(2);
    QSize itemSizeHint = profileButton->sizeHint();
    QRect geom(playButtonRect.left() + playButtonRect.width() - 22, rect.y() - 6, itemSizeHint.width(), itemSizeHint.height());
    
    profileButton->setGeometry(geom);
}

// Heat colors for profiling display
#define SLIM_YELLOW_FRACTION 0.10
#define SLIM_SATURATION 0.75

QColor slimColorForFraction(double fraction)
{
    if (fraction < SLIM_YELLOW_FRACTION)
	{
		// small fractions fall on a ramp from white (0.0) to yellow (SLIM_YELLOW_FRACTION)
        return QtSLiMColorWithHSV(1.0 / 6.0, (fraction / SLIM_YELLOW_FRACTION) * SLIM_SATURATION, 1.0, 1.0);
	}
	else
	{
		// larger fractions ramp from yellow (SLIM_YELLOW_FRACTION) to red (1.0)
        return QtSLiMColorWithHSV((1.0 / 6.0) * (1.0 - (fraction - SLIM_YELLOW_FRACTION) / (1.0 - SLIM_YELLOW_FRACTION)), SLIM_SATURATION, 1.0, 1.0);
	}
}

// Nicely formatted memory usage strings

QString stringForByteCount(uint64_t bytes)
{
    if (bytes > 512.0 * 1024.0 * 1024.0 * 1024.0)
		return QString("%1 TB").arg(bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	else if (bytes > 512.0 * 1024.0 * 1024.0)
		return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	else if (bytes > 512.0 * 1024.0)
		return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
	else if (bytes > 512.0)
		return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
	else
		return QString("%1 bytes").arg(bytes);
}

QString attributedStringForByteCount(uint64_t bytes, double total, QTextCharFormat &format)
{
    QString byteString = stringForByteCount(bytes);
	double fraction = bytes / total;
	QColor fractionColor = slimColorForFraction(fraction);
	
    // We modify format for the caller, which they can use to colorize the returned string
    format.setBackground(fractionColor);
    
	return byteString;
}

// Running a panel to obtain numbers from the user
// The goal here is to avoid a proliferation of dumb forms, by programmatically generating the UI

QStringList QtSLiMRunLineEditArrayDialog(QWidget *p_parent, QString title, QStringList captions, QStringList values)
{
    if (captions.size() < 1)
        return QStringList();
    if (captions.size() != values.size())
    {
        qDebug("QtSLiMRunLineEditArrayDialog: captions and values are not the same length!");
        return QStringList();
    }
    
    // make the dialog with an overall vertical layout
    QDialog *dialog = new QDialog(p_parent);
    QVBoxLayout *verticalLayout = new QVBoxLayout(dialog);
    
    // title label
    {
        QLabel *titleLabel = new QLabel(dialog);
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        titleLabel->setText(title);
        titleLabel->setFont(font);
        verticalLayout->addWidget(titleLabel);
    }
    
    // below-title spacer
    {
        QSpacerItem *belowTitleSpacer = new QSpacerItem(20, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
        verticalLayout->addItem(belowTitleSpacer);
    }
    
    // grid layout
    QVector<QLineEdit *> lineEdits;
    
    {
        QGridLayout *gridLayout = new QGridLayout();
        int rowCount = captions.size();
        
        for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
        {
            QString caption = captions[rowIndex];
            QString value = values[rowIndex];
            
            QLabel *paramLabel = new QLabel(dialog);
            paramLabel->setText(caption);
            gridLayout->addWidget(paramLabel, rowIndex, 1);
            
            QLineEdit *paramLineEdit = new QLineEdit(dialog);
            paramLineEdit->setText(value);
            paramLineEdit->setFixedWidth(60);
            paramLineEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            gridLayout->addWidget(paramLineEdit, rowIndex, 3);
            
            lineEdits.push_back(paramLineEdit);
        }
        
        // spacers, which only need to exist in the first row of the grid
        {
            QSpacerItem *leftMarginSpacer = new QSpacerItem(16, 5, QSizePolicy::Fixed, QSizePolicy::Minimum);
            gridLayout->addItem(leftMarginSpacer, 0, 0);
        }
        {
            QSpacerItem *internalSpacer = new QSpacerItem(20, 5, QSizePolicy::Fixed, QSizePolicy::Minimum);
            gridLayout->addItem(internalSpacer, 0, 2);
        }
        
        verticalLayout->addLayout(gridLayout);
    }
    
    // button box
    {
        QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        verticalLayout->addWidget(buttonBox);
        
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    }
    
    // fix sizing
    dialog->setFixedSize(dialog->sizeHint());
    dialog->setSizeGripEnabled(false);
    
    // select the first lineEdit and run the dialog
    lineEdits[0]->selectAll();
    
    int result = dialog->exec();
    
    if (result == QDialog::Accepted)
    {
        QStringList returnList;
        
        for (QLineEdit *lineEdit : qAsConst(lineEdits))
            returnList.append(lineEdit->text());
        
        delete dialog;
        return returnList;
    }
    else
    {
        delete dialog;
        return QStringList();
    }
}


// A subclass of QPushButton that draws its image with antialiasing, for a better appearance; used for the About panel
// See QtSLiMPushButton below for further comments; it uses the same approach, with additional bells and whistles
void QtSLiMIconView::paintEvent(QPaintEvent * /* p_paintEvent */)
{
    QPainter painter(this);
    QRect bounds = rect();
    
    // This uses the icon to draw, which works because of Qt::AA_UseHighDpiPixmaps
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    icon().paint(&painter, bounds, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
    painter.restore();
}


// A subclass of QPushButton that draws its image at screen resolution, for a better appearance on Retina etc.
// Turns out setting Qt::AA_UseHighDpiPixmaps fixes that issue; but this subclass also makes the buttons draw
// correctly in Qt 5.14.2, where button icons are shifted right one pixel and then clipped in an ugly way.
// BCH 1/18/2021: This class now has additional smarts to handle dark mode.  First of all, the base name for
// the icon used should be set up at creation time with a call to qtslimSetIcon(), with a highlight of false,
// presumably; for an icon of :/buttons/foo.png, that would be qtslimSetIcon("foo", false).  When the icon
// should be changed, either in its base name or highlight state, this can be changed with another such call.
// This will lead to the use of one of four image files depending on highlight state and dark mode state:
// :/buttons/foo.png, :/buttons/foo_H.png, :/buttons_DARK/foo_DARK.png, or :/buttons_DARK/foo_H_DARK.png.
// If the corresponding image file does not exist, an error message will be logged to the console, and the
// button will probably not draw properly.  All button images should be exactly the same size.

QtSLiMPushButton::QtSLiMPushButton(const QIcon &p_icon, const QString &p_text, QWidget *p_parent) : QPushButton(p_icon, p_text, p_parent)
{
    sharedInit();
}

QtSLiMPushButton::QtSLiMPushButton(const QString &p_text, QWidget *p_parent) : QPushButton(p_text, p_parent)
{
    sharedInit();
}

QtSLiMPushButton::QtSLiMPushButton(QWidget *p_parent) : QPushButton(p_parent)
{
    sharedInit();
}

void QtSLiMPushButton::sharedInit(void)
{
    // This button class is designed to work with icon images that include a border and background,
    // and typically include a transparent background, so we use a style sheet to enforce that
    setStyleSheet(QString::fromUtf8("QPushButton:pressed {\n"
                                    "	background-color: #00000000;\n"
                                    "	border: 0px;\n"
                                    "}\n"
                                    "QPushButton:checked {\n"
                                    "	background-color: #00000000;\n"
                                    "	border: 0px;\n"
                                    "}"));
}

QtSLiMPushButton::~QtSLiMPushButton(void)
{
    qtslimFreeCachedIcons();
}

void QtSLiMPushButton::qtslimFreeCachedIcons(void)
{
    if (qtslimIcon) { delete qtslimIcon; qtslimIcon = nullptr; }
    if (qtslimIcon_H) { delete qtslimIcon_H; qtslimIcon_H = nullptr; }
    if (qtslimIcon_DARK) { delete qtslimIcon_DARK; qtslimIcon_DARK = nullptr; }
    if (qtslimIcon_H_DARK) { delete qtslimIcon_H_DARK; qtslimIcon_H_DARK = nullptr; }
}

bool QtSLiMPushButton::hitButton(const QPoint &mousePosition) const
{
    // I noticed that mouse tracking in QtSLiMPushButton was off; it seemed like the bounds were
    // kind of inset, and Qt doesn't know the buttons are circular, and so forth.  Therefore this.
    
    // mousePosition is in the same coordinate system as rect(); we want to consider mousePosition
    // to be a hit if it is inside the circle or oval bounded by rect(), so let's bust out Pythagoras
    QRect bounds = rect();
    double xd = (mousePosition.x() - bounds.left()) / (double)bounds.width() - 0.5;
    double yd = (mousePosition.y() - bounds.top()) / (double)bounds.height() - 0.5;
    double distance = std::sqrt(xd * xd + yd * yd);
    
    //qDebug() << "x ==" << x << ", y ==" << y << "; d ==" << d;
    
    return (distance <= 0.51);  // a little more than 0.5 to provide a little slop
}

void QtSLiMPushButton::paintEvent(QPaintEvent *p_paintEvent)
{
    // We need a base name to operate; without one, we punt to super and it draws whatever it draws
    if (qtslimBaseName.length() == 0)
    {
        qDebug() << "QtSLiMPushButton::paintEvent: base name not set for object" << objectName();
        QPushButton::paintEvent(p_paintEvent);
        return;
    }
    
    // We have a base name; get the cached icon corresponding to our state
    QIcon *cachedIcon = qtslimIconForState(qtslimHighlighted, QtSLiMInDarkMode());
    
    if (!cachedIcon)
    {
        qDebug() << "QtSLiMPushButton::paintEvent: icon not found for base name" << qtslimBaseName;
        QPushButton::paintEvent(p_paintEvent);
        return;
    }
    
    // We got a valid icon; draw with it
    QPainter painter(this);
    QRect bounds = rect();
    
    // This uses the icon to draw, which works because of Qt::AA_UseHighDpiPixmaps
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    if (temporaryIcon.isNull())
    {
        cachedIcon->paint(&painter, bounds, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
    }
    else
    {
        // assume that the temporary icon completely covers the base icon when opacity is 1.0; this avoids artifacts
        // in the appearance of the button with opacity 1.0 due to double-drawing pixels with partial alpha
        if (temporaryIconOpacity < 1.0)
            cachedIcon->paint(&painter, bounds, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
        
        if (temporaryIconOpacity > 0.0)
        {
            painter.setOpacity(temporaryIconOpacity);
            temporaryIcon.paint(&painter, bounds, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
        }
    }
    
    painter.restore();
    
    /*
    // This code would construct and draw a high-DPI image, but using Qt::AA_UseHighDpiPixmaps is better
    QTransform transform = painter.combinedTransform();
    QRect mappedBounds = transform.mapRect(bounds);
    QSize size = mappedBounds.size();
    QIcon ico = icon();
    QPixmap pix = ico.pixmap(size, isEnabled() ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
    QImage image = pix.toImage();
    
    painter.drawImage(rect(), image);
    */
}

void QtSLiMPushButton::qtslimSetHighlight(bool highlighted)
{
    if (qtslimBaseName.length() == 0)
        qDebug() << "QtSLiMPushButton::qtslimSetHighlight: base name not set for object" << objectName();
    
    // We're not changing our base name, so we don't need to throw out cached icons
    qtslimHighlighted = highlighted;
    update();
}

void QtSLiMPushButton::qtslimSetIcon(QString baseName, bool highlighted)
{
    if (baseName == qtslimBaseName)
    {
        // We're not changing our base name, so we don't need to throw out cached icons
        qtslimHighlighted = highlighted;
    }
    else
    {
        // We're changing base name, so throw out cached icons
        qtslimBaseName = baseName;
        qtslimHighlighted = highlighted;
        qtslimFreeCachedIcons();
    }
    
    update();
}

QIcon *QtSLiMPushButton::qtslimIconForState(bool highlighted, bool darkMode)
{
    if (!highlighted)
    {
        if (!darkMode)
        {
            if (!qtslimIcon)
            {
                QString path = ":/buttons/";
                path += qtslimBaseName;
                path += ".png";
                qtslimIcon = new QIcon(path);
            }
            return qtslimIcon;
        }
        else
        {
            if (!qtslimIcon_DARK)
            {
                QString path = ":/buttons_DARK/";
                path += qtslimBaseName;
                path += "_DARK.png";
                qtslimIcon_DARK = new QIcon(path);
            }
            return qtslimIcon_DARK;
        }
    }
    else
    {
        if (!darkMode)
        {
            if (!qtslimIcon_H)
            {
                QString path = ":/buttons/";
                path += qtslimBaseName;
                path += "_H.png";
                qtslimIcon_H = new QIcon(path);
            }
            return qtslimIcon_H;
        }
        else
        {
            if (!qtslimIcon_H_DARK)
            {
                QString path = ":/buttons_DARK/";
                path += qtslimBaseName;
                path += "_H_DARK.png";
                qtslimIcon_H_DARK = new QIcon(path);
            }
            return qtslimIcon_H_DARK;
        }
    }
}


// A subclass of QSplitterHandle that does some custom drawing
void QtSLiMSplitterHandle::paintEvent(QPaintEvent *p_paintEvent)
{
    QPainter painter(this);
    QRect bounds = rect();
    bool inDarkMode = QtSLiMInDarkMode();
    
    // provide a darkened and beveled appearance
    QRect begin1Strip, begin2Strip, centerStrip, end2Strip, end1Strip;
    
    if (orientation() == Qt::Vertical)
    {
        begin1Strip = bounds.adjusted(0, 0, 0, -(bounds.height() - 1));
        begin2Strip = bounds.adjusted(0, 1, 0, -(bounds.height() - 2));
        centerStrip = bounds.adjusted(0, 2, 0, -2);
        end2Strip = bounds.adjusted(0, bounds.height() - 2, 0, -1);
        end1Strip = bounds.adjusted(0, bounds.height() - 1, 0, 0);
    }
    else    // Qt::Horizontal
    {
        begin1Strip = bounds.adjusted(0, 0, -(bounds.width() - 1), 0);
        begin2Strip = bounds.adjusted(1, 0, -(bounds.width() - 2), 0);
        centerStrip = bounds.adjusted(2, 0, -2, 0);
        end2Strip = bounds.adjusted(bounds.width() - 2, 0, -1, 0);
        end1Strip = bounds.adjusted(bounds.width() - 1, 0, 0, 0);
    }
    
    painter.fillRect(begin1Strip, QtSLiMColorWithWhite(inDarkMode ? 0.227 : 0.773, 1.0));
    painter.fillRect(begin2Strip, QtSLiMColorWithWhite(inDarkMode ? 0.000 : 1.000, 1.0));
    painter.fillRect(centerStrip, QtSLiMColorWithWhite(inDarkMode ? 0.035 : 0.965, 1.0));
    painter.fillRect(end2Strip, QtSLiMColorWithWhite(inDarkMode ? 0.082 : 0.918, 1.0));
    painter.fillRect(end1Strip, QtSLiMColorWithWhite(inDarkMode ? 0.278 : 0.722, 1.0));
    
    
    // On Linux, super draws the knob one pixel to the right of where it ought to be, so we draw it ourselves
    // This code is modified from QtSplitterHandle in the Qt 5.14.2 sources (it's identical in Qt 5.9.8)
    // This may turn out to be undesirable, as it assumes that the Linux widget kit is the one I use on Ubuntu
#if defined(__linux__)
    if (orientation() == Qt::Horizontal)
    {
        QStyleOption opt(0);
        opt.rect = contentsRect().adjusted(0, 0, -1, 0);    // make the rect one pixel narrower, which shifts the knob
        opt.palette = palette();
        opt.state = QStyle::State_Horizontal;
        
        /*
        // We don't have access to the hover/pressed state as far as I know, but it seems to be unused anyway
        if (d->hover)
            opt.state |= QStyle::State_MouseOver;
        if (d->pressed)
            opt.state |= QStyle::State_Sunken;
        */
        
        if (isEnabled())
            opt.state |= QStyle::State_Enabled;
        
        parentWidget()->style()->drawControl(QStyle::CE_Splitter, &opt, &painter, splitter());
        return;
    }
#endif
    
    // call super to overlay the splitter knob
    QSplitterHandle::paintEvent(p_paintEvent);
}


// A subclass of QStatusBar that draws a top separator, so our splitters abut nicely
// BCH 9/20/2020: this now draws the message as HTML text too, allowing colorized signatures
QtSLiMStatusBar::QtSLiMStatusBar(QWidget *p_parent) : QStatusBar(p_parent)
{
    // whenever our message changes, we resize vertically to accommodate it
    connect(this, &QStatusBar::messageChanged, this, [this]() { setHeightFromContent(); });
}

void QtSLiMStatusBar::paintEvent(QPaintEvent * /*p_paintEvent*/)
{
    QPainter p(this);
    QRect bounds = rect();
    bool inDarkMode = QtSLiMInDarkMode();
    
    // fill the interior; we no longer try to inherit this from QStatusBar, that was a headache
    p.fillRect(bounds, QtSLiMColorWithWhite(inDarkMode ? 0.118 : 0.965, 1.0));
    
    // draw the top separator and bevel lines
    QRect bevelLine = bounds.adjusted(0, 0, 0, -(bounds.height() - 1));
    
    p.fillRect(bevelLine, QtSLiMColorWithWhite(inDarkMode ? 0.278 : 0.722, 1.0));
    p.fillRect(bevelLine.adjusted(0, 1, 0, 1), QtSLiMColorWithWhite(inDarkMode ? 0.000 : 1.000, 1.0));
    p.fillRect(bevelLine.adjusted(0, (bounds.height() - 1), 0, (bounds.height() - 1)), QtSLiMColorWithWhite(inDarkMode ? 0.082 : 0.918, 1.0));
    
    // draw the message
    if (!currentMessage().isEmpty())
    {
        // would be nice for these coordinates not to be magic
#ifdef __APPLE__
        p.translate(QPointF(6, 3));
#else
        p.translate(QPointF(5, 1));
#endif

        p.setPen(inDarkMode ? Qt::white : Qt::black);
        QSizeF pageSize(bounds.width() - 10, 200);           // wrap to our width, with a maximum height of 200 (which should never happen)
        QTextDocument td;
        td.setPageSize(pageSize);
        td.setHtml(currentMessage());
        td.drawContents(&p, bounds);
    }
}

void QtSLiMStatusBar::resizeEvent(QResizeEvent *p_resizeEvent)
{
    // first call super to realize all consequences of the resize
    QStatusBar::resizeEvent(p_resizeEvent);
    
    // Then calculate our new minimum height, as a result of wrapping, and set it in a deferred manner to avoid recursion issues
    QTimer::singleShot(0, this, &QtSLiMStatusBar::setHeightFromContent);
}

void QtSLiMStatusBar::setHeightFromContent(void)
{
    // this mirrors the code in QtSLiMStatusBar::paintEvent()
    QRect bounds = rect();
    QSizeF pageSize(bounds.width() - 10, 200);           // wrap to our width, with a maximum height of 200 (which should never happen)
    QTextDocument td;
    td.setPageSize(pageSize);
    td.setHtml(currentMessage());
    
    // now get the drawn text height and calculate our minimum height
    QSizeF textSize = td.documentLayout()->documentSize();
    QSize minSizeHint = minimumSizeHint();
    QSize oldMinSize = minimumSize();
    QSize newMinSize;
    int newMaxHeight;
    
    if (textSize.height() < minSizeHint.height())
    {
        newMinSize = QSize(0, 0);
        newMaxHeight = minSizeHint.height();
    }
    else
    {
#ifdef __linux__
        newMinSize = QSize(minSizeHint.width(), textSize.height() + 0);
#else
        newMinSize = QSize(minSizeHint.width(), textSize.height() + 6);
#endif
        newMaxHeight = newMinSize.height();
    }
    
    // set the new size only if it is different from the old height, to minimize thrash
    if (newMinSize != oldMinSize)
    {
        setMinimumSize(newMinSize);
        setMaximumHeight(newMaxHeight);  // we have to set the max height also, to make the Eidos console's status bar work properly
        
        //qDebug() << "setHeightFromContent():";
        //qDebug() << "   minSizeHint == " << minSizeHint;
        //qDebug() << "   textSize == " << textSize;
        //qDebug() << "   newMinSize == " << newMinSize;
    }
}

QPixmap QtSLiMDarkenPixmap(QPixmap p_pixmap)
{
    QPixmap pixmap(p_pixmap);
    
    {
        QPainter painter(&pixmap);
        
        painter.fillRect(pixmap.rect(), QtSLiMColorWithWhite(0.0, 0.35));
    }
    
    return pixmap;
}


// find flashing; see https://bugreports.qt.io/browse/QTBUG-83147

static QPalette QtSLiMFlashPalette(QPlainTextEdit *te)
{
    // Returns a palette for QtSLiMTextEdit for highlighting errors, which could depend on platform and dark mode
    // Note that this is based on the current palette, and derives only the highlight colors
    QPalette p = te->palette();
    p.setColor(QPalette::Highlight, QColor(QColor(Qt::yellow)));
    p.setColor(QPalette::HighlightedText, QColor(Qt::black));
    return p;
}

void QtSLiMFlashHighlightInTextEdit(QPlainTextEdit *te)
{
    const int delayMillisec = 80;   // seems good?  12.5 times per second
    
    // set to the flash color
    te->setPalette(QtSLiMFlashPalette(te));
    
    // set up timers to flash the color again; we don't worry about being called multiple times,
    // cancelling old timers, etc., because this is so quick that it really doesn't matter;
    // it sorts itself out more quickly than the user can really notice any discrepancy
    QTimer::singleShot(delayMillisec, te, [te]() { te->setPalette(qApp->palette(te)); });
    QTimer::singleShot(delayMillisec * 2, te, [te]() { te->setPalette(QtSLiMFlashPalette(te)); });
    QTimer::singleShot(delayMillisec * 3, te, [te]() { te->setPalette(qApp->palette(te)); });
}






































