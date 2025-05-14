//
//  QtSLiMOpenGL_Emulation.h
//  SLiM
//
//  Created by Ben Haller on 8/25/2024.
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

#ifndef QTSLIMOPENGL_EMULATION_H
#define QTSLIMOPENGL_EMULATION_H


/*
 * This header defines utility macros and functions for drawing with OpenGL, but they are emulated with Qt.
 * This should be included only locally within Qt-specific rendering code.  See also QtSLiMOpenGL.h.
 */


#define SLIM_GL_PREPARE()

#define SLIM_GL_DEFCOORDS(rect)                                 \
    QRect &RECT_TO_DRAW = rect;

#define SLIM_GL_PUSHRECT()

#define SLIM_GL_PUSHRECT_COLORS()

#define SLIM_GL_CHECKBUFFERS()                                                  \
    {                                                                           \
        QColor COLOR_TO_DRAW;                                                   \
        COLOR_TO_DRAW.setRgbF(colorRed, colorGreen, colorBlue, colorAlpha);     \
        painter.fillRect(RECT_TO_DRAW, COLOR_TO_DRAW);                          \
    }

#define SLIM_GL_CHECKBUFFERS_NORECT()                                           \
{                                                                               \
        QColor COLOR_TO_DRAW;                                                   \
        COLOR_TO_DRAW.setRgbF(colorRed, colorGreen, colorBlue, colorAlpha);     \
        QRect RECT_TO_DRAW(left, top, right-left, bottom-top);                  \
        painter.fillRect(RECT_TO_DRAW, COLOR_TO_DRAW);                          \
}

#define SLIM_GL_FINISH()


#endif // QTSLIMOPENGL_EMULATION_H




















