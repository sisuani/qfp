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

#include "serialport.h"

#include <QDebug>


SerialPort::SerialPort(const QString &type, unsigned int port)
{
    QString v_port = type;
#if defined (Q_OS_UNIX)
    if(type.compare("USB") == 0) {
        v_port = QString(QLatin1String("/dev/ttyUSB%1")).arg(port - 1);
    } else {
        v_port = QString(QLatin1String("ttyS%1")).arg(port - 1);
    }
#elif defined (Q_OS_WIN32)
    if(type.compare("USB") == 0) {
        // v_port = QLatin1String("").arg(port);
        // FIX unsop
    } else {
        v_port = QString(QLatin1String("COM%1")).arg(port);
    }
#endif

    m_serialPort = new QextSerialPort(v_port, QextSerialPort::Polling);
    m_serialPort->setBaudRate(BAUD9600);
    m_serialPort->setFlowControl(FLOW_OFF);
    m_serialPort->setParity(PAR_NONE);
    m_serialPort->setDataBits(DATA_8);
    m_serialPort->setStopBits(STOP_1);
    m_serialPort->open(QIODevice::ReadWrite | QIODevice::Unbuffered);
}

bool SerialPort::isOpen()
{
    return m_serialPort->isOpen();
}

void SerialPort::close()
{
    m_serialPort->close();
    delete m_serialPort;
}

const qreal SerialPort::write(const QByteArray &data)
{
#ifdef DEBUG
    log() << QString("SerialPort::write() %1").arg(data.toHex());
#endif
    const qreal size = m_serialPort->write(data.data(), data.size());
    m_serialPort->flush();
    return size;
}

QByteArray SerialPort::read(const qreal size)
{
    return m_serialPort->read(size);
}

QByteArray SerialPort::readAll()
{
    return m_serialPort->readAll();
}

const qreal SerialPort::bytesAvailable()
{
    return m_serialPort->bytesAvailable();
}
