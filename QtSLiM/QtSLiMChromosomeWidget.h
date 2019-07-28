#ifndef QTSLIMCHROMOSOMEWIDGET_H
#define QTSLIMCHROMOSOMEWIDGET_H

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include "slim_globals.h"


class QtSLiMWindow;
class QPainter;

struct QtSLiMRange
{
    int64_t location, length;
    
    explicit QtSLiMRange(int64_t p_location, int64_t p_length) : location(p_location), length(p_length) {}
};

class QtSLiMChromosomeWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT    
    
    bool selectable_;
    QtSLiMChromosomeWidget *referenceChromosomeView_;
    
    bool shouldDrawGenomicElements_;
    bool shouldDrawRateMaps_;
    bool shouldDrawMutations_;
    bool shouldDrawFixedSubstitutions_;
    
    // Selection
	bool hasSelection_;
	slim_position_t selectionFirstBase_, selectionLastBase_;
	
	// Selection memory – saved and restored across events like recycles
	bool savedHasSelection_;
	slim_position_t savedSelectionFirstBase_, savedSelectionLastBase_;
	
    // OpenGL buffers
	float *glArrayVertices;
	float *glArrayColors;
    
public:
    explicit QtSLiMChromosomeWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMChromosomeWidget() override;
    
    inline void setSelectable(bool p_flag) { selectable_ = p_flag; }
    inline void setReferenceChromosomeView(QtSLiMChromosomeWidget *p_ref_widget) { referenceChromosomeView_ = p_ref_widget; }
    
    inline void setShouldDrawGenomicElements(bool p_flag) { shouldDrawGenomicElements_ = p_flag; }
    inline void setShouldDrawRateMaps(bool p_flag) { shouldDrawRateMaps_ = p_flag; }
    inline void setShouldDrawMutations(bool p_flag) { shouldDrawMutations_ = p_flag; }
    inline void setShouldDrawFixedSubstitutions(bool p_flag) { shouldDrawFixedSubstitutions_ = p_flag; }
    
    QtSLiMRange getSelectedRange(void);
    void setSelectedRange(QtSLiMRange p_selectionRange);
    void restoreLastSelection(void);
    
    QtSLiMRange getDisplayedRange(void);
    
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    QRect rectEncompassingBaseToBase(slim_position_t startBase, slim_position_t endBase, QRect interiorRect, QtSLiMRange displayedRange);
    QRect getContentRect(void);
    QRect getInteriorRect(void);
    
    void drawTicksInContentRect(QRect contentRect, QtSLiMWindow *controller, QtSLiMRange displayedRange, QPainter &painter);
    void glDrawRect(void);
};

#endif // QTSLIMCHROMOSOMEWIDGET_H
















