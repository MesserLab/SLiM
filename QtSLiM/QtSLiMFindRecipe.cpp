//
//  QtSLiMFindRecipe.cpp
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


#include "QtSLiMFindRecipe.h"
#include "ui_QtSLiMFindRecipe.h"

#include "QtSLiMPreferences.h"
#include "QtSLiMSyntaxHighlighting.h"
#include "QtSLiMAppDelegate.h"

#include <QDir>
#include <QCollator>
#include <QTextStream>
#include <QDebug>


QtSLiMFindRecipe::QtSLiMFindRecipe(QWidget *parent) : QDialog(parent), ui(new Ui::QtSLiMFindRecipe)
{
    ui->setupUi(this);
    
    // change the app icon to our multi-size app icon for best results
    ui->iconSLiM->setIcon(qtSLiMAppDelegate->applicationIcon());
    
    // load recipes and get ready to search
    loadRecipes();
	constructMatchList();
    updateMatchListWidget();
	
	validateOK();
    updatePreview();
	
    // set up the script preview with syntax coloring and tab stops
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    int tabWidth = 0;
    
    ui->scriptPreviewTextEdit->setFont(prefs.displayFontPref(&tabWidth));
    ui->scriptPreviewTextEdit->setTabStopWidth(tabWidth);                   // deprecated in 5.10
    
    if (prefs.scriptSyntaxHighlightPref())
        new QtSLiMScriptHighlighter(ui->scriptPreviewTextEdit->document());
    
    // wire things up
    connect(ui->keyword1LineEdit, &QLineEdit::textChanged, this, &QtSLiMFindRecipe::keywordChanged);
    connect(ui->keyword2LineEdit, &QLineEdit::textChanged, this, &QtSLiMFindRecipe::keywordChanged);
    connect(ui->keyword3LineEdit, &QLineEdit::textChanged, this, &QtSLiMFindRecipe::keywordChanged);
    
    connect(ui->matchListWidget, &QListWidget::itemSelectionChanged, this, &QtSLiMFindRecipe::matchListSelectionChanged);
    connect(ui->matchListWidget, &QListWidget::itemDoubleClicked, this, &QtSLiMFindRecipe::matchListDoubleClicked);
}

QtSLiMFindRecipe::~QtSLiMFindRecipe()
{
    delete ui;
}

QString QtSLiMFindRecipe::selectedRecipeFilename(void)
{
    const QList<QListWidgetItem *> &selectedItems = ui->matchListWidget->selectedItems();
    
    if (selectedItems.size() == 1)
        return selectedItems[0]->text();
    
    // We should always have a selection when this is called, actually
    return QString("");
}

QString QtSLiMFindRecipe::selectedRecipeScript(void)
{
    return ui->scriptPreviewTextEdit->toPlainText();
}

void QtSLiMFindRecipe::loadRecipes(void)
{
    QDir recipesDir(":/recipes/", "Recipe *.*", QDir::NoSort, QDir::Files | QDir::NoSymLinks);
    QStringList entryList = recipesDir.entryList(QStringList("Recipe *.*"));   // the previous name filter seems to be ignored
    QCollator collator;
    
    collator.setNumericMode(true);
    std::sort(entryList.begin(), entryList.end(), collator);
    
    recipeFilenames = entryList;
    matchRecipeFilenames = entryList;
    
    for (QString &filename : recipeFilenames)
    {
        QString filePath = ":/recipes/" + filename;
        QFile recipeFile(filePath);
        
        if (recipeFile.open(QFile::ReadOnly | QFile::Text))
        {
            QTextStream fileTextStream(&recipeFile);
            QString fileContents = fileTextStream.readAll();
            
            recipeContents.push_back(fileContents);
        }
        else
        {
            recipeContents.push_back("### An error occurred reading the contents of this recipe");
        }
    }
}

QString QtSLiMFindRecipe::displayStringForRecipeFilename(const QString &name)
{
    if (name.endsWith(".txt"))
    {
        // Remove the .txt extension for SLiM models
        return name.mid(7, name.length() - 11);
    }
    else if (name.endsWith(".py"))
    {
        // Leave the .py extension for Python models, and add a python
        // FIXME it would be nice to force these lines to have the same line height, but I can't find a way to do so
        return name.mid(7, name.length() - 7) + QString(" 🐍");
    }
    
    return "";
}

