//
//  QtSLiMExtras.cpp
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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
#include <QDebug>
#include <cmath>

#include "QtSLiMPreferences.h"

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
QtSLiMGenerationLineEdit::QtSLiMGenerationLineEdit(const QString &contents, QWidget *parent) : QLineEdit(contents, parent) { }
QtSLiMGenerationLineEdit::QtSLiMGenerationLineEdit(QWidget *parent) : QLineEdit(parent) { }
QtSLiMGenerationLineEdit::~QtSLiMGenerationLineEdit() {}

void QtSLiMGenerationLineEdit::focusInEvent(QFocusEvent *event)
{
    // First let the base class process the event
    QLineEdit::focusInEvent(event);
    
    // Then select the text by a single shot timer, so that everything will
    // be processed before (calling selectAll() directly won't work)
    QTimer::singleShot(0, this, &QLineEdit::selectAll);
}

void ColorizePropertySignature(const EidosPropertySignature *property_signature, double pointSize, QTextCursor lineCursor)
{
    //
    //	Note this logic is paralleled in the function operator<<(ostream &, const EidosPropertySignature &).
    //	These two should be kept in synch so the user-visible format of signatures is consistent.
    //
    QString docSigString = lineCursor.selectedText();
    std::ostringstream ss;
    ss << *property_signature;
    QString propertySigString = QString::fromStdString(ss.str());
    
    if (docSigString != propertySigString)
    {
        qDebug() << "*** property signature mismatch:\nold: " << docSigString << "\nnew: " << propertySigString;
        return;
    }
    
    // the signature conforms to expectations, so we can colorize it
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    QTextCharFormat ttFormat;
    QFont displayFont(prefs.displayFontPref());
    displayFont.setPointSizeF(pointSize);
    ttFormat.setFont(displayFont);
    lineCursor.setCharFormat(ttFormat);
    
    QTextCharFormat functionAttrs(ttFormat), typeAttrs(ttFormat);
    functionAttrs.setForeground(QBrush(QtSLiMColorWithRGB(28/255.0, 0/255.0, 207/255.0, 1.0)));
    typeAttrs.setForeground(QBrush(QtSLiMColorWithRGB(0/255.0, 116/255.0, 0/255.0, 1.0)));
    
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
    
    QTextCharFormat typeAttrs(ttFormat), functionAttrs(ttFormat), paramAttrs(ttFormat);
    typeAttrs.setForeground(QBrush(QtSLiMColorWithRGB(28/255.0, 0/255.0, 207/255.0, 1.0)));
    functionAttrs.setForeground(QBrush(QtSLiMColorWithRGB(0/255.0, 116/255.0, 0/255.0, 1.0)));
    paramAttrs.setForeground(QBrush(QtSLiMColorWithRGB(170/255.0, 13/255.0, 145/255.0, 1.0)));
    
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
            const EidosObjectClass *arg_obj_class = call_signature->arg_classes_[arg_index];
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
                int obj_type_name_len = QString::fromStdString(arg_obj_class->ElementType()).length();
                
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
        QSize sizeHint = layoutItem->sizeHint();
        
        size.rwidth() += sizeHint.width();
        size.rheight() = std::max(size.height(), sizeHint.height());
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
        QSize minimumSize = layoutItem->minimumSize();
        
        size.rwidth() += minimumSize.width();
        size.rheight() = std::max(size.height(), minimumSize.height());
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
        QSize sizeHint = layoutItem->sizeHint();
        QRect geom(position, rect.y(), sizeHint.width(), sizeHint.height());
        
        layoutItem->setGeometry(geom);
        position += sizeHint.width() + spacing();
        
        if (i == 1)
            playButtonRect = geom;
    }
    
    // position the profile button; the button must lie inside the bounds of the parent widget due to clipping
    QLayoutItem *profileButton = itemAt(2);
    QSize sizeHint = profileButton->sizeHint();
    QRect geom(playButtonRect.left() + playButtonRect.width() - 22, rect.y() - 6, sizeHint.width(), sizeHint.height());
    
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
    if (bytes > 512L * 1024L * 1024L * 1024L)
		return QString("%1 TB").arg(bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	else if (bytes > 512L * 1024L * 1024L)
		return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	else if (bytes > 512L * 1024L)
		return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
	else if (bytes > 512L)
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

QStringList QtSLiMRunLineEditArrayDialog(QWidget *parent, QString title, QStringList captions, QStringList values)
{
    if (captions.size() < 1)
        return QStringList();
    if (captions.size() != values.size())
    {
        qDebug("QtSLiMRunLineEditArrayDialog: captions and values are not the same length!");
        return QStringList();
    }
    
    // make the dialog with an overall vertical layout
    QDialog *dialog = new QDialog(parent);
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
        
        for (QLineEdit *lineEdit : lineEdits)
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


// A subclass of QPushButton that draws its image at screen resolution, for a better appearance on Retina etc.
// Turns out setting Qt::AA_UseHighDpiPixmaps fixes that issue; but this subclass also makes the buttons draw
// correctly in Qt 5.14.2, where button icons are shifted right one pixel and then clipped in an ugly way.
void QtSLiMPushButton::paintEvent(QPaintEvent * /* paintEvent */)
{
    QPainter painter(this);
    QRect bounds = rect();
    
    // This uses the icon to draw, which works because of Qt::AA_UseHighDpiPixmaps
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    icon().paint(&painter, bounds, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
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


// A subclass of QSplitterHandle that does some custom drawing
void QtSLiMSplitterHandle::paintEvent(QPaintEvent *paintEvent)
{
    QPainter painter(this);
    QRect bounds = rect();
    
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
    
    painter.fillRect(begin1Strip, QtSLiMColorWithWhite(0.773, 1.0));
    painter.fillRect(begin2Strip, QtSLiMColorWithWhite(1.000, 1.0));
    painter.fillRect(centerStrip, QtSLiMColorWithWhite(0.965, 1.0));
    painter.fillRect(end2Strip, QtSLiMColorWithWhite(0.918, 1.0));
    painter.fillRect(end1Strip, QtSLiMColorWithWhite(0.722, 1.0));
    
    // On Linux, super draws the knob one pixel to the right of where it ought to be, so we draw it ourselves
    // This code is modified from QtSplitterHandle in the Qt 5.14.2 sources (it's identical in Qt 5.9.8)
    // This may turn out to be undesirable, as it assumes that the Linux widget kit is the one I use on Ubuntu
#if !defined(__APPLE__)
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
    QSplitterHandle::paintEvent(paintEvent);
}


// A subclass of QStatusBar that draws a top separator, so our splitters abut nicely
// BCH 9/20/2020: this now draws the message as HTML text too, allowing colorized signatures
void QtSLiMStatusBar::paintEvent(QPaintEvent * /*paintEvent*/)
{
    QPainter p(this);
    QRect bounds = rect();
    
    // fill the interior; we no longer try to inherit this from QStatusBar, that was a headache
    p.fillRect(bounds, QtSLiMColorWithWhite(0.965, 1.0));
    
    // draw the top separator and bevel lines
    QRect bevelLine = bounds.adjusted(0, 0, 0, -(bounds.height() - 1));
    
    p.fillRect(bevelLine, QtSLiMColorWithWhite(0.722, 1.0));
    p.fillRect(bevelLine.adjusted(0, 1, 0, 1), QtSLiMColorWithWhite(1.000, 1.0));
    p.fillRect(bevelLine.adjusted(0, (bounds.height() - 1), 0, (bounds.height() - 1)), QtSLiMColorWithWhite(0.918, 1.0));
    
    // draw the message
    if (!currentMessage().isEmpty())
    {
        // would be nice for these coordinates not to be magic
#ifdef __APPLE__
        p.translate(QPointF(6, 3));
#else
        p.translate(QPointF(5, 1));
#endif

        p.setPen(Qt::black);
        QTextDocument td;
        td.setHtml(currentMessage());
        td.drawContents(&p, bounds);
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







































