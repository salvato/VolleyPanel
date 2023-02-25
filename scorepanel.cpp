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

#include <QtGlobal>
#include <QtNetwork>
#include <QtWidgets>
#include <QProcess>
#include <QWebSocket>
#include <QVBoxLayout>
#include <QSettings>
#include <QDebug>


//#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
//    // The libraries for using GPIO pins on Raspberry
//    #include "pigpiod_if2.h"
//#endif

#include "slidewindow.h"
#include "fileupdater.h"
#include "scorepanel.h"
#include "utility.h"
#include "panelorientation.h"
#include "volleyapplication.h"


/*! \todo Do we have to send the port numbers to use with
 * the message sent by the Server upon a connection ?
 */
#define SPOT_UPDATE_PORT      45455
#define SLIDE_UPDATE_PORT     45456

#define PAN_PIN  14 // GPIO Numbers are Broadcom (BCM) numbers
#define TILT_PIN 26 // GPIO Numbers are Broadcom (BCM) numbers

//==============================================================
// Informations for connecting two servos for camera Pan & Tilt:
//
// For Raspberry Pi GPIO pin numbering see https://pinout.xyz/
//
// +5V pins 2 or 4 in the 40 pin GPIO connector.
// GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
// in the 40 pin GPIO connector.
//
// Samwa servo pinout
// 1) PWM Signal
// 2) GND
// 3) +5V
//==============================================================

ScorePanel::ScorePanel(const QString &serverUrl, QFile *myLogFile, QWidget *parent)
    : QMainWindow(parent)
    , isMirrored(false)
    , isScoreOnly(false)
    , pPanelServerSocket(Q_NULLPTR)
    , logFile(myLogFile)
    , videoPlayer(Q_NULLPTR)
    , cameraPlayer(Q_NULLPTR)
    , panPin(PAN_PIN)  // BCM14 is Pin  8 in the 40 pin GPIO connector.
    , tiltPin(TILT_PIN)// BCM26 IS Pin 37 in the 40 pin GPIO connector.
    , gpioHostHandle(-1)
{
    iCurrentSpot  = 0;
    iCurrentSlide = 0;

    pMySlideWindow = Q_NULLPTR;

    pPanel = new QWidget(this);

    // Turns off the default window title hints.
    // We don't want windows decorations
    setWindowFlags(Qt::CustomizeWindowHint);

    pSettings = new QSettings("Gabriele Salvato", "Score Panel");
    isScoreOnly = pSettings->value("panel/scoreOnly",  false).toBool();
    isMirrored  = pSettings->value("panel/orientation",  false).toBool();

    QString sBaseDir;
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");

    // Spot management
    pSpotUpdaterThread = Q_NULLPTR;
    pSpotUpdater       = Q_NULLPTR;
    spotUpdatePort     = SPOT_UPDATE_PORT;
    spotUpdaterRestartTimer.setSingleShot(true);
    connect(&spotUpdaterRestartTimer, SIGNAL(timeout()),
            this, SLOT(onCreateSpotUpdaterThread()));
    sSpotDir = QString("%1spots/").arg(sBaseDir);

    // Slide management
    pSlideUpdaterThread = Q_NULLPTR;
    pSlideUpdater       = Q_NULLPTR;
    slideUpdatePort     = SLIDE_UPDATE_PORT;
    slideUpdaterRestartTimer.setSingleShot(true);
    connect(&slideUpdaterRestartTimer, SIGNAL(timeout()),
            this, SLOT(onCreateSlideUpdaterThread()));
    sSlideDir= QString("%1slides/").arg(sBaseDir);

    // Camera management
    initCamera();

    // Slide Window
    pMySlideWindow = new SlideWindow();

    // We are ready to connect to the remote Panel Server
    pPanelServerSocket = new QWebSocket();

    connect(pPanelServerSocket, SIGNAL(connected()),
            this, SLOT(onPanelServerConnected()));
    connect(pPanelServerSocket, SIGNAL(disconnected()),
            this, SLOT(onPanelServerDisconnected()));
    connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));

    // To silent some warnings
    pPanelServerSocket->ignoreSslErrors();
    // Open the Server socket to talk to
    pPanelServerSocket->open(QUrl(serverUrl));

    // Connect the refreshTimer timeout with its SLOT
    connect(&refreshTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToRefreshStatus()));

}


