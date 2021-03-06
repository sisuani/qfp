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

#include "driverfiscalepsonext.h"
#include "packagefiscal.h"
#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

int PackageEpsonExt::m_secuence = 0x81;

DriverFiscalEpsonExt::DriverFiscalEpsonExt(QObject *parent, Connector *m_connector)
    : QThread(parent), DriverFiscal(parent, m_connector)
{
    m_nak_count = 0;
    m_error = false;
    m_isinvoice = false;
    m_iscreditnote = false;
    m_continue = true;
    clear();
}

void DriverFiscalEpsonExt::setModel(const FiscalPrinter::Model model)
{
    m_model = model;
}

void DriverFiscalEpsonExt::finish()
{
    m_continue = false;
    queue.clear();
    quit();

    while(isRunning()) {
        msleep(100);
        quit();
    }
}

void DriverFiscalEpsonExt::run()
{

    while(!queue.empty() && m_continue) {
        PackageEpsonExt *pkg = queue.first();
        m_connector->write(pkg->fiscalPackage());

        QByteArray ret = readData(pkg->cmd(), 0);
        if(!ret.isEmpty()) {
            if(ret.at(0) == PackageFiscal::NAK && m_nak_count <= 3) { // ! NAK
                m_nak_count++;
#ifdef DEBUG
                log << "DriverFiscalEpsonExt::run() -> NAK";
#endif
                SleeperThread::msleep(100);
                continue;
            }

            if (!processStatus(ret)) {
                queue.clear();
                continue;
            }

            m_nak_count = 0;

            if (pkg->cmd() == CMD_CLOSEFISCALRECEIPT_INVOICE_CN) {
                emit fiscalReceiptNumber(pkg->id(), getReceiptNumber(ret), 1);
            } else if (pkg->cmd() == CMD_CLOSEFISCALRECEIPT_INVOICE ||
                    pkg->cmd() == CMD_CLOSEFISCALRECEIPT_TICKET || pkg->cmd() == CMD_CLOSEDNFH) {
                emit fiscalReceiptNumber(pkg->id(), getReceiptNumber(ret), 0);
            } else if (pkg->cmd() == CMD_CONTINUEAUDIT) {
                QByteArray tmp = ret;
                tmp.remove(0, 9);
                tmp.remove(2, tmp.size());
                if (tmp.toHex() == QByteArray("0000"))
                    continueAudit();
                else
                    closeAudit();
            } else if (pkg->cmd() == CMD_DOWNLOADREPORTBYNUMBER ||
                    pkg->cmd() == CMD_DOWNLOADREPORTBYDATE) {

                QByteArray tmp = ret;
                tmp.remove(0, 13);
                tmp.remove(tmp.size()-5, tmp.size());
                emit fiscalData(FiscalPrinter::DownloadReport, tmp.data());
            } else if (pkg->cmd() == CMD_DOWNLOADCONTINUE) {
                QByteArray tmp = ret;
                tmp.remove(0, 13);
                tmp.remove(tmp.size()-7, tmp.size());
                emit fiscalData(FiscalPrinter::DownloadContinue, tmp.data());
            } else if (pkg->cmd() == CMD_DOWNLOADFINALIZE) {
                emit fiscalData(FiscalPrinter::DownloadFinalize, QVariant());
            }


            queue.pop_front();
            sendAck();
            delete pkg;

        } else {
#ifdef DEBUG
            log << QString("DriverFiscalEpsonExt::run() -> fiscal error?");
#endif
            sendAck();
            queue.clear();
        }
    }
}

bool DriverFiscalEpsonExt::processStatus(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 5);
    tmp.remove(2, tmp.size());

    bool ok;
    const int status = tmp.toHex().toInt(&ok, 16);

    if (status & (1 << 11))  { // 11 FISCAL
        emit fiscalStatus(FiscalPrinter::FullFiscalMemory);
        return false;
    }

    return true;
}

