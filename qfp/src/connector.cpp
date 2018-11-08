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
&*/


#include "connector.h"
#include "fiscalprinter.h"

Connector::Connector(QObject *parent, int model, const QString &port_type, const QString &port)
    : QObject(parent)
    , m_type(port_type)
    , m_networkPort(0)
    , m_serialPort(0)
    , m_usbPort(0)
{
    if (model == FiscalPrinter::Hasar1000F) {
        m_networkPort = new NetworkPort(this, port_type, port.toInt());
        connect(m_networkPort, SIGNAL(finished()), this, SLOT(con_finished()));
#ifdef DEBUG
        log() << QString("networkport - %1 %2").arg(port_type).arg(port);
#endif
    } else {
        if (port_type.compare("COM") == 0) {
            m_serialPort = new SerialPort(port_type, port.toInt());
#ifdef DEBUG
            log() << QString("serialport - %1 %2 %3").arg(port_type).arg(port).arg(m_serialPort->isOpen());
#endif
        } else { // USB

            bool ok;
            const quint16 vid = 0x04b8;
            quint16 pid = 0x0;
            if (model == FiscalPrinter::EpsonTM900)
                pid = 0x0201;
            else if (model == FiscalPrinter::EpsonTMU220)
                pid = 0x0202;

            m_usbPort = new UsbPort(vid, pid);
#ifdef DEBUG
            log() << QString("usbport - %1 %2 %3").arg(port_type).arg(port).arg(m_usbPort->isOpen());
#endif
        }
    }
}

bool Connector::isOpen()
{
    if (m_networkPort)
        return true;
    else if (m_serialPort)
        return m_serialPort->isOpen();
    return m_usbPort->isOpen();
}

void Connector::close()
{
    if (m_serialPort)
        m_serialPort->close();
    else if (m_usbPort)
        m_usbPort->close();
}

const QString &Connector::type()
{
    return m_type;
}

QVariantMap Connector::lastReply() const
{
    return m_networkPort->lastReply();
}

const int Connector::lastError() const
{
    return m_networkPort->lastError();
}

void Connector::con_finished()
{
    emit finished();
}

void Connector::post(const QVariantMap &body)
{
    m_networkPort->post(body);
}

const qreal Connector::write(const QByteArray &data)
{
#ifdef DEBUG
    log() << QString("Connector::write() %1").arg(data.toString());
#endif
    if (m_serialPort)
        return m_serialPort->write(data);
    return m_usbPort->write(data);
}

QByteArray Connector::read(const qreal size)
{
    const QByteArray r = m_serialPort ? m_serialPort->read(size) : m_usbPort->read(size);

#ifdef DEBUG
    log() << QString("Connector::read() %1").arg(r.toString());
#endif

    return r;

}

QByteArray Connector::readAll()
{
    if (m_serialPort)
        return m_serialPort->readAll();
    return m_usbPort->readAll();
}

const qreal Connector::bytesAvailable()
{
    if (m_serialPort)
        return m_serialPort->bytesAvailable();
    return m_usbPort->bytesAvailable();
}
