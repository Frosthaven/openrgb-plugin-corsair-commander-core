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
    switch(controller->GetProductID())
    {
        case COMMANDER_CORE_PID:
            name        = "Corsair Commander Core";
            description = "Corsair Commander Core RGB Controller";
            break;
        case COMMANDER_CORE2_PID:
            name        = "Corsair Commander Core 2";
            description = "Corsair Commander Core 2 RGB Controller";
            break;
        case COMMANDER_CORE3_PID:
            name        = "Corsair Commander Core 3";
            description = "Corsair Commander Core 3 RGB Controller";
            break;
        case COMMANDER_CORE4_PID:
            name        = "Corsair Commander Core 4";
            description = "Corsair Commander Core 4 RGB Controller";
            break;
        case COMMANDER_CORE5_PID:
            name        = "Corsair Commander Core 5";
            description = "Corsair Commander Core 5 RGB Controller";
            break;
        case COMMANDER_CORE6_PID:
            name        = "Corsair Commander Core 6";
            description = "Corsair Commander Core 6 RGB Controller";
            break;
        case COMMANDER_CORE_XT_PID:
            name        = "Corsair Commander Core XT";
            description = "Corsair Commander Core XT RGB Controller";
            break;
        default:
            name        = "Corsair Commander Core";
            description = "Corsair Commander Core RGB Controller";
            break;
    }

    vendor                  = "Corsair";
    type                    = DEVICE_TYPE_COOLER;
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
    /*-----------------------------------------------------------------*\
    | The Commander Core expects each fan port (zones 1+) to occupy a   |
    | fixed 34-LED (102-byte) slot in the color buffer, regardless of   |
    | how many LEDs the fan actually has.  Zone 0 (pump head) is NOT    |
    | padded — its data is exactly led_count * 3 bytes.                 |
    |                                                                   |
    | Layout:                                                           |
    |   [zone 0: pump LEDs × 3]                                        |
    |   [zone 1: fan LEDs × 3 + padding to 34 × 3]                    |
    |   [zone 2: fan LEDs × 3 + padding to 34 × 3]                    |
    |   ...                                                             |
    \*-----------------------------------------------------------------*/
    static const unsigned int LEDS_PER_FAN_SLOT = 34;

    std::vector<ChannelInfo>& ch = controller->GetChannels();

    std::vector<uint8_t> color_data;
    unsigned int color_idx = 0;

    for(unsigned int zone_idx = 0; zone_idx < ch.size(); zone_idx++)
    {
        unsigned int led_count = ch[zone_idx].led_count;

        for(unsigned int i = 0; i < led_count && color_idx < colors.size(); i++, color_idx++)
        {
            color_data.push_back(RGBGetRValue(colors[color_idx]));
            color_data.push_back(RGBGetGValue(colors[color_idx]));
            color_data.push_back(RGBGetBValue(colors[color_idx]));
        }

        /*-------------------------------------------------------------*\
        | Pad fan ports (zone > 0) to 34-LED slots                      |
        \*-------------------------------------------------------------*/
        if(zone_idx != 0 && led_count < LEDS_PER_FAN_SLOT)
        {
            color_data.resize(color_data.size() + (LEDS_PER_FAN_SLOT - led_count) * 3, 0x00);
        }
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
