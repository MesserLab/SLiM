#ifndef QTSLIMFINDRECIPE_H
#define QTSLIMFINDRECIPE_H

#include <QString>
#include <QStringList>
#include <QDialog>


namespace Ui {
class QtSLiMFindRecipe;
}

class QtSLiMFindRecipe : public QDialog
{
    Q_OBJECT
    
    QStringList recipeFilenames;
    QStringList recipeContents;
    
    QStringList matchRecipeFilenames;
    
public:
    explicit QtSLiMFindRecipe(QWidget *parent = nullptr);
    virtual ~QtSLiMFindRecipe() override;
    
    QString selectedRecipeFilename(void);
    QString selectedRecipeScript(void);
    
protected:
    void loadRecipes(void);
    QString displayStringForRecipeFilename(const QString &name);
    bool recipeIndexMatchesKeyword(int recipeIndex, QString &keyword);
    void constructMatchList(void);
    void updateMatchListWidget(void);
    void validateOK(void);
    void updatePreview(void);
    void highlightPreview(void);
    
protected slots:
    void keywordChanged();
    void matchListSelectionChanged();
    void matchListDoubleClicked();
    
private:
    Ui::QtSLiMFindRecipe *ui;
};


#endif // QTSLIMFINDRECIPE_H







































