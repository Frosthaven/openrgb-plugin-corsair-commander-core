#pragma once

#include "OpenRGBPluginInterface.h"
#include "ResourceManager.h"

#include <QtPlugin>

class CorsairCapellixXTPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    CorsairCapellixXTPlugin() = default;
    ~CorsairCapellixXTPlugin() override = default;

    OpenRGBPluginInfo   GetPluginInfo()                                     override;
    unsigned int        GetPluginAPIVersion()                               override;

    void                Load(bool dark_theme, ResourceManager* rm)          override;
    QWidget*            GetWidget()                                         override;
    QMenu*              GetTrayMenu()                                       override;

    void                Unload()                                            override;

private:
    ResourceManager*    resource_manager = nullptr;
    bool                loaded           = false;
};
