/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LDAPSCHECKER_H
#define LDAPSCHECKER_H

#include "directorychecker.h"
#include <QSslSocket>

class LdapsChecker : public DirectoryChecker
{
public:
    Result check(const QString &host, quint16 port, int timeoutMs) override;

private:
    bool sendLdapBind(QSslSocket &socket);
    bool readLdapBindResponse(QSslSocket &socket, int timeoutMs);
};

#endif // LDAPSCHECKER_H
