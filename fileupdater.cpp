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
#include "fileupdater.h"
#include <QtNetwork>
#include <QTime>
#include <QTimer>

#include "utility.h"

#define CHUNK_SIZE 512*1024


/*!
 * \brief FileUpdater::FileUpdater Base Class for the Slides and Spots File Transfer
 * \param sName A string to identify this particular instance of FileUpdater.
 * \param myServerUrl The Url of the File Server to connect to.
 * \param myLogFile The File for logging (if any).
 * \param parent The parent object.
 *
 * It is responsible to get the Slides and the Spots from the Server.
 * Indeed each panel maintain a local copy of the files.
 * In this way, after an initial delay due to the transfer,
 * no further delays are expected for launching the Slide or Movie Show.
 */
FileUpdater::FileUpdater(QString sName, QUrl myServerUrl, QFile *myLogFile, QObject *parent)
    : QObject(parent)
    , logFile(myLogFile)
    , serverUrl(myServerUrl)
{
    sMyName = sName;
    pUpdateSocket = Q_NULLPTR;
    destinationDir = QString(".");
    bytesReceived = 0;
}


/*!
 * \brief FileUpdater::setDestination Set the file destination folder.
 * \param myDstinationDir The destination folder
 * \param sExtensions The file extensions to look for
 * \return true if the folder is ok; false otherwise
 *
 *  If the Folder does not exists it will be created
 */
bool
FileUpdater::setDestination(QString myDstinationDir, QString sExtensions) {
    destinationDir = myDstinationDir;
    sFileExtensions = sExtensions;
    QDir outDir(destinationDir);
    if(!outDir.exists()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Creating new directory: %1")
                   .arg(destinationDir));
        if(!outDir.mkdir(destinationDir)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to create directory: %1")
                       .arg(destinationDir));
            return false;
        }
    }
    return true;
}


/*!
 * \brief FileUpdater::startUpdate
 * Try to connect asynchronously to the File Server
 */
void
FileUpdater::startUpdate() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" Connecting to file server: %1")
               .arg(serverUrl.toString()));
#endif
    // Initialize the socket...
    pUpdateSocket = new QWebSocket();
    // And connect its various signals with the local slots
    connect(pUpdateSocket, SIGNAL(connected()),
            this, SLOT(onUpdateSocketConnected()));
    connect(pUpdateSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onUpdateSocketError(QAbstractSocket::SocketError)));
    connect(pUpdateSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onProcessTextMessage(QString)));
    connect(pUpdateSocket, SIGNAL(binaryFrameReceived(QByteArray, bool)),
            this,SLOT(onProcessBinaryFrame(QByteArray, bool)));
    connect(pUpdateSocket, SIGNAL(disconnected()),
            this, SLOT(onServerDisconnected()));
    // To silent some diagnostic messages...
    pUpdateSocket->ignoreSslErrors();
    // Let's try to open the connection
    pUpdateSocket->open(QUrl(serverUrl));
}


/*!
 * \brief FileUpdater::onUpdateSocketConnected
 * Invoked asynchronously when the server socket connects
 *
 * Ask for the list of files that the server wants to transfer
 */
void
FileUpdater::onUpdateSocketConnected() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" Connected to: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
#endif
    // Query the file's list
    askFileList();
}


/*!
 * \brief FileUpdater::askFileList
 * Ask the file server for a list of files to transfer
 */
void
FileUpdater::askFileList() {
    if(pUpdateSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<send_file_list>1</send_file_list>");
        qint64 bytesSent = pUpdateSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Unable to ask for file list"));
            returnCode = ERROR_SOCKET;
            thread()->exit(returnCode);
            return;
        }
#ifdef LOG_MESG
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Sent %1 to %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
#endif
    }
}


/*!
 * \brief FileUpdater::onServerDisconnected
 * Invoked asynchronously whe the server disconnects
 *
 * Stop the running Thread with SERVER_DISCONNECTED return code
 */
void
FileUpdater::onServerDisconnected() {
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" WebSocket disconnected from: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    returnCode = SERVER_DISCONNECTED;
    thread()->exit(returnCode);
}


/*!
 * \brief FileUpdater::onUpdateSocketError
 * File transfer error handler
 * \param error The socket error
 *
 * It stops the Thread with SOCKET_ERROR return code
 */
void
FileUpdater::onUpdateSocketError(QAbstractSocket::SocketError error) {
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" %1 %2 Error %3")
               .arg(pUpdateSocket->localAddress().toString())
               .arg(pUpdateSocket->errorString())
               .arg(error));
    returnCode = ERROR_SOCKET;
    thread()->exit(returnCode);
}


/*!
 * \brief FileUpdater::onProcessBinaryFrame
 * Invoked asynchronously when a binary chunk of information is available
 * \param baMessage [in] the chunk of information
 * \param isLastFrame [in] is this the last chunk ?
 */
