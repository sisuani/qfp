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

#include "driverfiscalhasar.h"
#include "packagefiscal.h"
#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>

int PackageHasar::m_secuence = 0x20;

DriverFiscalHasar::DriverFiscalHasar(QObject *parent, Connector *m_connector, int m_TIME_WAIT)
    : QThread(parent), DriverFiscal(parent, m_connector, m_TIME_WAIT)
{
#if LOGGER
    //Logger::instance()->init(QCoreApplication::applicationDirPath() + "/fiscal.txt");
#endif
    m_nak_count = 0;
    m_error = false;
    errorHandler_count = 0;
    m_continue = true;
}

void DriverFiscalHasar::setModel(const FiscalPrinter::Model model)
{
    m_model = model;
}

void DriverFiscalHasar::finish()
{
    m_continue = false;
    queue.clear();
    quit();

    while(isRunning()) {
        msleep(100);
        quit();
    }
}

void DriverFiscalHasar::run()
{
    while(!queue.empty() && m_continue) {
        PackageHasar *pkg = queue.first();
        m_connector->write(pkg->fiscalPackage());

        QByteArray ret = readData(pkg->cmd(), 0);
        if(ret == "-1") {
            queue.clear();
            m_connector->readAll();

            if(errorHandler_count > 4)
                return;

            errorHandler_count++;

            /*
            for(int i = 0 ; i < 100; i++)
                sendAck();*/

            errorHandler();

        } else if(!ret.isEmpty()) {
            if(ret.at(0) == PackageFiscal::NAK && m_nak_count <= 3) { // ! NAK
                m_nak_count++;
                SleeperThread::msleep(100);
                continue;
            }

            m_nak_count = 0;
            errorHandler_count = 0;

            sendAck();

            if(pkg->cmd() == CMD_CLOSEFISCALRECEIPT || pkg->cmd() == CMD_CLOSEDNFH) {
                emit fiscalReceiptNumber(pkg->id(), getReceiptNumber(ret), pkg->ftype());
            }

            queue.pop_front();
            delete pkg;

        } else {

            sendAck();
            queue.clear();
            /*
            if(m_nak_count <= 3) {
                m_nak_count++;
                continue;
            }*/

            //m_nak_count++;
        }

    }
}

void DriverFiscalHasar::errorHandler()
{
#ifdef DEBUG
    log << "DriverFiscalHasar::errorHandler() -> fixing";
#endif

    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_TOTALTENDER);

    QByteArray d;
    d.append("Contado");
    d.append(PackageFiscal::FS);
    d.append(QString::number(1, 'f', 2));
    d.append(PackageFiscal::FS);
    d.append('T');
    if(m_model == FiscalPrinter::Hasar615F || m_model == FiscalPrinter::Hasar715F) {
        d.append(PackageFiscal::FS);
        d.append("0");
    }
    p->setData(d);
    m_connector->write(p->fiscalPackage());


    delete p;
    sendAck();

    p = new PackageHasar;
    p->setCmd(CMD_CLOSEFISCALRECEIPT);
    p->setFtype(0);
    p->setId(-1);
    m_connector->write(p->fiscalPackage());

    delete p;
    sendAck();

    p = new PackageHasar;
    p->setCmd(CMD_CLOSENONFISCALRECEIPT);

    QByteArray z;
    p->setData(z);
    m_connector->write(p->fiscalPackage());

    delete p;
    sendAck();


    SleeperThread::msleep(200);
    statusRequest();

}

void DriverFiscalHasar::sendAck()
{
#ifdef DEBUG
    log << QString("Sending ACK");
#endif

    QByteArray ack;
    ack.append(PackageFiscal::ACK);
    m_connector->write(ack);
}

int DriverFiscalHasar::getReceiptNumber(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 14);
    for(int i = 0; i < tmp.size(); i++) {
        if(m_model == FiscalPrinter::Hasar330F) {
            if(tmp.at(i) == PackageFiscal::FS) {
                tmp.remove(i, tmp.size());
                break;
            }
        } else {
            if(tmp.at(i) == PackageFiscal::ETX) {
                tmp.remove(i, tmp.size());
                break;
            }
        }
    }

#ifdef DEBUG
    log << QString("F. Num: %1").arg(tmp.trimmed().toInt());
#endif
    return tmp.trimmed().toInt();
}

