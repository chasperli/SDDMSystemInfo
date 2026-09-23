/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kerberoschecker.h"
#include <QTcpSocket>
#include <QUdpSocket>

DirectoryChecker::Result KerberosChecker::check(const QString &host, quint16 port, int timeoutMs)
{
    Result result;

    // Kerberos KDC listens on both TCP and UDP (AS-REQ/AS-REP)
    // A service is considered reachable if either responds.
    // UDP is preferred for small requests, TCP for large tickets.

    if (checkUdp(host, port, timeoutMs) || checkTcp(host, port, timeoutMs)) {
        result.reachable = true;
    } else {
        result.error = QStringLiteral("no response on tcp or udp port %1").arg(port);
    }

    return result;
}

bool KerberosChecker::checkTcp(const QString &host, quint16 port, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);
    return socket.waitForConnected(timeoutMs);
}

bool KerberosChecker::checkUdp(const QString &host, quint16 port, int timeoutMs)
{
    QUdpSocket socket;
    // Send an empty datagram — many KDCs will at least accept it,
    // and we can detect ICMP port-unreachable vs. actual listeners.
    if (socket.writeDatagram(QByteArray(), QHostAddress(host), port) < 0)
        return false;

    if (!socket.waitForReadyRead(timeoutMs))
        return false;

    char buf[1];
    socket.readDatagram(buf, sizeof(buf));
    return true;
}