/*!
 * \brief ScorePanel::~ScorePanel The Score Panel destructor
 */
ScorePanel::~ScorePanel() {
    refreshTimer.disconnect();
    refreshTimer.stop();
    if(pPanelServerSocket)
        pPanelServerSocket->disconnect();
//#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
//    if(gpioHostHandle>=0) {
//        pigpio_stop(gpioHostHandle);
//    }
//#endif
    if(pSettings) delete pSettings;
    pSettings = Q_NULLPTR;

    doProcessCleanup();

    if(pPanelServerSocket)
        delete pPanelServerSocket;
    pPanelServerSocket = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::buildLayout Utility function to build the ScorePanel layout
 */
void
ScorePanel::buildLayout() {
    QWidget* oldPanel = pPanel;
    pPanel = new QWidget(this);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addLayout(createPanel());
    pPanel->setLayout(panelLayout);
    setCentralWidget(pPanel);
    if(oldPanel != nullptr)
        delete oldPanel;

}


//========================================
// Spot Updater Thread Management routines
//========================================
/*!
 * \brief ScorePanel::onCreateSpotUpdaterThread
 * Create a "Spot Updater" client to be run on a separated Thread
 */
void
ScorePanel::onCreateSpotUpdaterThread() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Creating a Spot Update Thread"));
#endif
    // Create the Spot Updater Thread
    pSpotUpdaterThread = new QThread();
    connect(pSpotUpdaterThread, SIGNAL(finished()),
            this, SLOT(onSpotUpdaterThreadDone()));
    // And the Spot Update Server
    QString spotUpdateServer;
    spotUpdateServer= QString("ws://%1:%2").arg(pPanelServerSocket->peerAddress().toString()).arg(spotUpdatePort);
    pSpotUpdater = new FileUpdater(QString("SpotUpdater"), spotUpdateServer, logFile);
    pSpotUpdater->moveToThread(pSpotUpdaterThread);
    connect(this, SIGNAL(updateSpots()),
            pSpotUpdater, SLOT(startUpdate()));
    pSpotUpdaterThread->start();
    pSpotUpdater->setDestination(sSpotDir, QString("*.mp4 *.MP4"));
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Spot Update thread started"));
#endif
    emit updateSpots();
}


/*!
 * \brief ScorePanel::closeSpotUpdaterThread
 * Closes the "Spot Updater" Thread.
 */
void
ScorePanel::closeSpotUpdaterThread() {
    if(pSpotUpdaterThread) {
        pSpotUpdaterThread->disconnect();
        if(pSpotUpdaterThread->isRunning()) {
            pSpotUpdaterThread->requestInterruption();
            if(pSpotUpdaterThread->wait(5000)) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Spot Update Thread regularly closed"));
            }
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Spot Update Thread forced to close"));
            }
        }
        delete pSpotUpdaterThread;
    }
    pSpotUpdaterThread = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::onSpotUpdaterThreadDone
 * Invoked Asynchronously when the "Spot Updater" Thread is done.
 */
void
ScorePanel::onSpotUpdaterThreadDone() {
    if(pSpotUpdaterThread)
        pSpotUpdaterThread->disconnect();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Spot Updater Thread regularly closed"));
#endif
    closeSpotUpdaterThread();
    if(pSpotUpdater->returnCode == FileUpdater::TRANSFER_DONE) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Spot Updater closed without errors"));
#endif
    }
    else if(pSpotUpdater->returnCode == FileUpdater::ERROR_SOCKET) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Spot Updater closed with errors"));
        spotUpdaterRestartTimer.start(rand()%5000+5000);
    }
    else if(pSpotUpdater->returnCode == FileUpdater::FILE_ERROR) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Spot Updater got a File Error"));
    }
    else if(pSpotUpdater->returnCode == FileUpdater::SERVER_DISCONNECTED) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Spot Updater Server Unexpectedly Closed the Connection"));
        spotUpdaterRestartTimer.start(rand()%5000+5000);
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Spot Updater Closed for Unknown Reason: %1")
                   .arg(pSpotUpdater->returnCode));
    }
}
//========================================
// End of Spot Server Management routines
//========================================


