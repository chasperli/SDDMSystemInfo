/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tailscaleprovider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>

TailscaleProvider::TailscaleProvider(QObject *parent)
    : QObject(parent)
{
}

void TailscaleProvider::setSocketPath(const QString &path)
{
    if (m_socketPath == path)
        return;
    m_socketPath = path;
}

void TailscaleProvider::refresh()
{
    if (m_socketPath.isEmpty()) {
        m_lastError = QStringLiteral("no socket path configured");
        m_state = QStringLiteral("unavailable");
        emit statusChanged();
        return;
    }

    QFileInfo fi(m_socketPath);
    if (!fi.exists()) {
        m_lastError = QStringLiteral("socket not found");
        m_state = QStringLiteral("unavailable");
        m_peerCount = 0;
        m_exitNodeActive = false;
        emit statusChanged();
        return;
    }

    QLocalSocket socket;
    socket.connectToServer(m_socketPath);

    if (!socket.waitForConnected(2000)) {
        m_lastError = QStringLiteral("connection failed");
        m_state = QStringLiteral("unavailable");
        m_peerCount = 0;
        m_exitNodeActive = false;
        emit statusChanged();
        return;
    }

    const QByteArray request = QByteArrayLiteral(
        "GET /localapi/v0/status HTTP/1.0\r\n"
        "Host: local\r\n"
        "\r\n"
    );

    socket.write(request);
    if (!socket.waitForBytesWritten(1000)) {
        m_lastError = QStringLiteral("write timeout");
        m_state = QStringLiteral("unknown");
        emit statusChanged();
        return;
    }

    QByteArray body;
    if (!readHttpBody(socket, body)) {
        m_lastError = QStringLiteral("read timeout or invalid response");
        m_state = QStringLiteral("unknown");
        emit statusChanged();
        return;
    }

    parseJson(body);
}

bool TailscaleProvider::readHttpBody(QLocalSocket &socket, QByteArray &outBody)
{
    QByteArray header;
    // Read until end of HTTP headers
    while (!header.contains(QByteArrayLiteral("\r\n\r\n"))) {
        if (!socket.waitForReadyRead(3000))
            return false;
        header.append(socket.readAll());
    }

    const int sep = header.indexOf(QByteArrayLiteral("\r\n\r\n"));
    if (sep < 0)
        return false;

    outBody = header.mid(sep + 4);

    // Check for Content-Length and read remaining bytes if needed
    const QByteArray headerPart = header.left(sep);
    int clIdx = headerPart.toLower().indexOf(QByteArrayLiteral("content-length:"));
    if (clIdx >= 0) {
        int lineEnd = headerPart.indexOf('\r', clIdx);
        QByteArray clValue = headerPart.mid(clIdx + 15, lineEnd - (clIdx + 15)).trimmed();
        bool ok = false;
        int contentLength = clValue.toInt(&ok);
        if (ok && contentLength > outBody.size()) {
            while (outBody.size() < contentLength) {
                if (!socket.waitForReadyRead(3000))
                    break;
                outBody.append(socket.readAll());
            }
        }
    } else {
        // No Content-Length: read until disconnected (HTTP/1.0 behavior)
        while (socket.state() == QLocalSocket::ConnectedState) {
            if (!socket.waitForReadyRead(1000))
                break;
            outBody.append(socket.readAll());
        }
        outBody.append(socket.readAll());
    }

    return true;
}

void TailscaleProvider::parseJson(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        m_lastError = QStringLiteral("json parse error");
        m_state = QStringLiteral("unknown");
        emit statusChanged();
        return;
    }

    QJsonObject root = doc.object();

    // BackendState: Running, Stopped, Starting, NoState
    QString backendState = root.value(QStringLiteral("BackendState")).toString();
    if (backendState == QLatin1String("Running")) {
        m_state = QStringLiteral("connected");
    } else if (backendState == QLatin1String("Stopped")) {
        m_state = QStringLiteral("disconnected");
    } else {
        m_state = QStringLiteral("unknown");
    }

    // Peers
    QJsonObject peerObj = root.value(QStringLiteral("Peer")).toObject();
    m_peerCount = static_cast<quint32>(peerObj.size());

    // Exit node active (Self node)
    QJsonObject self = root.value(QStringLiteral("Self")).toObject();
    m_exitNodeActive = self.value(QStringLiteral("ExitNode")).toBool(false);
    if (!m_exitNodeActive) {
        for (const QString &key : peerObj.keys()) {
            QJsonObject peer = peerObj.value(key).toObject();
            if (peer.value(QStringLiteral("ExitNode")).toBool(false)) {
                m_exitNodeActive = true;
                break;
            }
        }
    }

    m_lastError.clear();
    emit statusChanged();
}
