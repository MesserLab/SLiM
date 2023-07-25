//
//  QtSLiMPreferences.cpp
//  SLiM
//
//  Created by Ben Haller on 8/3/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
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


#include "QtSLiMPreferences.h"
#include "ui_QtSLiMPreferences.h"

#include <QSettings>
#include <QFontMetricsF>
#include <QDebug>

#include "QtSLiMAppDelegate.h"


//
//  QSettings keys for the prefs we control; these are private
//

static const char *QtSLiMAppStartupAction = "QtSLiMAppStartupAction";
static const char *QtSLiMForceDarkMode = "QtSLiMForceDarkMode";
static const char *QtSLiMForceFusionStyle = "QtSLiMForceFusionStyle";
static const char *QtSLiMDisplayFontFamily = "QtSLiMDisplayFontFamily";
static const char *QtSLiMDisplayFontSize = "QtSLiMDisplayFontSize";
static const char *QtSLiMSyntaxHighlightScript = "QtSLiMSyntaxHighlightScript";
static const char *QtSLiMSyntaxHighlightOutput = "QtSLiMSyntaxHighlightOutput";
static const char *QtSLiMShowLineNumbers = "QtSLiMShowLineNumbers";
static const char *QtSLiMHighlightCurrentLine = "QtSLiMHighlightCurrentLine";
static const char *QtSLiMAutosaveOnRecycle = "QtSLiMAutosaveOnRecycle";
static const char *QtSLiMShowSaveInUntitled = "QtSLiMShowSaveInUntitled";


static QFont &defaultDisplayFont(void)
{
    // This function determines the default font chosen when the user has expressed no preference.
    // It depends upon font availability, so it can't be hard-coded.
    static QFont *defaultFont = nullptr;
    
    if (!defaultFont)
    {
        QFontDatabase fontdb;
        QStringList families = fontdb.families();
        
        // Use filter() to look for matches, since the foundry can be appended after the name (why isn't this easier??)
        if (families.filter("Consola").size() > 0)                 // good on Windows
            defaultFont = new QFont("Consola", 13);
        else if (families.filter("Courier New").size() > 0)         // good on Mac
            defaultFont = new QFont("Courier New", 13);
        else if (families.filter("Menlo").size() > 0)               // good on Mac
            defaultFont = new QFont("Menlo", 12);
        else if (families.filter("Ubuntu Mono").size() > 0)         // good on Ubuntu
            defaultFont = new QFont("Ubuntu Mono", 11);
        else if (families.filter("DejaVu Sans Mono").size() > 0)    // good on Ubuntu
            defaultFont = new QFont("DejaVu Sans Mono", 9);
        else
            defaultFont = new QFont("Courier", 10);                 // a reasonable default that should be omnipresent
    }
    
    return *defaultFont;
}


//
//  QtSLiMPreferencesNotifier: the pref supplier and notifier
//

QtSLiMPreferencesNotifier &QtSLiMPreferencesNotifier::instance(void)
{
    static QtSLiMPreferencesNotifier *inst = nullptr;
    
    if (!inst)
        inst = new QtSLiMPreferencesNotifier();
    
    return *inst;
}

// pref value fetching

int QtSLiMPreferencesNotifier::appStartupPref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMAppStartupAction, QVariant(1)).toInt();
}

bool QtSLiMPreferencesNotifier::forceDarkModePref(void)
{
#ifdef __APPLE__
    // On macOS this pref is always considered to be false
    return false;
#endif
    
    QSettings settings;
    
    return settings.value(QtSLiMForceDarkMode, QVariant(false)).toBool();
}

bool QtSLiMPreferencesNotifier::forceFusionStylePref(void)
{
#ifdef __APPLE__
    // On macOS this pref is always considered to be false
    return false;
#endif
    
    QSettings settings;
    
    return settings.value(QtSLiMForceFusionStyle, QVariant(false)).toBool();
}

QFont QtSLiMPreferencesNotifier::displayFontPref(double *tabWidth) const
{
    QFont &defaultFont = defaultDisplayFont();
    QString defaultFamily = defaultFont.family();
    int defaultSize = defaultFont.pointSize();
    
    QSettings settings;
    QString fontFamily = settings.value(QtSLiMDisplayFontFamily, QVariant(defaultFamily)).toString();
    int fontSize = settings.value(QtSLiMDisplayFontSize, QVariant(defaultSize)).toInt();
    QFont font(fontFamily, fontSize);
    
    font.setFixedPitch(true);    // I think this is a hint to help QFont match to similar fonts?
    
    if (tabWidth)
    {
        QFontMetricsF fm(font);
        
#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
        *tabWidth = fm.width("   ");                // deprecated in 5.11
#else
        *tabWidth = fm.horizontalAdvance("   ");    // added in Qt 5.11
#endif
    }
    
    return font;
}

bool QtSLiMPreferencesNotifier::scriptSyntaxHighlightPref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMSyntaxHighlightScript, QVariant(true)).toBool();
}

bool QtSLiMPreferencesNotifier::outputSyntaxHighlightPref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMSyntaxHighlightOutput, QVariant(true)).toBool();
}