bool DriverFiscalHasar::getStatus(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 11);
    tmp.remove(5, tmp.size());

    return tmp == QByteArray("0000");
}

QByteArray DriverFiscalHasar::readData(const int pkg_cmd, const QByteArray &secuence)
{
    bool ok = false;
    int count_tw = 0;
    QByteArray bytes;

    do {
        if(m_connector->bytesAvailable() <= 0 && m_continue) {
//            qDebug() << "NB tw";
            SleeperThread::msleep(100);
        }

        if(!m_continue)
            return "";

        QByteArray bufferBytes = m_connector->read(1);
        if(bufferBytes.at(0) == PackageFiscal::ACK) {
            SleeperThread::msleep(100);
        } else if(bufferBytes.at(0) == PackageFiscal::DC1 || bufferBytes.at(0) == PackageFiscal::DC2
                || bufferBytes.at(0) == PackageFiscal::DC3 || bufferBytes.at(0) == PackageFiscal::DC4
                || bufferBytes.at(0) == PackageFiscal::ACK) {
            SleeperThread::msleep(100);
            count_tw -= 30;
            continue;
        } else if(bufferBytes.at(0) == PackageFiscal::NAK) {
#ifdef DEBUG
            log << QString("NAK");
#endif
            return bufferBytes;
        } else if(bufferBytes.at(0) == PackageFiscal::STX) {
            bytes += PackageFiscal::STX;

            bufferBytes = m_connector->read(1);
            while(bufferBytes.at(0) != PackageFiscal::ETX) {
                count_tw++;
                if(bufferBytes.isEmpty()) {
                    SleeperThread::msleep(100);
                }

                bytes += bufferBytes;
                bufferBytes = m_connector->read(1);

                if(count_tw >= MAX_TW)
                    break;
            }

            bytes += PackageFiscal::ETX;

            QByteArray checkSumArray;
            int checksumCount = 0;
            while(checksumCount != 4) {
                checkSumArray = m_connector->read(1);
                count_tw++;
                if(checkSumArray.isEmpty()) {
                    SleeperThread::msleep(100);
                } else {
                    checksumCount++;
                    bytes += checkSumArray;
                }

                if(count_tw >= MAX_TW)
                    break;
            }

            ok = true;
            break;
        } else {
            bytes += bufferBytes;
        }
        count_tw++;

    } while(ok != true && count_tw <= MAX_TW && m_continue);

#ifdef DEBUG
    log << QString("DriverFiscalHasar::readData() -> counter:  %1 %2").arg(count_tw).arg(MAX_TW);
#endif

    ok = verifyResponse(bytes, pkg_cmd);

    if(!ok) {
#ifdef DEBUG
        log << QString("DriverFiscalHasar::readData() -> error : %1").arg(bytes.toHex().data());
#endif
        if(pkg_cmd == 42) {
#ifdef DEBUG
            log << QString("DriverFiscalHasar::readData() -> sending FiscalPrinter::Error");
#endif
            m_connector->readAll();
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return "-1";
    } else {
#ifdef DEBUG
        log << QString("DriverFiscalHasar::readData() -> OK PGV3: %1").arg(bytes.toHex().data());
#endif
        emit fiscalStatus(FiscalPrinter::Ok);
    }

    return bytes;
}

bool DriverFiscalHasar::verifyResponse(const QByteArray &bytes, const int pkg_cmd)
{
    if(bytes.at(0) != PackageFiscal::STX) {
//        qDebug() << QString("NO STX %1").arg(bytes.toHex().data());
        if(pkg_cmd == 42) {
            //qDebug() << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    if(QChar(bytes.at(2)).unicode() != pkg_cmd) {
        //qDebug() << QString("ERR - diff cmds: %1 %2").arg(QChar(bytes.at(2)).unicode()).arg(pkg_cmd);
        if(pkg_cmd == 42) {
          //  qDebug() << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }

        return false;
    }

    m_error = false;

    if(bytes.at(3) != PackageFiscal::FS) {
        //qDebug() << QString("Error: diff FS");

        if(pkg_cmd == 42) {
            //qDebug() << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    if(bytes.at(bytes.size() - 5) != PackageFiscal::ETX) {
        //qDebug() << QString("Error: ETX");

        if(pkg_cmd == 42) {
            //qDebug() << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    return true;

    if(!checkSum(bytes)) {
        //qDebug() << QString("Error: checksum");

        if(pkg_cmd == 42) {
            //qDebug() << QString("SIGNAL ->> ENVIO STATUS ERROR");
            emit fiscalStatus(FiscalPrinter::Error);
        }
        return false;
    }

    return true;
}

bool DriverFiscalHasar::checkSum(const QString &data)
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

void DriverFiscalHasar::statusRequest()
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_STATUS);

    queue.append(p);
    start();
}

void DriverFiscalHasar::dailyClose(const char type)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_DAILYCLOSE);

    QByteArray d;
    d.append(type);
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::dailyCloseByDate(const QDate &from, const QDate &to)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_DAILYCLOSEBYDATE);

    QByteArray d;
    d.append(from.toString("yyMMdd"));
    d.append(PackageFiscal::FS);
    d.append(to.toString("yyMMdd"));
    d.append(PackageFiscal::FS);
    d.append('T');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::dailyCloseByNumber(const int from, const int to)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_DAILYCLOSEBYNUMBER);

    QByteArray d;
    d.append(QString::number(from));
    d.append(PackageFiscal::FS);
    d.append(QString::number(to));
    d.append(PackageFiscal::FS);
    d.append('T');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::setCustomerData(const QString &name, const QString &cuit, const char tax_type,
        const QString &doc_type, const QString &address)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_SETCUSTOMERDATA);

    QByteArray d;
    if(m_model == FiscalPrinter::Hasar330F || m_model == FiscalPrinter::Hasar320F
            || m_model == FiscalPrinter::Hasar715F) {
        d.append(name.left(49));
        d.append(PackageFiscal::FS);
        d.append(cuit);
        d.append(PackageFiscal::FS);
        d.append(tax_type);
        d.append(PackageFiscal::FS);
        d.append(doc_type);
        d.append(PackageFiscal::FS);
        d.append(address.left(40));
    } else {
        d.append(name.left(49));
        d.append(PackageFiscal::FS);
        d.append(cuit);
        d.append(PackageFiscal::FS);
        d.append(tax_type);
        d.append(PackageFiscal::FS);
        d.append(doc_type);
        if(m_model == FiscalPrinter::Hasar715F) {
            d.append(PackageFiscal::FS);
            d.append(address.left(40));
        } else {
            /// setHeaderTrailer??
        }
    }
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::openFiscalReceipt(const char type)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_OPENFISCALRECEIPT);

    QByteArray d;
    d.append(type);
    d.append(PackageFiscal::FS);
    d.append('T');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::printFiscalText(const QString &text)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_PRINTFISCALTEXT);

    QByteArray d;
    d.append(text.left(50));
    d.append(PackageFiscal::FS);
    d.append("0");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::printLineItem(const QString &description, const qreal quantity,
        const qreal price, const QString &tax, const char qualifier, const qreal excise)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_PRINTLINEITEM);

    QByteArray d;
    if(m_model == FiscalPrinter::Hasar615F || m_model == FiscalPrinter::Hasar715F)
        d.append(description.left(18));
    else
        d.append(description.left(62));
    d.append(PackageFiscal::FS);
    d.append(QString::number(quantity, 'f', 2));
    d.append(PackageFiscal::FS);
    d.append(QString::number(price, 'f', 2));
    d.append(PackageFiscal::FS);
    d.append(tax);
    d.append(PackageFiscal::FS);
    d.append(qualifier);
    d.append(PackageFiscal::FS);
    d.append("0.00");
    d.append(PackageFiscal::FS);
    d.append("0");
    d.append(PackageFiscal::FS);
    d.append('T');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::perceptions(const QString &desc, qreal tax_amount)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_PERCEPTIONS);

    QByteArray d;
    d.append("**.**");
    d.append(PackageFiscal::FS);
    d.append(desc);
    d.append(PackageFiscal::FS);
    d.append(QString::number(tax_amount, 'f', 2));
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::subtotal(const char print)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_SUBTOTAL);

    QByteArray d;
    d.append(print);
    d.append(PackageFiscal::FS);
    d.append("Subtotal");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::totalTender(const QString &description, const qreal amount, const char type)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_TOTALTENDER);

    QByteArray d;
    d.append(description.left(49));
    d.append(PackageFiscal::FS);
    d.append(QString::number(amount, 'f', 2));
    d.append(PackageFiscal::FS);
    d.append(type);
    if(m_model == FiscalPrinter::Hasar615F || m_model == FiscalPrinter::Hasar715F) {
        d.append(PackageFiscal::FS);
        d.append("0");
    }
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_GENERALDISCOUNT);

    QByteArray d;
    d.append(description.left(49));
    d.append(PackageFiscal::FS);
    d.append(QString::number(amount, 'f', 2));
    d.append(PackageFiscal::FS);
    d.append(type);
    d.append(PackageFiscal::FS);
    d.append("0");
    d.append(PackageFiscal::FS);
    d.append('T');

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::closeFiscalReceipt(const char intype, const char f_type, const int id)
{
    Q_UNUSED(intype);

    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_CLOSEFISCALRECEIPT);
    if(f_type == 'S')
        p->setFtype(0);
    else if(f_type == 'r')
        p->setFtype(3);
    p->setId(id);
    queue.append(p);
    start();
}

