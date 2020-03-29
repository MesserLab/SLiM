//
//  QtSLiMExtras.h
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

#ifndef QTSLIMEXTRAS_H
#define QTSLIMEXTRAS_H

#include <QObject>
#include <QWidget>
#include <QColor>
#include <QRect>
#include <QPainter>
#include <QLineEdit>
#include <QTextCursor>
#include <QHBoxLayout>
#include <QList>

#include "eidos_property_signature.h"
#include "eidos_call_signature.h"


void QtSLiMFrameRect(const QRect &p_rect, const QColor &p_color, QPainter &p_painter);

QColor QtSLiMColorWithWhite(double p_white, double p_alpha);
QColor QtSLiMColorWithRGB(double p_red, double p_green, double p_blue, double p_alpha);
QColor QtSLiMColorWithHSV(double p_hue, double p_saturation, double p_value, double p_alpha);

void RGBForFitness(double fitness, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);
void RGBForSelectionCoeff(double selectionCoeff, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);

// A subclass of QLineEdit that selects all its text when it receives keyboard focus
class QtSLiMGenerationLineEdit : public QLineEdit
{
    Q_OBJECT
    
public:
    QtSLiMGenerationLineEdit(const QString &contents, QWidget *parent = nullptr);
    QtSLiMGenerationLineEdit(QWidget *parent = nullptr);
    virtual	~QtSLiMGenerationLineEdit() override;
    
protected:
    virtual void focusInEvent(QFocusEvent *event) override;
    
private:
    QtSLiMGenerationLineEdit() = delete;
    QtSLiMGenerationLineEdit(const QtSLiMGenerationLineEdit&) = delete;
    QtSLiMGenerationLineEdit& operator=(const QtSLiMGenerationLineEdit&) = delete;
};

void ColorizePropertySignature(const EidosPropertySignature *property_signature, double pointSize, QTextCursor lineCursor);
void ColorizeCallSignature(const EidosCallSignature *call_signature, double pointSize, QTextCursor lineCursor);

// A subclass of QHBoxLayout specifically designed to lay out the play controls in the main window
class QtSLiMPlayControlsLayout : public QHBoxLayout
{
public:
    QtSLiMPlayControlsLayout(QWidget *parent): QHBoxLayout(parent) {}
    QtSLiMPlayControlsLayout(): QHBoxLayout() {}
    ~QtSLiMPlayControlsLayout() override;

    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
};

// Heat colors for profiling display
QColor slimColorForFraction(double fraction);

// Nicely formatted memory usage strings
QString stringForByteCount(uint64_t bytes);
QString attributedStringForByteCount(uint64_t bytes, double total, QTextCharFormat &format);

// Running a panel to obtain numbers from the user
QStringList QtSLiMRunLineEditArrayDialog(QString title, QStringList captions);


#endif // QTSLIMEXTRAS_H





































