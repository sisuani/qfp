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
#include "driverfiscalhasar2g.h"
#include "logger.h"

#include <QCoreApplication>
#include <QRegExp>
#include <QDebug>

FiscalPrinter::FiscalPrinter(QObject *parent, FiscalPrinter::Brand brand,
        FiscalPrinter::Model model, const QString &port_type, const QString &port, int m_TIME_WAIT)
    : QObject(parent)
    , m_model(model)

{

#ifdef DEBUG
    Logger::instance()->init(QCoreApplication::applicationDirPath() + "/fiscal_log.txt");
    log << "";
    log << QString("Start Logging at %1").arg(QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss"));
#endif

    m_connector = new Connector(this, model, port_type, port);

    if (brand == FiscalPrinter::Epson) {
        if (model == EpsonTM900) {
            m_driverFiscal = new DriverFiscalEpsonExt(this, m_connector);
            dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal)->setModel(model);
            connect(dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                    this, SIGNAL(fiscalReceiptNumber(int, int, int)));
            connect(dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal), SIGNAL(fiscalData(int, QVariant)),
                    this, SIGNAL(fiscalData(int, QVariant)));
            connect(dynamic_cast<DriverFiscalEpsonExt *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                    this, SIGNAL(fiscalStatus(int)));
        } else {
            m_driverFiscal = new DriverFiscalEpson(this, m_connector);
            dynamic_cast<DriverFiscalEpson *>(m_driverFiscal)->setModel(model);
            connect(dynamic_cast<DriverFiscalEpson *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                    this, SIGNAL(fiscalReceiptNumber(int, int, int)));
            connect(dynamic_cast<DriverFiscalEpson *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                    this, SIGNAL(fiscalStatus(int)));
        }
    } else {
        if (model == FiscalPrinter::Hasar1000F) {
            m_driverFiscal = new DriverFiscalHasar2G(this, m_connector);
            dynamic_cast<DriverFiscalHasar2G *>(m_driverFiscal)->setModel(model);
            connect(dynamic_cast<DriverFiscalHasar2G *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                    this, SIGNAL(fiscalReceiptNumber(int, int, int)));
            connect(dynamic_cast<DriverFiscalHasar2G *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                    this, SIGNAL(fiscalStatus(int)));
        } else {
            m_driverFiscal = new DriverFiscalHasar(this, m_connector, m_TIME_WAIT);
            dynamic_cast<DriverFiscalHasar *>(m_driverFiscal)->setModel(model);
            connect(dynamic_cast<DriverFiscalHasar *>(m_driverFiscal), SIGNAL(fiscalReceiptNumber(int, int, int)),
                    this, SIGNAL(fiscalReceiptNumber(int, int, int)));
            connect(dynamic_cast<DriverFiscalHasar *>(m_driverFiscal), SIGNAL(fiscalStatus(int)),
                    this, SIGNAL(fiscalStatus(int)));
        }
    }
}

FiscalPrinter::~FiscalPrinter()
{
    m_driverFiscal->finish();
    if (m_connector) {
        if(m_connector->isOpen())
            m_connector->close();
        delete m_connector;
    }
}

int FiscalPrinter::model()
{
    return m_model;
}

bool FiscalPrinter::supportTicket()
{
    if(m_model == EpsonTMU220 || m_model == EpsonTM900 || m_model == Hasar615F
            || m_model == Hasar715F || m_model == Hasar1000F)
        return true;
    return false;
}

bool FiscalPrinter::isOpen()
{
    if (model() == FiscalPrinter::Hasar1000F)
        return true;

    return m_connector->isOpen();
}

void FiscalPrinter::statusRequest()
{
#ifdef DEBUG
    log << "statusRequest()";
#endif
    m_driverFiscal->statusRequest();
}

void FiscalPrinter::dailyClose(const char type)
{
#ifdef DEBUG
    log << QString("dailyClose() %1").arg(type);
#endif
    m_driverFiscal->dailyClose(type);
}

void FiscalPrinter::dailyCloseByDate(const QDate &from, const QDate &to)
{
#ifdef DEBUG
    log << QString("dailyCloseByDate() %1 %2").arg(from.toString()).arg(to.toString());
#endif
    m_driverFiscal->dailyCloseByDate(from, to);
}

