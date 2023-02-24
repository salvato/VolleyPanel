#include "volleyapplication.h"

int
main(int argc, char *argv[]) {
    qputenv("QT_LOGGING_RULES","*.debug=false;qt.qpa.*=false"); // supress anoying messages
    QString sVersion = QString("0.1");
    QApplication::setApplicationVersion(sVersion);

    VolleyApplication a(argc, argv);
    int iResult = a.exec();
    return iResult;
}