bool QtSLiMFindRecipe::recipeIndexMatchesKeyword(int recipeIndex, QString &keyword)
{
	// an empty keyword matches all recipes
    if (keyword.length() == 0)
        return true;
	
	// look for a match in the filename
    const QString &filename = recipeFilenames[recipeIndex];
	
    if (filename.contains(keyword, Qt::CaseInsensitive))
		return true;
	
	// look for a match in the file contents
    const QString &contents = recipeContents[recipeIndex];
	
    if (contents.contains(keyword, Qt::CaseInsensitive))
		return true;
	
	return false;
}

void QtSLiMFindRecipe::constructMatchList(void)
{
    matchRecipeFilenames.clear();
    
    QString keyword1 = ui->keyword1LineEdit->text();
    QString keyword2 = ui->keyword2LineEdit->text();
    QString keyword3 = ui->keyword3LineEdit->text();
    
    for (int i = 0; i < recipeFilenames.size(); ++i)
    {
        if (recipeIndexMatchesKeyword(i, keyword1) && recipeIndexMatchesKeyword(i, keyword2) && recipeIndexMatchesKeyword(i, keyword3))
            matchRecipeFilenames.append(recipeFilenames[i]);
    }
}

void QtSLiMFindRecipe::updateMatchListWidget(void)
{
    QListWidget *matchList = ui->matchListWidget;
    
    matchList->clear();
    
    for (const QString &match : matchRecipeFilenames)
        matchList->addItem(displayStringForRecipeFilename(match));
}

void QtSLiMFindRecipe::validateOK(void)
{
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    
    okButton->setEnabled(ui->matchListWidget->selectedItems().size() > 0);
}

void QtSLiMFindRecipe::updatePreview(void)
{
	if (ui->matchListWidget->selectedItems().size() == 0)
	{
        ui->scriptPreviewTextEdit->clear();
	}
	else
	{
        int selectedRowIndex = ui->matchListWidget->currentRow();
        QString &filename = matchRecipeFilenames[selectedRowIndex];
        
        QString filePath = ":/recipes/" + filename;
        QFile recipeFile(filePath);
        
        if (recipeFile.open(QFile::ReadOnly | QFile::Text))
        {
            QTextStream fileTextStream(&recipeFile);
            QString fileContents = fileTextStream.readAll();
            
            ui->scriptPreviewTextEdit->setPlainText(fileContents);
        }
        else
        {
            ui->scriptPreviewTextEdit->setPlainText("### An error occurred reading the contents of this recipe");
        }
        
		highlightPreview();
	}
}

void QtSLiMFindRecipe::highlightPreview(void)
{
    // thanks to https://stackoverflow.com/a/16149381/2752221
    QTextEdit *script = ui->scriptPreviewTextEdit;
    const QString &scriptString = script->toPlainText();
    QTextCursor tc = script->textCursor();
    tc.select(QTextCursor::Document);
    
    QString keyword1 = ui->keyword1LineEdit->text();
    QString keyword2 = ui->keyword2LineEdit->text();
    QString keyword3 = ui->keyword3LineEdit->text();
    std::vector<QString> keywords{keyword1, keyword2, keyword3};
    
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextCharFormat format;
    
    format.setBackground(Qt::yellow);
    
    for (QString keyword : keywords)
    {
        if (keyword.length() == 0)
            continue;
        
        int from = 0;
        
        while (true)
        {
            int matchPos = scriptString.indexOf(keyword, from, Qt::CaseInsensitive);
            
            if (matchPos == -1)
                break;
            
            QTextEdit::ExtraSelection sel;
            sel.format = format;
            sel.cursor = tc;                    // this makes the cursor refer to the document, I think
            sel.cursor.clearSelection();        // this seems unnecessary to me, but I'm not certain
            sel.cursor.setPosition(matchPos);
            sel.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, keyword.length());
            extraSelections.append(sel);
            
            from = matchPos + 1;
        }
    }
    
    script->setExtraSelections(extraSelections);
}

void QtSLiMFindRecipe::keywordChanged()
{
    // FIXME would be nice to preserve the selection across this; see -[FindRecipeController keywordChanged:]
    constructMatchList();
    updateMatchListWidget();
	validateOK();
    highlightPreview();
}

void QtSLiMFindRecipe::matchListSelectionChanged()
{
    validateOK();
    updatePreview();
}

void QtSLiMFindRecipe::matchListDoubleClicked()
{
    if (ui->matchListWidget->selectedItems().size() > 0)
        done(QDialog::Accepted);
}