void
FileUpdater::onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame) {
    QString sMessage;
    // Check if the file transfer must be stopped
    if(thread()->isInterruptionRequested()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Received an Exit Request"));
        returnCode = TRANSFER_DONE;
        thread()->exit(returnCode);
        return;
    }
    if(bytesReceived == 0) {// It's a new file...
        // Get the header...
        QByteArray header = baMessage.left(1024);
        int iSeparator = header.indexOf(",");
        // Get the Filename
        sCurrentFileName = QString(header.left(iSeparator));
        header = header.mid(iSeparator+1);
        iSeparator = header.indexOf('\0');
        // Get the file length...
        int len = header.left(iSeparator).toInt();
        file.setFileName(destinationDir + sCurrentFileName);
        // Is the next row necessary ? (Check)
        file.remove();
        file.setFileName(destinationDir + sCurrentFileName + QString(".temp"));
        // Is the next row necessary ? (Check)
        file.remove();// Just in case of a previous aborted transfer

        if(file.open(QIODevice::Append)) {
            len = baMessage.size()-1024;
            qint64 written = file.write(baMessage.mid(1024));
            bytesReceived += written;
            if(len != written) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" Writing File %1 Error: bytes written(%2/%3)")
                           .arg(sCurrentFileName)
                           .arg(written)
                           .arg(len));
                handleWriteFileError();
                return;
            }
        } else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Unable to open file: %1")
                       .arg(sCurrentFileName));
            handleOpenFileError();
            return;
        }
    }// if(bytesReceived == 0)
    else {// It's a new frame for an already existing file...
        int len = baMessage.size();
        qint64 written = file.write(baMessage);
        bytesReceived += written;
        if(len != written) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Writing File %1 Error: bytes written(%2/%3)")
                       .arg(sCurrentFileName)
                       .arg(written)
                       .arg(len));
            handleWriteFileError();
            return;
        }
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" Received %1 bytes").arg(bytesReceived));
#endif
    if(isLastFrame) {
        if(bytesReceived < queryList.last().fileSize) {// File length mismatch !!!!
            sMessage = QString("<get>%1,%2,%3</get>")
                       .arg(queryList.last().fileName)
                       .arg(bytesReceived)
                       .arg(CHUNK_SIZE);
            qint64 written = pUpdateSocket->sendTextMessage(sMessage);
            if(written != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" Error writing %1").arg(sMessage));
                returnCode = ERROR_SOCKET;
                thread()->exit(returnCode);
                return;
            }
#ifdef LOG_VERBOSE
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" Sent %1 to: %2")
                           .arg(sMessage)
                           .arg(pUpdateSocket->peerAddress().toString()));
            }
#endif
        }// if(File length Mismatch !!!!)
        else {// OK: File length Match
            file.close();
            QDir renamed;// Remove the .temp exstension
            renamed.rename(destinationDir + sCurrentFileName + QString(".temp"),
                           destinationDir + sCurrentFileName);
            // Go to transfer the next file (if any)
            queryList.removeLast();
            if(!queryList.isEmpty()) {
                bytesReceived = 0;
                QFile tempFile;
                sCurrentFileName = destinationDir + queryList.last().fileName;
                tempFile.setFileName(sCurrentFileName + QString(".temp"));
                if(tempFile.exists()) {
                    bytesReceived = tempFile.size();
                    file.setFileName(sCurrentFileName + QString(".temp"));
                    if(!file.open(QIODevice::Append)) {
                        logMessage(logFile,
                                   Q_FUNC_INFO,
                                   sMyName +
                                   QString(" Unable to open file: %1")
                                   .arg(sCurrentFileName + QString(".temp")));
                        handleOpenFileError();
                        return;
                    }
                }
                QString sMessage = QString("<get>%1,%2,%3</get>")
                                   .arg(queryList.last().fileName)
                                   .arg(bytesReceived)
                                   .arg(CHUNK_SIZE);
                qint64 written = pUpdateSocket->sendTextMessage(sMessage);
                if(written != sMessage.length()) {
                    logMessage(logFile,
                               Q_FUNC_INFO,
                               sMyName +
                               QString(" Error writing %1").arg(sMessage));
                    returnCode = ERROR_SOCKET;
                    thread()->exit(returnCode);
                    return;
                }
#ifdef LOG_VERBOSE
                else {
                    logMessage(logFile,
                               Q_FUNC_INFO,
                               sMyName +
                               QString(" Sent %1 to: %2")
                               .arg(sMessage)
                               .arg(pUpdateSocket->peerAddress().toString()));
                }
#endif
            }
            else {
#ifdef LOG_VERBOSE
                logMessage(logFile,
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" No more file to transfer"));
#endif
                returnCode = TRANSFER_DONE;
                thread()->exit(returnCode);
                return;
            }
        }
    }
}


/*!
 * \brief FileUpdater::handleWriteFileError Write file error handler
 */
void
FileUpdater::handleWriteFileError() {
    file.close();
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Error writing File: %1")
               .arg(file.fileName()));
    returnCode = FILE_ERROR;
    thread()->exit(returnCode);
}


