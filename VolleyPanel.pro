QT += core
QT += gui
QT += websockets
QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    fileupdater.cpp \
    main.cpp \
    messagewindow.cpp \
    scorepanel.cpp \
    serverdiscoverer.cpp \
    slidewindow.cpp \
    timeoutwindow.cpp \
    utility.cpp \
    volleyapplication.cpp \
    volleypanel.cpp


HEADERS += \
    fileupdater.h \
    messagewindow.h \
    panelorientation.h \
    scorepanel.h \
    serverdiscoverer.h \
    slidewindow.h \
    timeoutwindow.h \
    utility.h \
    volleyapplication.h \
    volleypanel.h


TRANSLATIONS += \
    VolleyPanel_en_US.ts

#    INCLUDEPATH += /usr/local/include
#    LIBS += -L"/usr/local/lib" -lpigpiod_if2 # To include libpigpiod_if2.so from /usr/local/lib

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    VolleyPanel.qrc
