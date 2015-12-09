#include <QCoreApplication>

#include "commandlinethread.h"


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if(argc < 4) {
        qDebug() << "Usage: ./fp brand model port";
        return 0;
    }

    CommandLineThread cmdt(0, argv[1], argv[2], argv[3]);
    cmdt.start();


    return app.exec();
}
