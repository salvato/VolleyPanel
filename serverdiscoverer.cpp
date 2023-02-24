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

#include <QCoreApplication>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QUdpSocket>
#include <QWebSocket>
#include <QHostInfo>
#include <QSettings>

#include "serverdiscoverer.h"
#include "messagewindow.h"
#include "utility.h"
#include "scorepanel.h"
#include "volleypanel.h"

#define DISCOVERY_PORT 45453
#define SERVER_PORT    45454

#define SERVER_CONNECTION_TIMEOUT 3000

/*!
 * \brief ServerDiscoverer::ServerDiscoverer
 * \param myLogFile
 * \param parent
 *
 * This class manage the "Server Discovery" process.
 * It send a multicast message and listen for a correct answer
 * The it try to connect to the server and if it succeed create and
 * show the rigth Score Panel
 */
ServerDiscoverer::ServerDiscoverer(QFile *myLogFile, QObject *parent)
    : QObject(parent)
    , logFile(myLogFile)
    , discoveryPort(DISCOVERY_PORT)
    , serverPort(SERVER_PORT)
    , discoveryAddress(QHostAddress("224.0.0.1"))
    , pNoServerWindow(Q_NULLPTR)
    , pScorePanel(Q_NULLPTR)
{
    pNoServerWindow = new MessageWindow(Q_NULLPTR);
    pNoServerWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
}





/*!
* \brief ServerDiscoverer::Discover
* \return
*
* Multicast the "sever discovery" message on all the usable network interfaces.
* Retrns true if at least a message has been sent.
* If a message has been sent, a "connection timeout" timer is started.
*/
bool
ServerDiscoverer::Discover() {
    qint64 written;
    bool bStarted = false;
    QString sMessage = "<getServer>"+ QHostInfo::localHostName() + "</getServer>";
    QByteArray datagram = sMessage.toUtf8();

    if(pNoServerWindow == Q_NULLPTR) {
        pNoServerWindow = new MessageWindow(Q_NULLPTR);
        pNoServerWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    // No other window should obscure this one
    if(!pNoServerWindow->isVisible())
        pNoServerWindow->showFullScreen();
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for(int i=0; i<ifaces.count(); i++) {
        QNetworkInterface iface = ifaces.at(i);
        if(iface.flags().testFlag(QNetworkInterface::IsUp) &&
           iface.flags().testFlag(QNetworkInterface::IsRunning) &&
           iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
          !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            QUdpSocket* pDiscoverySocket = new QUdpSocket(this);
            // We need to save all the created sockets...
            discoverySocketArray.append(pDiscoverySocket);
            // To manage socket errors
            connect(pDiscoverySocket, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(onDiscoverySocketError(QAbstractSocket::SocketError)));
            // To manage the messages from the socket
            connect(pDiscoverySocket, SIGNAL(readyRead()),
                    this, SLOT(onProcessDiscoveryPendingDatagrams()));
            if(!pDiscoverySocket->bind()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to bind the Discovery Socket"));
                continue;
            }
            pDiscoverySocket->setMulticastInterface(iface);
            pDiscoverySocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
            written = pDiscoverySocket->writeDatagram(datagram.data(), datagram.size(),
                                                      discoveryAddress, discoveryPort);
#ifdef LOG_VERBOSE_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Writing %1 to %2 - interface# %3/%4 : %5")
                       .arg(sMessage)
                       .arg(discoveryAddress.toString())
                       .arg(i)
                       .arg(ifaces.count())
                       .arg(iface.humanReadableName()));
#endif
            if(written != datagram.size()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to write to Discovery Socket"));
            }
            else {
                bStarted = true;
            }
        }
    }
    if(bStarted) {
        connect(&serverConnectionTimeoutTimer, SIGNAL(timeout()),
                this, SLOT(onServerConnectionTimeout()));
        serverConnectionTimeoutTimer.start(SERVER_CONNECTION_TIMEOUT);
    }
    return bStarted;
}


/*!
 * \brief ServerDiscoverer::onDiscoverySocketError
 * \param socketError
 */
void
ServerDiscoverer::onDiscoverySocketError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError)
    QUdpSocket *pClient = qobject_cast<QUdpSocket *>(sender());
    logMessage(logFile,
               Q_FUNC_INFO,
               pClient->errorString());
    return;
}


/*!
 * \brief ServerDiscoverer::onProcessDiscoveryPendingDatagrams
 *
 * A Panel Server sent back an answer...
 */
void
ServerDiscoverer::onProcessDiscoveryPendingDatagrams() {
    QUdpSocket* pSocket = qobject_cast<QUdpSocket*>(sender());
    QByteArray datagram = QByteArray();
    QByteArray answer = QByteArray();
    QString sToken;
    QString sNoData = QString("NoData");
    while(pSocket->hasPendingDatagrams()) {
        datagram.resize(int(pSocket->pendingDatagramSize()));
        if(pSocket->readDatagram(datagram.data(), datagram.size()) == -1) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Error reading from udp socket: %1")
                       .arg(serverUrl));
        }
        answer.append(datagram);
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("pDiscoverySocket Received: %1")
               .arg(answer.data()));
#endif
    sToken = XML_Parse(answer.data(), "serverIP");
    if(sToken != sNoData) {
        serverList = QStringList(sToken.split(";",Qt::SkipEmptyParts));
        if(serverList.isEmpty())
            return;
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Found %1 addresses")
                   .arg(serverList.count()));
#endif
        // A well formed answer has been received.
        serverConnectionTimeoutTimer.stop();
        serverConnectionTimeoutTimer.disconnect();
        // Remove all the "discovery sockets" to avoid overlapping
        cleanDiscoverySockets();
        checkServerAddresses();
    }
}


