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
    int white_value = static_cast<int>(std::round(p_white * 255.0));
    int alpha_value = static_cast<int>(std::round(p_alpha * 255.0));
    
    return QColor(white_value, white_value, white_value, alpha_value);
}

QColor QtSLiMColorWithRGB(double p_red, double p_green, double p_blue, double p_alpha)
{
    int red_value = static_cast<int>(std::round(p_red * 255.0));
    int green_value = static_cast<int>(std::round(p_green * 255.0));
    int blue_value = static_cast<int>(std::round(p_blue * 255.0));
    int alpha_value = static_cast<int>(std::round(p_alpha * 255.0));
    
    return QColor(red_value, green_value, blue_value, alpha_value);
}












