void DriverFiscalEpsonExt::sendAck()
{
    QByteArray ack;
    ack.append(PackageFiscal::ACK);
    m_connector->write(ack);
    QByteArray bufferBytes = m_connector->read(1);
#ifdef DEBUG
    log << QString("DriverFiscalEpsonExt::sendAck() %1").arg(bufferBytes.toHex().constData());
#endif
}

int DriverFiscalEpsonExt::getReceiptNumber(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 13);
    for(int i = 0; i < tmp.size(); i++) {
        if(tmp.at(i) == PackageFiscal::FS) {
            tmp.remove(i, tmp.size());
            break;
        }
    }

#ifdef DEBUG
    log << QString("DriverFiscalEpsonExt::getReceiptNumber() -> F. Num: %1").arg(tmp.trimmed().toInt());
#endif
    return tmp.trimmed().toInt();
}

bool DriverFiscalEpsonExt::getStatus(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 11);
    tmp.remove(5, tmp.size());

    return tmp == QByteArray("0000");
}

QByteArray DriverFiscalEpsonExt::readData(const int pkg_cmd, const QByteArray &secuence)
{
    bool ok = false;
    int count_tw = 0;
    QByteArray bytes;

    do {
        if(m_connector->type().compare("COM") == 0  && (m_connector->bytesAvailable() <= 0 && m_continue)) {
            //log << QString("NB tw");
            count_tw++;
            SleeperThread::msleep(100);
        }

        if(!m_continue)
            return "";

        QByteArray bufferBytes = m_connector->read(1);
        if(bufferBytes.at(0) == PackageFiscal::DC1 || bufferBytes.at(0) == PackageFiscal::DC2
                || bufferBytes.at(0) == PackageFiscal::DC3 || bufferBytes.at(0) == PackageFiscal::DC4
                || bufferBytes.at(0) == PackageFiscal::FNU || bufferBytes.at(0) == PackageFiscal::ACK) {
            SleeperThread::msleep(100);
            //count_tw -= 20;
            continue;
        } else if(bufferBytes.at(0) == PackageFiscal::NAK) {
            return bufferBytes;
        } else if(bufferBytes.at(0) == PackageFiscal::STX) {
            bufferBytes = m_connector->read(1);
            verifyIntermediatePackage(bufferBytes);


            bytes += PackageFiscal::STX;

            while(bufferBytes.at(0) != PackageFiscal::ETX) {

                if(count_tw >= MAX_TW)
                    break;

                if(bufferBytes.isEmpty()) {
                    SleeperThread::msleep(100);
                    count_tw++;
                } else {
                    if (count_tw > 100)
                        count_tw -= 20;
                }

                if(bufferBytes.at(0) == PackageFiscal::STX) {
                    bufferBytes = m_connector->read(1);
                    verifyIntermediatePackage(bufferBytes);
                }
                bytes += bufferBytes;
                bufferBytes = m_connector->read(1);

            }

            bytes += PackageFiscal::ETX;

            QByteArray checkSumArray;
            int checksumCount = 0;
            while(checksumCount != 4 && m_continue) {
                checkSumArray = m_connector->read(1);
                if(checkSumArray.isEmpty()) {
                    SleeperThread::msleep(100);
                    count_tw++;
                } else {
                    if (count_tw > 100)
                        count_tw -= 20;

                    checksumCount++;
                    bytes += checkSumArray;
                }

                if(count_tw >= MAX_TW)
                    break;
            }

            ok = true;
            break;
        } else {
            verifyIntermediatePackage(bufferBytes);
            bytes += bufferBytes;
        }

        if (bufferBytes.isEmpty())
            count_tw++;

    } while(ok != true && count_tw <= MAX_TW && m_continue);

#ifdef DEBUG
    log << QString("DriverFiscalEpsonExt::readData() -> counter: %1 %2").arg(count_tw).arg(MAX_TW);
#endif

    ok = verifyResponse(bytes, pkg_cmd);

    if(!ok) {
#ifdef DEBUG
        log << QString("DriverFiscalEpsonExt::readData() -> error read: %1").arg(bytes.toHex().data());
#endif
        if(pkg_cmd == 42) {
            //log << QString("SIGNAL ->> ENVIO STATUS ERROR");
            m_connector->readAll();
            emit fiscalStatus(FiscalPrinter::Error);
        }
    } else {
#ifdef DEBUG
        log << QString("DriverFiscalEpsonExt::readData() -> OK PKGV3: %1").arg(bytes.toHex().data());
#endif
        emit fiscalStatus(FiscalPrinter::Ok);
    }

    return bytes;
}

