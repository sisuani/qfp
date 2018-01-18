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

#include "driverfiscalhasar2g.h"
#include "packagefiscal.h"

#include <QDateTime>
#include <QEventLoop>


DriverFiscalHasar2G::DriverFiscalHasar2G(QObject *parent, NetworkPort *m_networkPort, int m_TIME_WAIT)
    : QThread(parent), DriverFiscal(parent, m_networkPort, m_TIME_WAIT)
{
    m_error = false;
    cancel_count = 0;
    m_continue = true;
    connect(this, SIGNAL(sendData(const QVariantMap &)),
            m_networkPort, SLOT(post(const QVariantMap &)));
}

void DriverFiscalHasar2G::setModel(const FiscalPrinter::Model model)
{
    qDebug() << "MODEL: " << model << "ORIG: " << FiscalPrinter::Hasar1000F;
    m_model = model;
}

void DriverFiscalHasar2G::run()
{
    while(!queue.empty() && m_continue) {
        QVariantMap pkg = queue.first();
        QVariantMap result;
        sendData(pkg);

        QEventLoop loop;
        connect(m_networkPort, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        if (m_networkPort->lastError() != NetworkPort::NP_NO_ERROR) {
            qDebug() << "Error: " << m_networkPort->lastError();
            continue;
        }

        if (!verifyPackage(pkg, m_networkPort->lastReply())) {
            queue.clear();
            cancel_count++;
            if (cancel_count < 3)
                cancel();
            continue;
        }

        cancel_count = 0;

        queue.pop_front();
    }

}

void DriverFiscalHasar2G::finish()
{
    m_continue = false;
    queue.clear();
    quit();

    while(isRunning()) {
        msleep(100);
        quit();
    }
}


void DriverFiscalHasar2G::errorHandler()
{

}

// XXX: 1
int DriverFiscalHasar2G::getReceiptNumber(const QByteArray &data)
{
    /*
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

#if LOGGER
    qDebug() << QString("F. Num: %1").arg(tmp.trimmed().toInt());
#endif
    return tmp.trimmed().toInt();
    */
}

bool DriverFiscalHasar2G::getStatus(const QVariantMap &status)
{
    if (!status.size())
        return false;


    if (status["Impresora"].toList().size()) {
        if (status["Impresora"].toList().contains("TapaAbierta"))
            return false;
    }

    if (status["Fiscal"].toList().size()) {
        if (status["Fiscal"].toList().contains("ErrorEstado"))
            return false;
    }

    //qDebug() << status["Fiscal"].toMap().size();
    return true;
}

bool DriverFiscalHasar2G::verifyPackage(const QVariantMap &pkg, const QVariantMap &reply)
{
    if (!pkg.keys().size() || !reply.keys().size()) {
        return false;
    }

    const QString cmd = pkg.keys()[0];

    if (cmd != reply.keys()[0])
        return false;

    if (!getStatus(reply[cmd].toMap()["Estado"].toMap()))
        return false;


    qDebug() << "OK";
    return true;
}

void DriverFiscalHasar2G::statusRequest()
{
    QVariantMap d;
    QVariantMap state;

    state["MensajeCF"];
    d["ConsultarEstado"] = state;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::dailyClose(const char type)
{
    QVariantMap d;
    QVariantMap report;

    report["Reporte"] = type == 'Z' ? "ReporteZ" : "ReporteX";
    d["CerrarJornadaFiscal"] = report;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::dailyCloseByDate(const QDate &from, const QDate &to)
{
    QVariantMap d;
    QVariantMap report;

    report["FechaInicial"] = from.toString("yyMMdd");
    report["FechaFinal"] = to.toString("yyMMdd");
    report["Reporte"] = "ReportarAuditoriaGlobal";
    d["ReportarZetasPorFecha"] = report;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::dailyCloseByNumber(const int from, const int to)
{
    QVariantMap d;
    QVariantMap report;

    report["ZetaInicial"] = QString::number(from);
    report["ZetaFinal"] = QString::number(to);
    report["Reporte"] = "ReportarAuditoriaGlobal";
    d["ReportarZetasPorNumeroZeta"] = report;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::setCustomerData(const QString &name, const QString &cuit, const char tax_type,
        const QString &doc_type, const QString &address)
{

    QVariantMap d;
    QVariantMap customer;

    customer["RazonSocial"] = name;
    customer["NumeroDocumento"] = cuit;
    switch (tax_type) {
        case 'C':
            customer["ResponsabilidadIVA"] = "ConsumidorFinal";
            break;
        case 'E':
            customer["ResponsabilidadIVA"] = "ResponsableExento";
            break;
        case 'I':
            customer["ResponsabilidadIVA"] = "ResponsableInscripto";
            break;
        case 'M':
            customer["ResponsabilidadIVA"] = "Monotributista";
            break;
        case 'N':
            customer["ResponsabilidadIVA"] = "NoResponsable";
            break;
        default:
            customer["ResponsabilidadIVA"] = "ConsumidorFinal";
            break;
    }

    if (doc_type.compare("C") == 0)
        customer["TipoDocumento"] = "TipoCUIT";
    else if (doc_type.compare("L") == 0)
        customer["TipoDocumento"] = "TipoCUIL";
    else if (doc_type.compare("2") == 0)
        customer["TipoDocumento"] = "TipoDNI";
    else if (doc_type.compare("3") == 0)
        customer["TipoDocumento"] = "TipoPasaporte";

    customer["Domicilio"] = address;
    d["CargarDatosCliente"] = customer;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::openFiscalReceipt(const char type)
{
    QVariantMap d;
    QVariantMap openReceipt;

    switch (type) {
        case 'B':
            openReceipt["CodigoComprobante"] = "TiqueFacturaB";
            break;
        case 'A':
            openReceipt["CodigoComprobante"] = "TiqueFacturaA";
            break;
        case 'M':
            openReceipt["CodigoComprobante"] = "TiqueNotaCreditoA";
            break;
            // XXX
        case '-':
            openReceipt["CodigoComprobante"] = "TiqueNotaCreditoB";
            break;
        case 'T':
            openReceipt["CodigoComprobante"] = "Tique";
            break;
    }

    d["AbrirDocumento"] = openReceipt;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::printFiscalText(const QString &text)
{
    QVariantMap d;
    QVariantMap t;

    t["Texto"] = text;
    d["ImprimirTextoFiscal"] = t;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::printLineItem(const QString &description, const qreal quantity,
        const qreal price, const QString &tax, const char qualifier, const qreal excise)
{
    Q_UNUSED(quantity);
    Q_UNUSED(excise);

    QVariantMap d;
    QVariantMap item;

    item["Descripcion"] =  description;
    item["Cantidad"] = QString::number(quantity, 'f', 2);
    item["PrecioUnitario"] = QString::number(price, 'f', 2);
    item["CondicionIVA"] = "Gravado";
    item["AlicuotaIVA"] = tax;
    item["OperacionMonto"] = "ModoSumaMonto";
    item["TipoImpuestoInterno"] = "IIVariableKIVA";
    item["MagnitudImpuestoInterno"] = "0.00";
    item["ModoDisplay"] = "DisplayNo";
    item["ModoBaseTotal"] = "ModoPrecioTotal";
    item["UnidadReferencia"] = "1";
    item["CodigoProducto"] = QString(description).remove(" ");
    item["CodigoInterno"] = "";
    item["UnidadMedida"] = "Unidad";
    d["ImprimirItem"] = item;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::perceptions(const QString &desc, qreal tax_amount)
{
    QVariantMap d;
    QVariantMap perc;

    QString code = desc;
    perc["Codigo"] = code.remove(" ");
    perc["Descripcion"] = desc;
    perc["BaseImponible"] = "**.**";
    perc["Importe"] = QString::number(tax_amount, 'f', 2);
    d["ImprimirOtrosTributos"] = perc;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::subtotal(const char print)
{
    QVariantMap d;
    QVariantMap subt;

    subt["Impresion"] = print == 'P' ? "ImprimeSubtotal" : "NoImprimeSubtotal";
    d["ConsultarSubtotal"] = subt;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::totalTender(const QString &description, const qreal amount, const char type)
{
    Q_UNUSED(type);

    QVariantMap d;
    QVariantMap payment;

    payment["Descripcion"] = description;
    payment["Monto"] = QString::number(amount, 'f', 2);
    payment["Operacion"] = "Pagar";
    d["ImprimirPago"] = payment;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::generalDiscount(const QString &description, const qreal amount, const qreal tax_percent, const char type)
{
    Q_UNUSED(tax_percent);

    QVariantMap d;
    QVariantMap discount;

    discount["Descripcion"] = description;
    discount["Monto"] = QString::number(amount, 'f', 2);
    discount["ModoBaseTotal"] = "ModoPrecioTotal";
    d["ImprimirPago"] = discount;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::closeFiscalReceipt(const char intype, const char f_type, const int id)
{
    Q_UNUSED(intype);
    Q_UNUSED(f_type);

    QVariantMap d;
    QVariantMap doc;

    doc["Copias"] = "0";
    d["CerrarDocumento"] = doc;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::openNonFiscalReceipt()
{
    QVariantMap d;
    QVariantMap doc;

    doc["CodigoComprobante"] = "Generico";
    d["AbrirDocumento"] = doc;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::printNonFiscalText(const QString &text)
{
    QVariantMap d;
    QVariantMap mt;

    QString t = text;
    while(!t.isEmpty()) {
        mt["Texto"] = t.left(39);
        d["ImprimirTextoGenerico"] = mt;
        t.remove(0, 39);
        queue.append(d);
    }

    start();
}

void DriverFiscalHasar2G::closeNonFiscalReceipt()
{
    QVariantMap d;
    QVariantMap doc;

    doc["Copias"] = "0";
    d["CerrarDocumento"] = doc;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::openDrawer()
{
    QVariantMap d;

    d["AbrirCajonDinero"] = QVariantMap();

    queue.append(d);
    start();
}

// XXX: 2
void DriverFiscalHasar2G::setHeaderTrailer(const QString &header, const QString &trailer)
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

    /*

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
    */
}

void DriverFiscalHasar2G::setEmbarkNumber(const int doc_num, const QString &description)
{
    Q_UNUSED(description);

    QVariantMap d;
    QVariantMap doc;

    doc["NumeroLinea"] = "1";
    doc["NumeroComprobante"] = QString::number(doc_num).rightJustified('0', 8);
    d["CargarDocumentoAsociado"] = doc;

    queue.append(d);
    start();
}

// XXX: 3
void DriverFiscalHasar2G::openDNFH(const char type, const char fix_value, const QString &doc_num)
{

    /*
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
    */
}

// XXX: 4
void DriverFiscalHasar2G::closeDNFH(const int id, const char f_type, const int copies)
{
    /*
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
    */
}

void DriverFiscalHasar2G::cancel()
{
    QVariantMap d;

    d["Cancelar"] = QVariantMap();

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::setDateTime(const QDateTime &dateTime)
{
    QVariantMap d;
    QVariantMap report;

    report["Fecha"] = dateTime.date().toString("yyMMdd");
    report["Hora"] = dateTime.time().toString("HHmmss");
    d["ConfigurarFechaHora"] = report;

    queue.append(d);
    start();
}

// --------------------------------- //
void DriverFiscalHasar2G::reprintDocument(const QString &doc_type, const int doc_number)
{
}

void DriverFiscalHasar2G::reprintContinue()
{
}

void DriverFiscalHasar2G::reprintFinalize()
{
}

void DriverFiscalHasar2G::setFixedData(const QString &shop, const QString &phone)
{
    /*
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
    d.append("Entra con este ticket a:");
    p1->setData(d);
    queue.append(p1);
    d.clear();

    PackageHasar *p2 = new PackageHasar;
    p2->setCmd(0x5d);

    d.append("13");
    d.append(PackageFiscal::FS);
    d.append("www.cuentaleasubway.com y llevate una");
    p2->setData(d);
    queue.append(p2);
    d.clear();

    PackageHasar *p3 = new PackageHasar;
    p3->setCmd(0x5d);

    d.append("14");
    d.append(PackageFiscal::FS);
    d.append("COOKIE GRATIS, Rest. ID: " + shop);
    p3->setData(d);
    queue.append(p3);
    d.clear();


    start();
    */
}

void DriverFiscalHasar2G::getTransactionalMemoryInfo()
{
}

void DriverFiscalHasar2G::downloadReportByDate(const QString &type, const QDate &form, const QDate &to)
{
}

void DriverFiscalHasar2G::downloadReportByNumber(const QString &type, const int from, const int to)
{
}

void DriverFiscalHasar2G::downloadContinue()
{
}

void DriverFiscalHasar2G::downloadFinalize()
{
}

void DriverFiscalHasar2G::downloadDelete(const int to)
{
}

bool DriverFiscalHasar2G::getStatus(const QByteArray &data)
{
    Q_UNUSED(data);
}

QByteArray DriverFiscalHasar2G::readData(const int pkg_cmd, const QByteArray &secuence)
{
    Q_UNUSED(pkg_cmd);
    Q_UNUSED(secuence);
}

bool DriverFiscalHasar2G::verifyResponse(const QByteArray &bytes, const int pkg_cmd)
{
    Q_UNUSED(bytes);
    Q_UNUSED(pkg_cmd);
}

void DriverFiscalHasar2G::ack()
{
}

bool DriverFiscalHasar2G::checkSum(const QString &data)
{
    Q_UNUSED(data)
}

void DriverFiscalHasar2G::receiptText(const QString &text)
{
    Q_UNUSED(text);
}

void DriverFiscalHasar2G::printEmbarkItem(const QString &description, const qreal quantity)
{
    Q_UNUSED(description);
    Q_UNUSED(quantity);
}

