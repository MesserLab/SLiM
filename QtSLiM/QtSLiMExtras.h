//
//  QtSLiMExtras.h
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2025 Benjamin C. Haller.  All rights reserved.
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
#include <QString>
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
#include <QPlainTextEdit>
#include <QLabel>

#include <cmath>
#include <algorithm>

#include "eidos_property_signature.h"
#include "eidos_call_signature.h"

class QPaintEvent;

// legend positions for QtSLiMGraphView
typedef enum {
    kUnconfigured = -1,
    kTopLeft = 0,
    kTopRight,
    kBottomLeft,
    kBottomRight
} QtSLiM_LegendPosition;

void QtSLiMMakeWindowVisibleAndExposed(QWidget *window);

void QtSLiMClearLayout(QLayout *layout, bool deleteWidgets = true);

void QtSLiMFrameRect(const QRect &p_rect, const QColor &p_color, QPainter &p_painter);
void QtSLiMFrameRect(const QRectF &p_rect, const QColor &p_color, QPainter &p_painter, double p_lineWidth);

QColor QtSLiMColorWithWhite(double p_white, double p_alpha);
QColor QtSLiMColorWithRGB(double p_red, double p_green, double p_blue, double p_alpha);
QColor QtSLiMColorWithHSV(double p_hue, double p_saturation, double p_value, double p_alpha);

void RGBForFitness(double fitness, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);
void RGBForSelectionCoeff(double selectionCoeff, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);


// Whether we're in "dark mode" for user interface rendering
bool QtSLiMInDarkMode(void);    

// Standard paths for our images, ending in _H for highlighted, and then in _DARK for dark mode icons, and then in .png
QString QtSLiMImagePath(QString baseName, bool highlighted);


// A subclass of QLineEdit that selects all its text when it receives keyboard focus
// It also supports showing a "progress bar" under its text, and it has a modified
// appearance that can be disabled but still show fairly dark text for readability
class QtSLiMGenerationLineEdit : public QLineEdit
{
    Q_OBJECT
    
public:
    QtSLiMGenerationLineEdit(const QString &contents, QWidget *p_parent = nullptr);
    QtSLiMGenerationLineEdit(QWidget *p_parent = nullptr);
    virtual	~QtSLiMGenerationLineEdit() override;
    
    // can optionally display "progress" in the background of the lineedit
    void setProgress(double p_progress);
    
    // set its appearance/behavior; do not use setEnabled(), use this!
    void setAppearance(bool p_enabled, bool p_dimmed);
    
protected:
    virtual void focusInEvent(QFocusEvent *p_event) override;
    virtual void paintEvent(QPaintEvent *p_paintEvent) override;
    
private:
    QtSLiMGenerationLineEdit() = delete;
    QtSLiMGenerationLineEdit(const QtSLiMGenerationLineEdit&) = delete;
    QtSLiMGenerationLineEdit& operator=(const QtSLiMGenerationLineEdit&) = delete;
    
    void _ReconfigureAppearance(void);
    
    double progress = 0.0;
    bool dimmed = false;
};

void ColorizePropertySignature(const EidosPropertySignature *property_signature, double pointSize, QTextCursor lineCursor);
void ColorizeCallSignature(const EidosCallSignature *call_signature, double pointSize, QTextCursor lineCursor);

// A subclass of QHBoxLayout specifically designed to lay out the play controls in the main window
class QtSLiMPlayControlsLayout : public QHBoxLayout
{
    Q_OBJECT
    
public:
    QtSLiMPlayControlsLayout(QWidget *p_parent): QHBoxLayout(p_parent) {}
    QtSLiMPlayControlsLayout(): QHBoxLayout() {}
    virtual ~QtSLiMPlayControlsLayout() override;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSize() const override;
    virtual void setGeometry(const QRect &rect) override;
};

// Heat colors for profiling display
QColor slimColorForFraction(double fraction);

// Nicely formatted memory usage strings
QString stringForByteCount(uint64_t bytes);
QString attributedStringForByteCount(uint64_t bytes, double total, QTextCharFormat &format);

// Nicely formatted dateline for output
QString slimDateline(void);

// Running a panel to obtain numbers from the user
QStringList QtSLiMRunLineEditArrayDialog(QWidget *p_parent, QString title, QStringList captions, QStringList values);

