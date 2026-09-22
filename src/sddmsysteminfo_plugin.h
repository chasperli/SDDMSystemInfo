#ifndef SDDMSYSTEMINFO_PLUGIN_H
#define SDDMSYSTEMINFO_PLUGIN_H

#include <QQmlExtensionPlugin>

class SddmSystemInfoPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri) override;
};

#endif
