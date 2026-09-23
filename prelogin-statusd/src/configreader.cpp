/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configreader.h"
#include <QFileInfo>
#include <QDebug>

ConfigReader::ConfigReader(QObject *parent)
    : QObject(parent)
{
}

bool ConfigReader::load(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        qWarning() << "Config file not found:" << path;
        m_loaded = false;
        return false;
    }

    m_settings = new QSettings(path, QSettings::IniFormat, this);
    m_loaded = true;

    return true;
}

QString ConfigReader::tailscaleSocket() const
{
    if (!m_loaded)
        return QStringLiteral("/var/run/tailscale/tailscaled.sock");

    return m_settings->value(QStringLiteral("Tailscale/Socket"),
                             QStringLiteral("/var/run/tailscale/tailscaled.sock")).toString();
}

QString ConfigReader::directoryType() const
{
    if (!m_loaded)
        return QStringLiteral("none");

    return m_settings->value(QStringLiteral("Directory/Type"),
                             QStringLiteral("none")).toString().toLower();
}

QString ConfigReader::directoryEndpoint() const
{
    if (!m_loaded)
        return QString();

    return m_settings->value(QStringLiteral("Directory/Endpoint"),
                             QString()).toString();
}

int ConfigReader::directoryTimeoutMs() const
{
    if (!m_loaded)
        return 2000;

    return m_settings->value(QStringLiteral("Directory/TimeoutMilliseconds"), 2000).toInt();
}

int ConfigReader::updateIntervalSeconds() const
{
    if (!m_loaded)
        return 5;

    return m_settings->value(QStringLiteral("General/UpdateIntervalSeconds"), 5).toInt();
}

bool ConfigReader::exposeErrors() const
{
    if (!m_loaded)
        return false;

    return m_settings->value(QStringLiteral("General/ExposeErrors"), false).toBool();
}
