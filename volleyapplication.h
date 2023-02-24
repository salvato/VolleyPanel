#pragma once

#include <QApplication>
#include <QTranslator>
#include <QTimer>


QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(ServerDiscoverer)
QT_FORWARD_DECLARE_CLASS(MessageWindow)
QT_FORWARD_DECLARE_CLASS(QFile)


class VolleyApplication : public QApplication
{
    Q_OBJECT
public:
    VolleyApplication(int& argc, char** argv);

private slots:
    void onTimeToCheckNetwork();
    void onRecheckNetwork();

private:
    bool isConnectedToNetwork();
    bool PrepareLogFile();

public:
    QTranslator        Translator;

private:
    QSettings         *pSettings;
    QFile             *logFile;
    ServerDiscoverer  *pServerDiscoverer;
    MessageWindow     *pNoNetWindow;
    QString            sLanguage;
    QString            logFileName;
    QTimer             networkReadyTimer;
};
