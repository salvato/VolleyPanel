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
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

#include "utility.h"


/*!
 * \brief XML_Parse Very simple XML Parser
 * \param input_string: the string to parse
 * \param token: the token to look for
 * \return XML_Parse("<score>1</score>","score") will return QString("1") or QString() on error
 */
QString
XML_Parse(QString input_string, QString token) {
    QString start_token, end_token, result = QString("NoData");
    start_token = "<" + token + ">";
    end_token = "</" + token + ">";

    int start_pos=-1, end_pos=-1, len=0;

    start_pos = input_string.indexOf(start_token);
    end_pos   = input_string.indexOf(end_token);

    if(start_pos < 0 || end_pos < 0) {
        result = QString("NoData");
    }
    else {
        start_pos += start_token.length();
        len = end_pos - start_pos;
        if(len>0)
            result = input_string.mid(start_pos, len);
        else
            result = "";
    }

    return result;
}


/*!
 * \brief logMessage Log messages on a file (if enabled) or on stdout
 * \param logFile The file where to write the log
 * \param sFunctionName The Function which requested to write the message
 * \param sMessage The informative message
 */
void
logMessage(QFile *logFile, QString sFunctionName, QString sMessage) {
    QDateTime dateTime;
    QString sDebugMessage = dateTime.currentDateTime().toString() +
                            QString(" - ") +
                            sFunctionName +
                            QString(" - ") +
                            sMessage;
    if(logFile) {
        if(logFile->isOpen()) {
            logFile->write(sDebugMessage.toUtf8().data());
            logFile->write("\n");
            logFile->flush();
        }
        else
            qDebug() << sDebugMessage;
    }
    else
        qDebug() << sDebugMessage;
}


