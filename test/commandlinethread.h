#ifndef COMMANDLINETHREAD_H
#define COMMANDLINETHREAD_H

#include <stdio.h>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>

#include "errorparser.h"
#include "../qfp/src/fiscalprinter.h"

class CommandLineThread : public QObject
{
    Q_OBJECT

public:
    CommandLineThread(QObject *parent = 0, const QString &sbrand="", const QString &smodel="", const QString &shost= "", const QString &sport="") : QObject(parent) {
        FiscalPrinter::Brand brand;
        FiscalPrinter::Model model;
        unsigned int port = sport.toInt();
        QNetworkAccessManager *networkManager;
        if(sbrand.compare("epson") == 0) {
            brand = FiscalPrinter::Epson;

            if(smodel.compare("220") == 0)
                model = FiscalPrinter::EpsonTMU220;
            else if(smodel.compare("900") == 0)
                model = FiscalPrinter::EpsonTM900;
            else
                model = FiscalPrinter::EpsonTMU220; // Error model
        } else {
            brand = FiscalPrinter::Hasar;

            if(smodel.compare("320") == 0)
                model = FiscalPrinter::Hasar320F;
            else if(smodel.compare("330") == 0)
                model = FiscalPrinter::Hasar330F;
            else if(smodel.compare("615") == 0)
                model = FiscalPrinter::Hasar615F;
            else if(smodel.compare("715") == 0)
                model = FiscalPrinter::Hasar715F;
            else if(smodel.compare("5100") == 0 || smodel.compare("1000") == 0 || smodel.compare("250") == 0) {
                networkManager = new QNetworkAccessManager(this);
                model = FiscalPrinter::Hasar1000F;
            } else
                model = FiscalPrinter::Hasar715F; // Error model
        }

        fp = new FiscalPrinter(0, brand, model, shost, port, 400);
        ep = new ErrorParser(0, fp);
        connect(fp, SIGNAL(fiscalStatus(int)), ep, SLOT(fiscalStatus(int)));
        connect(fp, SIGNAL(fiscalData(int, QVariant)), this, SLOT(fiscalData(int, QVariant)));
        //fp->statusRequest();

        while (true) {
            int c = 0;
            while((c = getchar())) {
                switch (c) {
                    case 's':
                        fp->statusRequest();
                        break;
                    case 'x':
                        fp->dailyClose('X');
                        break;
                    case 'z':
                        fp->dailyClose('Z');
                        break;
                    case 'd':
                        fp->dailyCloseByDate(QDate(2016, 2, 1), QDate(2016, 2, 8));
                        break;
                    case 'n':
                        fp->dailyCloseByNumber(1, 12);
                        break;
                    case 'f':
                        fp->setHeaderTrailer("", "test prueba si");
                        fp->openNonFiscalReceipt();
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->printNonFiscalText("Prueba de texto con corte automatico, maximo caracteres 39 por linea");
                        fp->closeNonFiscalReceipt();
                        fp->setHeaderTrailer("", "");
                        break;
                    case 'A':
                        fp->setHeaderTrailer("", "test prueba si");
                        fp->setEmbarkNumber(1, "001-00001-00000001");
                        fp->setCustomerData("Nombre Sr Fac A", "20285142084", 'I', "C", "Juan B. Justo 1234");
                        fp->openFiscalReceipt('A');
                        fp->printLineItem("Producto de prueba", 1, 100, "21.00", 'M');
                        //fp->printLineItem("Producto de prueba", 1, 50, "10.50", 'M');
                        fp->generalDiscount("Descuento", 12, 21.0, 'm');
                        //fp->generalDiscount("Descuento", 8, 10.5, 'm');
                        //fp->perceptions("2.5", 0.025);
                        //fp->totalTender("Contado", 1, 'T');
                        fp->closeFiscalReceipt('T', 'A', 1 + 0.025);
                        fp->setHeaderTrailer("", "");
                        break;
                    case 'B':
                        fp->setCustomerData("Nombre Sr Fac B", "20285142084", 'C', "C", "Juan B. Justo 1234");
                        fp->openFiscalReceipt('B');
                        fp->printLineItem("Producto de prueba", 1.00, 10.00, "10.50", 'M');
                        //fp->generalDiscount("Descuento", 0.50, 'm');
                        fp->totalTender("Efectivo", 5, 'T');
                        fp->closeFiscalReceipt('T', 'B', 1);
                        break;
                    case 'C':
                        fp->openFiscalReceipt('B');
                        fp->printLineItem("Producto de prueba", 1, 1, "10.50", 'M');
                        //fp->generalDiscount("Descuento", 0.50, 'm');
                        ///fp->totalTender("Contado", 1, 'T');
                        fp->closeFiscalReceipt('T', 'B', 1);
                        break;
                    case 'T':
                        fp->openFiscalReceipt('T');
                        fp->printLineItem("Producto de prueba", 1, 1, "21.00", 'M');
                        //fp->totalTender("Contado", 0.5, 'T');
                        //fp->totalTender("Tarjeta", 0.5, 'T');
                        fp->closeFiscalReceipt('0', '0', 1);
                        break;
                    case 'M':
                        fp->openFiscalReceipt('M');
                        fp->printLineItem("Producto de prueba", 1, 1, "21.00", 'M');
                        //fp->totalTender("Contado", 1, 'T');
                        fp->closeFiscalReceipt('0', '0', 1);
                        break;
                    case 'D':
                        //fp->setEmbarkNumber(1, "001-99999-0000678");
                        fp->setCustomerData("Nombre Sr Fac A", "20285142084", 'I', "C", "Juan B. Justo 1234");
                        fp->openDNFH('S', 'T', "123-45678-99999990");
                        fp->printLineItem("Producto de prueba", 1, 10, "21.00", 'M');
                        //fp->generalDiscount("Descuento", 5, 'm');
                        //fp->perceptions("Perc. Ej", 1);
                        //fp->totalTender("Contado", 1, 'T');
                        fp->closeDNFH(1, 'r', 1);
                        break;
                    case 'r':
                        fp->reprintDocument("081", 20);
                        fp->reprintContinue();
                        fp->reprintFinalize();
                        break;
                    case 'i':
                        fp->getTransactionalMemoryInfo();
                        pause();
                        pause();
                        fp->downloadReportByNumber("CTD", 1, 10);
                        pause();
                        fp->downloadContinue();
                        fp->downloadContinue();
                        pause();
                        fp->downloadFinalize();
                    case 'w':
                        fp->openDrawer();
                        break;
                    case 'c':
                        fp->cancel();
                        break;
                    case 't':
                        fp->setDateTime(QDateTime::currentDateTime());
                        //fp->totalTender("Cancelar", 0.00, 'C');
                        break;
                    case 'O':
                        fp->setFixedData("12346", "08001233333");
                        break;
                    case 'm':
                        for(int i = 0; i < 40; i++) {
                            fp->ack();
                        }
                        break;
                    case 'q':
                        qApp->quit();
                }

            }
        }
    }

private slots:
    void fiscalData(int cmd, QVariant data) {
        qDebug() << "CMD: " << cmd;
        qDebug() << "DATA: " << data;
    }

private:
    void pause() {
        qDebug() << "PASUSE";
        QFile in;
        in.open(stdin, QIODevice::ReadOnly);
        QString line = in.readLine();
    }

    FiscalPrinter *fp;
    ErrorParser *ep;
};

#endif // COMMANDLINETHREAD_H