/*!
 * \brief ServerDiscoverer::checkServerAddresses
 * Try to connect to all the Panel Server addresses
 */
void
ServerDiscoverer::checkServerAddresses() {
    panelType = 0;
    connect(&serverConnectionTimeoutTimer, SIGNAL(timeout()),
            this, SLOT(onServerConnectionTimeout()));
    serverConnectionTimeoutTimer.start(SERVER_CONNECTION_TIMEOUT);
    for(int i=0; i<serverList.count(); i++) {
        QStringList arguments = QStringList(serverList.at(i).split(",",Qt::SkipEmptyParts));
        if(arguments.count() > 1) {
            serverUrl= QString("ws://%1:%2").arg(arguments.at(0)).arg(serverPort);
            // Last Panel Type will win (is this right ?)
            panelType = arguments.at(1).toInt();
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Trying Server URL: %1")
                       .arg(serverUrl));
#endif
            pPanelServerSocket = new QWebSocket();
            serverSocketArray.append(pPanelServerSocket);
            connect(pPanelServerSocket, SIGNAL(connected()),
                    this, SLOT(onPanelServerConnected()));
            connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));
            pPanelServerSocket->ignoreSslErrors();
            pPanelServerSocket->open(QUrl(serverUrl));
        }
    }
}


/*!
 * \brief ServerDiscoverer::onPanelServerConnected First come ... first served !
 */
void
ServerDiscoverer::onPanelServerConnected() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Connected to Server URL: %1")
               .arg(serverUrl));
#endif
    serverConnectionTimeoutTimer.stop();
    serverConnectionTimeoutTimer.disconnect();
    QWebSocket* pSocket = qobject_cast<QWebSocket*>(sender());
    serverUrl = pSocket->requestUrl().toString();
    cleanServerSockets();

    // Delete old Panel instance to prevent memory leaks
    if(pScorePanel) {
        pScorePanel->disconnect();
        delete pScorePanel;
        pScorePanel = Q_NULLPTR;
    }
    pScorePanel = new VolleyPanel(serverUrl, logFile);

    connect(pScorePanel, SIGNAL(panelClosed()),
            this, SLOT(onPanelClosed()));

    delete pNoServerWindow;
    pNoServerWindow = Q_NULLPTR;
    pScorePanel->showFullScreen();
}


/*!
 * \brief ServerDiscoverer::onPanelServerSocketError
 * \param error
 */
void
ServerDiscoverer::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("%1 %2 Error %3")
               .arg(pPanelServerSocket->requestUrl().toString())
               .arg(pPanelServerSocket->errorString())
               .arg(error));
}


/*!
 * \brief ServerDiscoverer::onServerConnectionTimeout
 * Called when no messages or connections are received within the
 * SERVER_CONNECTION_TIMEOUT time
 */
void
ServerDiscoverer::onServerConnectionTimeout() {
    serverConnectionTimeoutTimer.stop();
    serverConnectionTimeoutTimer.disconnect();
    if(pNoServerWindow == Q_NULLPTR) {
        pNoServerWindow = new MessageWindow(Q_NULLPTR);
        pNoServerWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    // No other window should obscure this one
    if(!pNoServerWindow->isVisible())
        pNoServerWindow->showFullScreen();
    cleanDiscoverySockets();
    cleanServerSockets();
    // Restart the discovery process
    if(!Discover()) {
        delete pNoServerWindow;
        pNoServerWindow = Q_NULLPTR;
        emit checkNetwork();
    }
}


/*!
 * \brief ServerDiscoverer::onPanelClosed Invoked from the Score Panel when it closes
 */
void
ServerDiscoverer::onPanelClosed() {
    if(pNoServerWindow == Q_NULLPTR) {
        pNoServerWindow = new MessageWindow(Q_NULLPTR);
        pNoServerWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    // No other window should obscure this one
    if(!pNoServerWindow->isVisible())
        pNoServerWindow->showFullScreen();
    cleanDiscoverySockets();
    cleanServerSockets();
    if(!Discover()) {
        delete pNoServerWindow;
        pNoServerWindow = Q_NULLPTR;
        emit checkNetwork();
    }
}


/*!
 * \brief ServerDiscoverer::cleanDiscoverySockets
 */
void
ServerDiscoverer::cleanDiscoverySockets() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Cleaning Discovery Sockets"));
#endif
    for(int i=0; i<discoverySocketArray.count(); i++) {
        QUdpSocket *pDiscovery = qobject_cast<QUdpSocket *>(discoverySocketArray.at(i));
        pDiscovery->disconnect();
        pDiscovery->abort();
        // deleteLater() to Allow for the processing of
        // already queued events.
        // Don't change in: delete pDiscovery;
        pDiscovery->deleteLater();//
    }
    discoverySocketArray.clear();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Done Cleaning Discovery Sockets"));
#endif
}


/*!
 * \brief ServerDiscoverer::cleanServerSockets
 */
void
ServerDiscoverer::cleanServerSockets() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Cleaning Server Sockets"));
#endif
    for(int i=0; i<serverSocketArray.count(); i++) {
        QWebSocket *pServer = qobject_cast<QWebSocket *>(serverSocketArray.at(i));
        pServer->disconnect();
        pServer->abort();
        // deleteLater() to Allow for the processing of
        // already queued events.
        // Don't change in: delete pServer;
        pServer->deleteLater();
    }
    serverSocketArray.clear();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Done Cleaning Server Sockets"));
#endif
}

