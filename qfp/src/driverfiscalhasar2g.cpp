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
#include "networkport.h"

#include <QDateTime>
#include <QEventLoop>

#define CLOSEDOCCMD "CerrarDocumento"

DriverFiscalHasar2G::DriverFiscalHasar2G(QObject *parent, Connector *m_connector, int m_TIME_WAIT)
    : QThread(parent), DriverFiscal(parent, m_connector, m_TIME_WAIT)
{
    m_error = false;
    cancel_count = 0;
    m_continue = true;
    connect(this, SIGNAL(sendData(const QVariantMap &)),
            m_connector, SLOT(post(const QVariantMap &)));
}

void DriverFiscalHasar2G::setModel(const FiscalPrinter::Model model)
{
    m_model = model;
}

void DriverFiscalHasar2G::run()
{
    while(!queue.empty() && m_continue) {
        QVariantMap pkg = queue.first();
        QVariantMap result;
        sendData(pkg);


        QEventLoop loop;
        connect(m_connector, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        if (m_connector->lastError() != NetworkPort::NP_NO_ERROR) {
            emit fiscalStatus(FiscalPrinter::Error);
#ifdef DEBUG
            log() << QString("DriverFiscalHasar2G::run() -> Error: %1").arg(m_connector->lastError());
#endif
            continue;
        }

        const QVariantMap reply = m_connector->lastReply();

        if (!verifyPackage(pkg, reply)) {
            queue.clear();
            cancel_count++;
            if (cancel_count < 3)
                cancel();
            continue;
        }

        emit fiscalStatus(FiscalPrinter::Ok);

        if (reply.keys()[0].compare(CLOSEDOCCMD) == 0) {
            emit fiscalReceiptNumber(pkg[CLOSEDOCCMD].toMap()["id"].toInt(),
                    getReceiptNumber(reply[CLOSEDOCCMD].toMap()["NumeroComprobante"].toByteArray()),
                    pkg[CLOSEDOCCMD].toMap()["ftype"].toInt());
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

int DriverFiscalHasar2G::getReceiptNumber(const QByteArray &data)
{
#ifdef DEBUG
    log() << QString("F. Num: %1").arg(data.trimmed().toInt());
#endif
    return data.trimmed().toInt();
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

    return true;
}

bool DriverFiscalHasar2G::verifyPackage(const QVariantMap &pkg, const QVariantMap &reply)
{
    if (!pkg.keys().size() || !reply.keys().size()) {
        emit fiscalStatus(FiscalPrinter::Error);
        return false;
    }

    const QString cmd = pkg.keys()[0];

    if (cmd != reply.keys()[0]) {
        emit fiscalStatus(FiscalPrinter::Error);
        return false;
    }

    if (!getStatus(reply[cmd].toMap()["Estado"].toMap())) {
        emit fiscalStatus(FiscalPrinter::Error);
        return false;
    }

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
            customer["ResponsabilidadIVA"] = "Monotributo";
            break;
        case 'A':
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
        case 'T':
            openReceipt["CodigoComprobante"] = "Tique";
            break;
        case 'R':
            openReceipt["CodigoComprobante"] = "TiqueNotaCreditoA";
            break;
        case 'S':
            openReceipt["CodigoComprobante"] = "TiqueNotaCreditoB";
            break;
        case 'r':
            openReceipt["CodigoComprobante"] = "RemitoR";
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
    discount["Operacion"] = type == 'M' ? "AjustePos" : "AjusteNeg";
    d["ImprimirAjuste"] = discount;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::closeFiscalReceipt(const char intype, const char f_type, const int id)
{
    Q_UNUSED(intype);

    QVariantMap d;
    QVariantMap doc;


    switch (f_type) {
        case 'R':
            doc["ftype"] = 1;
            break;
        case 'S':
            doc["ftype"] = 0;
            break;
        case 'r':
            doc["ftype"] = 3;
            break;
        default:
            doc["ftype"] = 0;
            break;
    }

    doc["id"] = QString::number(id);
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

void DriverFiscalHasar2G::setHeaderTrailer(const QString &header, const QString &trailer)
{
    QVariantMap d;
    QVariantMap zona;

    if (!header.isEmpty()) {
        zona["NumeroLinea"] = "1";
        zona["Descripcion"] = header;
    } else {
        zona["NumeroLinea"] = "0";
    }

    zona["Estacion"] = "EstacionPorDefecto";
    zona["IdentificadorZona"] = "Zona1Encabezado";
    d["ConfigurarZona"] = zona;
    queue.append(d);
    start();

    if (!trailer.isEmpty()) {
        zona["NumeroLinea"] = "1";
        zona["Descripcion"] = trailer;
    } else {
        zona["NumeroLinea"] = "0";
    }

    zona["Estacion"] = "EstacionPorDefecto";
    zona["IdentificadorZona"] = "Zona1Cola";
    d["ConfigurarZona"] = zona;

    queue.append(d);
    start();

}

void DriverFiscalHasar2G::setEmbarkNumber(const int doc_num, const QString &description, const char type)
{
    QVariantMap d;
    QVariantMap doc;

    doc["NumeroLinea"] = "1";

    switch (type) {
        case 'B':
            doc["CodigoComprobante"] = "TiqueFacturaB";
            break;
        case 'A':
            doc["CodigoComprobante"] = "TiqueFacturaA";
            break;
        case 'M':
            doc["CodigoComprobante"] = "TiqueNotaCreditoA";
            break;
        case 'T':
            doc["CodigoComprobante"] = "Tique";
            break;
        case 'R':
            doc["CodigoComprobante"] = "TiqueNotaCreditoA";
            break;
        case 'S':
            doc["CodigoComprobante"] = "TiqueNotaCreditoB";
            break;
        case 'r':
            doc["CodigoComprobante"] = "RemitoR";
            break;
        default:
            doc["CodigoComprobante"] = "Generico";
            break;
    }

    doc["NumeroPos"] = description.left(4);
    doc["NumeroComprobante"] = description.right(8);
    d["CargarDocumentoAsociado"] = doc;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::openDNFH(const char type, const char fix_value, const QString &doc_num)
{
    Q_UNUSED(fix_value);
    Q_UNUSED(doc_num);
    openFiscalReceipt(type);

}

void DriverFiscalHasar2G::closeDNFH(const int id, const char f_type, const int copies)
{
    Q_UNUSED(copies);
    closeFiscalReceipt(0, f_type, id);
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
    QVariantMap d;
    QVariantMap doc;

    if (doc_type.compare("081") == 0)
        doc["CodigoComprobante"] = "TiqueFacturaA";
    else if (doc_type.compare("082") == 0)
        doc["CodigoComprobante"] = "TiqueFacturaB";
    else if (doc_type.compare("112") == 0)
        doc["CodigoComprobante"] = "TiqueNotaCreditoA";
    else if (doc_type.compare("113") == 0)
        doc["CodigoComprobante"] = "TiqueNotaCreditoB";
    else if (doc_type.compare("110") == 0)
        doc["CodigoComprobante"] = "TiqueNotaCreditoB";
    else
        doc["CodigoComprobante"] = "Tique";

    doc["NumeroComprobante"] = QString::number(doc_number);
    d["CopiarComprobante"] = doc;

    queue.append(d);
    start();
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

void DriverFiscalHasar2G::downloadReportByDate(const QString &type, const QDate &from, const QDate &to)
{

    QVariantMap d;
    QVariantMap report;

    report["FechaInicial"] = from.toString("yyMMdd");
    report["FechaFinal"] = to.toString("yyMMdd");
    report["TipoReporte"] = "ReporteAFIPCompleto";
    d["ObtenerPrimerBloqueReporteElectronico"] = report;

    queue.append(d);
    start();
}

void DriverFiscalHasar2G::downloadReportByNumber(const QString &type, const int from, const int to)
{

}

void DriverFiscalHasar2G::downloadContinue()
{
    QVariantMap d;

    d["ObtenerSiguienteBloqueReporteElectronico"] = "";

    queue.append(d);
    start();
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

