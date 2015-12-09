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

#include "packageepsonext.h"
#include "packagefiscal.h"
#include "driverfiscal.h"

PackageEpsonExt::PackageEpsonExt(QObject *parent)
    : QObject(parent)
{
    m_id = 0;
    m_last_secuence = m_secuence;
    nextSecuence();
}

void PackageEpsonExt::setId(int id)
{
    m_id = id;
}

int PackageEpsonExt::id()
{
    return m_id;
}

void PackageEpsonExt::setCmd(const int cmd)
{
    m_cmd = cmd;
}

int PackageEpsonExt::cmd()
{
    return m_cmd;
}

void PackageEpsonExt::setData(const QByteArray &data)
{
    m_data = data;
}

QByteArray &PackageEpsonExt::data()
{
    return m_data;
}

QByteArray &PackageEpsonExt::fiscalPackage() {

    m_bytes.clear();
    m_bytes.append(PackageFiscal::STX);
    m_bytes.append(m_last_secuence);
    m_bytes.append(m_data);
    m_bytes.append(PackageFiscal::ETX);
    m_bytes.append(QString("%1").arg(checksum(), 4, 16, QChar('0')).toUpper());
    return m_bytes;
}

int PackageEpsonExt::checksum() {
    int v_checksum = 0;
    for(int i = 0; i < m_bytes.size(); i++) {
        v_checksum += QString::number ((uchar) m_bytes[i], 10).toInt();
    }
    return v_checksum;
}
