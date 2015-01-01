#include <QCoreApplication>

#include "commandlinethread.h"


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if(argc < 3) {
        qDebug() << "Usage: ./fp brand model";
        return 0;
    }

    CommandLineThread cmdt(0, argv[1], argv[2]);
    cmdt.start();


    return app.exec();
}
