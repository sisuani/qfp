/*
 *
 *
 * Author: Samuel Isuani <sisuani@gmail.com>
 *
 *
 */

#include "helper.h"

#include <QVariant>
#include <QDateTime>
// #include <QDebug>

static const QString strDateTime("dd-MM-yyyy hh:mm:ss");

Helper::Helper(const QString &filePath) {

    m_file.setFileName(filePath);
    m_file.open(QFile::WriteOnly | QFile::Text | QFile::Append);
    m_OutputStream.setDevice(&m_file);
}

Helper::~Helper()
{
}

void Helper::write(const QVariant &message)
{
    if(!m_file.isOpen())
        return;

    QMutexLocker lock(&m_logMutex);
    m_OutputStream << QDateTime::currentDateTime().toString(strDateTime)
        << ": "
        << message.toString()
        << endl;
    m_OutputStream.flush();
    // qDebug() << QDateTime::currentDateTime().toString(strDateTime)
            // << ": "
            // << message.toString();
}