//=========================================
// Slide Updater Thread Management routines
//=========================================
/*!
 * \brief ScorePanel::onCreateSlideUpdaterThread
 * Create a "Slide Updater" client to be run on a separated Thread
 */
void
ScorePanel::onCreateSlideUpdaterThread() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Creating a Slide Update Thread"));
#endif
    // Create the Slide Updater Thread
    pSlideUpdaterThread = new QThread();
    connect(pSlideUpdaterThread, SIGNAL(finished()),
            this, SLOT(onSlideUpdaterThreadDone()));
    // And the Slide Update Server
    QString slideUpdateServer;
    slideUpdateServer= QString("ws://%1:%2").arg(pPanelServerSocket->peerAddress().toString()).arg(slideUpdatePort);
    pSlideUpdater = new FileUpdater(QString("SlideUpdater"), slideUpdateServer, logFile);
    pSlideUpdater->moveToThread(pSlideUpdaterThread);
    connect(this, SIGNAL(updateSlides()),
            pSlideUpdater, SLOT(startUpdate()));
    pSlideUpdaterThread->start();
    pSlideUpdater->setDestination(sSlideDir, QString("*.jpg *.jpeg *.png *.JPG *.JPEG *.PNG"));
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Slide Update thread started"));
#endif
    emit updateSlides();
}


/*!
 * \brief ScorePanel::closeSlideUpdaterThread
 * Closes the "Slide Updater" Thread.
 */
void
ScorePanel::closeSlideUpdaterThread() {
    if(pSlideUpdaterThread) {
        pSlideUpdaterThread->disconnect();
        if(pSlideUpdaterThread->isRunning()) {
            pSlideUpdaterThread->requestInterruption();
            if(pSlideUpdaterThread->wait(1000)) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Slide Update Thread regularly closed"));
            }
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Slide Update Thread forced to close"));
            }
        }
        delete pSlideUpdaterThread;
    }
    pSlideUpdaterThread = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::onSlideUpdaterThreadDone
 * Invoked Asynchronously when the "Slide Updater" thread is done.
 */
void
ScorePanel::onSlideUpdaterThreadDone() {
    if(pSlideUpdaterThread)
        pSlideUpdaterThread->disconnect();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Slide Update Thread regularly closed"));
#endif
    closeSlideUpdaterThread();
    if(pSlideUpdater->returnCode == FileUpdater::TRANSFER_DONE) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Slide Updater closed without errors"));
#endif
    }
    else if(pSlideUpdater->returnCode == FileUpdater::ERROR_SOCKET) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Slide Updater closed with errors"));
        slideUpdaterRestartTimer.start(rand()%5000+5000);
    }
    else if(pSlideUpdater->returnCode == FileUpdater::FILE_ERROR) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Slide Updater got a File Error"));
    }
    else if(pSlideUpdater->returnCode == FileUpdater::SERVER_DISCONNECTED) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Slide Updater Server Suddenly Closed the Connection"));
        slideUpdaterRestartTimer.start(rand()%5000+5000);
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Slide Updater Closed for Unknown Reason %1")
                   .arg(pSlideUpdater->returnCode));
    }
}
//=========================================
// End of Slide Server Management routines
//=========================================


//==================
// Panel management
//==================
/*!
 * \brief ScorePanel::setScoreOnly To set or reset the "Score Only" mode for the panel
 * \param bScoreOnly True if the Panel has to show only the Score (no Slides, Spots or Camera)
 */
