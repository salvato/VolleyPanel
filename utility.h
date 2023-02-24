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
#ifndef UTILITY_H
#define UTILITY_H

#include <QString>
#include <QFile>

//#define LOG_MESG
//#define LOG_VERBOSE
//#define LOG_VERBOSE_VERBOSE

#define VOLLEY_PANEL   0
#define FIRST_PANEL  VOLLEY_PANEL
#define BASKET_PANEL   1
#define HANDBALL_PANEL 2
#define LAST_PANEL   HANDBALL_PANEL


enum commands {
    AreYouThere    = char(0xAA),
    Stop           = char(0x01),
    Start          = char(0x02),
    Start14        = char(0x04),
    NewGame        = char(0x11),
    RadioInfo      = char(0x21),
    Configure      = char(0x31),
    Time           = char(0x41),
    Possess        = char(0x42),
    StartSending   = char(0x81),
    StopSending    = char(0x82)
};


QString XML_Parse(QString input_string, QString token);
void logMessage(QFile *logFile, QString sFunctionName, QString sMessage);

#endif // UTILITY_H
