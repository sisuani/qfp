#include <QObject>
#include <QDebug>

#include "errorparser.h"


ErrorParser::ErrorParser(QObject *parent, FiscalPrinter *fp)
    : QObject(parent)
    , fp(fp)
{
    error_count = 0;
}

void ErrorParser::fiscalStatus(bool ok) {
    if(!ok && error_count < 3) {
        error_count++;
        qDebug() << "REPARANDO ERROR";
        fp->totalTender("Contado", 1, 'T');
        fp->closeFiscalReceipt('T', 'A', -1);
        fp->closeNonFiscalReceipt();
        fp->statusRequest();
    }

    if(error_count >= 3) {
        qDebug() << "EL ERROR NO SE PUDO REPARAR, CERRAR TODO";
    }
}
