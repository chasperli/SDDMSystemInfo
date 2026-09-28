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

    const QString absolutePath = fi.absoluteFilePath();
    qDebug() << "Loading config from:" << absolutePath;

    m_settings = new QSettings(absolutePath, QSettings::IniFormat, this);
    m_settings->sync(); // force read from disk

    if (m_settings->status() != QSettings::NoError) {
        qWarning() << "Failed to parse config file:" << absolutePath;
        m_loaded = false;
        return false;
    }

    // DEBUG: Show what QSettings actually sees
    qDebug() << "QSettings child groups:" << m_settings->childGroups();
    qDebug() << "QSettings all keys:" << m_settings->allKeys();

    m_loaded = true;

    // Debug output: show what we have actually read
    qDebug() << "Config loaded: UpdateIntervalSeconds =" << updateIntervalSeconds()
             << "| TailscaleSocket =" << tailscaleSocket()
             << "| DirectoryType =" << directoryType()
             << "| DirectoryEndpoint =" << directoryEndpoint();

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

    return m_settings->value(QStringLiteral("UpdateIntervalSeconds"), 5).toInt();
}

bool ConfigReader::exposeErrors() const
{
    if (!m_loaded)
        return false;

    return m_settings->value(QStringLiteral("ExposeErrors"), false).toBool();
}
