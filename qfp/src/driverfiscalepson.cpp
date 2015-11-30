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

#include "driverfiscalepson.h"
#include "packagefiscal.h"

#if LOGGER
#include "logger.h"
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

int PackageEpson::m_secuence = 0x20;

DriverFiscalEpson::DriverFiscalEpson(QObject *parent, SerialPort *m_serialPort)
    : QThread(parent), DriverFiscal(parent, m_serialPort)
{
    QString path = QDir::homePath() + QDir::separator() + ".config" + QDir::separator() + "Subway" + QDir::separator();
#if LOGGER
    Logger::instance()->init(path + "fiscal.txt");
    log << QString("INICIO Time: %1").arg(m_TIME_WAIT);
#endif
    m_nak_count = 0;
    m_error = false;
    m_isinvoice = false;
    m_continue = true;
    clear();
}

void DriverFiscalEpson::setModel(const FiscalPrinter::Model model)
{
    m_model = model;
}

void DriverFiscalEpson::finish()
{
    m_continue = false;
    queue.clear();
    quit();

    while(isRunning()) {
        msleep(100);
        quit();
    }
}

void DriverFiscalEpson::run()
{

    while(!queue.empty() && m_continue) {
        PackageEpson *pkg = queue.first();
        m_serialPort->write(pkg->fiscalPackage());

        QByteArray ret = readData(pkg->cmd());
        if(!ret.isEmpty()) {
            if(ret.at(0) == PackageFiscal::NAK && m_nak_count <= 3) { // ! NAK
                m_nak_count++;
                SleeperThread::msleep(100);
                continue;
            }

            m_nak_count = 0;

            if(pkg->cmd() == CMD_CLOSEFISCALRECEIPT_INVOICE ||
                    pkg->cmd() == CMD_CLOSEFISCALRECEIPT_TICKET || pkg->cmd() == CMD_CLOSEDNFH) {
                if(pkg->data()[0] == 'M') {
                    emit fiscalReceiptNumber(pkg->id(), getReceiptNumber(ret), 1);
                } else {
                    emit fiscalReceiptNumber(pkg->id(), getReceiptNumber(ret), 0);
                }
            }

            queue.pop_front();
            delete pkg;

        } else {
#if LOGGER
            log << QString("FISCAL ERROR?");
#endif
            queue.clear();
        }
    }
}

int DriverFiscalEpson::getReceiptNumber(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 14);
    for(int i = 0; i < tmp.size(); i++) {
        if(tmp.at(i) == PackageFiscal::ETX) {
            tmp.remove(i, tmp.size());
            break;
        }
    }

#if LOGGER
    log << QString("F. Num: %1").arg(tmp.trimmed().toInt());
#endif
    return tmp.trimmed().toInt();
}

bool DriverFiscalEpson::getStatus(const QByteArray &data)
{
    QByteArray tmp = data;
    tmp.remove(0, 11);
    tmp.remove(5, tmp.size());

    return tmp == QByteArray("0000");
}