void FiscalPrinter::dailyCloseByNumber(const int from, const int to)
{
#ifdef DEBUG
    log << QString("dailyCloseByNumber() %1 %2").arg(from).arg(to);
#endif
    m_driverFiscal->dailyCloseByNumber(from, to);
}

void FiscalPrinter::setCustomerData(const QString &name, const QString &cuit, const char tax_type,
        const QString &doc_type, const QString &address)
{
#ifdef DEBUG
    log << QString("setCustomerData() %1 %2 %3 %4 %5").arg(name).arg(cuit).arg(tax_type).arg(doc_type).arg(address);
#endif
    m_driverFiscal->setCustomerData(name, cuit, tax_type, doc_type, address);
}

void FiscalPrinter::openFiscalReceipt(const char type)
{
#ifdef DEBUG
    log << QString("openFiscalReceipt() %1").arg(type);
#endif
    m_driverFiscal->openFiscalReceipt(type);
}

void FiscalPrinter::printFiscalText(const QString &text)
{
#ifdef DEBUG
    log << QString("printFiscalText() %1").arg(text);
#endif
    m_driverFiscal->printFiscalText(text);
}

void FiscalPrinter::printLineItem(const QString &description, const qreal quantity,
        const qreal price, const QString &tax, const char qualifier, const qreal excise)
{
#ifdef DEBUG
    log << QString("printLineItem() %1 %2 %3 %4 %5 %6").arg(description).arg(quantity).arg(price).arg(tax).arg(qualifier).arg(excise);
#endif
    m_driverFiscal->printLineItem(description, quantity, price, tax, qualifier, excise);
}

void FiscalPrinter::perceptions(const QString &desc, qreal tax_amount)
{
#ifdef DEBUG
    log << QString("perceptions() %1 %2").arg(desc).arg(tax_amount);
#endif
    m_driverFiscal->perceptions(desc, tax_amount);
}

void FiscalPrinter::subtotal(const char print)
{
#ifdef DEBUG
    log << QString("subtotal() %1").arg(print);
#endif
    m_driverFiscal->subtotal(print);
}

void FiscalPrinter::totalTender(const QString &description, const qreal amount, const char type)
{
#ifdef DEBUG
    log << QString("totalTender() %1 %2 %3").arg(description).arg(amount).arg(type);
#endif
    m_driverFiscal->totalTender(description, amount, type);
}

void FiscalPrinter::generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type)
{
#ifdef DEBUG
    log << QString("generalDiscount() %1 %2 %3 %4").arg(description).arg(amount).arg(tax_percent).arg(type);
#endif
    m_driverFiscal->generalDiscount(description, amount, tax_percent, type);
}

void FiscalPrinter::closeFiscalReceipt(const char intype, const char type, const int id)
{
#ifdef DEBUG
    log << QString("closeFiscalReceipt() %1 %2 %3").arg(intype).arg(type).arg(id);
#endif
    m_driverFiscal->closeFiscalReceipt(intype, type, id);
}

void FiscalPrinter::openNonFiscalReceipt()
{
#ifdef DEBUG
    log << QString("openNonFiscalReceipt()");
#endif
    m_driverFiscal->openNonFiscalReceipt();
}

void FiscalPrinter::printNonFiscalText(const QString &text)
{
#ifdef DEBUG
    log << QString("printNonFiscalText() %1").arg(text);
#endif

    m_driverFiscal->printNonFiscalText(text);
}

void FiscalPrinter::closeNonFiscalReceipt()
{
#ifdef DEBUG
    log << QString("closeNonFiscalReceipt() ");
#endif
    m_driverFiscal->closeNonFiscalReceipt();
}

void FiscalPrinter::openDrawer()
{
#ifdef DEBUG
    log << QString("openDrawer() ");
#endif
    m_driverFiscal->openDrawer();
}

void FiscalPrinter::setHeaderTrailer(const QString &header, const QString &trailer)
{
#ifdef DEBUG
    log << QString("setHeaderTrailer() %1 %2").arg(header).arg(trailer);
#endif
    m_driverFiscal->setHeaderTrailer(header, trailer);
}

