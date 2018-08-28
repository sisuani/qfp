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
        qDebug() << "networkport - " << port_type << port;
    } else {
        if (port_type.compare("COM") == 0) {
            m_serialPort = new SerialPort(port_type, port.toInt());
            qDebug() << "serialport - " << port_type << port << m_serialPort->isOpen();
        } else { // USB

            bool ok;
            const quint16 vid = port.left(port.indexOf(":")).toInt(&ok, 16);
            const quint16 pid = port.right(port.indexOf(":")).toInt(&ok, 16);

            m_usbPort = new UsbPort(pid, vid);
            qDebug() << "usbport - " << port_type << port << m_usbPort->isOpen();
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
    m_networkPort->lastReply();
}

const int Connector::lastError() const
{
    m_networkPort->lastError();
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
    if (m_serialPort)
        return m_serialPort->write(data);
    return m_usbPort->write(data);
}

QByteArray Connector::read(const qreal size)
{
    if (m_serialPort)
        return m_serialPort->read(size);
    return m_usbPort->read(size);
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