QByteArray DriverFiscalEpson::readData(const int pkg_cmd)
{
    bool ok = false;
    int count_tw = 0;
    QByteArray bytes;

    do {
        if(m_serialPort->bytesAvailable() <= 0 && m_continue) {
#if LOGGER
            log << QString("NB tw");
#endif
            count_tw++;
            SleeperThread::msleep(100);
        }

        if(!m_continue)
            return "";

        QByteArray bufferBytes = m_serialPort->read(1);
        if(bufferBytes.at(0) == PackageFiscal::DC1 || bufferBytes.at(0) == PackageFiscal::DC2
                || bufferBytes.at(0) == PackageFiscal::DC3 || bufferBytes.at(0) == PackageFiscal::DC4
                || bufferBytes.at(0) == PackageFiscal::FNU) {
            SleeperThread::msleep(100);
            //count_tw -= 20;
            continue;
        } else if(bufferBytes.at(0) == PackageFiscal::NAK) {
            return bufferBytes;
        } else if(bufferBytes.at(0) == PackageFiscal::STX) {
            bytes += PackageFiscal::STX;

            bufferBytes = m_serialPort->read(1);
            while(bufferBytes.at(0) != PackageFiscal::ETX) {
                count_tw++;
                if(bufferBytes.isEmpty()) {
                    SleeperThread::msleep(100);
                }

                bytes += bufferBytes;
                bufferBytes = m_serialPort->read(1);

                if(count_tw >= MAX_TW)
                    break;
            }

            bytes += PackageFiscal::ETX;

            QByteArray checkSumArray;
            int checksumCount = 0;
            while(checksumCount != 4 && m_continue) {
                checkSumArray = m_serialPort->read(1);
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

#if LOGGER
    log << QString("COUNTER: %1 %2").arg(count_tw).arg(MAX_TW);
#endif

    ok = verifyResponse(bytes, pkg_cmd);

    if(!ok) {
#if LOGGER
        log << QString("ERROR READ: %1").arg(bytes.toHex().data());
#endif
        if(pkg_cmd == 42) {
#if LOGGER
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
#endif
            m_serialPort->readAll();
            emit fiscalStatus(false);
        }
    } else {
#if LOGGER
        log << QString("--> OK PKGV3: %1").arg(bytes.toHex().data());
#endif
    }

    return bytes;
}

bool DriverFiscalEpson::verifyResponse(const QByteArray &bytes, const int pkg_cmd)
{
    if(bytes.at(0) != PackageFiscal::STX) {
#if LOGGER
        log << QString("NO STX %1").arg(bytes.toHex().data());
#endif
        if(pkg_cmd == 42) {
#if LOGGER
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
#endif
            emit fiscalStatus(false);
        }
        return false;
    }

    if(QChar(bytes.at(2)).unicode() != pkg_cmd) {
#if LOGGER
        log << QString("ERR - diff cmds: %1 %2").arg(QChar(bytes.at(2)).unicode()).arg(pkg_cmd);
#endif
        if(pkg_cmd == 42) {
#if LOGGER
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
#endif
            emit fiscalStatus(false);
        }

        return false;
    }

    m_error = false;

    if(bytes.at(3) != PackageFiscal::FS) {
#if LOGGER
        log << QString("Error: diff FS");
#endif

        if(pkg_cmd == 42) {
#if LOGGER
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
#endif
            emit fiscalStatus(false);
        }
        return false;
    }

    if(bytes.at(bytes.size() - 5) != PackageFiscal::ETX) {
#if LOGGER
        log << QString("Error: ETX");
#endif

        if(pkg_cmd == 42) {
#if LOGGER
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
#endif
            emit fiscalStatus(false);
        }
        return false;
    }

    if(!checkSum(bytes)) {
#if LOGGER
        log << QString("Error: checksum");
#endif

        if(pkg_cmd == 42) {
#if LOGGER
            log << QString("SIGNAL ->> ENVIO STATUS ERROR");
#endif
            emit fiscalStatus(false);
        }
        return false;
    }

    return true;
}

bool DriverFiscalEpson::checkSum(const QString &data)
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

void DriverFiscalEpson::statusRequest()
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_STATUS);
    p->setData(QByteArray("S"));

    queue.append(p);
    start();
}

void DriverFiscalEpson::dailyClose(const char type)
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_DAILYCLOSE);

    QByteArray d;
    d.append(type);
    d.append(PackageFiscal::FS);
    d.append('P');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::dailyCloseByDate(const QDate &from, const QDate &to)
{
    PackageEpson *p = new PackageEpson;
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

void DriverFiscalEpson::dailyCloseByNumber(const int from, const int to)
{
    PackageEpson *p = new PackageEpson;
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

void DriverFiscalEpson::setCustomerData(const QString &name, const QString &cuit, const char tax_type,
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
        m_doc_type = "CUIT";
    else if(doc_type.compare("3") == 0)
        m_doc_type = "PASAPO";
    else
        m_doc_type = "DNI";
    m_address = address;
}

void DriverFiscalEpson::openFiscalReceipt(const char type)
{
    PackageEpson *p = new PackageEpson;

    QByteArray d;
    if(type == 'A' || type == 'B') {
        m_isinvoice = true;
        p->setCmd(CMD_OPENFISCALRECEIPT);
        d.append('T');
        d.append(PackageFiscal::FS);
        d.append('C');
        d.append(PackageFiscal::FS);
        d.append(type);
        d.append(PackageFiscal::FS);
        d.append('1');
        d.append(PackageFiscal::FS);
        d.append('F');
        d.append(PackageFiscal::FS);
        d.append(QString::number(12).toLatin1());
        d.append(PackageFiscal::FS);
        d.append('I');
        d.append(PackageFiscal::FS);
        d.append(m_tax_type);
        d.append(PackageFiscal::FS);
        d.append(m_name);
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append(m_doc_type);
        d.append(PackageFiscal::FS);
        d.append(m_cuit);
        d.append(PackageFiscal::FS);
        d.append('N');
        d.append(PackageFiscal::FS);
        d.append(m_address);
        d.append(PackageFiscal::FS);
        d.append(m_address1);
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append(m_refer);
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
    } else { // 'T'
        m_isinvoice = false;
        p->setCmd(CMD_OPENTICKET);
    }

    d.append('C');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::printFiscalText(const QString &text)
{
}

void DriverFiscalEpson::printLineItem(const QString &description, const qreal quantity,
        const qreal price, const QString &tax, const char qualifier)
{

    PackageEpson *p = new PackageEpson;
    p->setCmd(m_isinvoice ? CMD_PRINTLINEITEM_INVOICE : CMD_PRINTLINEITEM_TICKET);

    QByteArray d;
    d.append(description.left(20));
    d.append(PackageFiscal::FS);
    d.append(QString::number(quantity * 1000, 'f', 0));
    d.append(PackageFiscal::FS);
    if(m_tax_type == 'I') {
        d.append(QString::number(price/(1+(tax.toDouble()/100)) * 100, 'f', 0));
    } else {
        d.append(QString::number(price * 100, 'f', 0));
    }
    d.append(PackageFiscal::FS);
    d.append(QString::number(tax.toDouble() * 100, 'f', 0));
    d.append(PackageFiscal::FS);
    d.append(qualifier);
    d.append(PackageFiscal::FS);
    if(m_isinvoice) {
        d.append("0000");
        d.append(PackageFiscal::FS);
        d.append("00000000");
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append("0000");
        d.append(PackageFiscal::FS);
        d.append("00000000000000000");
    } else {
        d.append("0");
        d.append(PackageFiscal::FS);
        d.append("00000000");
        d.append(PackageFiscal::FS);
        d.append("00000000000000000");
    }
    p->setData(d);

    queue.append(p);
    start();

}

void DriverFiscalEpson::perceptions(const QString &desc, qreal tax_amount)
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_PERCEPTIONS);

    QByteArray d;
    d.append(desc);
    d.append(PackageFiscal::FS);
    d.append('O');
    d.append(PackageFiscal::FS);
    d.append(QString::number(tax_amount * 100, 'f', 0));
    d.append(PackageFiscal::FS);
    d.append("0");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::subtotal(const char print)
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_SUBTOTAL);

    QByteArray d;
    d.append(print);
    d.append(PackageFiscal::FS);
    d.append("Subtotal");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::totalTender(const QString &description, const qreal amount, const char type)
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(m_isinvoice ? CMD_TOTALTENDER_INVOICE : CMD_TOTALTENDER_TICKET);

    QByteArray d;
    d.append(description);
    d.append(PackageFiscal::FS);
    d.append(QString::number(amount * 100, 'f', 0).rightJustified(9, '0'));
    d.append(PackageFiscal::FS);
    d.append(type);
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::generalDiscount(const QString &description, const qreal amount, const char type)
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(m_isinvoice ? CMD_PRINTLINEITEM_INVOICE : CMD_PRINTLINEITEM_TICKET);

    QByteArray d;
    d.append(description.left(20));
    d.append(PackageFiscal::FS);
    d.append("1000");
    d.append(PackageFiscal::FS);
    if(m_tax_type == 'I') {
        d.append(QString::number((amount/1.21) * 100, 'f', 0));
//	const qreal a = amount * 21 / 100;
 //       d.append(QString::number((amount) * 100, 'f', 0));
    } else {
        d.append(QString::number(amount * 100, 'f', 0));
    }
    d.append(PackageFiscal::FS);
    d.append("2100");
    d.append(PackageFiscal::FS);
    d.append(type == 'M' ? 'M' : 'R');
    d.append(PackageFiscal::FS);
    if(m_isinvoice) {
        d.append("0000");
        d.append(PackageFiscal::FS);
        d.append("00000000");
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append("");
        d.append(PackageFiscal::FS);
        d.append("0000");
        d.append(PackageFiscal::FS);
        d.append("00000000000000000");
    } else {
        d.append("0");
        d.append(PackageFiscal::FS);
        d.append("00000000");
        d.append(PackageFiscal::FS);
        d.append("00000000000000000");
    }
    p->setData(d);

    queue.append(p);
    start();

}

