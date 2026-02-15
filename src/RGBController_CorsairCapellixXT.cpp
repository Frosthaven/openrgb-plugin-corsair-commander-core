#include "RGBController_CorsairCapellixXT.h"

/**------------------------------------------------------------------*\
    @name Corsair H150i Elite CAPELLIX XT
    @category Cooler
    @type USB
    @save :x:
    @direct :white_check_mark:
    @effects :x:
    @detectors DetectCorsairCapellixXT
    @comment Controls the Commander Core bundled with Corsair
            CAPELLIX XT series AIO liquid coolers.
\*------------------------------------------------------------------*/

RGBController_CorsairCapellixXT::RGBController_CorsairCapellixXT(CorsairCapellixXTController* ctrl)
    : controller(ctrl)
{
    name                    = "Corsair H150i Elite CAPELLIX XT";
    vendor                  = "Corsair";
    type                    = DEVICE_TYPE_COOLER;
    description             = "Corsair iCUE H150i Elite CAPELLIX XT AIO Liquid CPU Cooler";
    version                 = controller->GetFirmwareVersion();
    serial                  = controller->GetSerialString();
    location                = controller->GetDevicePath();

    mode Direct;
    Direct.name             = "Direct";
    Direct.value            = 0;
    Direct.flags            = MODE_FLAG_HAS_PER_LED_COLOR;
    Direct.color_mode       = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    SetupZones();
}

RGBController_CorsairCapellixXT::~RGBController_CorsairCapellixXT()
{
    delete controller;
}

void RGBController_CorsairCapellixXT::SetupZones()
{
    std::vector<ChannelInfo>& ch = controller->GetChannels();

    leds.clear();
    zones.clear();
    colors.clear();

    for(const ChannelInfo& info : ch)
    {
        zone z;
        z.name          = info.name;
        z.type          = ZONE_TYPE_LINEAR;
        z.leds_min      = info.led_count;
        z.leds_max      = info.led_count;
        z.leds_count    = info.led_count;

        /*-------------------------------------------------------------*\
        | Use a matrix map for the pump head if it has the standard     |
        | 33-LED CAPELLIX layout — otherwise linear                     |
        \*-------------------------------------------------------------*/
        z.matrix_map    = nullptr;

        zones.push_back(z);

        for(unsigned int i = 0; i < info.led_count; i++)
        {
            led l;
            l.name = info.name + " LED " + std::to_string(i);
            leds.push_back(l);
        }
    }

    SetupColors();
}

void RGBController_CorsairCapellixXT::ResizeZone(int /*zone*/, int /*new_size*/)
{
    /* Fixed-size zones — nothing to do */
}

void RGBController_CorsairCapellixXT::DeviceUpdateLEDs()
{
    unsigned int total = controller->GetTotalLEDCount();
    std::vector<uint8_t> color_data(total * 3);

    for(unsigned int i = 0; i < total && i < colors.size(); i++)
    {
        color_data[i * 3 + 0] = RGBGetRValue(colors[i]);
        color_data[i * 3 + 1] = RGBGetGValue(colors[i]);
        color_data[i * 3 + 2] = RGBGetBValue(colors[i]);
    }

    controller->SendColors(color_data);
}

void RGBController_CorsairCapellixXT::UpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_CorsairCapellixXT::UpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_CorsairCapellixXT::DeviceUpdateMode()
{
    /* Only direct mode is supported — nothing to switch */
}
