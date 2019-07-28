#include "QtSLiMExtras.h"

#include <cmath>


void QtSLiMFrameRect(const QRect &p_rect, const QColor &p_color, QPainter &p_painter)
{
    p_painter.fillRect(QRect(p_rect.left(), p_rect.top(), p_rect.width(), 1), p_color);                 // top edge
    p_painter.fillRect(QRect(p_rect.left(), p_rect.top() + 1, 1, p_rect.height() - 2), p_color);        // left edge (without corner pixels)
    p_painter.fillRect(QRect(p_rect.right(), p_rect.top() + 1, 1, p_rect.height() - 2), p_color);       // right edge (without corner pixels)
    p_painter.fillRect(QRect(p_rect.left(), p_rect.bottom(), p_rect.width(), 1), p_color);              // bottom edge
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

const float greenBrightness = 0.8f;

void RGBForFitness(double value, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor)
{
	// apply the scaling factor
	value = (value - 1.0) * scalingFactor + 1.0;
	
	if (value <= 0.5)
	{
		// value <= 0.5 is a shade of red, going down to black
		*colorRed = (float)(value * 2.0);
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value >= 2.0)
	{
		// value >= 2.0 is a shade of green, going up to white
		*colorRed = (float)((value - 2.0) * greenBrightness / value);
		*colorGreen = greenBrightness;
		*colorBlue = (float)((value - 2.0) * greenBrightness / value);
	}
	else if (value <= 1.0)
	{
		// value <= 1.0 (but > 0.5) goes from red (unfit) to yellow (neutral)
		*colorRed = 1.0;
		*colorGreen = (float)((value - 0.5) * 2.0);
		*colorBlue = 0.0;
	}
	else	// 1.0 < value < 2.0
	{
		// value > 1.0 (but < 2.0) goes from yellow (neutral) to green (fit)
		*colorRed = (float)(2.0 - value);
		*colorGreen = (float)(greenBrightness + (1.0 - greenBrightness) * (2.0 - value));
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
		*colorRed = (float)(value + 0.5);
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value < 1.0)
	{
		// value <= 1.0 (but > 0.5) goes from red (very unfit) to orange (nearly neutral)
		*colorRed = 1.0;
		*colorGreen = (float)((value - 0.5) * 1.0);
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
		*colorGreen = greenBrightness;
		*colorBlue = (float)((value - 1.0) * 2.0);
	}
	else if (value <= 2.0)
	{
		// value > 1.5 (but < 2.0) goes from cyan (fit) to blue (very fit)
		*colorRed = 0.0;
		*colorGreen = (float)(greenBrightness * ((2.0 - value) * 2.0));
		*colorBlue = 1.0;
	}
	else // (value > 2.0)
	{
		// value > 2.0 is a shade of blue, going up toward white
		*colorRed = (float)((value - 2.0) * 0.75 / value);
		*colorGreen = (float)((value - 2.0) * 0.75 / value);
		*colorBlue = 1.0;
	}
}











































