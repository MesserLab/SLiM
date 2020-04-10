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
#include <QPushButton>
#include <QList>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStatusBar>

#include "eidos_property_signature.h"
#include "eidos_call_signature.h"

class QPaintEvent;


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
    Q_OBJECT
    
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
QStringList QtSLiMRunLineEditArrayDialog(QWidget *parent, QString title, QStringList captions, QStringList values);

// A subclass of QPushButton that draws its image with antialiasing, for a better appearance
class QtSLiMPushButton : public QPushButton
{
    Q_OBJECT
    
public:
    QtSLiMPushButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr) : QPushButton(icon, text, parent) {}
    QtSLiMPushButton(const QString &text, QWidget *parent = nullptr) : QPushButton(text, parent) {}
    QtSLiMPushButton(QWidget *parent = nullptr) : QPushButton(parent) {}
    ~QtSLiMPushButton(void) override {}
    
protected:
    void paintEvent(QPaintEvent *paintEvent) override;
};

// A subclass of QSplitterHandle that does some custom drawing
class QtSLiMSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
    
public:
    QtSLiMSplitterHandle(Qt::Orientation orientation, QSplitter *parent) : QSplitterHandle(orientation, parent) {}
    ~QtSLiMSplitterHandle(void) override {}
    
protected:
    void paintEvent(QPaintEvent *paintEvent) override;
};

// A subclass of QSplitter that supplies a custom QSplitterHandle subclass
class QtSLiMSplitter : public QSplitter
{
    Q_OBJECT
    
public:
    QtSLiMSplitter(Qt::Orientation orientation, QWidget *parent = nullptr) : QSplitter(orientation, parent) {}
    QtSLiMSplitter(QWidget *parent = nullptr) : QSplitter(parent) {}
    ~QtSLiMSplitter(void) override {}
    
protected:
    QSplitterHandle *createHandle(void) override { return new QtSLiMSplitterHandle(orientation(), this); }
};

// A subclass of QStatusBar that draws a top separator on Linux, so our splitters abut nicely
class QtSLiMStatusBar : public QStatusBar
{
    Q_OBJECT
    
public:
    QtSLiMStatusBar(QWidget *parent = nullptr) : QStatusBar(parent) {}
    ~QtSLiMStatusBar(void) override {}
    
protected:
    void paintEvent(QPaintEvent *paintEvent) override;
};

#endif // QTSLIMEXTRAS_H





































