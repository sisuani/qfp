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

#ifndef DRIVERFISCAL_H
#define DRIVERFISCAL_H

#include <QVector>
#include <QThread>
#include <QDate>
#include <QDebug>

#include "networkport.h"
#include "serialport.h"
#include "packagefiscal.h"

#define LOGGER 1

#define TIME_WAIT 350
#define MAX_TW 150

class DriverFiscal
{

public:
    DriverFiscal(QObject *parent = 0, SerialPort *m_serialPort = 0, int m_TIME_WAIT = 300) : m_serialPort(m_serialPort) {};
    DriverFiscal(QObject *parent = 0, NetworkPort *m_networkPort= 0, int m_TIME_WAIT = 300) : m_networkPort(m_networkPort) {};
    virtual ~DriverFiscal() {};

    enum {
        CMD_STATUS                  = 0x2a,
        CMD_DAILYCLOSE              = 0x39,
        CMD_DAILYCLOSEBYDATE        = 0x3a,
        CMD_DAILYCLOSEBYNUMBER      = 0x3b,
        CMD_OPENNONFISCALRECEIPT    = 0x48,
        CMD_PRINTNONTFISCALTEXT     = 0x49,
        CMD_CLOSENONFISCALRECEIPT   = 0x4a,
        CMD_OPENDRAWER              = 0x7b
    };

    virtual QByteArray readData(const int pkg_cmd, const QByteArray &secuence) = 0;
    virtual int getReceiptNumber(const QByteArray &data) = 0;
    virtual bool verifyResponse(const QByteArray &bytes, const int pkg_cmd) = 0;
    virtual bool checkSum(const QString &data) = 0;

    virtual void statusRequest() = 0;
    virtual void dailyClose(const char type) = 0;
    virtual void dailyCloseByDate(const QDate &form, const QDate &to) = 0;
    virtual void dailyCloseByNumber(const int from, const int to) = 0;
    virtual void setCustomerData(const QString &name, const QString &cuit, const char tax_type,
            const QString &doc_type, const QString &address) = 0;
    virtual void openFiscalReceipt(const char type) = 0;
    virtual void printFiscalText(const QString &text) = 0;
    virtual void printLineItem(const QString &description, const qreal quantity,
            const qreal price, const QString &tax, const char qualifier, const qreal excise) = 0;
    virtual void perceptions(const QString &desc, qreal tax_amount) = 0;
    virtual void subtotal(const char print) = 0;
    virtual void generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type) = 0;
    virtual void totalTender(const QString &description, const qreal amount, const char type) = 0;
    virtual void closeFiscalReceipt(const char intype, const char type, const int id) = 0;
    virtual void openNonFiscalReceipt() = 0;
    virtual void printNonFiscalText(const QString &text) = 0;
    virtual void closeNonFiscalReceipt() = 0;
    virtual void openDrawer() = 0;
    virtual void setHeaderTrailer(const QString &header, const QString &trailer) = 0;
    virtual void setEmbarkNumber(const int doc_num, const QString &description) = 0;
    virtual void openDNFH(const char type, const char fix_value, const QString &doc_num) = 0;
    virtual void printEmbarkItem(const QString &description, const qreal quantity) = 0;
    virtual void closeDNFH(const int id, const char f_type, const int copies) = 0;
    virtual void receiptText(const QString &text) = 0;
    virtual void reprintDocument(const QString &doc_type, const int doc_number) = 0;
    virtual void reprintContinue() = 0;
    virtual void reprintFinalize() = 0;
    virtual void cancel() = 0;
    virtual void ack() = 0;
    virtual void setDateTime(const QDateTime &dateTime) = 0;
    virtual void setFixedData(const QString &shop, const QString &phone) = 0;
    virtual void finish() = 0;

    virtual void getTransactionalMemoryInfo() = 0;
    virtual void downloadReportByDate(const QString &type, const QDate &form, const QDate &to) = 0;
    virtual void downloadReportByNumber(const QString &type, const int from, const int to) = 0;
    virtual void downloadContinue() = 0;
    virtual void downloadFinalize() = 0;
    virtual void downloadDelete(const int to) = 0;


protected:
    virtual void fiscalReceiptNumber(int id, int number, int type) = 0; // type == 0 Factura, == 1 NC
    SerialPort *m_serialPort;
    NetworkPort *m_networkPort;
    int m_TIME_WAIT;
    bool m_continue;
};

class SleeperThread : public QThread
{
    public:
        static void msleep(unsigned long msecs)
        {
            QThread::msleep(msecs);
        }
};

#endif // DRIVERFISCAL_H
