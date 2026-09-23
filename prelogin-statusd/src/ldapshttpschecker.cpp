/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ldapshttpschecker.h"
#include <QSslConfiguration>

DirectoryChecker::Result LdapsChecker::check(const QString &host, quint16 port, int timeoutMs)
{
    Result result;

    QSslSocket socket;
    socket.setPeerVerifyMode(QSslSocket::QueryPeer);
    socket.connectToHostEncrypted(host, port);
    if (!socket.waitForEncrypted(timeoutMs)) {
        result.error = QStringLiteral("tls handshake failed: ") + socket.errorString();
        return result;
    }

    if (!sendLdapBind(socket)) {
        result.error = QStringLiteral("failed to send bind request");
        return result;
    }

    if (!readLdapBindResponse(socket, timeoutMs)) {
        result.error = QStringLiteral("bind failed or timeout");
        return result;
    }

    result.reachable = true;
    return result;
}

bool LdapsChecker::sendLdapBind(QSslSocket &socket)
{
    // Identical BER LDAP Bind Request as plain LDAP
    QByteArray bindRequest;
    bindRequest.append(char(0x80));
    bindRequest.append(char(0x00));

    QByteArray name;
    name.append(char(0x04));
    name.append(char(0x00));

    QByteArray version;
    version.append(char(0x02));
    version.append(char(0x01));
    version.append(char(0x03));

    QByteArray bindReqContent = version + name + bindRequest;
    QByteArray bindReqSeq;
    bindReqSeq.append(char(0x60));
    bindReqSeq.append(char(bindReqContent.size()));
    bindReqSeq.append(bindReqContent);

    QByteArray msgId;
    msgId.append(char(0x02));
    msgId.append(char(0x01));
    msgId.append(char(0x01));

    QByteArray ldapMsg = msgId + bindReqSeq;
    QByteArray seq;
    seq.append(char(0x30));
    seq.append(char(ldapMsg.size()));
    seq.append(ldapMsg);

    return socket.write(seq) == seq.size();
}

bool LdapsChecker::readLdapBindResponse(QSslSocket &socket, int timeoutMs)
{
    if (!socket.waitForReadyRead(timeoutMs))
        return false;

    QByteArray response = socket.readAll();
    if (response.size() < 14)
        return false;

    int idx = response.indexOf(char(0x61));
    if (idx < 0 || idx + 3 >= response.size())
        return false;

    int seqIdx = idx + 2;
    if (static_cast<quint8>(response.at(seqIdx)) != 0x30)
        return false;

    int contentIdx = seqIdx + 2;
    if (static_cast<quint8>(response.at(contentIdx)) != 0x02)
        return false;

    int intLen = static_cast<quint8>(response.at(contentIdx + 1));
    if (intLen != 1)
        return false;

    quint8 resultCode = static_cast<quint8>(response.at(contentIdx + 2));
    return resultCode == 0;
}