void DriverFiscalEpsonExt::verifyIntermediatePackage(QByteArray &bufferBytes)
{
    if (QString(bufferBytes.toHex()) != "80")
        return;

    int count_tw = 0;
    while (QString(bufferBytes.toHex()) == "80") {
        count_tw++;

        bufferBytes = m_connector->read(1);
        while(bufferBytes.at(0) != PackageFiscal::ETX) {
            bufferBytes = m_connector->read(1);
            count_tw++;
            if(count_tw >= MAX_TW)
                break;
        }

        bufferBytes.clear();

        QByteArray checkSumArray;
        int checksumCount = 0;
        while(checksumCount != 4 && m_continue) {
            checkSumArray = m_connector->read(1);
            count_tw++;
            if(checkSumArray.isEmpty()) {
                SleeperThread::msleep(100);
            } else {
                checksumCount++;
            }

            if(count_tw >= MAX_TW)
                break;
        }
    }

}

bool DriverFiscalEpsonExt::verifyResponse(const QByteArray &bytes, const int pkg_cmd)
{
    if(bytes.at(0) != PackageFiscal::STX) {
#ifdef DEBUG
        log << QString("DriverFiscalEpsonExt::verifyResponse() -> error: Not STX");
#endif
        if(pkg_cmd == 42) {
            //log << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    /*
    if(QChar(bytes.at(2)).unicode() != pkg_cmd) {
        log << QString("ERR - diff cmds: %1 %2").arg(QChar(bytes.at(2)).unicode()).arg(pkg_cmd);
        if(pkg_cmd == 42) {
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }

        return false;
    }
    */

    if(bytes.at(4) != PackageFiscal::FS) {
#ifdef DEBUG
        log << QString("DriverFiscalEpsonExt::verifyResponse() -> error: diff FS");
#endif

        if(pkg_cmd == 42) {
            //log << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }


    if(bytes.at(bytes.size() - 5) != PackageFiscal::ETX) {
#ifdef DEBUG
        log << QString("DriverFiscalEpsonExt::verifyResponse() -> error: ETX");
#endif
        if(pkg_cmd == 42) {
            //log << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    return true;

    /*
    if(!checkSum(bytes)) {
#ifdef DEBUG
        log << QString("DriverFiscalEpsonExt::verifyResponse() -> error: checksum");
#endif
        if(pkg_cmd == 42) {
            //log << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    return true;
    */
}


bool DriverFiscalEpsonExt::checkSum(const QString &data)
{
    return true;
}

bool DriverFiscalEpsonExt::checkSum(const QByteArray &data)
{
    bool ok;
    int checkSum = data.right(4).toInt(&ok, 16);
    if(!ok) {
        return false;
    }

    int ck_cmd = 0;
    for(int i = 0; i < data.size()-4; i++) {
        ck_cmd += QChar(data.at(i)).unicode();
    }

    return (checkSum == ck_cmd);
}

void DriverFiscalEpsonExt::statusRequest()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_STATUS);
    QByteArray d;

    // CMD
    d.append(QByteArray::fromHex("0"));
    d.append(0x01);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::dailyClose(const char type)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DAILYCLOSE);

    QByteArray d;

    // CMD
    d.append(0x08);
    if (type == 'X') {
        d.append(0x1B);
        d.append(0x02);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(0x0C);
        d.append(0x01);
    } else { // Z
        d.append(0x01);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(0x0C);
        d.append(QByteArray::fromHex("0"));
    }

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::dailyCloseByDate(const QDate &from, const QDate &to)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DAILYCLOSEBYDATE);

    QByteArray d;
    d.append(0x08);
    d.append(0x12);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(0x01);
    d.append(PackageFiscal::FS);
    d.append(from.toString("ddMMyy"));
    d.append(PackageFiscal::FS);
    d.append(to.toString("ddMMyy"));
    p->setData(d);

    queue.append(p);
    start();

    continueAudit();
}

void DriverFiscalEpsonExt::dailyCloseByNumber(const int from, const int to)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DAILYCLOSEBYNUMBER);

    QByteArray d;
    d.append(0x08);
    d.append(0x13);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(0x01);
    d.append(PackageFiscal::FS);
    d.append(QString::number(from));
    d.append(PackageFiscal::FS);
    d.append(QString::number(to));
    p->setData(d);


    queue.append(p);
    start();

    continueAudit();
    // for (int i = 0; i <= (to - from); i++)

    //closeAudit();
}

