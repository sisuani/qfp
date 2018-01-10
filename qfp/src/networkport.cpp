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
#include <QDebug>

NetworkPort::NetworkPort(const QString &host, unsigned int port)
    : host(host),
      port(port)
{
    networkManager = new QNetworkAccessManager(this);
}

bool NetworkPort::post(const QVariantMap &body, QVariantMap *result)
{
    QUrl url;
    url.setUrl(host);
    url.setPort(port);
    QNetworkRequest request(url);
    request.setRawHeader("Content-type", "application/json");

    QJson::Serializer serializer;
    qDebug() << "0.1";
    QNetworkReply *reply = networkManager->post(request, serializer.serialize(body));
    qDebug() << "1";

    return waitAndparseData(reply, result);
}

bool NetworkPort::waitAndparseData(QNetworkReply *reply, QVariantMap *result)
{
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray json = reply->readAll();
    qDebug() << "JSON: " << json;
    reply->deleteLater();

    QJson::Parser parser;
    bool parseOk = true;
    *result = parser.parse(json, &parseOk).toMap();
    return parseOk;
}
