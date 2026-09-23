/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "directoryserviceprovider.h"
#include "directorychecker.h"
#include "ldapchecker.h"
#include "ldapshttpschecker.h"
#include "kerberoschecker.h"
#include <QDebug>

DirectoryServiceProvider::DirectoryServiceProvider(QObject *parent)
    : QObject(parent)
{
}

DirectoryServiceProvider::~DirectoryServiceProvider() = default;

QString DirectoryServiceProvider::typeString() const
{
    switch (m_type) {
    case Type::LDAP:     return QStringLiteral("ldap");
    case Type::LDAPS:    return QStringLiteral("ldaps");
    case Type::Kerberos: return QStringLiteral("kerberos");
    default:             return QStringLiteral("none");
    }
}

void DirectoryServiceProvider::setType(Type type)
{
    if (m_type == type)
        return;
    m_type = type;
    m_checker.reset(); // Force recreation on next refresh
}

void DirectoryServiceProvider::setTypeString(const QString &type)
{
    QString t = type.toLower();
    if (t == QLatin1String("ldap")) {
        setType(Type::LDAP);
    } else if (t == QLatin1String("ldaps")) {
        setType(Type::LDAPS);
    } else if (t == QLatin1String("kerberos")) {
        setType(Type::Kerberos);
    } else {
        setType(Type::None);
    }
}

void DirectoryServiceProvider::setEndpoint(const QString &endpoint)
{
    if (m_endpoint == endpoint)
        return;
    m_endpoint = endpoint;
}

void DirectoryServiceProvider::setTimeoutMs(int ms)
{
    if (ms < 100)
        ms = 100;
    m_timeoutMs = ms;
}

void DirectoryServiceProvider::refresh()
{
    if (m_type == Type::None || m_endpoint.isEmpty()) {
        m_state = QStringLiteral("unknown");
        emit statusChanged();
        return;
    }

    ensureChecker();
    if (!m_checker) {
        m_state = QStringLiteral("error");
        emit statusChanged();
        return;
    }

    QString host = m_endpoint;
    quint16 port = 0;

    // host:port parsing
    int colonIdx = host.lastIndexOf(QLatin1String(":"));
    if (colonIdx > 0) {
        bool ok = false;
        quint16 p = host.mid(colonIdx + 1).toUShort(&ok);
        if (ok) {
            port = p;
            host = host.left(colonIdx);
        }
    }

    // Default ports
    if (port == 0) {
        switch (m_type) {
        case Type::LDAP:     port = 389;  break;
        case Type::LDAPS:    port = 636;  break;
        case Type::Kerberos: port = 88;   break;
        default:             port = 0;    break;
        }
    }

    if (port == 0) {
        m_state = QStringLiteral("error");
        emit statusChanged();
        return;
    }

    auto result = m_checker->check(host, port, m_timeoutMs);
    m_state = result.reachable ? QStringLiteral("reachable") : QStringLiteral("unavailable");
    if (!result.reachable && !result.error.isEmpty()) {
        qDebug() << "Directory check failed:" << result.error;
    }

    emit statusChanged();
}

void DirectoryServiceProvider::ensureChecker()
{
    if (m_checker)
        return;

    switch (m_type) {
    case Type::LDAP:
        m_checker = std::make_unique<LdapChecker>();
        break;
    case Type::LDAPS:
        m_checker = std::make_unique<LdapsChecker>();
        break;
    case Type::Kerberos:
        m_checker = std::make_unique<KerberosChecker>();
        break;
    default:
        m_checker.reset();
        break;
    }
}