void DriverFiscalEpsonExt::continueAudit()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_CONTINUEAUDIT);

    QByteArray d;
    d.append(0x08);
    d.append(0x14);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));
    p->setData(d);

    queue.append(p);
    start();

}

void DriverFiscalEpsonExt::closeAudit()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DAILYCLOSEBYNUMBER);

    QByteArray d;
    d.append(0x08);
    d.append(0x15);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));
    p->setData(d);

    queue.append(p);
    start();

}

void DriverFiscalEpsonExt::setCustomerData(const QString &name, const QString &cuit, const char tax_type,
        const QString &doc_type, const QString &address)
{
    m_name = name;
    m_cuit = cuit;
    if(tax_type == 'A')
        m_tax_type = 'N';
    else if(tax_type == 'C')
        m_tax_type = 'F';
    else
        m_tax_type = tax_type;


    if(doc_type.compare("C") == 0)
        m_doc_type = "T";
    else if(doc_type.compare("3") == 0)
        m_doc_type = "P";
    else
        m_doc_type = "D";
    m_address = address;
}

void DriverFiscalEpsonExt::openFiscalReceipt(const char type)
{
    PackageEpsonExt *p = new PackageEpsonExt;

    QByteArray d;
    if(type == 'A' || type == 'B') {
        m_isinvoice = true;
        p->setCmd(CMD_OPENFISCALRECEIPT);
        // CMD
        d.append(0x0B);
        d.append(0x01);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
        d.append(PackageFiscal::FS);
        d.append(m_name);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(m_address);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(m_doc_type);
        d.append(PackageFiscal::FS);
        d.append(m_cuit);
        d.append(PackageFiscal::FS);
        d.append(m_tax_type);
        d.append(PackageFiscal::FS);
        d.append(m_refer.isEmpty() ? "901-99998-99999998" : m_refer);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
    } else { // 'T'
        m_isinvoice = false;
        p->setCmd(CMD_OPENTICKET);
        // CMD
        d.append(0x0A);
        d.append(0x01);

        // DATA
        d.append(PackageFiscal::FS);
        if (type == 'M')
            d.append(0x40);
        else
            d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
    }

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::printFiscalText(const QString &text)
{
}

void DriverFiscalEpsonExt::printLineItem(const QString &description, const qreal quantity,
        const qreal price, const QString &tax, const char qualifier, const qreal excise)
{

    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(m_isinvoice ? CMD_PRINTLINEITEM_INVOICE : CMD_PRINTLINEITEM_TICKET);

    QByteArray d;
    if (m_isinvoice) {
        d.append(0x0B);
        d.append(0x1B);
        d.append(0x02);
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(0x10);
    } else if (m_iscreditnote) {
        d.append(0x0D);
        d.append(0x1B);
        d.append(0x02);
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(0x10);
    } else {
        d.append(0x0A);
        d.append(0x1B);
        d.append(0x02);
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(0x10);
    }

    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(description.left(40));
    d.append(PackageFiscal::FS);
    d.append(QString::number(quantity * 10000, 'f', 0));
    d.append(PackageFiscal::FS);
    if (m_isinvoice || m_iscreditnote) {
        d.append(QString::number(price/(1+(tax.toDouble()/100)) * 10000, 'f', 0));
    } else {
        d.append(QString::number(price * 10000, 'f', 0));
    }
    d.append(PackageFiscal::FS);
    d.append(QString::number(tax.toDouble() * 100, 'f', 0));
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    QString code = description;
    code.replace(" ", "");
    code.remove(QRegExp("[^a-zA-Z//\\d\\s]"));
    d.append(code.left(40));
    d.append(PackageFiscal::FS);
    d.append("07");
    d.append(PackageFiscal::FS);
    p->setData(d);

    queue.append(p);
    start();

}

void DriverFiscalEpsonExt::perceptions(const QString &desc, qreal tax_amount)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_PERCEPTIONS);

    QByteArray d;
    if (m_iscreditnote) {
        d.append(0x0D);
        d.append(0x1B);
    } else {
        d.append(0x0B);
        d.append(0x1B);
    }
    d.append(0x20);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));
    d.append(PackageFiscal::FS);
    d.append(desc.left(30));
    d.append(PackageFiscal::FS);
    d.append(QString::number(tax_amount * 100, 'f', 0));
    d.append(PackageFiscal::FS);
    d.append("2100");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::subtotal(const char print)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_SUBTOTAL);

    QByteArray d;
    if (m_isinvoice) {
        d.append(0x0B);
        d.append(0x03);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
    } else {
        d.append(0x0A);
        d.append(0x03);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
    }
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::totalTender(const QString &description, const qreal amount, const char type)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(m_isinvoice ? CMD_TOTALTENDER_INVOICE : CMD_TOTALTENDER_TICKET);

    QByteArray d;
    if (m_isinvoice) {
        d.append(0x0B);
        d.append(0x05);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
    } else {
        d.append(0x0A);
        d.append(0x05);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
    }

    d.append(PackageFiscal::FS);
    //d.append(description);
    d.append(PackageFiscal::FS);
    //d.append(description);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    if (description == "Cheque") {
        d.append("03");
    } else if (description == "Cuenta Corriente") {
        d.append("06");
    } else if (description == "Banco") {
        d.append("07");
    } else if ((description == "Tarjeta de Credito") || (description == "Mercado Pago")) {
        d.append("20");
    } else if (description == "Tarjeta de Debito") {
        d.append("21");
    } else {
        d.append("08");
    }

    d.append(PackageFiscal::FS);
    d.append(QString::number(amount * 100, 'f', 0));

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(m_isinvoice ? CMD_PRINTLINEITEM_INVOICE : CMD_PRINTLINEITEM_TICKET);

    QByteArray d;
    if(m_isinvoice) {
        d.append(0x0B);
        d.append(0x04);
    } else if (m_iscreditnote) {
        d.append(0x0D);
        d.append(0x04);
    } else {
        d.append(0x0A);
        d.append(0x04);
    }

    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));

    if (type == 'M')
        d.append(QByteArray::fromHex("1"));
    else
        d.append(QByteArray::fromHex("0"));

    d.append(PackageFiscal::FS);
    d.append(description);
    d.append(PackageFiscal::FS);

    if(m_tax_type == 'I') {
        d.append(QString::number((amount / (1 + tax_percent/100)) * 100, 'f', 0));
        d.append(PackageFiscal::FS);
        d.append(QString::number(tax_percent * 100));
    } else {
        d.append(QString::number(amount * 100, 'f', 0));
        d.append(PackageFiscal::FS);
        d.append(QString::number(tax_percent * 100));
    }

    d.append(PackageFiscal::FS);
    d.append("CodigoInterno4567891123456789012345678901234567891");
    d.append(PackageFiscal::FS);

    p->setData(d);
    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::closeFiscalReceipt(const char intype, const char type, const int id)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setId(id);
    p->setCmd(m_isinvoice ? CMD_CLOSEFISCALRECEIPT_INVOICE : CMD_CLOSEFISCALRECEIPT_TICKET);

    QByteArray d;
    if(m_isinvoice) {
        d.append(0x0B);
        d.append(0x06);
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(0x1B);
        d.append(0x03);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
    } else {
        d.append(0x0A);
        d.append(0x06);
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(0x1B);
        d.append(0x03);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
        d.append(PackageFiscal::FS);
    }

    clear();

    p->setData(d);
    queue.append(p);
    start();

}