/*!
 * \brief FileUpdater::handleOpenFileError
 */
void
FileUpdater::handleOpenFileError() {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Error Opening File: %1")
               .arg(file.fileName()));
    returnCode = FILE_ERROR;
    thread()->exit(returnCode);
}


/*!
 * \brief FileUpdater::onProcessTextMessage
 * Asynchronously handle the text messages
 * \param sMessage
 *
 * The only message handled is the one conatining the list of files to transfer
 */
void
FileUpdater::onProcessTextMessage(QString sMessage) {
    QString sToken;
    QString sNoData = QString("NoData");
    sToken = XML_Parse(sMessage, "file_list");
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               " " +
               sToken);
#endif
    if(sToken != sNoData) {
        QStringList tmpFileList = QStringList(sToken.split(",", Qt::SkipEmptyParts));
        remoteFileList.clear();
        QStringList tmpList;
        for(int i=0; i< tmpFileList.count(); i++) {
            tmpList =   QStringList(tmpFileList.at(i).split(";", Qt::SkipEmptyParts));
            if(tmpList.count() > 1) {// Both name and size are presents
                files newFile;
                newFile.fileName = tmpList.at(0);
                newFile.fileSize = tmpList.at(1).toLong();
                remoteFileList.append(newFile);
            }
        }
        updateFiles();
    }// file_list
    else {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Nessun file da trasferire"));
#endif
        returnCode = TRANSFER_DONE;
        thread()->exit(returnCode);
    }
}


/*!
 * \brief FileUpdater::updateFiles
 * Helper function to select which files to update.
 */
void
FileUpdater::updateFiles() {
    bool bFound;
    QDir fileDir(destinationDir);
    QFileInfoList localFileInfoList = QFileInfoList();
    if(fileDir.exists()) {// Get the list of the spots already present
        QStringList nameFilter(sFileExtensions.split(" "));
        // Append also the uncompleted files
        nameFilter.append(QString("*.temp"));
        fileDir.setNameFilters(nameFilter);
        fileDir.setFilter(QDir::Files);
        localFileInfoList = fileDir.entryInfoList();
    }
    // Build the list of files to copy from server including the
    // uncompleted ones (since the filenames and length does not match) !
    queryList = QList<files>();
    for(int i=0; i<remoteFileList.count(); i++) {
        bFound = false;
        for(int j=0; j<localFileInfoList.count(); j++) {
            if(remoteFileList.at(i).fileName == localFileInfoList.at(j).fileName() &&
               remoteFileList.at(i).fileSize == localFileInfoList.at(j).size()) {
                bFound = true;
                break;
            }
        }
        if(!bFound) {
            queryList.append(remoteFileList.at(i));
        }
    }
    // Remove the local files not anymore requested
    for(int j=0; j<localFileInfoList.count(); j++) {
        QString sTempFilename = localFileInfoList.at(j).fileName();
        // Remove the file extension (to remove ".temp" if any)
        sTempFilename = sTempFilename.left(sTempFilename.lastIndexOf("."));
        bFound = false;
        for(int i=0; i<remoteFileList.count(); i++) {
            if(remoteFileList.at(i).fileName == localFileInfoList.at(j).fileName() &&
               remoteFileList.at(i).fileSize == localFileInfoList.at(j).size()) {
                bFound = true;
                break;
            }
            if(remoteFileList.at(i).fileName == sTempFilename) {
                bFound = true;
                break;
            }
        }
        if(!bFound) {
            QFile::remove(localFileInfoList.at(j).absoluteFilePath());
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Removed %1").arg(localFileInfoList.at(j).absoluteFilePath()));
#endif
        }
    }
    if(queryList.isEmpty()) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" All files are up to date !"));
#endif
        returnCode = TRANSFER_DONE;
        thread()->exit(returnCode);
        return;
    }
    else {
        askFirstFile();
    }
}


/*!
 * \brief FileUpdater::askFirstFile
 * Utility function for asking the Server to start updating the files
 */
void
FileUpdater::askFirstFile() {
    bytesReceived = 0;
    QFile tempFile;
    sCurrentFileName = queryList.last().fileName;
    tempFile.setFileName(destinationDir + sCurrentFileName + QString(".temp"));
    if(tempFile.exists()) {
        bytesReceived = tempFile.size();
        file.setFileName(destinationDir + sCurrentFileName + QString(".temp"));
        if(!file.open(QIODevice::Append)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Unable to open file: %1")
                       .arg(sCurrentFileName + QString(".temp")));
            handleOpenFileError();
            return;
        }
    }
    QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(sCurrentFileName)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
    qint64 written = pUpdateSocket->sendTextMessage(sMessage);
    if(written != sMessage.length()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Error writing %1").arg(sMessage));
        returnCode = ERROR_SOCKET;
        thread()->exit(returnCode);
        return;
    }
#ifdef LOG_VERBOSE
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Sent %1 to: %2")
                   .arg(sMessage)
                   .arg(pUpdateSocket->peerAddress().toString()));
    }
#endif
}
