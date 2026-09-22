#include "sddmsysteminfo_plugin.h"
#include "jsonstatusreader.h"
#include <qqml.h>

void SddmSystemInfoPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("SddmSystemInfo"));
    qmlRegisterType<JsonStatusReader>(uri, 1, 0, "JsonStatusReader");
}