void DriverFiscalEpsonExt::openNonFiscalReceipt()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_OPENNONFISCALRECEIPT);

    QByteArray d;
    // CMD
    d.append(0x0E);
    d.append(0x01);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::printNonFiscalText(const QString &text)
{
    QString t = text;
    while(!t.isEmpty()) {
        PackageEpsonExt *p = new PackageEpsonExt;
        p->setCmd(CMD_PRINTNONTFISCALTEXT);

        QByteArray d;
        // CMD
        d.append(0x0E);
        d.append(0x1B);
        d.append(0x02);
        // DATA
        d.append(PackageFiscal::FS);
        d.append(QByteArray::fromHex("0"));
        d.append(QByteArray::fromHex("0"));
        d.append(PackageFiscal::FS);
        d.append(t.left(40));
        p->setData(d);

        t.remove(0, 40);

        queue.append(p);
    }

    start();
}

void DriverFiscalEpsonExt::closeNonFiscalReceipt()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_CLOSENONFISCALRECEIPT);

    QByteArray d;
    // CMD
    d.append(0x0E);
    d.append(0x06);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(0x01);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::openDrawer()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_OPENDRAWER);
    QByteArray d;
    // CMD
    d.append(0x07);
    d.append(0x07);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(0x01);
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::setHeaderTrailer(const QString &header, const QString &trailer)
{
}