// A subclass of QPushButton that draws its image with antialiasing, for a better appearance; used for the About panel
class QtSLiMIconView : public QPushButton
{
    Q_OBJECT
    
public:
    QtSLiMIconView(const QIcon &p_icon, const QString &p_text, QWidget *p_parent = nullptr) : QPushButton(p_icon, p_text, p_parent) {}
    QtSLiMIconView(const QString &p_text, QWidget *p_parent = nullptr) : QPushButton(p_text, p_parent) {}
    QtSLiMIconView(QWidget *p_parent = nullptr) : QPushButton(p_parent) {}
    virtual ~QtSLiMIconView(void) override {}
    
protected:
    virtual void paintEvent(QPaintEvent *p_paintEvent) override;
};

// A subclass of QPushButton that draws its image with antialiasing, for a better appearance, and handles dark mode appearance
class QtSLiMPushButton : public QPushButton
{
    Q_OBJECT
    
public:
    QtSLiMPushButton(const QIcon &p_icon, const QString &p_text, QWidget *p_parent = nullptr);
    QtSLiMPushButton(const QString &p_text, QWidget *p_parent = nullptr);
    QtSLiMPushButton(QWidget *p_parent = nullptr);
    virtual ~QtSLiMPushButton(void) override;
    
    void qtslimSetBaseName(QString baseName) { qtslimSetIcon(baseName, false); }
    void qtslimSetHighlight(bool highlighted);
    void qtslimSetIcon(QString baseName, bool highlighted);
    
    // An added feature beyond QPushButton: support for a "temporary icon" drawn on top of the
    // normal cached icon, with variable opacity.  This supports the pulsing debug output button.
    void setTemporaryIcon(QIcon tempIcon) { temporaryIcon = tempIcon; update(); }
    void setTemporaryIconOpacity(double opacity) { temporaryIconOpacity = opacity; update(); }
    void clearTemporaryIcon(void) { temporaryIcon = QIcon(); update(); }
    
protected:
    void sharedInit(void);
    virtual bool hitButton(const QPoint &pos) const override;
    virtual void paintEvent(QPaintEvent *p_paintEvent) override;
    
    QString qtslimBaseName;                 // base name, such as "foo"
    bool qtslimHighlighted = false;         // highlighted state (appends _H to the base name)
    
    QIcon *qtslimIcon = nullptr;
    QIcon *qtslimIcon_H = nullptr;
    QIcon *qtslimIcon_DARK = nullptr;
    QIcon *qtslimIcon_H_DARK = nullptr;
    
    QIcon temporaryIcon;
    double temporaryIconOpacity = 0.0;
    
    void qtslimFreeCachedIcons(void);
    QIcon *qtslimIconForState(bool highlighted, bool darkMode);
};

// A subclass of QSplitterHandle that does some custom drawing
class QtSLiMSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
    
public:
    QtSLiMSplitterHandle(Qt::Orientation p_orientation, QSplitter *p_parent) : QSplitterHandle(p_orientation, p_parent) {}
    virtual ~QtSLiMSplitterHandle(void) override {}
    
protected:
    virtual void paintEvent(QPaintEvent *p_paintEvent) override;
};

// A subclass of QSplitter that supplies a custom QSplitterHandle subclass
class QtSLiMSplitter : public QSplitter
{
    Q_OBJECT
    
public:
    QtSLiMSplitter(Qt::Orientation p_orientation, QWidget *p_parent = nullptr) : QSplitter(p_orientation, p_parent) {}
    QtSLiMSplitter(QWidget *p_parent = nullptr) : QSplitter(p_parent) {}
    virtual ~QtSLiMSplitter(void) override {}
    
protected:
    virtual QSplitterHandle *createHandle(void) override { return new QtSLiMSplitterHandle(orientation(), this); }
};

// A subclass of QStatusBar that draws a top separator on Linux, so our splitters abut nicely
class QtSLiMStatusBar : public QStatusBar
{
    Q_OBJECT
    
public:
    QtSLiMStatusBar(QWidget *p_parent = nullptr);
    virtual ~QtSLiMStatusBar(void) override {}
    
protected:
    virtual void paintEvent(QPaintEvent *p_paintEvent) override;
    virtual void resizeEvent(QResizeEvent *p_resizeEvent) override;
    void setHeightFromContent(void);
};

// Used to create the dark app icon displayed when running a model
QPixmap QtSLiMDarkenPixmap(QPixmap p_pixmap);

void QtSLiMFlashHighlightInTextEdit(QPlainTextEdit *te);

// A QLabel subclass that shows shortened text with an ellipsis; see https://stackoverflow.com/a/73316405/2752221
class QtSLiMEllipsisLabel : public QLabel
{
    Q_OBJECT
    
public:
    explicit QtSLiMEllipsisLabel(QWidget *parent = nullptr);
    explicit QtSLiMEllipsisLabel(QString text, QWidget *parent = nullptr);
    void setText(QString);
    virtual QSize minimumSizeHint() const;
    
signals:
    void pressed(void);
    
protected:
    void resizeEvent(QResizeEvent *p_event);
    void mousePressEvent(QMouseEvent *p_event);
    
private:
    void updateText();
    QString m_text;
};