void
ScorePanel::setScoreOnly(bool bScoreOnly) {
    isScoreOnly = bScoreOnly;
    if(isScoreOnly) {
        // Terminate, if running, Videos, Slides and Camera
        if(pMySlideWindow) {
            pMySlideWindow->close();
        }
        if(videoPlayer) {
#ifdef LOG_MESG
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Closing Video Player..."));
#endif
            videoPlayer->disconnect();
            videoPlayer->close();
            videoPlayer->waitForFinished(3000);
            videoPlayer->deleteLater();
            videoPlayer = Q_NULLPTR;
        }
        if(cameraPlayer) {
            cameraPlayer->close();
            cameraPlayer->waitForFinished(3000);
            cameraPlayer->deleteLater();
            cameraPlayer = Q_NULLPTR;
        }
    }
}


/*!
 * \brief ScorePanel::getScoreOnly
 * \return true if the Panel shows only the score (no Slides, Spots or Camera)
 */
bool
ScorePanel::getScoreOnly() {
    return isScoreOnly;
}


/*!
 * \brief ScorePanel::onPanelServerConnected Invoked asynchronously upon the Server connection
 */
void
ScorePanel::onPanelServerConnected() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Started"));
#endif
    QString sMessage;
    sMessage = QString("<getStatus>%1</getStatus>").arg(QHostInfo::localHostName());
    qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Unable to ask the initial status"));
    }
    onCreateSpotUpdaterThread();
    onCreateSlideUpdaterThread();
    bStillConnected = false;
    refreshTimer.start(rand()%2000+3000);
}


void
ScorePanel::onTimeToRefreshStatus() {
    if(!bStillConnected) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Panel Server Disconnected"));
#endif
        if(pPanelServerSocket)
            pPanelServerSocket->deleteLater();
        pPanelServerSocket =Q_NULLPTR;
        doProcessCleanup();
        close();
        emit panelClosed();
        return;
    }
    QString sMessage;
    sMessage = QString("<getStatus>%1</getStatus>").arg(QHostInfo::localHostName());
    qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Unable to refresh the Panel status"));
#endif
        if(pPanelServerSocket)
            pPanelServerSocket->deleteLater();
        pPanelServerSocket =Q_NULLPTR;
        doProcessCleanup();
        close();
        emit panelClosed();
    }
    bStillConnected = false;
}


/*!
 * \brief ScorePanel::onPanelServerDisconnected
 * Invoked asynchronously upon the Server disconnection
 */
void
ScorePanel::onPanelServerDisconnected() {
    doProcessCleanup();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("emitting panelClosed()"));
#endif
    if(pPanelServerSocket)
        pPanelServerSocket->deleteLater();
    pPanelServerSocket =Q_NULLPTR;
    emit panelClosed();
}


/*!
 * \brief ScorePanel::doProcessCleanup
 * Responsible to clean all the running processes upon a Server disconnection.
 */
void
ScorePanel::doProcessCleanup() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Cleaning all processes"));
#endif
    refreshTimer.disconnect();
    spotUpdaterRestartTimer.disconnect();
    slideUpdaterRestartTimer.disconnect();
    refreshTimer.stop();
    spotUpdaterRestartTimer.stop();
    slideUpdaterRestartTimer.stop();
    closeSpotUpdaterThread();
    closeSlideUpdaterThread();

    if(pMySlideWindow) {
        pMySlideWindow->close();
    }
    if(videoPlayer) {
        videoPlayer->disconnect();
        videoPlayer->close();
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Closing Video Player..."));
        videoPlayer->waitForFinished(3000);
        videoPlayer->deleteLater();
        videoPlayer = Q_NULLPTR;
    }
    if(cameraPlayer) {
        cameraPlayer->close();
        cameraPlayer->waitForFinished(3000);
        cameraPlayer->deleteLater();
        cameraPlayer = Q_NULLPTR;
    }
}


/*!
 * \brief ScorePanel::onPanelServerSocketError
 * Invoked asynchronously upon a Server socket error.
 * \param error
 */
void
ScorePanel::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    doProcessCleanup();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("%1 %2 Error %3")
               .arg(pPanelServerSocket->peerAddress().toString())
               .arg(pPanelServerSocket->errorString())
               .arg(error));
