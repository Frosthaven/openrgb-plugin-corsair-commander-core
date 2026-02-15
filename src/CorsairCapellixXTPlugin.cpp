#include "CorsairCapellixXTPlugin.h"
#include "CorsairCapellixXTDetect.h"

OpenRGBPluginInfo CorsairCapellixXTPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name           = "Corsair CAPELLIX XT";
    info.Description    = "Adds support for the Corsair iCUE H150i Elite CAPELLIX XT "
                          "AIO liquid CPU cooler via the Commander Core controller.";
    info.Version        = VERSION_STRING;
    info.Commit         = GIT_COMMIT_ID;
    info.URL            = "https://github.com/Frosthaven/openrgb-h150i-corsair-capellix-xt";
    info.Icon.load(":/fan.svg");

    info.Label          = "";
    info.Location       = OPENRGB_PLUGIN_LOCATION_NONE;

    return info;
}

unsigned int CorsairCapellixXTPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void CorsairCapellixXTPlugin::Load(bool /*dark_theme*/, ResourceManager* rm)
{
    if(loaded)
    {
        return;
    }

    resource_manager = rm;

    /*-------------------------------------------------------------*\
    | Register our device detector with the resource manager        |
    \*-------------------------------------------------------------*/
    resource_manager->RegisterDeviceDetector(
        "Corsair H150i Elite CAPELLIX XT",
        DetectCorsairCapellixXT
    );

    loaded = true;
}

QWidget* CorsairCapellixXTPlugin::GetWidget()
{
    /* No custom UI widget */
    return nullptr;
}

QMenu* CorsairCapellixXTPlugin::GetTrayMenu()
{
    /* No tray menu */
    return nullptr;
}

void CorsairCapellixXTPlugin::Unload()
{
    loaded = false;
}
