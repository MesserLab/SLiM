//
//  QtSLiMPreferences.h
//  SLiM
//
//  Created by Ben Haller on 8/3/2019.
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
    bool forceDarkModePref(void);
    bool forceFusionStylePref(void);
    bool useOpenGLPref(void);
    QFont displayFontPref(double *tabWidth = nullptr) const;
    bool scriptSyntaxHighlightPref(void) const;
    bool outputSyntaxHighlightPref(void) const;
    bool showLineNumbersPref(void) const;
    bool showPageGuidePref(void) const;
    int pageGuideColumnPref(void) const;
    bool highlightCurrentLinePref(void) const;
    bool autosaveOnRecyclePref(void) const;
    bool reloadOnSafeExternalEditsPref(void) const;
    bool showSaveIfUntitledPref(void) const;
    
    // Change preferences values in ways other than the Preferences panel itself
    void displayFontBigger(void);
    void displayFontSmaller(void);
    
signals:
    // Get notified when a pref value changes
    void appStartupPrefChanged(void);
    void useOpenGLPrefChanged(void);
    void displayFontPrefChanged(void);
    void scriptSyntaxHighlightPrefChanged(void);
    void outputSyntaxHighlightPrefChanged(void);
    void showLineNumbersPrefChanged(void);
    void pageGuidePrefsChanged(void);
    void highlightCurrentLinePrefChanged(void);
    void autosaveOnRecyclePrefChanged(void);
    void reloadOnSafeExternalEditsChanged(void);
    void showSaveIfUntitledPrefChanged(void);
    
private:
    // singleton pattern
    QtSLiMPreferencesNotifier() = default;
    ~QtSLiMPreferencesNotifier() = default;
    QtSLiMPreferencesNotifier(const QtSLiMPreferencesNotifier&) = delete;
    QtSLiMPreferencesNotifier& operator=(const QtSLiMPreferencesNotifier&) = delete;
    
private slots:
    void startupRadioChanged();
    void forceDarkModeToggled();
    void forceFusionStyleToggled();
    void useOpenGLToggled();
    void fontChanged(const QFont &font);
    void fontSizeChanged(int newSize);
    void syntaxHighlightScriptToggled();
    void syntaxHighlightOutputToggled();
    void showLineNumbersToggled();
    void showPageGuideToggled();
    void pageGuideColumnChanged(int newColumn);
    void highlightCurrentLineToggled();
    void autosaveOnRecycleToggled();
    void reloadOnSafeExternalEditsToggled();
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
    static QtSLiMPreferences *instanceForcingAllocation(bool force_allocation);
    static QtSLiMPreferences &instance(void);
    
private:
    // singleton pattern
    explicit QtSLiMPreferences(QWidget *p_parent = nullptr);
    QtSLiMPreferences() = default;
    ~QtSLiMPreferences();
    QtSLiMPreferences(const QtSLiMPreferencesNotifier&) = delete;
    QtSLiMPreferences& operator=(const QtSLiMPreferencesNotifier&) = delete;
    
private:
    Ui::QtSLiMPreferences *ui;
    
    friend class QtSLiMPreferencesNotifier;
};


#endif // QTSLIMPREFERENCES_H






