void FiscalPrinter::setEmbarkNumber(const int doc_num, const QString &description, const char type)
{
#ifdef DEBUG
    log << QString("setEmbarkNumber() %1 %2 %3").arg(doc_num).arg(description).arg(type);
#endif
    m_driverFiscal->setEmbarkNumber(doc_num, description, type);
}

void FiscalPrinter::openDNFH(const char type, const char fix_value, const QString &doc_num)
{
#ifdef DEBUG
    log << QString("openDNFH() %1 %2 %3").arg(type).arg(fix_value).arg(doc_num);
#endif
    m_driverFiscal->openDNFH(type, fix_value, doc_num);
}

void FiscalPrinter::printEmbarkItem(const QString &description, const qreal quantity)
{
#ifdef DEBUG
    log << QString("printEmbarkItem() %1 %2").arg(description).arg(quantity);
#endif
    m_driverFiscal->printEmbarkItem(description, quantity);
}

void FiscalPrinter::closeDNFH(const int id, const char f_type, const int copies)
{
#ifdef DEBUG
    log << QString("closeDNFH() %1 %2 %3").arg(id).arg(f_type).arg(copies);
#endif
    m_driverFiscal->closeDNFH(id, f_type, copies);
}

void FiscalPrinter::cancel()
{
#ifdef DEBUG
    log << QString("cancel() ");
#endif
    m_driverFiscal->cancel();
}

void FiscalPrinter::ack()
{
#ifdef DEBUG
    log << QString("ask() ");
#endif
    m_driverFiscal->ack();
}

void FiscalPrinter::setDateTime(const QDateTime &dateTime)
{
#ifdef DEBUG
    log << QString("setDateTime() %1").arg(dateTime.toString());
#endif
    m_driverFiscal->setDateTime(dateTime);
}

void FiscalPrinter::receiptText(const QString &text)
{
#ifdef DEBUG
    log << QString("receiptText() %1").arg(text);
#endif
    m_driverFiscal->receiptText(text);
}

void FiscalPrinter::reprintDocument(const QString &doc_type, const int doc_number)
{
#ifdef DEBUG
    log << QString("reprintDocument() %1 %2").arg(doc_type).arg(doc_number);
#endif
    m_driverFiscal->reprintDocument(doc_type, doc_number);
}

void FiscalPrinter::reprintContinue()
{
#ifdef DEBUG
    log << QString("reprintContinue() ");
#endif
    m_driverFiscal->reprintContinue();
}

void FiscalPrinter::reprintFinalize()
{
#ifdef DEBUG
    log << QString("reprintFinalize() ");
#endif
    m_driverFiscal->reprintFinalize();
}

void FiscalPrinter::setFixedData(const QString &shop, const QString &phone)
{
#ifdef DEBUG
    log << QString("setFixedData() %1 %2").arg(shop).arg(phone);
#endif
    m_driverFiscal->setFixedData(shop, phone);
}

void FiscalPrinter::getTransactionalMemoryInfo()
{
#ifdef DEBUG
    log << QString("getTransactionalMemoryInfo()");
#endif
    m_driverFiscal->getTransactionalMemoryInfo();
}

void FiscalPrinter::downloadReportByDate(const QString &type, const QDate &from, const QDate &to)
{
#ifdef DEBUG
    log << QString("downloadReportByDate() %1 %2 %3").arg(type).arg(from.toString()).arg(to.toString());
#endif
    m_driverFiscal->downloadReportByDate(type, from, to);
}

void FiscalPrinter::downloadReportByNumber(const QString &type, const int from, const int to)
{
#ifdef DEBUG
    log << QString("downloadReportByNumber() %1 %2 %3").arg(type).arg(from).arg(to);
#endif
    m_driverFiscal->downloadReportByNumber(type, from, to);
}

void FiscalPrinter::downloadContinue()
{
#ifdef DEBUG
    log << QString("downloadContinue()");
#endif
    m_driverFiscal->downloadContinue();
}

void FiscalPrinter::downloadFinalize()
{
#ifdef DEBUG
    log << QString("downloadFinalize()");
#endif
    m_driverFiscal->downloadFinalize();
}

void FiscalPrinter::downloadDelete(const int to)
{
#ifdef DEBUG
    log << QString("downloadDelete() %1").arg(to);
#endif
}
