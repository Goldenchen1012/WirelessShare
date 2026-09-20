#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("WirelessShare"));
    QCoreApplication::setApplicationName(QStringLiteral("WirelessShare"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.5"));
    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
    if (!QCoreApplication::arguments().contains(QStringLiteral("--background")))
        w.show();
    return a.exec();
}
