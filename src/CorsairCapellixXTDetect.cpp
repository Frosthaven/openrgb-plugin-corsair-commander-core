#include "CorsairCapellixXTDetect.h"
#include "CorsairCapellixXTController.h"
#include "RGBController_CorsairCapellixXT.h"

#include <hidapi.h>

/**------------------------------------------------------------------*\
| Scan for Corsair Commander Core devices and create an               |
| RGBController for each one found.                                   |
|                                                                     |
| Supported PIDs:                                                     |
|   0x0C1C  Commander Core (original, 96-byte buffer)                 |
|   0x0C32  Commander ST   (newer revision, 64-byte buffer)           |
\*------------------------------------------------------------------*/

std::vector<RGBController*> DetectCorsairCapellixXT()
{
    std::vector<RGBController*> controllers;

    for(size_t p = 0; p < COMMANDER_CORE_PID_COUNT; p++)
    {
        uint16_t pid = COMMANDER_CORE_PIDS[p];

        hid_device_info* devs = hid_enumerate(CORSAIR_VID, pid);
        hid_device_info* cur  = devs;

        while(cur)
        {
            /*---------------------------------------------------------*\
            | We want interface 0 — the bidirectional control channel   |
            \*---------------------------------------------------------*/
            if(cur->interface_number == 0)
            {
                hid_device* dev = hid_open_path(cur->path);

                if(dev)
                {
                    hid_set_nonblocking(dev, 0);

                    CorsairCapellixXTController* controller =
                        new CorsairCapellixXTController(dev, cur->path, pid);

                    controller->Initialize();

                    RGBController_CorsairCapellixXT* rgb =
                        new RGBController_CorsairCapellixXT(controller);

                    controllers.push_back(rgb);
                }
            }

            cur = cur->next;
        }

        hid_free_enumeration(devs);
    }

    return controllers;
}
