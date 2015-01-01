/*
 *
 *
 * Author: Samuel Isuani <sisuani@gmail.com>
 *
 *
 */

#include "logger.h"

Logger *Logger::m_instance = 0;
Logger::Logger()
{
    helper = 0;
    m_instance = this;
}

Logger::~Logger()
{
}

void Logger::init(const QString &filePath)
{
    helper = new Helper(filePath);
}

Logger *Logger::instance()
{
    if(!m_instance)
        new Logger();

    return m_instance;
}

Logger operator<<(Logger logger, const QVariant &message)
{
    if(logger.helper)
        logger.helper->write(message);

    return logger;
}