void DriverFiscalEpsonExt::setEmbarkNumber(const int doc_num, const QString &description, const char type)
{
    m_refer = description;
    if (m_tax_type == 'I')
        m_refer.prepend("003-");
    else
        m_refer.prepend("008-");
}

void DriverFiscalEpsonExt::openDNFH(const char type, const char fix_value, const QString &doc_num)
{
    PackageEpsonExt *p = new PackageEpsonExt;

    m_iscreditnote = true;
    p->setCmd(CMD_OPENFISCALRECEIPT);

    QString dn;
    if (!doc_num.isEmpty()) {
        dn = doc_num;
        if (m_tax_type == 'I')
            dn.prepend("003-");
        else
            dn.prepend("008-");
    }

    /*
    if (type == 'A')
        m_isinvoice = true;
        */

    QByteArray d;
    // CMD
    d.append(0x0D);
    d.append(0x01);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));
    d.append(PackageFiscal::FS);
    d.append(m_name);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(m_address);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(m_doc_type);
    d.append(PackageFiscal::FS);
    d.append(m_cuit);
    d.append(PackageFiscal::FS);
    d.append(m_tax_type);
    d.append(PackageFiscal::FS);
    //d.append(m_refer.isEmpty() ? "901-99999-99999999" : m_refer);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(dn.isEmpty() ? "902-99998-99999998" : dn);

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::printEmbarkItem(const QString &description, const qreal quantity)
{
}

void DriverFiscalEpsonExt::closeDNFH(const int id, const char f_type, const int copies)
{
    Q_UNUSED(f_type);
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setId(id);
    p->setCmd(m_iscreditnote ? CMD_CLOSEFISCALRECEIPT_INVOICE_CN : CMD_CLOSEFISCALRECEIPT_INVOICE);

    QByteArray d;
    // CMD
    d.append(0x0D);
    d.append(0x06);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(0x1B);
    d.append(0x03);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);
    d.append(PackageFiscal::FS);

    clear();

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::cancel()
{
}

void DriverFiscalEpsonExt::ack()
{
}

