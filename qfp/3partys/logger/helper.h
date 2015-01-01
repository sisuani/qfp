/*
 *
 *
 * Author: Samuel Isuani <sisuani@gmail.com>
 *
 *
 */

#ifndef HELPER_H
#define HELPER_H

#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>

class Helper {

Q_DISABLE_COPY(Helper)

public:
    Helper(const QString &filePath);
    virtual ~Helper();
    void write(const QVariant &message);

private:
    QString m_filePath;
    QMutex m_logMutex;
    QTextStream m_OutputStream;
    QFile m_file;
};

#endif // HELPER_H
