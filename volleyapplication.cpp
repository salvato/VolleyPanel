#include <QNetworkInterface>
#include <QFile>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

#include "volleyapplication.h"
#include "serverdiscoverer.h"
#include "messagewindow.h"


#define NETWORK_CHECK_TIME    3000 // In msec


VolleyApplication::VolleyApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , logFile(nullptr)
    , pServerDiscoverer(nullptr)
    , pNoNetWindow(nullptr)
{
    pSettings = new QSettings("Gabriele Salvato", "Volley Panel");
    sLanguage = pSettings->value("language/current",  QString("Italiano")).toString();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Initial Language: %1").arg(sLanguage));
#endif
    if(sLanguage == QString("English")) {
        if(Translator.load(":/panelChooser_en"))
            QCoreApplication::installTranslator(&Translator);
    }

    // We want the cursor set for all widgets,
    // even when outside the window then:
    setOverrideCursor(Qt::BlankCursor);

    // Initialize the random number generator
    QTime time(QTime::currentTime());
    srand(uint(time.msecsSinceStartOfDay()));

    // Starts a timer to check for a ready network connection
    connect(&networkReadyTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckNetwork()));

    QString sBaseDir;
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
    logFileName = QString("%1volley_panel.txt").arg(sBaseDir);
    PrepareLogFile();

    // Create a message window
    pNoNetWindow = new MessageWindow(Q_NULLPTR);
    pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
    pNoNetWindow->showFullScreen();

    // Create a "PanelServer Discovery Service" but not start it
    // until we are sure that there is an active network connection
    pServerDiscoverer = new ServerDiscoverer(logFile);
    connect(pServerDiscoverer, SIGNAL(checkNetwork()),
            this, SLOT(onRecheckNetwork()));

    // When the network becomes available we will start the
    // "PanelServer Discovery Service".
    // Let's start the periodic check for the network
    networkReadyTimer.start(NETWORK_CHECK_TIME);

    // And now it is time to check if the Network
    // is already up and working
    onTimeToCheckNetwork();
}


void
VolleyApplication::onTimeToCheckNetwork() {
    networkReadyTimer.stop();
    if(isConnectedToNetwork()) {
        // Let's start the "Server Discovery Service"
        if(!pServerDiscoverer->Discover()) {
            if(pNoNetWindow == Q_NULLPTR)
                pNoNetWindow = new MessageWindow(Q_NULLPTR);
            // If the service is unable to start then probably
            // The network connection went down.
            pNoNetWindow->setDisplayedText(tr("Errore: Server Discovery Non Avviato"));
            // then restart checking...
            networkReadyTimer.start(NETWORK_CHECK_TIME);
        }
        else {
            delete pNoNetWindow;
            pNoNetWindow = Q_NULLPTR;
        }
    }
    else {// The network connection is down !
        if(pNoNetWindow == Q_NULLPTR)
            pNoNetWindow = new MessageWindow();
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
        // No other window should obscure this one
        pNoNetWindow->showFullScreen();
    }
}


void
VolleyApplication::onRecheckNetwork() {
    if(pNoNetWindow == Q_NULLPTR)
        pNoNetWindow = new MessageWindow(Q_NULLPTR);
    pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
    // No other window should obscure this one
    pNoNetWindow->showFullScreen();
    networkReadyTimer.start(NETWORK_CHECK_TIME);
}


bool
VolleyApplication::isConnectedToNetwork() {
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    bool result = false;

    for(int i=0; i<ifaces.count(); i++) {
        QNetworkInterface iface = ifaces.at(i);
        if(iface.flags().testFlag(QNetworkInterface::IsUp) &&
           iface.flags().testFlag(QNetworkInterface::IsRunning) &&
           iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
          !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            for(int j=0; j<iface.addressEntries().count(); j++) {
                // we have an interface that is up, and has an ip address
                // therefore the link is present
                if(result == false)
                    result = true;
            }
        }
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               result ? QString("true") : QString("false"));
#endif
    return result;
}


bool
VolleyApplication::PrepareLogFile() {
#ifdef LOG_MESG
    QFileInfo checkFile(logFileName);
    if(checkFile.exists() && checkFile.isFile()) {
        QDir renamed;
        renamed.remove(logFileName+QString(".bkp"));
        renamed.rename(logFileName, logFileName+QString(".bkp"));
    }
    logFile = new QFile(logFileName);
    if (!logFile->open(QIODevice::WriteOnly)) {
        QMessageBox::information(Q_NULLPTR, "Segnapunti Volley",
                                 QString("Impossibile aprire il file %1: %2.")
                                 .arg(logFileName).arg(logFile->errorString()));
        delete logFile;
        logFile = Q_NULLPTR;
    }
#endif
    return true;
}