void DriverFiscalHasar::openNonFiscalReceipt()
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_OPENNONFISCALRECEIPT);

    QByteArray d;
    d.append(" ");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::printNonFiscalText(const QString &text)
{
    QString t = text;
    while(!t.isEmpty()) {
        PackageHasar *p = new PackageHasar;
        p->setCmd(CMD_PRINTNONTFISCALTEXT);
        p->setData(t.left(40));
        t.remove(0, 40);
        queue.append(p);
    }

    start();
}

void DriverFiscalHasar::closeNonFiscalReceipt()
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_CLOSENONFISCALRECEIPT);

    QByteArray d;
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::openDrawer()
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_OPENDRAWER);

    queue.append(p);
    start();
}

void DriverFiscalHasar::setHeaderTrailer(const QString &header, const QString &trailer)
{
    // HEADER is set
    /*
    if(!header.isEmpty()) {
        PackageHasar *p = new PackageHasar;
        p->setCmd(CMD_SETHEADERTRAILER);

        QByteArray d;
        d.append("1");
        d.append(PackageFiscal::FS);
        d.append(header.left(120));
        p->setData(d);

        queue.append(p);
        start();
    }*/


    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_SETHEADERTRAILER);
    QByteArray d;
    d.append("12");
    d.append(PackageFiscal::FS);

    if(!trailer.isEmpty()) {
        d.append(trailer.left(120));
    } else {
        d.append(0x7f);
    }

    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::setEmbarkNumber(const int doc_num, const QString &description, const char type)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_EMBARKNUMBER);

    QByteArray d;
    d.append(QString::number(doc_num));
    d.append(PackageFiscal::FS);
    d.append(description);
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::openDNFH(const char type, const char fix_value, const QString &doc_num)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_OPENDNFH);

    QByteArray d;
    d.append(type);
    d.append(PackageFiscal::FS);
    d.append(fix_value);

    if(m_model == FiscalPrinter::Hasar330F || m_model == FiscalPrinter::Hasar320F) {
        d.append(PackageFiscal::FS);
        d.append(doc_num);
    }
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::printEmbarkItem(const QString &description, const qreal quantity)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_PRINTEMBARKITEM);

    QByteArray d;
    d.append(description);
    d.append(PackageFiscal::FS);
    d.append(QString::number(quantity, 'f', 4));
    d.append(PackageFiscal::FS);
    d.append("0");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::closeDNFH(const int id, const char f_type, const int copies)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_CLOSEDNFH);

    if(m_model == FiscalPrinter::Hasar330F || m_model == FiscalPrinter::Hasar320F) {
        QByteArray d;
        d.append(QString::number(copies));
        p->setData(d);
    }
    p->setFtype(f_type == 'R' ? 1 : 2);
    p->setId(id);

    queue.append(p);
    start();
}