// Natural sorting (sorting numerically when the first difference is a numeric substring)
bool EidosNaturalSort(QString &a, QString &b);


// Incremental sorting
//
// This is from https://github.com/KukyNekoi/magicode by Erik Regla, released under the GPL 3.
// The algorithms involved are described in Paredes & Navarro (2006) "Optimal Incremental
// Sorting" and Regla & Paredes (2015) "Worst-case Optimal Incremental Sorting".  Thanks very
// much to Erik Regla for making this code available for use.

#define FIXED_PIVOT_SELECTION 1 // remove to allow random initial pivot selection
#define USE_FAT_PARTITION 1    // use three-way-partitioning
#define USE_ALPHA_LESS_THAN_P30 1    // use three-way-partitioning

template<class T>
class BareBoneIQS {
public:
    BareBoneIQS(T *target_ptr, std::size_t target_size);
    ~BareBoneIQS();
    inline void swap(std::size_t lhs, std::size_t rhs);
    inline std::size_t partition(T pivot_value, std::size_t lhs, std::size_t rhs);
    std::size_t partition_redundant(T pivot_value, std::size_t lhs, std::size_t rhs);
    inline std::size_t stack_pop();
    inline std::size_t stack_peek();
    inline void stack_push(std::size_t value);
    virtual T next();

protected:
    /**
     * In this example we used a stack which is the same length of the array. This is only for
     * testing purposes and can be changed into a proper stack later on if desired
     *
     */
    std::size_t *stack;
    std::size_t stack_length;

    std::size_t target_size;
    std::size_t extracted_count;
    T *target_ptr;

};

template <class T>
class BareBoneIIQS : public BareBoneIQS<T> {
public:
    BareBoneIIQS(T *p_target_ptr, std::size_t p_target_size);
    ~BareBoneIIQS();
    T next();
    std::size_t bfprt(std::size_t lhs, std::size_t rhs, std::size_t median_length);
    std::size_t median(std::size_t lhs, std::size_t rhs);
};

/* This constructor allows in-place ordering */
template <class T>
BareBoneIQS<T>::BareBoneIQS(T *p_target_ptr, std::size_t p_target_size){
    this->target_ptr = p_target_ptr;
    this->target_size = p_target_size;

    this->stack = (std::size_t *) std::malloc(p_target_size * sizeof(std::size_t)) ;
    this->stack[0] = p_target_size - 1; // index of the last element
    this->stack_length = 1; //starts with a single element, the top

    this->extracted_count = 0; // this way, after adding +1, we can partition as whole
}

template <class T>
BareBoneIQS<T>::~BareBoneIQS() {
    std::free(this->stack);
}

/**
 * @brief Swaps two elements in the referenced array
 *
 * @param lhs left index to be swapped
 * @param rhs right index to be swapped
 */
template <class T>
inline void BareBoneIQS<T>::swap(std::size_t lhs, std::size_t rhs){
    T _temp_val = this->target_ptr[lhs];
    this->target_ptr[lhs] = this->target_ptr[rhs];
    this->target_ptr[rhs] = _temp_val;
}

/**
 * @brief Implementation of Hoare's partition algorithm. Can be found on
 * Cormen's "Introduction to algorithms - 2nd edition" p146
 * This implementation is not resistant to the case on which the elements are repeated.
 *
 * @param pivot_value the pivot value to use
 * @param lhs the left boundary for partition algorithm (inclusive)
 * @param rhs the right boundary for partition algorithm (inclusive)
 * @return std::size_t the index on which the partition value belongs
 */
template <class T>
inline std::size_t BareBoneIQS<T>::partition(T pivot_value, std::size_t lhs, std::size_t rhs){
    if(lhs == rhs) return lhs;
    lhs--;
    rhs++;

    while(1){
        do{
            lhs++;
        } while(this->target_ptr[lhs] < pivot_value);
        do{
            rhs--;
        } while(pivot_value < this->target_ptr[rhs]);
        if (lhs >= rhs) return rhs;
        this->swap(lhs, rhs);
    }
}

/**
 * Modified version of hoare's algorithm intended to be resistant to redundant elements along the
 * partition. This scheme is also known as three-way partitioning. Make sure to select the forcing pivot
 * scheme that matches your problem accordingly
 * @param pivot_value the pivot value to use
 * @param lhs the left boundary for partition algorithm (inclusive)
 * @param rhs the right boundary for partition algorithm (inclusive)
 * @return std::size_t the index on which the partition value belongs
 */