void DriverFiscalEpson::closeFiscalReceipt(const char intype, const char type, const int id)
{
    PackageEpson *p = new PackageEpson;
    p->setId(id);
    p->setCmd(m_isinvoice ? CMD_CLOSEFISCALRECEIPT_INVOICE : CMD_CLOSEFISCALRECEIPT_TICKET);

    QByteArray d;
    if(m_isinvoice) {
        d.append(intype);
        d.append(PackageFiscal::FS);
        d.append(type);
        d.append(PackageFiscal::FS);
        d.append("0");
    } else {
        d.append('T');
    }


    clear();

    p->setData(d);
    queue.append(p);
    start();

}

void DriverFiscalEpson::openNonFiscalReceipt()
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_OPENNONFISCALRECEIPT);

    QByteArray d;
    d.append(" ");
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::printNonFiscalText(const QString &text)
{
    QString t = text;
    while(!t.isEmpty()) {
        PackageEpson *p = new PackageEpson;
        p->setCmd(CMD_PRINTNONTFISCALTEXT);
        p->setData(t.left(39));
        t.remove(0, 39);
        queue.append(p);
    }

    start();
}

void DriverFiscalEpson::closeNonFiscalReceipt()
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_CLOSENONFISCALRECEIPT);

    QByteArray d;
    d.append('T');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::openDrawer()
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_OPENDRAWER);

    queue.append(p);
    start();
}