bool QtSLiMPreferencesNotifier::showLineNumbersPref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMShowLineNumbers, QVariant(true)).toBool();
}

bool QtSLiMPreferencesNotifier::highlightCurrentLinePref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMHighlightCurrentLine, QVariant(true)).toBool();
}

bool QtSLiMPreferencesNotifier::autosaveOnRecyclePref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMAutosaveOnRecycle, QVariant(false)).toBool();
}

bool QtSLiMPreferencesNotifier::showSaveIfUntitledPref(void) const
{
    QSettings settings;
    
    return settings.value(QtSLiMShowSaveInUntitled, QVariant(false)).toBool();
}

void QtSLiMPreferencesNotifier::displayFontBigger(void)
{
    QFont &defaultFont = defaultDisplayFont();
    int defaultSize = defaultFont.pointSize();
    
    QSettings settings;
    int fontSize = settings.value(QtSLiMDisplayFontSize, QVariant(defaultSize)).toInt();
    
    if (fontSize < 50)  // matches value in QtSLiMPreferences.ui
    {
        // if the prefs window exists, we need to tell it to adjust itself
        // if not, we send ourselves the message it would have sent us
        QtSLiMPreferences *prefsWindow = QtSLiMPreferences::instanceForcingAllocation(false);
        
        if (prefsWindow)
            prefsWindow->ui->fontSizeSpinBox->setValue(fontSize + 1);
        else
            fontSizeChanged(fontSize + 1);            
    }
    else
        qApp->beep();
}

void QtSLiMPreferencesNotifier::displayFontSmaller(void)
{
    QFont &defaultFont = defaultDisplayFont();
    int defaultSize = defaultFont.pointSize();
    
    QSettings settings;
    int fontSize = settings.value(QtSLiMDisplayFontSize, QVariant(defaultSize)).toInt();
    
    if (fontSize > 6)  // matches value in QtSLiMPreferences.ui
    {
        // if the prefs window exists, we need to tell it to adjust itself
        // if not, we send ourselves the message it would have sent us
        QtSLiMPreferences *prefsWindow = QtSLiMPreferences::instanceForcingAllocation(false);
        
        if (prefsWindow)
            prefsWindow->ui->fontSizeSpinBox->setValue(fontSize - 1);
        else
            fontSizeChanged(fontSize - 1);            
    }
    else
        qApp->beep();
}


// slots; these update the settings and then emit new signals

void QtSLiMPreferencesNotifier::startupRadioChanged()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    if (prefsUI.ui->startupRadioCreateNew->isChecked())
        settings.setValue(QtSLiMAppStartupAction, QVariant(1));
    else if (prefsUI.ui->startupRadioOpenFile->isChecked())
        settings.setValue(QtSLiMAppStartupAction, QVariant(2));
    
    emit appStartupPrefChanged();
}

void QtSLiMPreferencesNotifier::forceDarkModeToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMForceDarkMode, QVariant(prefsUI.ui->forceDarkMode->isChecked()));
    
    // no signal is emitted for this pref; it takes effect on the next restart of the app
    //emit forceDarkModePrefChanged();
}

void QtSLiMPreferencesNotifier::forceFusionStyleToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMForceFusionStyle, QVariant(prefsUI.ui->forceFusionStyle->isChecked()));
    
    // no signal is emitted for this pref; it takes effect on the next restart of the app
    //emit forceFusionStylePrefChanged();
}

void QtSLiMPreferencesNotifier::fontChanged(const QFont &newFont)
{
    QString fontFamily = newFont.family();
    QSettings settings;
    
    settings.setValue(QtSLiMDisplayFontFamily, QVariant(fontFamily));
    
    emit displayFontPrefChanged();
}

void QtSLiMPreferencesNotifier::fontSizeChanged(int newSize)
{
    QSettings settings;
    
    settings.setValue(QtSLiMDisplayFontSize, QVariant(newSize));
    
    emit displayFontPrefChanged();
}

void QtSLiMPreferencesNotifier::syntaxHighlightScriptToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMSyntaxHighlightScript, QVariant(prefsUI.ui->syntaxHighlightScript->isChecked()));
    
    emit scriptSyntaxHighlightPrefChanged();
}

void QtSLiMPreferencesNotifier::syntaxHighlightOutputToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMSyntaxHighlightOutput, QVariant(prefsUI.ui->syntaxHighlightOutput->isChecked()));
    
    emit outputSyntaxHighlightPrefChanged();
}

void QtSLiMPreferencesNotifier::showLineNumbersToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMShowLineNumbers, QVariant(prefsUI.ui->showLineNumbers->isChecked()));
    
    emit showLineNumbersPrefChanged();
}

void QtSLiMPreferencesNotifier::highlightCurrentLineToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMHighlightCurrentLine, QVariant(prefsUI.ui->highlightCurrentLine->isChecked()));
    
    emit highlightCurrentLinePrefChanged();
}

void QtSLiMPreferencesNotifier::autosaveOnRecycleToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMAutosaveOnRecycle, QVariant(prefsUI.ui->autosaveOnRecycle->isChecked()));
    
    emit autosaveOnRecyclePrefChanged();
}

