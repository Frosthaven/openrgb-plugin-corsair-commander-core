#pragma once

#include "RGBController.h"
#include "CorsairCapellixXTController.h"

class RGBController_CorsairCapellixXT : public RGBController
{
public:
    RGBController_CorsairCapellixXT(CorsairCapellixXTController* controller);
    ~RGBController_CorsairCapellixXT();

    void SetupZones()                                   override;
    void ResizeZone(int zone, int new_size)              override;

    void DeviceUpdateLEDs()                              override;
    void UpdateZoneLEDs(int zone)                        override;
    void UpdateSingleLED(int led)                        override;

    void DeviceUpdateMode()                              override;

private:
    CorsairCapellixXTController* controller;
};
