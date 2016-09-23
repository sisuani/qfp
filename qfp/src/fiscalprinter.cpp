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

#include "fiscalprinter.h"
#include "driverfiscalepson.h"
#include "driverfiscalepsonext.h"
#include "driverfiscalhasar.h"

#include <QDebug>

FiscalPrinter::FiscalPrinter(QObject *parent, FiscalPrinter::Brand brand,
        FiscalPrinter::Model model, const QString &port_type, unsigned int port, int m_TIME_WAIT)
    : QObject(parent)
{
    m_serialPort = new SerialPort(port_type, port);
    qDebug() << "serialport - " << port_type << port << m_serialPort->isOpen();
    m_model = model;
    if(model == EpsonTMU220 || model == EpsonTM900 || model == Hasar615F || model == Hasar715F)
        m_supportTicket = true;
    else
        m_supportTicket = false;

    if (brand == FiscalPrinter::Epson) {
        if (model == EpsonTM900) {
            m_driverFiscal = new DriverFiscalEpsonExt(this, m_serialPort);
            dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal)->setModel(model);
            connect(dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                    this, SIGNAL(fiscalReceiptNumber(int, int, int)));
            connect(dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal), SIGNAL(fiscalData(int, QVariant)),
                    this, SIGNAL(fiscalData(int, QVariant)));
            connect(dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                    this, SIGNAL(fiscalStatus(int)));
        } else {
            m_driverFiscal = new DriverFiscalEpson(this, m_serialPort);
            dynamic_cast<DriverFiscalEpson *>(m_driverFiscal)->setModel(model);
            connect(dynamic_cast<DriverFiscalEpson *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                    this, SIGNAL(fiscalReceiptNumber(int, int, int)));
            connect(dynamic_cast<DriverFiscalEpson *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                    this, SIGNAL(fiscalStatus(int)));
        }
    } else {
        m_driverFiscal = new DriverFiscalHasar(this, m_serialPort, m_TIME_WAIT);
        dynamic_cast<DriverFiscalHasar *>(m_driverFiscal)->setModel(model);
        connect(dynamic_cast<DriverFiscalHasar *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                this, SIGNAL(fiscalReceiptNumber(int, int, int)));
        connect(dynamic_cast<DriverFiscalHasar *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                this, SIGNAL(fiscalStatus(int)));
    }
}

FiscalPrinter::~FiscalPrinter()
{
    m_driverFiscal->finish();
    if(m_serialPort) {
        if(m_serialPort->isOpen())
            m_serialPort->close();
        delete m_serialPort;
    }
}

int FiscalPrinter::model()
{
    return m_model;
}

bool FiscalPrinter::supportTicket()
{
    return m_supportTicket;
}

bool FiscalPrinter::isOpen()
{
    return m_serialPort->isOpen();
}

void FiscalPrinter::statusRequest()
{
    m_driverFiscal->statusRequest();
}

void FiscalPrinter::dailyClose(const char type)
{
    m_driverFiscal->dailyClose(type);
}

void FiscalPrinter::dailyCloseByDate(const QDate &from, const QDate &to)
{
    m_driverFiscal->dailyCloseByDate(from, to);
}

void FiscalPrinter::dailyCloseByNumber(const int from, const int to)
{
    m_driverFiscal->dailyCloseByNumber(from, to);
}

void FiscalPrinter::setCustomerData(const QString &name, const QString &cuit, const char tax_type,
        const QString &doc_type, const QString &address)
{
    m_driverFiscal->setCustomerData(name, cuit, tax_type, doc_type, address);
}

void FiscalPrinter::openFiscalReceipt(const char type)
{
    m_driverFiscal->openFiscalReceipt(type);
}

void FiscalPrinter::printFiscalText(const QString &text)
{
    m_driverFiscal->printFiscalText(text);
}

