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
#ifndef FILEUPDATER_H
#define FILEUPDATER_H

#include <QObject>
#include <QUrl>
#include <QWebSocket>
#include <QWidget>
#include <QFile>
#include <QFileInfoList>


QT_FORWARD_DECLARE_CLASS(QWebSocket)


/*!
 * \brief A struct that defines a file to transfer
 */
struct files {
    QString fileName;/*!< \brief  The file Name */
    qint64  fileSize;/*!< \brief its size (in bytes) */
};


class FileUpdater : public QObject
{
    Q_OBJECT
public:
    explicit FileUpdater(QString sName, QUrl myServerUrl, QFile *myLogFile = Q_NULLPTR, QObject *parent = Q_NULLPTR);
    bool setDestination(QString myDstinationDir, QString sExtensions);
    void askFileList();

    static const int TRANSFER_DONE       =  0;
    static const int ERROR_SOCKET        = -1;
    static const int SERVER_DISCONNECTED = -2;
    static const int FILE_ERROR          = -3;

public slots:
    void startUpdate();

private slots:
    void onUpdateSocketError(QAbstractSocket::SocketError error);
    void onUpdateSocketConnected();
    void onServerDisconnected();
    void onProcessTextMessage(QString sMessage);
    void onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame);

private:
    void handleWriteFileError();
    void handleOpenFileError();
    bool isConnectedToNetwork();
    void updateFiles();
    void askFirstFile();

public:
    int returnCode;

private:
    QFile       *logFile;
    QWebSocket  *pUpdateSocket;
    QString      sMyName;
    QFile        file;
    QUrl         serverUrl;
    QString      destinationDir;
    QString      sFileExtensions;
    qint64       bytesReceived;
    QString      sCurrentFileName;

    QList<files> queryList;
    QList<files> remoteFileList;
};

#endif // FILEUPDATER_H
