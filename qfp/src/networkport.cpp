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

#include "networkport.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJson/Parser>
#include <QJson/Serializer>
#include <QTimer>
#include <QDebug>

NetworkPort::NetworkPort(QObject *parent, const QString &host, const int port)
    : QThread(0)
{
    networkManager = new QNetworkAccessManager(this);
    url.setUrl(host);
    url.setPort(port);
}

NetworkPort::~NetworkPort()
{
}

QVariantMap NetworkPort::lastReply() const
{
    return m_lastReply;
}

const int NetworkPort::lastError() const
{
    return m_lastError;
}

void NetworkPort::post(const QVariantMap &body)
{
    QNetworkRequest request(url);
    request.setRawHeader("Content-type", "application/json");

    QJson::Serializer serializer;
    //qDebug() << serializer.serialize(body);
    QNetworkReply *reply = networkManager->post(request, serializer.serialize(body));
    m_lastError = NP_NO_ERROR;

    QTimer timer;
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QTimer::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(10 * 1000);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode == 200) {
        QByteArray json = reply->readAll();
        reply->deleteLater();

        QJson::Parser parser;
        bool parseOk = true;
        m_lastReply = parser.parse(json, &parseOk).toMap();
#ifdef DEBUG
        log() << QString("NetworkPort::post() -> reply : %1").arg(json);
#endif
        if (!parseOk)
            m_lastError = NP_ERROR_PARSE;
    } else {
        m_lastError = NP_ERROR_STATUS;
    }

    emit finished();
}
