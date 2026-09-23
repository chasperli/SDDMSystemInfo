/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KERBEROSCHECKER_H
#define KERBEROSCHECKER_H

#include "directorychecker.h"

class KerberosChecker : public DirectoryChecker
{
public:
    Result check(const QString &host, quint16 port, int timeoutMs) override;

private:
    bool checkTcp(const QString &host, quint16 port, int timeoutMs);
    bool checkUdp(const QString &host, quint16 port, int timeoutMs);
};

#endif // KERBEROSCHECKER_H
