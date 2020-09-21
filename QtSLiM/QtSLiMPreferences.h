//
//  QtSLiMPreferences.h
//  SLiM
//
//  Created by Ben Haller on 8/3/2019.
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

#ifndef QTSLIMPREFERENCES_H
#define QTSLIMPREFERENCES_H

#include <QDialog>


// This class provides a singleton object that interested parties can connect to
// This separated design allows clients to connect before the prefs panel exists

class QtSLiMPreferencesNotifier : public QObject
{
    Q_OBJECT
    
public:
    static QtSLiMPreferencesNotifier &instance(void);
    
    // Get the current pref values, falling back on defaults
    int appStartupPref(void) const;               // 0 == do nothing, 1 == create a new window, 2 == run an open panel
    QFont displayFontPref(int *tabWidth = nullptr) const;
    bool scriptSyntaxHighlightPref(void) const;
    bool outputSyntaxHighlightPref(void) const;
    bool showLineNumbersPref(void) const;
    bool highlightCurrentLinePref(void) const;
    bool autosaveOnRecyclePref(void) const;
    bool showSaveIfUntitledPref(void) const;
    
signals:
    // Get notified when a pref value changes
    void appStartupPrefChanged(void);
    void displayFontPrefChanged(void);
    void scriptSyntaxHighlightPrefChanged(void);
    void outputSyntaxHighlightPrefChanged(void);
    void showLineNumbersPrefChanged(void);
    void highlightCurrentLinePrefChanged(void);
    void autosaveOnRecyclePrefChanged(void);
    void showSaveIfUntitledPrefChanged(void);
    
private:
    // singleton pattern
    QtSLiMPreferencesNotifier() = default;
    ~QtSLiMPreferencesNotifier() = default;
    QtSLiMPreferencesNotifier(const QtSLiMPreferencesNotifier&) = delete;
    QtSLiMPreferencesNotifier& operator=(const QtSLiMPreferencesNotifier&) = delete;
    
private slots:
    void startupRadioChanged();
    void fontChanged(const QFont &font);
    void fontSizeChanged(int newSize);
    void syntaxHighlightScriptToggled();
    void syntaxHighlightOutputToggled();
    void showLineNumbersToggled();
    void highlightCurrentLineToggled();
    void autosaveOnRecycleToggled();
    void showSaveIfUntitledToggled();
    void resetSuppressedClicked();
    
    friend class QtSLiMPreferences;
};


// This is the actual UI stuff

namespace Ui {
class QtSLiMPreferences;
}

class QtSLiMPreferences : public QDialog
{
    Q_OBJECT
    
public:
    static QtSLiMPreferences &instance(void);
    
private:
    // singleton pattern
    explicit QtSLiMPreferences(QWidget *parent = nullptr);
    QtSLiMPreferences() = default;
    ~QtSLiMPreferences();
    QtSLiMPreferences(const QtSLiMPreferencesNotifier&) = delete;
    QtSLiMPreferences& operator=(const QtSLiMPreferencesNotifier&) = delete;
    
private:
    Ui::QtSLiMPreferences *ui;
    
    friend class QtSLiMPreferencesNotifier;
};


#endif // QTSLIMPREFERENCES_H






