template<class T>
inline std::size_t BareBoneIQS<T>::partition_redundant(T pivot_value, std::size_t lhs, std::size_t rhs) {
    std::size_t i = lhs - 1;
    std::size_t k = rhs + 1;
    while (1) {
        while (this->target_ptr[++i] < pivot_value);
        while (this->target_ptr[--k] > pivot_value);
        if (i >= k) break;
        this->swap(i, k);
    }
    i = k++;
    while(i > lhs && this->target_ptr[i] == pivot_value) i--;
    while(k < rhs && this->target_ptr[k] == pivot_value) k++;

    #ifdef FORCE_PIVOT_SELECTION_LEFT
        return i; // return left pivot
    #elif FORCE_PIVOT_SELECTION_RIGHT
        return k; // return left pivot
    #else
        return (i + k) / 2; // if there is a group, then return the middle element to guarantee a position
    #endif
}

/**
 * @brief Pops the last element on the stack
 * @return std::size_t element at the top of the stack
 */
template<class T>
inline std::size_t BareBoneIQS<T>::stack_pop(){
    return this->stack[--this->stack_length];
}

/**
 * @brief Peeks the last element on the stack
 * @return std::size_t element at the top of the stack
 */
template<class T>
inline std::size_t BareBoneIQS<T>::stack_peek(){
    return this->stack[this->stack_length-1];
}

/**
 * @brief Inserts an element on the top of the stack
 * @param value the element to insert
 */
template<class T>
inline void BareBoneIQS<T>::stack_push(std::size_t value){
    this->stack[this->stack_length] = value;
    this->stack_length++;
}

/**
 * @brief Retrieves the next sorted element. The basic idea is to
 * use quick select to find the smallest element, but store the pivots along the
 * way in order to shorten future calculations.
 *
 * @return T the next sorted element
 */
template<class T>
T BareBoneIQS<T>::next() {
    // This for allows the tail recursion
    while(1){
        // Base condition. If the element referenced by the top of the stack
        // is the element that we're actually searching, then retrieve it and
        // resize the search window
        if (this->extracted_count == this->stack_peek()){
            this->extracted_count++;
            return this->target_ptr[this->stack_pop()];
        }

        // Selects a random pivot from the remaining array

        #ifdef FIXED_PIVOT_SELECTION
            std::size_t pivot_idx = this->extracted_count;
        #else
            std::size_t rand_range = this->stack_peek() - this->extracted_count;
            std::size_t pivot_idx = this->extracted_count + (rand() % rand_range);
        #endif
        T pivot_value = this->target_ptr[pivot_idx];

        // pivot partition and indexing
        #ifdef USE_FAT_PARTITION
                pivot_idx = this->partition_redundant(pivot_value, this->extracted_count, this->stack_peek());
        #else
                pivot_idx = this->partition(pivot_value, this->extracted_count, this->stack_peek());
        #endif

        // Push and recurse the loop
        this->stack_push(pivot_idx);
    }
}

/* This constructor allows in-place ordering */
template <class T>
BareBoneIIQS<T>::BareBoneIIQS(T *p_target_ptr, std::size_t p_target_size): BareBoneIQS<T>(p_target_ptr, p_target_size){
}
template <class T>
BareBoneIIQS<T>::~BareBoneIIQS() {
}

/**
 *
 * @brief Retrieves the next sorted element. The basic idea is to
 * use quick select to find the smallest element, but store the pivots along the
 * way in order to shortent future calculations.
 *
 * @tparam T The template class/type to use
 * @return T the next sorted element
 */
