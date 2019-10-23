#ifndef QTSLIMEXTRAS_H
#define QTSLIMEXTRAS_H

#include <QObject>
#include <QWidget>
#include <QColor>
#include <QRect>
#include <QPainter>
#include <QLineEdit>


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
        

#endif // QTSLIMEXTRAS_H





































