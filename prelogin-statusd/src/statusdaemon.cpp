/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "statusdaemon.h"
#include "configreader.h"
#include "tailscaleprovider.h"
#include "directoryserviceprovider.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QTimer>
#include <QDebug>

StatusDaemon::StatusDaemon(ConfigReader *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_tailscale(new TailscaleProvider(this))
    , m_directory(new DirectoryServiceProvider(this))
    , m_normalTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
{
    if (m_config && m_config->loaded()) {
        m_tailscale->setSocketPath(m_config->tailscaleSocket());
        m_directory->setTypeString(m_config->directoryType());
        m_directory->setEndpoint(m_config->directoryEndpoint());
        m_directory->setTimeoutMs(m_config->directoryTimeoutMs());
    }

    connect(m_tailscale, &TailscaleProvider::statusChanged,
            this, &StatusDaemon::onTailscaleStatusChanged);
    connect(m_directory, &DirectoryServiceProvider::statusChanged,
            this, &StatusDaemon::onDirectoryStatusChanged);

    connect(m_normalTimer, &QTimer::timeout, this, &StatusDaemon::doPeriodicRefresh);
    connect(m_retryTimer, &QTimer::timeout, this, &StatusDaemon::doRetryRefresh);

    setupTimers();

    // Initial refresh on startup
    doPeriodicRefresh();
}

void StatusDaemon::setupTimers()
{
    int interval = 5; // default, seconds
    if (m_config && m_config->loaded()) {
        interval = m_config->updateIntervalSeconds();
    }

    if (interval <= 0) {
        m_normalTimer->stop();
        qInfo() << "Normal periodic updates disabled (UpdateIntervalSeconds = 0).";
    } else {
        m_normalTimer->setInterval(interval * 1000);
        m_normalTimer->start();
        qInfo() << "Normal updates every" << interval << "seconds.";
    }

    m_retryTimer->setInterval(RetryIntervalMs);
    // retryTimer stays stopped until needed
}

bool StatusDaemon::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qWarning() << "System bus is not available.";
        return false;
    }

    if (!bus.registerService(QStringLiteral("org.prelogin.Status1"))) {
        qWarning() << "Failed to register service:" << bus.lastError().message();
        return false;
    }

    if (!bus.registerObject(QStringLiteral("/org/prelogin/Status1"), this,
                            QDBusConnection::ExportAdaptors |
                            QDBusConnection::ExportAllProperties |
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        qWarning() << "Failed to register object:" << bus.lastError().message();
        return false;
    }

    qInfo() << "prelogin-statusd registered on D-Bus as org.prelogin.Status1";
    return true;
}

QString StatusDaemon::tailscaleState() const
{
    return m_tailscale->state();
}

quint32 StatusDaemon::tailscalePeerCount() const
{
    return m_tailscale->peerCount();
}

bool StatusDaemon::tailscaleExitNodeActive() const
{
    return m_tailscale->exitNodeActive();
}

QString StatusDaemon::directoryServiceState() const
{
    return m_directory->state();
}

QString StatusDaemon::directoryServiceType() const
{
    return m_directory->typeString();
}

void StatusDaemon::Refresh()
{
    m_tailscale->refresh();
    m_directory->refresh();
}

void StatusDaemon::doPeriodicRefresh()
{
    m_tailscale->refresh();
    m_directory->refresh();
    evaluateAndSwitchMode();
}

void StatusDaemon::doRetryRefresh()
{
    m_tailscale->refresh();
    m_directory->refresh();
    evaluateAndSwitchMode();
}

void StatusDaemon::evaluateAndSwitchMode()
{
    if (!anyServiceUnavailable()) {
        // All good — stop retry, normal timer is enough
        if (m_retryTimer->isActive()) {
            m_retryTimer->stop();
            qInfo() << "All services reachable. Stopping retry mode.";
        }
        return;
    }

    // Something is down
    if (m_retryTimer->isActive()) {
        // Already retrying — check if we exceeded the 5 minute window
        if (QDateTime::currentDateTime() > m_retryUntil) {
            m_retryTimer->stop();
            qInfo() << "Retry duration (5 min) exceeded. Returning to normal interval.";
        }
        return;
    }

    // Start retry mode
    m_retryUntil = QDateTime::currentDateTime().addMSecs(RetryDurationMs);
    m_retryTimer->start();
    qInfo() << "Service unavailable — entering retry mode (every 5s for max 5min).";
}

bool StatusDaemon::anyServiceUnavailable() const
{
    // Tailscale: anything other than "connected" is considered unavailable for retry
    if (m_tailscale->state() != QLatin1String("connected"))
        return true;

    // Directory: only check if configured (type != None)
    if (m_directory->type() != DirectoryServiceProvider::Type::None) {
        if (m_directory->state() != QLatin1String("reachable"))
            return true;
    }

    return false;
}

QStringList StatusDaemon::GetCapabilities()
{
    QStringList caps;
    caps << QStringLiteral("Tailscale");
    if (m_directory->type() != DirectoryServiceProvider::Type::None)
        caps << QStringLiteral("DirectoryService");
    return caps;
}

void StatusDaemon::onTailscaleStatusChanged()
{
    emit tailscaleStatusChanged();
}

void StatusDaemon::onDirectoryStatusChanged()
{
    emit directoryStatusChanged();
}