template<class T>
T BareBoneIIQS<T>::next() {
    while(1){
        // Base condition. If the element referenced by the top of the stack
        // is the element that we're actually searching, then retrieve it and
        // resize the search window
        std::size_t top_element = this->stack_peek();
        std::size_t range = top_element - this->extracted_count;
        std::size_t p70_idx = (std::size_t)std::ceil(range * 0.7);


        if (this->extracted_count == top_element ){
            this->extracted_count++;
            return this->target_ptr[this->stack_pop()];
        }

        #ifdef FIXED_PIVOT_SELECTION
            std::size_t pivot_idx = this->extracted_count;
        #else
            std::size_t rand_range = this->stack_peek() - this->extracted_count;
            std::size_t pivot_idx = this->extracted_count + (rand() % rand_range);
        #endif

        T pivot_value = this->target_ptr[pivot_idx];

        // pivot partition and indexing
        #ifdef USE_FAT_PARTITION
            pivot_idx = this->partition_redundant(pivot_value, this->extracted_count, top_element);
        #else
            pivot_idx = this->partition(pivot_value, this->extracted_count, top_element);
        #endif

        #ifdef REUSE_PIVOTS
            std::size_t previous_pivot_idx = pivot_idx;
        #endif

        // IIQS changes start! only check if range is less than the square root of the total size
        // First, we need to check if this pointer belongs P70 \union P30
        #ifdef USE_ALPHA_LESS_THAN_P30
            std::size_t p30_idx = (std::size_t)std::ceil(range * 0.3); // actually, if we don't care about balancing the stack, you can ignore the p30 condition
            if (p30_idx > pivot_idx || pivot_idx > p70_idx){
        #else
            if (pivot_idx > p70_idx){
        #endif
            // if we enter here, then it's because the index needs to be recomputed.
            // So, we ditch the index and get a nice approximate median median and reuse previous computation
            pivot_idx = this->bfprt(this->extracted_count, top_element, 5);
            pivot_value = this->target_ptr[pivot_idx];
            // then we re-partition, assuming that this median is better
            #ifdef USE_FAT_PARTITION
                pivot_idx = this->partition_redundant(pivot_value, this->extracted_count, top_element);
            #else
                pivot_idx = this->partition(pivot_value, this->extracted_count, top_element);
            #endif
        }

        // I need to see later how it does affect the stack this segment.
        #ifdef REUSE_PIVOTS
            if(previous_pivot_idx < pivot_idx){
                this->stack_push(pivot_idx);
                this->stack_push(previous_pivot_idx);
                continue;
            }
            else if(previous_pivot_idx > pivot_idx){
                this->stack_push(previous_pivot_idx);
                this->stack_push(pivot_idx);
                continue;
            }
        #endif
        // Push and recurse the loop
        this->stack_push(pivot_idx);
    }
}


/**
 * In-place implementation of bfprt. Instead of the classical implementation when auxiliary structures are used
 * this implementation forces two phenomena on the array which both are beneficial to IQS. First, given that we force
 * the selection of the first index, elements near the beginning have a high chance of being good pivots. Second, we
 * don't use extra memory to allocate those median results.
 * @tparam T The template class/type to use
 * @param lhs the left index to sort (inclusive)
 * @param rhs the right index to sort (inclusive)
 * @param median_length size of the median to use on bfprt, 5 is commonly used
 * @return the median value
 */
template<class T>
inline std::size_t BareBoneIIQS<T>::bfprt(std::size_t lhs, std::size_t rhs, std::size_t median_length) {
    std::size_t base_lhs = lhs;
    std::size_t medians_extracted = 0;

    while(1){
        // reset base conditions
        lhs = base_lhs;

        // check base case
        if( rhs <= base_lhs + median_length) {
            return this->median(base_lhs, rhs);
        }

        // tail recursion step for bfprt
        while(lhs + median_length <= rhs){
            std::size_t median_index = this->median(lhs, lhs + median_length);
            //move median to the start of the array
            this->swap(median_index, base_lhs + medians_extracted);
            // search for next stride
            lhs += median_length;
            medians_extracted++;
        }
        rhs = medians_extracted + base_lhs;
        medians_extracted = 0;
    }
}

/**
 * Median selection via quickselect. We can assume that this process is constant, as it is being always executed
 * with 5 elements (by default, you can change this later)
 *
 * @tparam T  T The template class/type to use
 * @param lhs the left boundary for median algorithm (inclusive)
 * @param rhs the right boundary for median algorithm (inclusive)
 * @return the median index
 */
template<class T>
inline std::size_t BareBoneIIQS<T>::median(std::size_t lhs, std::size_t rhs) {
    std::sort(this->target_ptr + lhs, this->target_ptr + rhs);
    return (lhs + rhs) / 2;
    /* implement heapsort later as it is more cache-friendly for small arrays, I'm too drunk now */
    /* explanation: due to how heapsort is implemented, it scatters in-memory operations, that's
     * on how the tree is represented on the array (the 2k +1 thing), so if you "recurse" long
     * enough (namely, you're searching for an element on which you need to trash the cache or even
     * worse, you lose the dram-bursting) then it gets its performance degraded.
     *
     * But since on median finding of a fixed set of elements it's small enough to fit on the cache
     * and to get dram-bursting benefits, it works better than other sorting algorithms in practice.
     */
}

//
// Incremental sorting ENDS
//


#endif // QTSLIMEXTRAS_H





































