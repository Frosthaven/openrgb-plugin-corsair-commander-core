#include "CorsairCapellixXTPlugin.h"
#include "CorsairCapellixXTDetect.h"

OpenRGBPluginInfo CorsairCapellixXTPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name           = "Corsair Commander Core";
    info.Description    = "Adds support for Corsair Commander Core RGB controllers "
                          "used in Capellix and Capellix XT series AIO coolers.";
    info.Version        = VERSION_STRING;
    info.Commit         = GIT_COMMIT_ID;
    info.URL            = "https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core";
    info.Icon.load(":/fan.svg");

    info.Label          = "";
    info.Location       = OPENRGB_PLUGIN_LOCATION_TOP;

    return info;
}

unsigned int CorsairCapellixXTPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void CorsairCapellixXTPlugin::Load(ResourceManagerInterface* rm)
{
    if(loaded)
    {
        return;
    }

    resource_manager = rm;

    /*-------------------------------------------------------------*\
    | Detect devices and register controllers directly               |
    \*-------------------------------------------------------------*/
    std::vector<RGBController*> detected = DetectCorsairCapellixXT();

    for(RGBController* ctrl : detected)
    {
        controllers.push_back(ctrl);
        resource_manager->RegisterRGBController(ctrl);
    }

    loaded = true;
}

QWidget* CorsairCapellixXTPlugin::GetWidget()
{
    /*-----------------------------------------------------------------*\
    | OpenRGB always passes this widget to OpenRGBPluginContainer which |
    | calls setParent() on it, so returning nullptr causes a crash.     |
    | Return an empty widget instead.                                   |
    \*-----------------------------------------------------------------*/
    return new QWidget();
}

QMenu* CorsairCapellixXTPlugin::GetTrayMenu()
{
    /* No tray menu */
    return nullptr;
}

void CorsairCapellixXTPlugin::Unload()
{
    for(RGBController* ctrl : controllers)
    {
        resource_manager->UnregisterRGBController(ctrl);
    }
    controllers.clear();
    loaded = false;
}
