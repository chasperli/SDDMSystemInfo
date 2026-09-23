/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LDAPCHECKER_H
#define LDAPCHECKER_H

#include "directorychecker.h"
#include <QTcpSocket>

class LdapChecker : public DirectoryChecker
{
public:
    Result check(const QString &host, quint16 port, int timeoutMs) override;

private:
    bool sendLdapBind(QTcpSocket &socket);
    bool readLdapBindResponse(QTcpSocket &socket, int timeoutMs);
};

#endif // LDAPCHECKER_H
