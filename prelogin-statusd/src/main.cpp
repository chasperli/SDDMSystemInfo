/*
 *  prelogin-statusd — System daemon for pre-login status information
 *  Copyright (C) 2026  Dein Name
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "statusdaemon.h"
#include "configreader.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Prelogin System Status Service"));
    parser.addHelpOption();

    QCommandLineOption configOption(QStringList() << QStringLiteral("c") << QStringLiteral("config"),
                                    QStringLiteral("Path to configuration file (INI)"),
                                    QStringLiteral("path"),
                                    QStringLiteral("/etc/prelogin-statusd/config.ini"));
    parser.addOption(configOption);
    parser.process(app);

    ConfigReader config;
    QString configPath = parser.value(configOption);
    if (!config.load(configPath)) {
        qWarning() << "Could not load config from" << configPath << "- using defaults.";
    }

    StatusDaemon daemon(&config);
    if (!daemon.registerOnBus()) {
        qCritical() << "Failed to register on D-Bus system bus. Exiting.";
        return 1;
    }

    return app.exec();
}
