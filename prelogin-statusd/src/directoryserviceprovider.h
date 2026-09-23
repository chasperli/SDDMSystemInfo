/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIRECTORYSERVICEPROVIDER_H
#define DIRECTORYSERVICEPROVIDER_H

#include <QObject>
#include <memory>

class DirectoryChecker;

class DirectoryServiceProvider : public QObject
{
    Q_OBJECT

public:
    enum class Type {
        None,
        LDAP,
        LDAPS,
        Kerberos
    };
    Q_ENUM(Type)

    explicit DirectoryServiceProvider(QObject *parent = nullptr);
    ~DirectoryServiceProvider();

    QString state() const { return m_state; }
    Type type() const { return m_type; }
    QString typeString() const;
    QString endpoint() const { return m_endpoint; }
    int timeoutMs() const { return m_timeoutMs; }

    void setType(Type type);
    void setTypeString(const QString &type);
    void setEndpoint(const QString &endpoint);
    void setTimeoutMs(int ms);

    void refresh();

signals:
    void statusChanged();

private:
    void ensureChecker();

    QString m_state = QStringLiteral("unknown");
    Type m_type = Type::None;
    QString m_endpoint;
    int m_timeoutMs = 2000;
    std::unique_ptr<DirectoryChecker> m_checker;
};

#endif // DIRECTORYSERVICEPROVIDER_H