void DriverFiscalHasar::cancel()
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_CANCEL);

    queue.append(p);
    start();
}

void DriverFiscalHasar::ack()
{
    sendAck();
}

void DriverFiscalHasar::setDateTime(const QDateTime &dateTime)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_SETDATETIME);
    QByteArray d;
    d.append(dateTime.date().toString("yyMMdd"));
    d.append(PackageFiscal::FS);
    d.append(dateTime.time().toString("HHmmss"));
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::receiptText(const QString &text)
{
    PackageHasar *p = new PackageHasar;
    p->setCmd(CMD_RECEIPTTEXT);
    QByteArray d;
    d.append(text.left(100));
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalHasar::reprintDocument(const QString &doc_type, const int doc_number)
{
}

void DriverFiscalHasar::reprintContinue()
{
}

void DriverFiscalHasar::reprintFinalize()
{
}

void DriverFiscalHasar::setFixedData(const QString &shop, const QString &phone)
{
    QByteArray d;

    PackageHasar *pp = new PackageHasar;
    pp->setCmd(0x5d);

    d.append("11");
    d.append(PackageFiscal::FS);
    d.append("DEFENSA CONSUMIDOR " + phone);
    pp->setData(d);
    queue.append(pp);
    d.clear();

    PackageHasar *p1 = new PackageHasar;
    p1->setCmd(0x5d);

    d.append("12");
    d.append(PackageFiscal::FS);
    d.append("Ingresa en www.global.subway.com Danos");
    p1->setData(d);
    queue.append(p1);
    d.clear();

    PackageHasar *p2 = new PackageHasar;
    p2->setCmd(0x5d);

    d.append("13");
    d.append(PackageFiscal::FS);
    d.append("tu opinion y obtene una COOKIE GRATIS");
    p2->setData(d);
    queue.append(p2);
    d.clear();

    PackageHasar *p3 = new PackageHasar;
    p3->setCmd(0x5d);

    d.append("14");
    d.append(PackageFiscal::FS);
    d.append(QString("en tu proxima compra. Tienda: %1-0").arg(shop));
    p3->setData(d);
    queue.append(p3);
    d.clear();

    /*
    PackageHasar *p4 = new PackageHasar;
    p4->setCmd(0x5d);

    d.append("15");
    d.append(PackageFiscal::FS);
    d.append("compra. Valido dentro de los 5 dias de");
    p4->setData(d);
    queue.append(p4);
    d.clear();

    PackageHasar *p5 = new PackageHasar;
    p5->setCmd(0x5d);

    d.append("16");
    d.append(PackageFiscal::FS);
    d.append(QString("emision del ticket. Tienda: %1-0").arg(shop));
    p5->setData(d);
    queue.append(p5);
    d.clear();

    PackageHasar *p6 = new PackageHasar;
    p6->setCmd(0x5d);

    d.append("17");
    d.append(PackageFiscal::FS);
    d.append(0x7f);
    p6->setData(d);
    queue.append(p6);
    d.clear();

    PackageHasar *p7 = new PackageHasar;
    p7->setCmd(0x5d);

    d.append("18");
    d.append(PackageFiscal::FS);
    d.append(0x7f);
    p7->setData(d);
    queue.append(p7);
    d.clear();

    PackageHasar *p8 = new PackageHasar;
    p8->setCmd(0x5d);

    d.append("19");
    d.append(PackageFiscal::FS);
    d.append(0x7f);
    p8->setData(d);
    queue.append(p8);
    d.clear();

    PackageHasar *p9 = new PackageHasar;
    p9->setCmd(0x5d);

    d.append("20");
    d.append(PackageFiscal::FS);
    d.append(0x7f);
    p9->setData(d);
    queue.append(p9);
    d.clear();

    PackageHasar *p10 = new PackageHasar;
    p10->setCmd(0x5d);

    d.append("21");
    d.append(PackageFiscal::FS);
    d.append(0x7f);
    p10->setData(d);
    queue.append(p10);
    d.clear();

    PackageHasar *p11 = new PackageHasar;
    p11->setCmd(0x5d);

    d.append("22");
    d.append(PackageFiscal::FS);
    d.append(0x7f);
    p11->setData(d);
    queue.append(p11);
    d.clear();
    */

    start();
}

void DriverFiscalHasar::getTransactionalMemoryInfo()
{
}

void DriverFiscalHasar::downloadReportByDate(const QString &type, const QDate &form, const QDate &to)
{
}

void DriverFiscalHasar::downloadReportByNumber(const QString &type, const int from, const int to)
{
}

void DriverFiscalHasar::downloadContinue()
{
}

void DriverFiscalHasar::downloadFinalize()
{
}

void DriverFiscalHasar::downloadDelete(const int to)
{
}
