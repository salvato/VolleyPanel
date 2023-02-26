#include <QtGlobal>
#include <QtNetwork>
#include <QtWidgets>
#include <QProcess>
#include <QWebSocket>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QTime>

#include "volleypanel.h"
#include "timeoutwindow.h"
#include "utility.h"

VolleyPanel::VolleyPanel(const QString& myServerUrl, QFile *myLogFile, QWidget *parent)
    : ScorePanel(myServerUrl, myLogFile, parent)
    , iServizio(0)
    , maxTeamNameLen(15)
    , pTimeoutWindow(Q_NULLPTR)
{
    sFontName = QString("Liberation Sans Bold");
    fontWeight = QFont::Black;

    QSize panelSize = QGuiApplication::primaryScreen()->geometry().size();
    iTeamFontSize    = std::min(panelSize.height()/8,
                                int(panelSize.width()/(2.2*maxTeamNameLen)));
    iScoreFontSize   = std::min(panelSize.height()/4,
                                int(panelSize.width()/9));
    iLabelsFontSize  = panelSize.height()/8; // 2 Righe
    iTimeoutFontSize = panelSize.height()/8; // 2 Righe
    iSetFontSize     = panelSize.height()/8; // 2 Righe

    connect(pPanelServerSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));
    connect(pPanelServerSocket, SIGNAL(binaryMessageReceived(QByteArray)),
            this, SLOT(onBinaryMessageReceived(QByteArray)));

    pSettings = new QSettings("Gabriele Salvato", "Segnapunti Volley");

    // QWidget propagates explicit palette roles from parent to child.
    // If you assign a brush or color to a specific role on a palette and
    // assign that palette to a widget, that role will propagate to all
    // the widget's children, overriding any system defaults for that role.
    panelPalette = QWidget::palette();
    panelGradient = QLinearGradient(0.0, 0.0, 0.0, height());
    panelGradient.setColorAt(0, QColor(0, 0, START_GRADIENT));
    panelGradient.setColorAt(1, QColor(0, 0, END_GRADIENT));
    panelBrush = QBrush(panelGradient);
    panelPalette.setBrush(QPalette::Active,   QPalette::Window, panelBrush);
    panelPalette.setBrush(QPalette::Inactive, QPalette::Window, panelBrush);
    panelPalette.setColor(QPalette::WindowText,    Qt::yellow);
    panelPalette.setColor(QPalette::Base,          Qt::black);
    panelPalette.setColor(QPalette::AlternateBase, Qt::blue);
    panelPalette.setColor(QPalette::Text,          Qt::yellow);
    panelPalette.setColor(QPalette::BrightText,    Qt::white);
    setPalette(panelPalette);


    pTimeoutWindow = new TimeoutWindow(Q_NULLPTR);
    connect(pTimeoutWindow, SIGNAL(doneTimeout()),
            this, SLOT(onTimeoutDone()));

    createPanelElements();
    buildLayout();
}


VolleyPanel::~VolleyPanel() {
    if(pSettings) delete pSettings;
}


void
VolleyPanel::closeEvent(QCloseEvent *event) {
    if(pSettings != Q_NULLPTR) delete pSettings;
    pSettings = Q_NULLPTR;
    ScorePanel::closeEvent(event);
    event->accept();
}


void
VolleyPanel::onTimeoutDone() {
    show();
    pTimeoutWindow->hide();
}

void
VolleyPanel::onBinaryMessageReceived(QByteArray baMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(baMessage.size()));
    ScorePanel::onBinaryMessageReceived(baMessage);
}

void
VolleyPanel::onTextMessageReceived(QString sMessage) {
    QString sToken;
    bool ok;
    int iVal;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "team0");
    if(sToken != sNoData){
        team[0]->setText(sToken.left(maxTeamNameLen));
    }// team0

    sToken = XML_Parse(sMessage, "team1");
    if(sToken != sNoData){
        team[1]->setText(sToken.left(maxTeamNameLen));
    }// team1

    sToken = XML_Parse(sMessage, "set0");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>3)
            iVal = 8;
        set[0]->setText(QString("%1").arg(iVal));
    }// set0

    sToken = XML_Parse(sMessage, "set1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>3)
            iVal = 8;
        set[1]->setText(QString("%1").arg(iVal));
    }// set1

    sToken = XML_Parse(sMessage, "timeout0");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>2)
            iVal = 8;
        timeout[0]->setText(QString("%1"). arg(iVal));
    }// timeout0

    sToken = XML_Parse(sMessage, "timeout1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>2)
            iVal = 8;
        timeout[1]->setText(QString("%1"). arg(iVal));
    }// timeout1

    sToken = XML_Parse(sMessage, "startTimeout");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0)
            iVal = 30;
        pTimeoutWindow->startTimeout(iVal*1000);
        pTimeoutWindow->showFullScreen();
        hide();
    }// timeout1

    sToken = XML_Parse(sMessage, "stopTimeout");
    if(sToken != sNoData) {
        pTimeoutWindow->stopTimeout();
        show();
        pTimeoutWindow->hide();
    }// timeout1

    sToken = XML_Parse(sMessage, "score0");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>99)
            iVal = 99;
        score[0]->setText(QString("%1").arg(iVal));
    }// score0

    sToken = XML_Parse(sMessage, "score1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>99)
            iVal = 99;
        score[1]->setText(QString("%1").arg(iVal));
    }// score1

    sToken = XML_Parse(sMessage, "servizio");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<-1 || iVal>1)
            iVal = 0;
        iServizio = iVal;
        if(iServizio == -1) {
            servizio[0]->setText(" ");
            servizio[1]->setText(" ");
        } else if(iServizio == 0) {
            servizio[0]->setPixmap(*pPixmapService);
            servizio[1]->setText(" ");
        } else if(iServizio == 1) {
            servizio[0]->setText(" ");
            servizio[1]->setPixmap(*pPixmapService);
        }
    }// servizio

    ScorePanel::onTextMessageReceived(sMessage);
}