#endif
    if(pPanelServerSocket) {
        pPanelServerSocket->disconnect();
        if(pPanelServerSocket->isValid())
            pPanelServerSocket->close();
        pPanelServerSocket->deleteLater();
    }
    pPanelServerSocket = Q_NULLPTR;
    close();// Closes the Widget
    emit panelClosed();
}


/*!
 * \brief ScorePanel::initCamera
 * Initialize the PWM control of the Pan-Tilt camera servos
 */
void
ScorePanel::initCamera() {
    // Get the initial camera position from the past stored values
    cameraPanAngle  = pSettings->value("camera/panAngle",  0.0).toDouble();
    cameraTiltAngle = pSettings->value("camera/tiltAngle", 0.0).toDouble();

    PWMfrequency    = 50;     // in Hz
    pulseWidthAt_90 = 600.0;  // in us
    pulseWidthAt90  = 2200;   // in us

//#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
//    gpioHostHandle = pigpio_start((char*)"localhost", (char*)"8888");
//    if(gpioHostHandle < 0) {
//        logMessage(logFile,
//                   Q_FUNC_INFO,
//                   QString("Non riesco ad inizializzare la GPIO."));
//    }
//    int iResult;
//    if(gpioHostHandle >= 0) {
//        iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
//        if(iResult < 0) {
//            logMessage(logFile,
//                       Q_FUNC_INFO,
//                       QString("Non riesco a definire la frequenza del PWM per il Pan."));
//        }
//        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In us
//        iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
//        if(iResult < 0) {
//            logMessage(logFile,
//                       Q_FUNC_INFO,
//                       QString("Non riesco a far partire il PWM per il Pan."));
//        }
//        set_PWM_frequency(gpioHostHandle, panPin, 0);

//        iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
//        if(iResult < 0) {
//            logMessage(logFile,
//                       Q_FUNC_INFO,
//                       QString("Non riesco a definire la frequenza del PWM per il Tilt."));
//        }
//        pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In us
//        iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
//        if(iResult < 0) {
//            logMessage(logFile,
//                       Q_FUNC_INFO,
//                       QString("Non riesco a far partire il PWM per il Tilt."));
//        }
//        set_PWM_frequency(gpioHostHandle, tiltPin, 0);
//    }
//#endif
}


/*!
 * \brief ScorePanel::closeEvent Handle the Closing of the Panel
 * \param event The closing event
 */
void
ScorePanel::closeEvent(QCloseEvent *event) {
    pSettings->setValue("camera/panAngle",  cameraPanAngle);
    pSettings->setValue("camera/tiltAngle", cameraTiltAngle);
    pSettings->setValue("panel/orientation", isMirrored);

    doProcessCleanup();

//#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
//    if(gpioHostHandle>=0) {
//        pigpio_stop(gpioHostHandle);
//        gpioHostHandle = -1;
//    }
//#endif
    event->accept();
}


/*!
 * \brief ScorePanel::keyPressEvent Close the window following and "Esc" key pressed
 * \param event The event descriptor
 */
void
ScorePanel::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        if(pPanelServerSocket) {
            pPanelServerSocket->disconnect();
            pPanelServerSocket->close(QWebSocketProtocol::CloseCodeNormal,
                                      tr("Il Client ha chiuso il collegamento"));
        }
        close();
    }
}


/*!
 * \brief ScorePanel::onSpotClosed Invoked asynchronously when the Spot Window closes
 * \param exitCode Unused
 * \param exitStatus Unused
 */
void
ScorePanel::onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(videoPlayer) {
        videoPlayer->disconnect();
        videoPlayer->close();// Closes all communication with the process and kills it.
        delete videoPlayer;
        videoPlayer = Q_NULLPTR;
        QString sMessage = "<closed_spot>1</closed_spot>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Sent %1")
                       .arg(sMessage));
        }
#endif
    } // if(videoPlayer)
    show(); // Restore the Score Panel
}


