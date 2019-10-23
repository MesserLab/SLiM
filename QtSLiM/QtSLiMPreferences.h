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
    int appStartupPref(void);               // 0 == do nothing, 1 == create a new window, 2 == run an open panel
    QFont displayFontPref(int *tabWidth = nullptr);
    bool scriptSyntaxHighlightPref(void);
    bool outputSyntaxHighlightPref(void);
    
signals:
    // Get notified when a pref value changes
    void appStartupPrefChanged(void);
    void displayFontPrefChanged(void);
    void scriptSyntaxHighlightPrefChanged(void);
    void outputSyntaxHighlightPrefChanged(void);
    
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






























