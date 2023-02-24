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
#ifndef SLIDEWINDOW_H
#define SLIDEWINDOW_H

#include <QTimer>
#include <QLabel>
#include <QFileInfoList>

#include <qevent.h>


class SlideWindow : public QLabel
{
    Q_OBJECT

public:
    SlideWindow(QWidget *parent = Q_NULLPTR);
    ~SlideWindow();
    void setSlideDir(QString sNewDir);
    void keyPressEvent(QKeyEvent *event);
    void addNewImage(QImage image);
    void startSlideShow();
    void stopSlideShow();
    void pauseSlideShow();
    bool isReady();
    bool isRunning();

public:
    /*!
     * \brief The transitionMode enum
     */
    enum transitionMode {
        transition_Abrupt,/*!< Abrupt transition */
        transition_FromLeft,/*!< Enter from Left */
        transition_Fade/*!< Fade Out - Fade In */
    };

private:
    void computeRegions(QRect* sourcePresent, QRect* destinationPresent, QRect* sourceNext, QRect* destinationNext);
    void updateSlideList();

public slots:
    void onNewSlideTimer();
    void onTransitionTimeElapsed();
    void resizeEvent(QResizeEvent *event);

private:
    QString sSlideDir;
    QFileInfoList slideList;
    QImage* pPresentImage;
    QImage* pNextImage;
    QImage* pPresentImageToShow;
    QImage* pNextImageToShow;
    QImage* pShownImage;

    QTimer showTimer;
    QTimer transitionTimer;

    int iCurrentSlide;
    int steadyShowTime;
    int transitionTime;
    int transitionGranularity;
    int transitionStepNumber;
    QSize mySize;
    QRect rectSourcePresent;
    QRect rectSourceNext;
    QRect rectDestinationPresent;
    QRect rectDestinationNext;

    transitionMode transitionType;
    bool bRunning;
    QPalette           panelPalette;
    QLinearGradient    panelGradient;
    QBrush             panelBrush;
};

#endif // SLIDEWINDOW_H