void DriverFiscalEpson::setHeaderTrailer(const QString &header, const QString &trailer)
{
}

void DriverFiscalEpson::setEmbarkNumber(const int doc_num, const QString &description)
{
    m_refer = description;
}

void DriverFiscalEpson::openDNFH(const char type, const char fix_value, const QString &doc_num)
{
    PackageEpson *p = new PackageEpson;

    QByteArray d;
    m_isinvoice = true;
    p->setCmd(CMD_OPENFISCALRECEIPT);
    d.append('M');
    d.append(PackageFiscal::FS);
    d.append('C');
    d.append(PackageFiscal::FS);
    d.append(type == 'R' ? 'A' : 'B');
    d.append(PackageFiscal::FS);
    d.append('1');
    d.append(PackageFiscal::FS);
    d.append('F');
    d.append(PackageFiscal::FS);
    d.append(QString::number(12).toLatin1());
    d.append(PackageFiscal::FS);
    d.append('I');
    d.append(PackageFiscal::FS);
    d.append(m_tax_type);
    d.append(PackageFiscal::FS);
    d.append(m_name);
    d.append(PackageFiscal::FS);
    d.append("");
    d.append(PackageFiscal::FS);
    d.append(m_doc_type);
    d.append(PackageFiscal::FS);
    d.append(m_cuit);
    d.append(PackageFiscal::FS);
    d.append('N');
    d.append(PackageFiscal::FS);
    d.append(m_address);
    d.append(PackageFiscal::FS);
    d.append(m_address1);
    d.append(PackageFiscal::FS);
    d.append("");
    d.append(PackageFiscal::FS);
    d.append(doc_num);
    d.append(PackageFiscal::FS);
    d.append("");
    d.append(PackageFiscal::FS);
    d.append('C');
    p->setData(d);

    queue.append(p);
    start();
}

void DriverFiscalEpson::printEmbarkItem(const QString &description, const qreal quantity)
{
}