/*!
 * \brief ScorePanel::onLiveClosed Invoked asynchronously when the Camera Window closes
 * \param exitCode Unused
 * \param exitStatus Unused
 */
void
ScorePanel::onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(cameraPlayer) {
        cameraPlayer->disconnect();
        cameraPlayer->close();
        delete cameraPlayer;
        cameraPlayer = Q_NULLPTR;
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Sent %1")
                       .arg(sMessage));
        }
#endif
    } // if(cameraPlayer)
    show(); // Restore the Score Panel
}


/*!
 * \brief ScorePanel::onStartNextSpot
 * \param exitCode
 * \param exitStatus
 */
void
ScorePanel::onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    show(); // Ripristina lo Score Panel
    // Update spot list just in case we are updating the spot list...
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
    spotDir.setNameFilters(nameFilter);
    spotDir.setFilter(QDir::Files);
    spotList = spotDir.entryInfoList();
    if(spotList.count() == 0) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("No spots available !"));
#endif
        if(videoPlayer) {
            videoPlayer->disconnect();
            delete videoPlayer;
            videoPlayer = Q_NULLPTR;
            QString sMessage = "<closed_spot>1</closed_spot>";
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send %1")
                           .arg(sMessage));
            }
        }
        return;
    }

    iCurrentSpot = iCurrentSpot % spotList.count();
    if(!videoPlayer) {
        videoPlayer = new QProcess(this);
        connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onStartNextSpot(int, QProcess::ExitStatus)));
    }
    QString sCommand = "/usr/bin/cvlc";
    QStringList sArguments = QStringList{"--no-osd", "--fullscreen", spotList.at(iCurrentSpot).absoluteFilePath(), "vlc://quit"};
    videoPlayer->start(sCommand, sArguments);
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Now playing: %1")
               .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
    if(!videoPlayer->waitForStarted(3000)) {
        videoPlayer->close();
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Impossibile mandare lo spot"));
        videoPlayer->disconnect();
        delete videoPlayer;
        videoPlayer = Q_NULLPTR;
        return;
    }
    hide();
}


/*!
 * \brief ScorePanel::onBinaryMessageReceived Invoked asynchronously upon a binary message has been received
 * \param baMessage The received message
 */
void
ScorePanel::onBinaryMessageReceived(QByteArray baMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(baMessage.size()));
}


/*!
 * \brief ScorePanel::onTextMessageReceived Invoked asynchronously upon a text message has been received
 * \param sMessage The received message
 *
 * The XML message is processed and the contained command will be executed
 */
