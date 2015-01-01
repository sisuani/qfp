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

#include "packagehasar.h"
#include "packagefiscal.h"

#include <QDebug>

PackageHasar::PackageHasar(QObject *parent)
    : QObject(parent)
{
    m_id = 0;
    m_ftype = 0;
    m_last_secuence = m_secuence;
    nextSecuence();
}

void PackageHasar::setCmd(int cmd)
{
    m_cmd = cmd;
}

int PackageHasar::cmd()
{
    return m_cmd;
}

void PackageHasar::setId(int id)
{
    m_id = id;
}

int PackageHasar::id()
{
    return m_id;
}

void PackageHasar::setFtype(int ftype)
{
    m_ftype = ftype;
}

int PackageHasar::ftype()
{
    return m_ftype;
}

void PackageHasar::setData(const QString &data)
{
    m_data = data;
}

QString &PackageHasar::data()
{
    return m_data;
}

QByteArray &PackageHasar::fiscalPackage()
{
    m_bytes.clear();
    m_bytes.append(PackageFiscal::STX);
    m_bytes.append(m_last_secuence);
    m_bytes.append(cmd());
    if(m_data.size()) {
        m_bytes.append(PackageFiscal::FS);
        m_bytes += m_data;
    }
    m_bytes.append(PackageFiscal::ETX);
    m_bytes.append(QString("%1").arg(checksum(), 4, 16, QChar('0')));
    return m_bytes;
}

int PackageHasar::checksum()
{
    int v_checksum = 0;
    for(int i = 0; i < m_bytes.size(); i++) {
        v_checksum += QChar(m_bytes.at(i)).unicode();
    }
    return v_checksum;
}
