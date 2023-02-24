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
#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include <QTimer>
#include <QWidget>
#include <QLabel>
#include <qevent.h>

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    #define horizontalAdvance width
#endif

class MessageWindow : public QWidget
{
    Q_OBJECT

public:
    MessageWindow(QWidget *parent = Q_NULLPTR);
    ~MessageWindow();
    void keyPressEvent(QKeyEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

private slots:
    void onTimeToMoveLabel();

public:
    void setDisplayedText(QString sNewText);

private:
    QPoint newLabelPosition();
    QPalette pal;

private:
    QLabel *pMyLabel;
    QTimer moveTimer;
    const QString sProducer = QString("© G. Salvato - 2023");

};

#endif // MESSAGEWINDOW_H