void
ScorePanel::onTextMessageReceived(QString sMessage) {
    refreshTimer.start(rand()%2000+3000);
    bStillConnected = true;
    QString sToken;
    bool ok;
    int iVal;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "kill");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>1)
            iVal = 0;
        if(iVal == 1) {
            pPanelServerSocket->disconnect();
            #ifdef Q_PROCESSOR_ARM
            system("sudo halt");
            #endif
            close();// emit the QCloseEvent that is responsible
                    // to clean up all pending processes
        }
    }// kill

    sToken = XML_Parse(sMessage, "spotloop");
    if(sToken != sNoData && !isScoreOnly) {
        startSpotLoop();
    }// spotloop

    sToken = XML_Parse(sMessage, "endspotloop");
    if(sToken != sNoData) {
        stopSpotLoop();
    }// endspoloop

    sToken = XML_Parse(sMessage, "slideshow");
    if(sToken != sNoData && !isScoreOnly){
        startSlideShow();
    }// slideshow

    sToken = XML_Parse(sMessage, "endslideshow");
    if(sToken != sNoData) {
        stopSlideShow();
    }// endslideshow

    sToken = XML_Parse(sMessage, "live");
    if(sToken != sNoData && !isScoreOnly) {
        startLiveCamera();
    }// live

    sToken = XML_Parse(sMessage, "endlive");
    if(sToken != sNoData) {
        stopLiveCamera();
    }// endlive

    sToken = XML_Parse(sMessage, "pan");
    if(sToken != sNoData) {
//#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
//    if(gpioHostHandle >= 0) {
//        cameraPanAngle = sToken.toDouble();
//        pSettings->setValue("camera/panAngle",  cameraPanAngle);
//        set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
//        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In ms
//        int iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
//        if(iResult < 0) {
//            logMessage(logFile,
//                       Q_FUNC_INFO,
//                       QString("Non riesco a far partire il PWM per il Pan."));
//        }
//        set_PWM_frequency(gpioHostHandle, panPin, 0);
//    }
//#endif
    }// pan

    sToken = XML_Parse(sMessage, "tilt");
    if(sToken != sNoData) {
//#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
//        if(gpioHostHandle >= 0) {
//            cameraTiltAngle = sToken.toDouble();
//            pSettings->setValue("camera/tiltAngle", cameraTiltAngle);
//            set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
//            double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In ms
//            int iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
//            if(iResult < 0) {
//              logMessage(logFile,
//                         Q_FUNC_INFO,
//                         QString("Non riesco a far partire il PWM per il Tilt."));
//            }
//            set_PWM_frequency(gpioHostHandle, tiltPin, 0);
//        }
//#endif
    }// tilt

    sToken = XML_Parse(sMessage, "getPanTilt");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            sMessage = QString("<pan_tilt>%1,%2</pan_tilt>").arg(int(cameraPanAngle)).arg(int(cameraTiltAngle));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send pan & tilt values."));
            }
        }
    }// getPanTilt

    sToken = XML_Parse(sMessage, "getOrientation");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            if(isMirrored)
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Reflected));
            else
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Normal));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send orientation value."));
            }
        }
    }// getOrientation

    sToken = XML_Parse(sMessage, "setOrientation");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        try {
            PanelOrientation newOrientation = static_cast<PanelOrientation>(iVal);
            if(newOrientation == PanelOrientation::Reflected)
                isMirrored = true;
            else
                isMirrored = false;
        } catch(...) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        pSettings->setValue("panel/orientation", isMirrored);
        buildLayout();
    }// setOrientation

    sToken = XML_Parse(sMessage, "getScoreOnly");
    if(sToken != sNoData) {
        getPanelScoreOnly();
    }// getScoreOnly

    sToken = XML_Parse(sMessage, "setScoreOnly");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal value fo ScoreOnly received: %1")
                               .arg(sToken));
            return;
        }
        if(iVal==0) {
            setScoreOnly(false);
        }
        else {
            setScoreOnly(true);
        }
        pSettings->setValue("panel/scoreOnly", isScoreOnly);
    }// setScoreOnly

    sToken = XML_Parse(sMessage, "language");
    if(sToken != sNoData) {
        VolleyApplication* application = static_cast<VolleyApplication *>(QApplication::instance());

        QCoreApplication::removeTranslator(&application->Translator);
        if(sToken == QString("English")) {
            if(application->Translator.load(":/panelChooser_en"))
                QCoreApplication::installTranslator(&application->Translator);
        }
        else {
            sToken = QString("Italiano");
        }
        pSettings->setValue("language/current", sToken);
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("New language: %1")
                       .arg(sToken));
#endif
    }// language
}


/*!
 * \brief ScorePanel::startLiveCamera
 */
void
ScorePanel::startLiveCamera() {
#ifdef Q_PROCESSOR_ARM
    if(!cameraPlayer) {
        cameraPlayer = new QProcess(this);
        connect(cameraPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onLiveClosed(int, QProcess::ExitStatus)));
        QString sCommand = QString();
        QStringList sArguments = QStringList();
        sCommand = QString("/usr/bin/libcamera-vid");
        sArguments = QStringList{"--fullscreen",
                                 "-t",
                                 "0",
                                 "auto",
                                 "--vflip",
                                 "--hflip",
                                 "--width",
                                 QString("%1").arg(QGuiApplication::primaryScreen()->geometry().width()),
                                 "--height",
                                 QString("%1").arg(QGuiApplication::primaryScreen()->geometry().height())};
        cameraPlayer->start(sCommand, sArguments);
        if(!cameraPlayer->waitForStarted(3000)) {
            cameraPlayer->close();
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Impossibile Avviare la telecamera"));
            delete cameraPlayer;
            cameraPlayer = Q_NULLPTR;
            QString sMessage = "<closed_live>1</closed_live>";
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send %1")
                           .arg(sMessage));
            }
    #ifdef LOG_VERBOSE
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Sent %1")
                           .arg(sMessage));
            }
    #endif
        }
        #ifdef LOG_VERBOSE
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Live Show is started."));
            }
        #endif
        hide();
    }
