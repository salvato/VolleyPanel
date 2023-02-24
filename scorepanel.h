/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#ifndef SCOREPANEL_H
#define SCOREPANEL_H

#include <QObject>
#include <QMainWindow>
#include <QProcess>
#include <QFileInfoList>
#include <QUrl>
#include <QtGlobal>
#include <QTranslator>
#include <QTimer>

#include "slidewindow.h"
#include "serverdiscoverer.h"

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    #define horizontalAdvance width
#endif


QT_BEGIN_NAMESPACE
QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(SlideWindow)
QT_FORWARD_DECLARE_CLASS(QGridLayout)
QT_FORWARD_DECLARE_CLASS(UpdaterThread)
QT_FORWARD_DECLARE_CLASS(FileUpdater)
QT_END_NAMESPACE


class ScorePanel : public QMainWindow
{
    Q_OBJECT

public:
    ScorePanel(const QString &serverUrl, QFile *myLogFile, QWidget *parent = Q_NULLPTR);
    ~ScorePanel();
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);
    void setScoreOnly(bool bScoreOnly);
    bool getScoreOnly();

signals:
    void updateSpots(); /*!< \brief emitted to start the Spot update process */
    void updateSlides();/*!< \brief emitted to start the Slide update process */
    void panelClosed(); /*!< \brief emitted to signal that the Panel has been closed */

protected slots:
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);


private slots:
    void onPanelServerConnected();
    void onPanelServerDisconnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);
    void onTimeToRefreshStatus();
    void onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus);
    void onCreateSpotUpdaterThread();
    void onCreateSlideUpdaterThread();

    void onSpotUpdaterThreadDone();
    void onSlideUpdaterThreadDone();

protected:
    virtual QGridLayout* createPanel();

    void buildLayout();
    void doProcessCleanup();
    void closeSpotUpdaterThread();
    void closeSlideUpdaterThread();

protected:
    /*!
     * \brief isMirrored true if the panel is horizontally reflected
     * with respect to the Server panel
     */
    bool               isMirrored;
    /*!
     * \brief isScoreOnly true if the panel shows only the score
     */
    bool               isScoreOnly;
    /*!
     * \brief pPanelServerSocket The WebSocket to talk with the server
     */
    QWebSocket        *pPanelServerSocket;
    /*!
     * \brief logFile the file for message logging (if any)
     */
    QFile             *logFile;
    QTranslator        Translator;

private:
    bool               bStillConnected;
    QTimer             refreshTimer;
    QProcess          *videoPlayer;
    QProcess          *cameraPlayer;
    QString            sProcess;
    QString            sProcessArguments;

    // Spots management
    quint16            spotUpdatePort;
    QThread           *pSpotUpdaterThread;
    FileUpdater       *pSpotUpdater;
    QString            sSpotDir;
    QFileInfoList      spotList;
    struct spot {
        QString spotFilename;
        qint64  spotFileSize;
    };
    QList<spot>        availabeSpotList;
    int                iCurrentSpot;
    QTimer             spotUpdaterRestartTimer;

    // Slides management
    quint16            slideUpdatePort;
    QThread           *pSlideUpdaterThread;
    FileUpdater       *pSlideUpdater;
    QString            sSlideDir;
    QFileInfoList      slideList;
    struct slide {
        QString slideFilename;
        qint64  slideFileSize;
    };
    QList<slide>       availabeSlideList;
    int                iCurrentSlide;
    QTimer             slideUpdaterRestartTimer;

    QString            logFileName;

    SlideWindow       *pMySlideWindow;

    unsigned           panPin;
    unsigned           tiltPin;
    double             cameraPanAngle;
    double             cameraTiltAngle;
    int                gpioHostHandle;
    unsigned           PWMfrequency;
    quint32            dutyCycle;
    double             pulseWidthAt_90;
    double             pulseWidthAt90;

private:
    void               initCamera();
    void               startLiveCamera();
    void               stopLiveCamera();
    void               startSpotLoop();
    void               stopSpotLoop();
    void               startSlideShow();
    void               stopSlideShow();
    void               getPanelScoreOnly();

private:
    QSettings         *pSettings;
    QWidget           *pPanel;
};

#endif // SCOREPANEL_H
