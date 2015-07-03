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

#ifndef DRIVERFISCALHASAR_H
#define DRIVERFISCALHASAR_H

#include <QThread>

#include "driverfiscal.h"
#include "packagehasar.h"
#include "fiscalprinter.h"

class DriverFiscalHasar : public QThread, virtual public DriverFiscal
{
    Q_OBJECT

public:
    explicit DriverFiscalHasar(QObject *parent = 0, SerialPort *m_serialPort = 0, int m_TIME_WAIT = 300);

    enum {
        CMD_OPENFISCALRECEIPT       = 0x40,
        CMD_PRINTFISCALTEXT         = 0x41,
        CMD_PRINTLINEITEM           = 0x42,
        //CMD_GENERALDISCOUNT       = 0x,
        CMD_SUBTOTAL                = 0x43,
        CMD_TOTALTENDER             = 0x44,
        CMD_CLOSEFISCALRECEIPT      = 0x45,
        CMD_PERCEPTIONS             = 0x60,
        CMD_SETCUSTOMERDATA         = 0x62,
        CMD_GENERALDISCOUNT         = 0x54,
        CMD_SETHEADERTRAILER        = 0x5d,
        CMD_SETDATETIME             = 0x58,
        CMD_OPENDNFH                = 0x80,
        CMD_PRINTEMBARKITEM         = 0x82,
        CMD_CLOSEDNFH               = 0x81,
        CMD_EMBARKNUMBER            = 0x93,
        CMD_CANCEL                  = 0x98,
        CMD_RECEIPTTEXT             = 0x97
    };

    void setModel(const FiscalPrinter::Model model);


    virtual QByteArray readData(const int pkg_cmd);
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
            const qreal price, const QString &tax, const char qualifier);
    virtual void perceptions(const QString &desc, qreal tax_amount);
    virtual void subtotal(const char print);
    virtual void generalDiscount(const QString &description, const qreal amount, const char type);
    virtual void totalTender(const QString &description, const qreal amount, const char type);
    virtual void closeFiscalReceipt(const char intype, const char f_type, const int id);
    virtual void openNonFiscalReceipt();
    virtual void printNonFiscalText(const QString &text);
    virtual void closeNonFiscalReceipt();
    virtual void openDrawer();
    virtual void setHeaderTrailer(const QString &header, const QString &trailer);
    virtual void setEmbarkNumber(const int doc_num, const QString &description);
    virtual void openDNFH(const char type, const char fix_value, const QString &doc_num);
    virtual void printEmbarkItem(const QString &description, const qreal quantity);
    virtual void closeDNFH(const int id, const char f_type, const int copies);
    virtual void receiptText(const QString &text);
    virtual void cancel();
    virtual void ack();
    virtual void setDateTime(const QDateTime &dateTime);
    virtual void setFixedData(const QString &shop, const QString &phone);
    virtual void finish();

signals:
    void fiscalReceiptNumber(int id, int number, int type); // type == 0 Factura, == 1 NC
    void fiscalStatus(bool ok);

protected:
    void run();

private:
    void sendAck();
    void errorHandler();
    bool m_error;
    QVector<PackageHasar *> queue;
    FiscalPrinter::Model m_model;
    int errorHandler_count;
    int m_nak_count;
};

#endif // DRIVERFISCALHASAR_H

