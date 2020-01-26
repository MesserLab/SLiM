//
//  QtSLiMFindRecipe.h
//  SLiM
//
//  Created by Ben Haller on 8/6/2019.
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







