#else
    startSpotLoop();
#endif
}


/*!
 * \brief ScorePanel::stopLiveCamera()
 * Invoked to stop a loop of Spots
 */
void
ScorePanel::stopLiveCamera() {
    if(cameraPlayer) {
        cameraPlayer->disconnect();
        connect(cameraPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onLiveClosed(int, QProcess::ExitStatus)));
        cameraPlayer->terminate();
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Live Show has been closed."));
#endif
    }
    else {
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Sent %1")
                       .arg(sMessage));
        }
#endif
        stopSpotLoop();
    }
}


/*!
 * \brief ScorePanel::getPanelScoreOnly
 * send a message indicating if the ScorePanel shows only the score
 */
void
ScorePanel::getPanelScoreOnly() {
    if(pPanelServerSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<isScoreOnly>%1</isScoreOnly>").arg(static_cast<int>(getScoreOnly()));
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send scoreOnly configuration"));
        }
    }
}


/*!
 * \brief ScorePanel::startSpotLoop
 * Invoked to start a loop of Spots
 */
void
ScorePanel::startSpotLoop() {
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    if(spotDir.exists()) {
        QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Found %1 spots").arg(spotList.count()));
#endif
    if(!spotList.isEmpty()) {
        iCurrentSpot = iCurrentSpot % spotList.count();
        if(!videoPlayer) {
            videoPlayer = new QProcess(this);
            connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                    this, SLOT(onStartNextSpot(int, QProcess::ExitStatus)));
            QString sCommand = "/usr/bin/cvlc";
            QStringList sArguments = QStringList{"--no-osd", "--fullscreen", spotList.at(iCurrentSpot).absoluteFilePath(), "vlc://quit"};
            videoPlayer->start(sCommand, sArguments);
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Now playing: %1")
                       .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
            iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
            if(!videoPlayer->waitForStarted(3000)) {
                videoPlayer->close();
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Impossibile mandare lo spot."));
                videoPlayer->disconnect();
                delete videoPlayer;
                videoPlayer = Q_NULLPTR;
                return;
            }
            hide(); // Hide the Score Panel
        } // if(!videoPlayer)
    }
}


/*!
 * \brief ScorePanel::stopSpotLoop
 * Invoked to stop a loop of Spots
 */
void
ScorePanel::stopSpotLoop() {
    if(videoPlayer) {
        videoPlayer->disconnect();
        connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onSpotClosed(int, QProcess::ExitStatus)));
        videoPlayer->terminate();
    }
}


/*!
 * \brief ScorePanel::startSlideShow
 * Invoked to start the SlideShow
 */
void
ScorePanel::startSlideShow() {
    if(videoPlayer || cameraPlayer)
        return;// No Slide Show if movies are playing or camera is active
    if(pMySlideWindow) {
        pMySlideWindow->showFullScreen();
        hide(); // Hide the Score Panel
        pMySlideWindow->setSlideDir(sSlideDir);
        pMySlideWindow->startSlideShow();
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Invalid Slide Window"));
    }
}


/*!
 * \brief ScorePanel::stopSlideShow
 * Invoked to stop the SlideShow
 */
void
ScorePanel::stopSlideShow() {
    if(pMySlideWindow) {
        pMySlideWindow->stopSlideShow();
        show(); // Show the Score Panel
        pMySlideWindow->hide();
    }
}


/*!
 * \brief ScorePanel::createPanel
 * \return
 */
QGridLayout*
ScorePanel::createPanel() {
    return new QGridLayout();
}
