#pragma once

#include "OpenRGBPluginInterface.h"
#include "ResourceManagerInterface.h"

#include <QtPlugin>
#include <vector>

class RGBController;
class CorsairCapellixXTController;

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

    void                Load(ResourceManagerInterface* rm)                  override;
    QWidget*            GetWidget()                                         override;
    QMenu*              GetTrayMenu()                                       override;

    void                Unload()                                            override;

private:
    ResourceManagerInterface*                   resource_manager = nullptr;
    std::vector<RGBController*>                  controllers;
    std::vector<CorsairCapellixXTController*>    pump_controllers;
    bool                                        loaded           = false;
};
