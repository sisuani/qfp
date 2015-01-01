/*
 *
 *
 * Author: Samuel Isuani <sisuani@gmail.com>
 *
 *
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "helper.h"

#define log (*Logger::instance())

class Logger {

public:
    static Logger *instance();
    Logger();
    ~Logger();

    void init(const QString &filePath);
    Helper *helper;

private:
    QString m_filePath;
    static Logger *m_instance;
};

Logger operator<<(Logger logger, const QVariant &message);

#endif // LOGGER_H
