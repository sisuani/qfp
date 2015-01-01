#ifndef ERRORPARSER_H
#define ERRORPARSER_H
#include <QObject>

#include "../qfp/src/fiscalprinter.h"

class ErrorParser : public QObject
{
    Q_OBJECT

public:
    ErrorParser(QObject *parent = 0, FiscalPrinter *fp = 0);

public slots:
    void fiscalStatus(bool ok);

private:
    FiscalPrinter *fp;
    int error_count;
};

#endif