void FiscalPrinter::printLineItem(const QString &description, const qreal quantity,
        const qreal price, const QString &tax, const char qualifier, const qreal excise)
{
    m_driverFiscal->printLineItem(description, quantity, price, tax, qualifier, excise);
}

void FiscalPrinter::perceptions(const QString &desc, qreal tax_amount)
{
    m_driverFiscal->perceptions(desc, tax_amount);
}

void FiscalPrinter::subtotal(const char print)
{
    m_driverFiscal->subtotal(print);
}

void FiscalPrinter::totalTender(const QString &description, const qreal amount, const char type)
{
    m_driverFiscal->totalTender(description, amount, type);
}

void FiscalPrinter::generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type)
{
    m_driverFiscal->generalDiscount(description, amount, tax_percent, type);
}

void FiscalPrinter::closeFiscalReceipt(const char intype, const char type, const int id)
{
    m_driverFiscal->closeFiscalReceipt(intype, type, id);
}

void FiscalPrinter::openNonFiscalReceipt()
{
    m_driverFiscal->openNonFiscalReceipt();
}

void FiscalPrinter::printNonFiscalText(const QString &text)
{
    m_driverFiscal->printNonFiscalText(text);
}

void FiscalPrinter::closeNonFiscalReceipt()
{
    m_driverFiscal->closeNonFiscalReceipt();
}

void FiscalPrinter::openDrawer()
{
    m_driverFiscal->openDrawer();
}

void FiscalPrinter::setHeaderTrailer(const QString &header, const QString &trailer)
{
    m_driverFiscal->setHeaderTrailer(header, trailer);
}

void FiscalPrinter::setEmbarkNumber(const int doc_num, const QString &description)
{
    m_driverFiscal->setEmbarkNumber(doc_num, description);
}

void FiscalPrinter::openDNFH(const char type, const char fix_value, const QString &doc_num)
{
    m_driverFiscal->openDNFH(type, fix_value, doc_num);
}

void FiscalPrinter::printEmbarkItem(const QString &description, const qreal quantity)
{
    m_driverFiscal->printEmbarkItem(description, quantity);
}

void FiscalPrinter::closeDNFH(const int id, const char f_type, const int copies)
{
    m_driverFiscal->closeDNFH(id, f_type, copies);
}

void FiscalPrinter::cancel()
{
    m_driverFiscal->cancel();
}

void FiscalPrinter::ack()
{
    m_driverFiscal->ack();
}

void FiscalPrinter::setDateTime(const QDateTime &dateTime)
{
    m_driverFiscal->setDateTime(dateTime);
}

void FiscalPrinter::receiptText(const QString &text)
{
    m_driverFiscal->receiptText(text);
}

void FiscalPrinter::reprintDocument(const QString &doc_type, const int doc_number)
{
    m_driverFiscal->reprintDocument(doc_type, doc_number);
}

void FiscalPrinter::reprintContinue()
{
    m_driverFiscal->reprintContinue();
}

void FiscalPrinter::reprintFinalize()
{
    m_driverFiscal->reprintFinalize();
}

void FiscalPrinter::setFixedData(const QString &shop, const QString &phone)
{
    m_driverFiscal->setFixedData(shop, phone);
}

void FiscalPrinter::getTransactionalMemoryInfo()
{
    m_driverFiscal->getTransactionalMemoryInfo();
}

void FiscalPrinter::downloadReportByDate(const QString &type, const QDate &from, const QDate &to)
{
    m_driverFiscal->downloadReportByDate(type, from, to);
}

void FiscalPrinter::downloadReportByNumber(const QString &type, const int from, const int to)
{
    m_driverFiscal->downloadReportByNumber(type, from, to);
}

void FiscalPrinter::downloadContinue()
{
    m_driverFiscal->downloadContinue();
}

void FiscalPrinter::downloadFinalize()
{
    m_driverFiscal->downloadFinalize();
}

void FiscalPrinter::downloadDelete(const int to)
{
}
