/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TAILSCALEPROVIDER_H
#define TAILSCALEPROVIDER_H

#include <QObject>
#include <QLocalSocket>

class TailscaleProvider : public QObject
{
    Q_OBJECT

public:
    explicit TailscaleProvider(QObject *parent = nullptr);

    QString state() const { return m_state; }
    quint32 peerCount() const { return m_peerCount; }
    bool exitNodeActive() const { return m_exitNodeActive; }
    QString lastError() const { return m_lastError; }

    QString socketPath() const { return m_socketPath; }
    void setSocketPath(const QString &path);

    void refresh();

signals:
    void statusChanged();

private:
    void parseJson(const QByteArray &data);
    bool readHttpBody(QLocalSocket &socket, QByteArray &outBody);

    QString m_state = QStringLiteral("unknown");
    quint32 m_peerCount = 0;
    bool m_exitNodeActive = false;
    QString m_lastError;
    QString m_socketPath = QStringLiteral("/var/run/tailscale/tailscaled.sock");
};

#endif // TAILSCALEPROVIDER_H
