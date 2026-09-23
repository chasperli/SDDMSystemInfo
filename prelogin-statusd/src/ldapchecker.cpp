/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ldapchecker.h"
#include <QDataStream>
#include <QDebug>

DirectoryChecker::Result LdapChecker::check(const QString &host, quint16 port, int timeoutMs)
{
    Result result;

    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(timeoutMs)) {
        result.error = QStringLiteral("connection timeout");
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

bool LdapChecker::sendLdapBind(QTcpSocket &socket)
{
    // BER-encoded LDAP Anonymous Bind Request (RFC 4511)
    QByteArray bindRequest;

    // Authentication: [0] OCTET STRING ""
    bindRequest.append(char(0x80)); // context-specific [0]
    bindRequest.append(char(0x00)); // length 0

    // Name: OCTET STRING ""
    QByteArray name;
    name.append(char(0x04)); // OCTET STRING tag
    name.append(char(0x00)); // length 0

    // Version: INTEGER 3
    QByteArray version;
    version.append(char(0x02)); // INTEGER tag
    version.append(char(0x01)); // length 1
    version.append(char(0x03)); // value 3

    // BindRequest: APPLICATION [0]
    QByteArray bindReqContent = version + name + bindRequest;
    QByteArray bindReqSeq;
    bindReqSeq.append(char(0x60)); // APPLICATION [0] = BindRequest
    bindReqSeq.append(char(bindReqContent.size()));
    bindReqSeq.append(bindReqContent);

    // MessageID: INTEGER 1
    QByteArray msgId;
    msgId.append(char(0x02)); // INTEGER tag
    msgId.append(char(0x01)); // length 1
    msgId.append(char(0x01)); // value 1

    // LDAPMessage: SEQUENCE
    QByteArray ldapMsg = msgId + bindReqSeq;
    QByteArray seq;
    seq.append(char(0x30)); // SEQUENCE tag
    seq.append(char(ldapMsg.size()));
    seq.append(ldapMsg);

    return socket.write(seq) == seq.size();
}

bool LdapChecker::readLdapBindResponse(QTcpSocket &socket, int timeoutMs)
{
    // Read minimum BER header (14 bytes for sequence tag + length + msg id + bind response header)
    if (!socket.waitForReadyRead(timeoutMs))
        return false;

    QByteArray response = socket.readAll();
    if (response.size() < 14)
        return false;

    // Very simplified parsing: look for BindResponse APPLICATION [1]
    // and check if result code (first integer after BindResponse) is 0 (success)
    int idx = response.indexOf(char(0x61)); // APPLICATION [1] = BindResponse
    if (idx < 0 || idx + 3 >= response.size())
        return false;

    // After BindResponse tag and length, expect: SEQUENCE { INTEGER resultCode, ... }
    int seqIdx = idx + 2; // skip tag + length
    if (static_cast<quint8>(response.at(seqIdx)) != 0x30) // not SEQUENCE
        return false;

    int contentIdx = seqIdx + 2; // skip sequence tag + length
    if (static_cast<quint8>(response.at(contentIdx)) != 0x02) // not INTEGER
        return false;

    int intLen = static_cast<quint8>(response.at(contentIdx + 1));
    if (intLen != 1)
        return false;

    quint8 resultCode = static_cast<quint8>(response.at(contentIdx + 2));
    return resultCode == 0; // 0 = success
}
