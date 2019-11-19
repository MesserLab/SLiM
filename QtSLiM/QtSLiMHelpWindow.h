#ifndef QTSLIMHELPWINDOW_H
#define QTSLIMHELPWINDOW_H

#include <QDialog>

#include "eidos_call_signature.h"

class QCloseEvent;
class EidosPropertySignature;
class EidosObjectClass;


// This class provides a singleton help window

namespace Ui {
class QtSLiMHelpWindow;
}

class QtSLiMHelpWindow : public QDialog
{
    Q_OBJECT
    
public:
    static QtSLiMHelpWindow &instance(void);
    
    void enterSearchForString(QString &searchString, bool titlesOnly = true);
    
    
private:
    // singleton pattern
    explicit QtSLiMHelpWindow(QWidget *parent = nullptr);
    QtSLiMHelpWindow() = default;
    virtual ~QtSLiMHelpWindow() override;
    
    // Add topics and items drawn from a specially formatted RTF file, under a designated top-level heading.
    // The signatures found for functions, methods, and prototypes will be checked against the supplied lists,
    // if they are not NULL, and if matches are found, colorized versions will be substituted.
    void addTopicsFromRTFFile(const QString &rtfFile,
                              const QString &topLevelHeading,
                              const std::vector<EidosFunctionSignature_SP> *functionList,
                              const std::vector<const EidosMethodSignature*> *methodList,
                              const std::vector<const EidosPropertySignature*> *propertyList);
    
    const std::vector<const EidosPropertySignature*> *slimguiAllPropertySignatures(void);
    const std::vector<const EidosMethodSignature*> *slimguiAllMethodSignatures(void);
    
    // Check for complete documentation
    void checkDocumentationOfFunctions(const std::vector<EidosFunctionSignature_SP> *functions);
    void checkDocumentationOfClass(EidosObjectClass *classObject);
    void checkDocumentationForDuplicatePointers(void);
    
    void searchChanged(void);
    void closeEvent(QCloseEvent *e) override;
    
    
private:
    Ui::QtSLiMHelpWindow *ui;
};


#endif // QTSLIMHELPWINDOW_H