void
VolleyPanel::createPanelElements() {
    // QWidget propagates explicit palette roles from parent to child.
    // If you assign a brush or color to a specific role on a palette and
    // assign that palette to a widget, that role will propagate to all
    // the widget's children, overriding any system defaults for that role.

    // Timeout
    timeoutLabel = new QLabel("Timeout");
    timeoutLabel->setFont(QFont(sFontName, iLabelsFontSize/2, fontWeight));
    timeoutLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++) {
        timeout[i] = new QLabel("8");
        timeout[i]->setFrameStyle(QFrame::NoFrame);
        timeout[i]->setFont(QFont(sFontName, iTimeoutFontSize, fontWeight));
    }

    // Set
    setLabel = new QLabel(tr("Set"));
    setLabel->setFont(QFont(sFontName, iLabelsFontSize/2, fontWeight));
    setLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++) {
        set[i] = new QLabel("8");
        set[i]->setFrameStyle(QFrame::NoFrame);
        set[i]->setFont(QFont(sFontName, iSetFontSize, fontWeight));
    }

    // Score
//    scoreLabel = new QLabel(tr("Punti"));
    scoreLabel = new QLabel(tr(""));
    scoreLabel->setFont(QFont(sFontName, iLabelsFontSize, fontWeight));
    scoreLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++){
        score[i] = new QLabel("88");
        score[i]->setAlignment(Qt::AlignHCenter);
        score[i]->setFont(QFont(sFontName, iScoreFontSize, fontWeight));
        score[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Servizio
    for(int i=0; i<2; i++){
        servizio[i] = new QLabel(" ");
        servizio[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Teams
    QPalette pal = panelPalette;
    pal.setColor(QPalette::WindowText, Qt::white);
    for(int i=0; i<2; i++) {
        team[i] = new QLabel();
        team[i]->setPalette(pal);
        team[i]->setFont(QFont(sFontName, iTeamFontSize, fontWeight));
        team[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    team[0]->setText(tr("Locali"));
    team[1]->setText(tr("Ospiti"));
}


QGridLayout*
VolleyPanel::createPanel() {
    QGridLayout *layout = new QGridLayout();

    int ileft  = 0;
    int iright = 1;
    if(isMirrored) {
        ileft  = 1;
        iright = 0;
    }
    QPixmap* pixmapLeftTop = new QPixmap(":/Logo_UniMe.png");
    QLabel* leftTopLabel = new QLabel();
    leftTopLabel->setPixmap(*pixmapLeftTop);

    QPixmap* pixmapRightTop = new QPixmap(":/SSD_UniMe.png");
    QLabel* rightTopLabel = new QLabel();
    rightTopLabel->setPixmap(*pixmapRightTop);

    pPixmapService = new QPixmap(":/ball2.png");
    *pPixmapService = pPixmapService->scaled(2*iLabelsFontSize/3, 2*iLabelsFontSize/3);

    layout->addWidget(team[ileft],      0, 0, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(team[iright],     0, 6, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);

    layout->addWidget(score[ileft],     2, 1, 4, 3, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(servizio[ileft],  2, 4, 4, 1, Qt::AlignLeft   |Qt::AlignTop);
    layout->addWidget(scoreLabel,       2, 5, 4, 2, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(servizio[iright], 2, 7, 4, 1, Qt::AlignRight  |Qt::AlignTop);
    layout->addWidget(score[iright],    2, 8, 4, 3, Qt::AlignHCenter|Qt::AlignVCenter);

    layout->addWidget(set[ileft],       6, 2, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(setLabel,         6, 3, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(set[iright],      6, 9, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);

    layout->addWidget(leftTopLabel,     8, 0, 2, 2, Qt::AlignLeft   |Qt::AlignBottom);
    layout->addWidget(timeout[ileft],   8, 2, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(timeoutLabel,     8, 3, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(timeout[iright],  8, 9, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(rightTopLabel,    8,10, 2, 2, Qt::AlignRight  |Qt::AlignBottom);

    return layout;
}


void
VolleyPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("%1  %2")
                   .arg(setLabel->text())
                   .arg(scoreLabel->text()));
#endif
        setLabel->setText(tr("Set"));
        scoreLabel->setText(tr("Punti"));
    } else
        QWidget::changeEvent(event);
}

