#ifndef DIRECTORYCHECKER_H
#define DIRECTORYCHECKER_H

#include <QString>

class DirectoryChecker
{
public:
    virtual ~DirectoryChecker() = default;

    struct Result {
        bool reachable = false;
        QString error;
    };

    virtual Result check(const QString &host, quint16 port, int timeoutMs) = 0;
};

#endif // DIRECTORYCHECKER_H
