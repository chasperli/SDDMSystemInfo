/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STATUSDAEMON_H
#define STATUSDAEMON_H

#include <QObject>
#include <QDBusContext>
#include <QDateTime>

class QTimer;
class ConfigReader;
class TailscaleProvider;
class DirectoryServiceProvider;

class StatusDaemon : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.prelogin.Status1")

    Q_PROPERTY(QString Version READ version CONSTANT)

    Q_PROPERTY(QString TailscaleState READ tailscaleState NOTIFY tailscaleStatusChanged)
    Q_PROPERTY(quint32 TailscalePeerCount READ tailscalePeerCount NOTIFY tailscaleStatusChanged)
    Q_PROPERTY(bool TailscaleExitNodeActive READ tailscaleExitNodeActive NOTIFY tailscaleStatusChanged)

    Q_PROPERTY(QString DirectoryServiceState READ directoryServiceState NOTIFY directoryStatusChanged)
    Q_PROPERTY(QString DirectoryServiceType READ directoryServiceType NOTIFY directoryStatusChanged)

public:
    explicit StatusDaemon(ConfigReader *config, QObject *parent = nullptr);

    bool registerOnBus();

    QString version() const { return QStringLiteral("1.0.0"); }

    QString tailscaleState() const;
    quint32 tailscalePeerCount() const;
    bool tailscaleExitNodeActive() const;

    QString directoryServiceState() const;
    QString directoryServiceType() const;

public slots:
    Q_SCRIPTABLE void Refresh();
    Q_SCRIPTABLE QStringList GetCapabilities();

signals:
    void tailscaleStatusChanged();
    void directoryStatusChanged();

private slots:
    void onTailscaleStatusChanged();
    void onDirectoryStatusChanged();
    void doPeriodicRefresh();
    void doRetryRefresh();

private:
    void setupTimers();
    void evaluateAndSwitchMode();
    bool anyServiceUnavailable() const;

    ConfigReader *m_config;
    TailscaleProvider *m_tailscale;
    DirectoryServiceProvider *m_directory;
    QTimer *m_normalTimer;
    QTimer *m_retryTimer;
    QDateTime m_retryUntil;
    static constexpr int RetryIntervalMs = 5000;
    static constexpr int RetryDurationMs = 300000; // 5 minutes
};

#endif // STATUSDAEMON_H