void DriverFiscalEpson::closeDNFH(const int id, const char f_type, const int copies)
{
    Q_UNUSED(f_type);
    PackageEpson *p = new PackageEpson;
    p->setId(id);
    p->setCmd(CMD_CLOSEFISCALRECEIPT_INVOICE);

    QByteArray d;
    d.append('M');
    d.append(PackageFiscal::FS);
    d.append(m_tax_type == 'I' ? 'A' : 'B');
    d.append(PackageFiscal::FS);
    d.append("0");

    clear();

    p->setData(d);

    qDebug() << (p->data()[0] == 'M');
    queue.append(p);
    start();
}

void DriverFiscalEpson::cancel()
{
}

void DriverFiscalEpson::ack()
{
}

void DriverFiscalEpson::setDateTime(const QDateTime &dateTime)
{
    PackageEpson *p = new PackageEpson;
    p->setCmd(CMD_SETDATETIME);

    QByteArray d;
    d.append(dateTime.toString("yyMMdd").toLatin1());
    d.append(PackageFiscal::FS);
    d.append(dateTime.toString("HHmmss").toLatin1());

    p->setData(d);
    queue.append(p);

    start();
}

void DriverFiscalEpson::clear()
{
    m_name = "Consumidor Final";
    m_cuit = "0";
    m_tax_type = 'F';
    m_doc_type = "DNI";
    m_address = "-";
    m_address1 = "";
    m_refer = "-";
}

void DriverFiscalEpson::receiptText(const QString &text)
{
}

void DriverFiscalEpson::setFixedData(const QString &shop, const QString &phone)
{
    QByteArray d;

    PackageEpson *pp = new PackageEpson;
    pp->setCmd(0x5d);

    d.append("00011");
    d.append(PackageFiscal::FS);
    d.append("DEFENSA CONSUMIDOR " + phone);
    pp->setData(d);
    queue.append(pp);
    d.clear();

    PackageEpson *p = new PackageEpson;
    p->setCmd(0x5d);


    d.append("00012");
    d.append(PackageFiscal::FS);
    d.append("*************************************");
    p->setData(d);
    queue.append(p);
    d.clear();

    PackageEpson *p1 = new PackageEpson;
    p1->setCmd(0x5d);

    d.append("00013");
    d.append(PackageFiscal::FS);
    d.append("Entra con este ticket a:");
    p1->setData(d);
    queue.append(p1);
    d.clear();

    PackageEpson *p2 = new PackageEpson;
    p2->setCmd(0x5d);

    d.append("00014");
    d.append(PackageFiscal::FS);
    d.append("www.cuentaleasubway.com y llevate una");
    p2->setData(d);
    queue.append(p2);
    d.clear();

    PackageEpson *p3 = new PackageEpson;
    p3->setCmd(0x5d);

    d.append("00015");
    d.append(PackageFiscal::FS);
    d.append("COOKIE GRATIS en tu proxima compra.");
    p3->setData(d);
    queue.append(p3);
    d.clear();

    PackageEpson *p4 = new PackageEpson;
    p4->setCmd(0x5d);

    d.append("00016");
    d.append(PackageFiscal::FS);
    d.append("Restaurante ID: " + shop);
    p4->setData(d);
    queue.append(p4);
    d.clear();

    PackageEpson *p5 = new PackageEpson;
    p5->setCmd(0x5d);

    d.append("00017");
    d.append(PackageFiscal::FS);
    d.append("*************************************");
    p5->setData(d);
    queue.append(p5);
    d.clear();

    PackageEpson *p6 = new PackageEpson;
    p6->setCmd(0x5d);

    d.append("00018");
    d.append(PackageFiscal::FS);
    d.append(" ");
    p6->setData(d);
    queue.append(p6);
    d.clear();

    PackageEpson *p7 = new PackageEpson;
    p7->setCmd(0x5d);

    d.append("00019");
    d.append(PackageFiscal::FS);
    d.append(" ");
    p7->setData(d);
    queue.append(p7);
    d.clear();

    PackageEpson *p8 = new PackageEpson;
    p8->setCmd(0x5d);

    d.append("00020");
    d.append(PackageFiscal::FS);
    d.append(" ");
    p8->setData(d);
    queue.append(p8);
    d.clear();

    PackageEpson *p9 = new PackageEpson;
    p9->setCmd(0x5d);

    d.append("00021");
    d.append(PackageFiscal::FS);
    d.append(" ");
    p9->setData(d);
    queue.append(p9);
    d.clear();

    start();
}
