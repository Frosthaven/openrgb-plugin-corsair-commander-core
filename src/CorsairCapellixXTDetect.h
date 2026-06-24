#pragma once

#include <vector>

class RGBController;
class CorsairCapellixXTController;

// Detects Commander Core devices and returns an RGBController for each.
// If raw_out is provided, the underlying hardware controllers are also
// appended to it (so the plugin UI can drive pump speed/modes).
std::vector<RGBController*> DetectCorsairCapellixXT(
    std::vector<CorsairCapellixXTController*>* raw_out = nullptr);
