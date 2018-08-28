/*
*
* Copyright (C)2018, Samuel Isuani <sisuani@gmail.com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the project's author nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#ifndef FISCALPRINTER_H
#define FISCALPRINTER_H

#include "connector.h"
#include "driverfiscal.h"

class FiscalPrinter : public QObject
{
    Q_OBJECT

public:
    enum Brand {
        Epson,
        Hasar
    };
    Q_DECLARE_FLAGS(Brands, Brand)

    enum Model {
        EpsonTMU220,
        EpsonTM900,
        Hasar320F,
        Hasar330F,
        Hasar615F,
        Hasar715F,
        Hasar1000F,
    };
    Q_DECLARE_FLAGS(Models, Model)

    enum State {
        Ok,
        Error,
        FullFiscalMemory,
        DownloadReport,
        DownloadContinue,
        DownloadFinalize
    };
    Q_DECLARE_FLAGS(States, State)


    FiscalPrinter(QObject *parent = 0, FiscalPrinter::Brand brand = Epson,
            FiscalPrinter::Model model = EpsonTMU220, const QString &port_type = "COM",
            const QString &port = "1", int m_TIME_WAIT = 300);

    ~FiscalPrinter();
    int model();
    bool isOpen();
    bool supportTicket();

    /* commands */
    void statusRequest();
    void dailyClose(const char type);
    void dailyCloseByDate(const QDate &form, const QDate &to);
    void dailyCloseByNumber(const int from, const int to);
    void setCustomerData(const QString &name, const QString &cuit, const char tax_type,
            const QString &doc_type, const QString &address);
    void openFiscalReceipt(const char type);
    void printFiscalText(const QString &text);
    void printLineItem(const QString &description, const qreal quantity,
            const qreal price, const QString &tax, const char qualifier, const qreal excise = 0);
    void perceptions(const QString &desc, qreal tax_amount);
    void subtotal(const char print);
    void totalTender(const QString &description, const qreal amount, const char type);
    void generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type);
    void closeFiscalReceipt(const char intype, const char type, const int id);
    void openNonFiscalReceipt();
    void printNonFiscalText(const QString &text);
    void closeNonFiscalReceipt();
    void openDrawer();
    void setHeaderTrailer(const QString &header, const QString &trailer);
    void setEmbarkNumber(const int doc_num, const QString &description, const char type = ' ');
    void openDNFH(const char type, const char fix_value, const QString  &doc_num);
    void printEmbarkItem(const QString &description, const qreal quantity);
    void closeDNFH(const int id, const char f_type, const int copies);
    void receiptText(const QString &text);
    void reprintDocument(const QString &doc_type, const int doc_number);
    void reprintContinue();
    void reprintFinalize();
    void cancel();
    void ack();
    void setDateTime(const QDateTime &dateTime);
    void setFixedData(const QString &shop, const QString &phone);

    void getTransactionalMemoryInfo();
    void downloadReportByDate(const QString &type, const QDate &form, const QDate &to);
    void downloadReportByNumber(const QString &type, const int from, const int to);
    void downloadContinue();
    void downloadFinalize();
    void downloadDelete(const int to);

signals:
    void fiscalReceiptNumber(int, int, int);
    void fiscalStatus(int);
    void fiscalData(int, QVariant);

private:
    Connector *m_connector;
    DriverFiscal *m_driverFiscal;
    int m_model;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FiscalPrinter::Brands)
Q_DECLARE_OPERATORS_FOR_FLAGS(FiscalPrinter::Models)

#endif // FISCALPRINTER_H
