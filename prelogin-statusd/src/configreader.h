/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include <QObject>
#include <QSettings>

class ConfigReader : public QObject
{
    Q_OBJECT

public:
    explicit ConfigReader(QObject *parent = nullptr);

    bool load(const QString &path);
    bool loaded() const { return m_loaded; }

    // Tailscale
    QString tailscaleSocket() const;

    // Directory Service
    QString directoryType() const;
    QString directoryEndpoint() const;
    int directoryTimeoutMs() const;

    // General
    int updateIntervalSeconds() const;
    bool exposeErrors() const;

private:
    QSettings *m_settings = nullptr;
    bool m_loaded = false;
};

#endif // CONFIGREADER_H