void QtSLiMPreferencesNotifier::showSaveIfUntitledToggled()
{
    QtSLiMPreferences &prefsUI = QtSLiMPreferences::instance();
    QSettings settings;
    
    settings.setValue(QtSLiMShowSaveInUntitled, QVariant(prefsUI.ui->showSaveIfUntitled->isChecked()));
    
    emit showSaveIfUntitledPrefChanged();
}

void QtSLiMPreferencesNotifier::resetSuppressedClicked()
{
    // All "do not show this again" settings should be removed here
    // There is no signal rebroadcast for this; nobody should cache these flags
    QSettings settings;
    settings.remove("QtSLiMSuppressScriptCheckSuccessPanel");
}


//
//  QtSLiMPreferences: the actual UI class
//

QtSLiMPreferences *QtSLiMPreferences::instanceForcingAllocation(bool force_allocation)
{
    static QtSLiMPreferences *inst = nullptr;
    
    if (!inst && force_allocation)
        inst = new QtSLiMPreferences(nullptr);
    
    return inst;
}

QtSLiMPreferences &QtSLiMPreferences::instance(void)
{
    return *QtSLiMPreferences::instanceForcingAllocation(true);
}

QtSLiMPreferences::QtSLiMPreferences(QWidget *p_parent) : QDialog(p_parent), ui(new Ui::QtSLiMPreferences)
{
    ui->setupUi(this);
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // set the initial state of the UI elements from QtSLiMPreferencesNotifier
    QtSLiMPreferencesNotifier *notifier = &QtSLiMPreferencesNotifier::instance();
    
    ui->startupRadioCreateNew->setChecked(notifier->appStartupPref() == 1);
    ui->startupRadioOpenFile->setChecked(notifier->appStartupPref() == 2);
    
    ui->fontComboBox->setCurrentFont(notifier->displayFontPref());
    ui->fontSizeSpinBox->setValue(notifier->displayFontPref().pointSize());
    
    ui->syntaxHighlightScript->setChecked(notifier->scriptSyntaxHighlightPref());
    ui->syntaxHighlightOutput->setChecked(notifier->outputSyntaxHighlightPref());
    
    ui->showLineNumbers->setChecked(notifier->showLineNumbersPref());
    ui->highlightCurrentLine->setChecked(notifier->highlightCurrentLinePref());
    
    ui->autosaveOnRecycle->setChecked(notifier->autosaveOnRecyclePref());
    ui->showSaveIfUntitled->setChecked(notifier->showSaveIfUntitledPref());
    ui->showSaveIfUntitled->setEnabled(notifier->autosaveOnRecyclePref());
    
    // connect the UI elements to QtSLiMPreferencesNotifier
    connect(ui->startupRadioOpenFile, &QRadioButton::toggled, notifier, &QtSLiMPreferencesNotifier::startupRadioChanged);
    connect(ui->startupRadioCreateNew, &QRadioButton::toggled, notifier, &QtSLiMPreferencesNotifier::startupRadioChanged);
    
    connect(ui->fontComboBox, &QFontComboBox::currentFontChanged, notifier, &QtSLiMPreferencesNotifier::fontChanged);
    connect(ui->fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), notifier, &QtSLiMPreferencesNotifier::fontSizeChanged);

    connect(ui->syntaxHighlightScript, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::syntaxHighlightScriptToggled);
    connect(ui->syntaxHighlightOutput, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::syntaxHighlightOutputToggled);
    
    connect(ui->showLineNumbers, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::showLineNumbersToggled);
    connect(ui->highlightCurrentLine, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::highlightCurrentLineToggled);
    
    connect(ui->autosaveOnRecycle, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::autosaveOnRecycleToggled);
    connect(ui->showSaveIfUntitled, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::showSaveIfUntitledToggled);
    connect(notifier, &QtSLiMPreferencesNotifier::autosaveOnRecyclePrefChanged, this, [this, notifier]() { ui->showSaveIfUntitled->setEnabled(notifier->autosaveOnRecyclePref()); });
    
    connect(ui->resetSuppressedButton, &QPushButton::clicked, notifier, &QtSLiMPreferencesNotifier::resetSuppressedClicked);
    
    // handle the user interface display prefs, which are hidden and disconnected on macOS
#ifdef __APPLE__
    ui->uiAppearanceGroup->setHidden(true);
    ui->verticalSpacer_uiAppearance->changeSize(0, 0);
    ui->verticalSpacer_uiAppearance->invalidate();
    ui->verticalLayout->invalidate();
#else
    ui->forceDarkMode->setChecked(notifier->forceDarkModePref());
    ui->forceFusionStyle->setChecked(notifier->forceFusionStylePref());
    
    connect(ui->forceDarkMode, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::forceDarkModeToggled);
    connect(ui->forceFusionStyle, &QCheckBox::toggled, notifier, &QtSLiMPreferencesNotifier::forceFusionStyleToggled);
#endif
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMPreferences::~QtSLiMPreferences()
{
    delete ui;
}




























