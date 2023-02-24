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
#ifndef SERVERDISCOVERER_H
#define SERVERDISCOVERER_H

#include <QObject>
#include <QList>
#include <QVector>
#include <QHostAddress>
#include <QSslError>
#include <QTimer>
#include <QSslError>

QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(MessageWindow)
QT_FORWARD_DECLARE_CLASS(ScorePanel)

class ServerDiscoverer : public QObject
{
    Q_OBJECT
public:
    explicit ServerDiscoverer(QFile *myLogFile=Q_NULLPTR, QObject *parent=Q_NULLPTR);

signals:
    void serverFound(QString serverUrl, int panelType);/*!< Emitted when a Server sent an correct answer */
    void checkNetwork();/*!< Emitted when the connection with the Server has been lost */

private slots:
    void onProcessDiscoveryPendingDatagrams();
    void onDiscoverySocketError(QAbstractSocket::SocketError error);
    void onPanelServerConnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);
    void onServerConnectionTimeout();
    void onPanelClosed();

public:
    bool Discover();

protected:
    void checkServerAddresses();

private:
    void cleanDiscoverySockets();
    void cleanServerSockets();

private:
    QFile               *logFile;
    QList<QHostAddress>  broadcastAddress;
    QVector<QUdpSocket*> discoverySocketArray;
    QVector<QWebSocket*> serverSocketArray;
    quint16              discoveryPort;
    quint16              serverPort;
    QHostAddress         discoveryAddress;
    int                  panelType;
    QStringList          serverList;
    QWebSocket          *pPanelServerSocket;
    QString              serverUrl;
    QTimer               serverConnectionTimeoutTimer;
    MessageWindow       *pNoServerWindow;
    ScorePanel          *pScorePanel;
};

#endif // SERVERDISCOVERER_H