void DriverFiscalEpsonExt::setDateTime(const QDateTime &dateTime)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_SETDATETIME);

    QByteArray d;
    // CMD
    d.append(0x05);
    d.append(0x01);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    d.append(PackageFiscal::FS);
    d.append(dateTime.toString("ddMMyy"));
    d.append(PackageFiscal::FS);
    d.append(dateTime.toString("hhmmss"));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::setFooter(int line, const QString &text)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_SETDATETIME);

    QByteArray d;
    // CMD
    d.append(0x05);
    d.append(0x0A);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    d.append(PackageFiscal::FS);
    d.append(QString::number(line));
    d.append(PackageFiscal::FS);
    d.append(text);

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::clear()
{
    m_isinvoice = false;
    m_iscreditnote = false;
    m_name = "Consumidor Final";
    m_cuit = "0";
    m_tax_type = 'F';
    m_doc_type = "D";
    m_address = "-";
    m_address1 = "";
    m_refer = "";
}

void DriverFiscalEpsonExt::receiptText(const QString &text)
{
}


void DriverFiscalEpsonExt::reprintDocument(const QString &doc_type, const int doc_number)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_REPRINTDOCUMENT);

    QByteArray d;
    // CMD
    d.append(0x08);
    d.append(0xF0);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(0x01);

    d.append(PackageFiscal::FS);
    d.append(doc_type);
    d.append(PackageFiscal::FS);
    d.append(QString::number(doc_number));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::reprintContinue()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_REPRINTCONTINUE);

    QByteArray d;
    // CMD
    d.append(0x08);
    d.append(0xF4);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::reprintFinalize()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_REPRINTFINALIZE);

    QByteArray d;
    // CMD
    d.append(0x08);
    d.append(0xF5);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::setFixedData(const QString &shop, const QString &phone)
{
    setFooter(1, "DEFENSA CONSUMIDOR " + phone);
    setFooter(2, "Ingresa en www.global.subway.com");
    setFooter(3, "Danos tu opinion y guarda el recibo para");
    setFooter(4, "obtener una COOKIE GRATIS en tu proxima");
    setFooter(5, "compra. Valido dentro de los 5 dias de");
    setFooter(6, QString("emision del ticket. Tienda: %1-0").arg(shop));
    setFooter(7, "");
    setFooter(8, "");
    setFooter(9, "");
    setFooter(10, "");
}


void DriverFiscalEpsonExt::getTransactionalMemoryInfo()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_GETTRANSACTIONALMEMORYINFO);

    QByteArray d;
    // CMD
    d.append(0x09);
    d.append(0x15);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::downloadReportByDate(const QString &type, const QDate &from, const QDate &to)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DOWNLOADREPORTBYDATE);

    QByteArray d;
    d.append(0x09);
    d.append(0x51);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    if (type == "CTD")
        d.append(QByteArray::fromHex("0"));
    else if (type == "A")
        d.append(0x02);
    else
        d.append(0x04);
    d.append(PackageFiscal::FS);
    d.append(from.toString("ddMMyy"));
    d.append(PackageFiscal::FS);
    d.append(to.toString("ddMMyy"));
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::downloadReportByNumber(const QString &type, const int from, const int to)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DOWNLOADREPORTBYNUMBER);

    QByteArray d;
    d.append(0x09);
    d.append(0x52);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    if (type == "CTD")
        d.append(QByteArray::fromHex("0"));
    else if (type == "A")
        d.append(0x02);
    else
        d.append(0x04);
    d.append(PackageFiscal::FS);
    d.append(QString::number(from));
    d.append(PackageFiscal::FS);
    d.append(QString::number(to));
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpsonExt::downloadContinue()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DOWNLOADCONTINUE);

    QByteArray d;
    // CMD
    d.append(0x09);
    d.append(0x70);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::downloadFinalize()
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DOWNLOADFINALIZE);

    QByteArray d;
    // CMD
    d.append(0x09);
    d.append(0x71);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpsonExt::downloadDelete(const int to)
{
    PackageEpsonExt *p = new PackageEpsonExt;
    p->setCmd(CMD_DOWNLOADDELETE);

    QByteArray d;
    // CMD
    d.append(0x09);
    d.append(0x10);
    // DATA
    d.append(PackageFiscal::FS);
    d.append(QByteArray::fromHex("0"));
    d.append(QByteArray::fromHex("0"));
    d.append(PackageFiscal::FS);
    d.append(QString::number(to));

    p->setData(d);
    queue.append(p);

    start();
}
