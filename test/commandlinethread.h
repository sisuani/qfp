#ifndef COMMANDLINETHREAD_H
#define COMMANDLINETHREAD_H

#include <QThread>
#include <stdio.h>
#include <QDebug>
#include <QCoreApplication>

#include "errorparser.h"
#include "../qfp/src/fiscalprinter.h"

class CommandLineThread : public QThread
{
    Q_OBJECT

public:
    CommandLineThread(QObject *parent = 0, const QString &sbrand="", const QString &smodel="", const QString &sport="") : QThread(parent) {
        FiscalPrinter::Brand brand;
        FiscalPrinter::Model model;
        unsigned int port = sport.toInt();
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
            else
                model = FiscalPrinter::Hasar715F; // Error model
        }


        QString port_type = "COM";

        fp = new FiscalPrinter(0, brand, model, port_type, port, 400);
        ep = new ErrorParser(0, fp);
        connect(fp, SIGNAL(fiscalStatus(bool)), ep, SLOT(fiscalStatus(bool)));
        //fp->statusRequest();
    }

    void run(void) {

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
                    fp->dailyCloseByDate(QDate(2015, 12, 5), QDate(2015, 12, 12));
                    break;
                case 'n':
                    fp->dailyCloseByNumber(12, 14);
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
                    fp->printLineItem("Producto de prueba", 1, 1, "21.00", 'M');
                    //fp->perceptions("2.5", 0.025);
                    //fp->totalTender("Contado", 1, 'T');
                    fp->closeFiscalReceipt('T', 'A', 1 + 0.025);
                    fp->setHeaderTrailer("", "");
                    break;
                case 'B':
                    fp->setCustomerData("Nombre Sr Fac B", "20285142084", 'C', "C", "Juan B. Justo 1234");
                    fp->openFiscalReceipt('B');
                    fp->printLineItem("Producto de prueba", 1.00, 1.00, "21.00", 'M');
                    //fp->generalDiscount("Cupon", 0.50, 'T');
                    //fp->totalTender("Cheque", 1, 'T');
                    fp->closeFiscalReceipt('T', 'B', 1);
                    break;
                case 'C':
                    fp->openFiscalReceipt('B');
                    fp->printLineItem("Producto de prueba", 1, 1, "21.00", 'M');
                    //fp->totalTender("Contado", 1, 'T');
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
                    fp->printLineItem("Producto de prueba", 1, 1, "21.00", 'M');
                    //fp->totalTender("Contado", 1, 'T');
                    fp->closeDNFH(1, 'r', 1);
                    break;
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

private:
    FiscalPrinter *fp;
    ErrorParser *ep;
};

#endif // COMMANDLINETHREAD_H
