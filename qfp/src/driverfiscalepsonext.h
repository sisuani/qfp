/*
*
* Copyright (C)2013, Samuel Isuani <sisuani@gmail.com>
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

#ifndef DRIVERFISCALEPSONEXT_H
#define DRIVERFISCALEPSONEXT_H

#include "driverfiscal.h"
#include "packageepsonext.h"
#include "fiscalprinter.h"

class DriverFiscalEpsonExt : public QThread, virtual public DriverFiscal
{
    Q_OBJECT

public:
    explicit DriverFiscalEpsonExt(QObject *parent = 0, SerialPort *m_serialPort = 0);

    enum {
        CMD_OPENTICKET              = 0x40,
        CMD_SETDATETIME             = 0x58,
        CMD_OPENFISCALRECEIPT       = 0x60,
        CMD_PRINTLINEITEM_TICKET    = 0x42,
        CMD_PRINTLINEITEM_INVOICE   = 0x62,
        //CMD_GENERALDISCOUNT       = 0x,
        CMD_SUBTOTAL                = 0x63,
        CMD_PERCEPTIONS             = 0x66,
        CMD_TOTALTENDER_TICKET      = 0x44,
        CMD_TOTALTENDER_INVOICE     = 0x64,
        CMD_CLOSEFISCALRECEIPT_TICKET = 0x45,
        CMD_CLOSEFISCALRECEIPT_INVOICE = 0x65,
        CMD_CLOSEFISCALRECEIPT_INVOICE_CN = 0x66,
        CMD_CLOSEDNFH               = 0xAB,
        CMD_CONTINUEAUDIT           = 0xF0,
        CMD_REPRINTDOCUMENT         = 0xF1,
        CMD_REPRINTCONTINUE         = 0xF2,
        CMD_REPRINTFINALIZE         = 0xF3,
        CMD_GETTRANSACTIONALMEMORYINFO = 0xF4,
        CMD_DOWNLOADREPORTBYDATE    = 0xF5,
        CMD_DOWNLOADREPORTBYNUMBER  = 0xF6,
        CMD_DOWNLOADCONTINUE        = 0xF7,
        CMD_DOWNLOADFINALIZE        = 0xF8,
        CMD_DOWNLOADDELETE          = 0xF9
    };

    void setModel(const FiscalPrinter::Model model);

    virtual QByteArray readData(const int pkg_cmd, const QByteArray &secuence);
    virtual int getReceiptNumber(const QByteArray &data);
    virtual bool getStatus(const QByteArray &data);
    virtual bool verifyResponse(const QByteArray &bytes, const int pkg_cmd);
    virtual bool checkSum(const QString &data);

    virtual void statusRequest();
    virtual void dailyClose(const char type);
    virtual void dailyCloseByDate(const QDate &form, const QDate &to);
    virtual void dailyCloseByNumber(const int from, const int to);
    virtual void setCustomerData(const QString &name, const QString &cuit, const char tax_type,
            const QString &doc_type, const QString &address);
    virtual void openFiscalReceipt(const char type);
    virtual void printFiscalText(const QString &text);
    virtual void printLineItem(const QString &description, const qreal quantity,
            const qreal price, const QString &tax, const char qualifier, const qreal excise);
    virtual void perceptions(const QString &desc, qreal tax_amount);
    virtual void subtotal(const char print);
    virtual void generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type);
    virtual void totalTender(const QString &description, const qreal amount, const char type);
    virtual void closeFiscalReceipt(const char intype, const char type, const int id);
    virtual void openNonFiscalReceipt();
    virtual void printNonFiscalText(const QString &text);
    virtual void closeNonFiscalReceipt();
    virtual void openDrawer();
    virtual void setHeaderTrailer(const QString &header, const QString &trailer);
    virtual void setEmbarkNumber(const int doc_num, const QString &description, const char type);
    virtual void openDNFH(const char type, const char fix_value, const QString &doc_num);
    virtual void printEmbarkItem(const QString &description, const qreal quantity);
    virtual void closeDNFH(const int id, const char f_type, const int copies);
    virtual void receiptText(const QString &text);
    virtual void reprintDocument(const QString &doc_type, const int doc_number);
    virtual void reprintContinue();
    virtual void reprintFinalize();
    virtual void cancel();
    virtual void ack();
    virtual void setDateTime(const QDateTime &dateTime);
    virtual void setFixedData(const QString &shop, const QString &phone);
    virtual void finish();

    virtual void getTransactionalMemoryInfo();
    virtual void downloadReportByDate(const QString &type, const QDate &form, const QDate &to);
    virtual void downloadReportByNumber(const QString &type, const int from, const int to);
    virtual void downloadContinue();
    virtual void downloadFinalize();
    virtual void downloadDelete(const int to);

signals:
    void fiscalReceiptNumber(int id, int number, int type); // type == 0 Factura, == 1 NC
    void fiscalStatus(int state);
    void fiscalData(int cmd, QVariant data);

protected:
    void run();

private:
    bool m_error;
    bool m_isinvoice;
    bool m_iscreditnote;
    QVector<PackageEpsonExt *> queue;
    FiscalPrinter::Model m_model;
    int m_nak_count;

    void clear();
    void sendAck();
    void verifyIntermediatePackage(QByteArray &bufferBytes);
    void setFooter(int line, const QString &text);
    bool checkSum(const QByteArray &data);
    bool processStatus(const QByteArray &data);
    void continueAudit();
    void closeAudit();
    QString m_name;
    QString m_cuit;
    char m_tax_type;
    QString m_doc_type;
    QString m_address;
    QString m_address1;
    QString m_refer;

};

#endif // DRIVERFISCALEPSONEXT_H

