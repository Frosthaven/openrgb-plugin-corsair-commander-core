#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <hidapi.h>

// Commander Core USB identifiers
#define CORSAIR_VID                 0x1B1C
#define COMMANDER_CORE_PID          0x0C1C  // Commander Core (original, 96-byte buffer)
#define COMMANDER_ST_PID            0x0C32  // Commander ST (newer revision, 64-byte buffer)

// Number of supported PIDs
static const uint16_t COMMANDER_CORE_PIDS[] = { COMMANDER_CORE_PID, COMMANDER_ST_PID };
static const size_t   COMMANDER_CORE_PID_COUNT = 2;

// Protocol constants
#define CC_PROTOCOL_HEADER          0x08    // Fixed byte at position 1 of every write
#define CC_HEADER_SIZE              2       // report_id + protocol header
#define CC_HEADER_WRITE_SIZE        4       // header in writeColor buffer construction
#define CC_LED_START_INDEX          6       // LED data offset in read response
#define CC_LED_BYTES_PER_CHANNEL    4       // bytes per channel in LED config
#define CC_MAX_LED_CHANNELS         7       // max LED channels on Commander Core

// Command bytes (endpoint parameter to transfer())
#define CMD_OPEN_ENDPOINT_0         0x0D
#define CMD_OPEN_ENDPOINT_1         0x01

#define CMD_OPEN_COLOR_ENDPOINT_0   0x0D
#define CMD_OPEN_COLOR_ENDPOINT_1   0x00

#define CMD_CLOSE_ENDPOINT_0        0x05
#define CMD_CLOSE_ENDPOINT_1        0x01
#define CMD_CLOSE_ENDPOINT_2        0x01

#define CMD_GET_FIRMWARE_0          0x02
#define CMD_GET_FIRMWARE_1          0x13

#define CMD_SOFTWARE_MODE_0         0x01
#define CMD_SOFTWARE_MODE_1         0x03
#define CMD_SOFTWARE_MODE_2         0x00
#define CMD_SOFTWARE_MODE_3         0x02

#define CMD_HARDWARE_MODE_0         0x01
#define CMD_HARDWARE_MODE_1         0x03
#define CMD_HARDWARE_MODE_2         0x00
#define CMD_HARDWARE_MODE_3         0x01

#define CMD_WRITE_COLOR_0           0x06
#define CMD_WRITE_COLOR_1           0x00

#define CMD_WRITE_COLOR_NEXT_0      0x07
#define CMD_WRITE_COLOR_NEXT_1      0x00

#define CMD_READ_0                  0x08
#define CMD_READ_1                  0x01

// Endpoint data modes (buffer parameter to transfer())
#define MODE_GET_LEDS               0x20
#define MODE_SET_COLOR              0x22

// Data type prefix for color writes
#define DATA_TYPE_SET_COLOR_0       0x12
#define DATA_TYPE_SET_COLOR_1       0x00

// LED status byte meaning
#define LED_STATUS_CONNECTED        0x02

struct ChannelInfo
{
    unsigned int    port;
    unsigned int    led_count;
    std::string     name;
};

class CorsairCapellixXTController
{
public:
    CorsairCapellixXTController(hid_device* dev, const char* path, uint16_t pid);
    ~CorsairCapellixXTController();

    std::string                 GetDevicePath();
    std::string                 GetFirmwareVersion();
    std::string                 GetSerialString();
    std::string                 GetDeviceName();

    unsigned int                GetTotalLEDCount();
    std::vector<ChannelInfo>&   GetChannels();

    void                        Initialize();
    void                        SetSoftwareMode();
    void                        SetHardwareMode();
    void                        QueryLEDConfig();
    void                        SendColors(const std::vector<uint8_t>& color_data);

private:
    hid_device*                 dev;
    uint16_t                    product_id;
    unsigned int                buffer_size;
    unsigned int                write_buffer_size;
    unsigned int                max_buf_per_request;

    std::string                 device_path;
    std::string                 firmware_version;
    std::string                 serial;
    std::string                 device_name;

    unsigned int                total_leds;
    std::vector<ChannelInfo>    channels;

    /*-----------------------------------------------------------------*\
    | Core transfer: every packet has 0x08 at byte[1]                   |
    |   bufferW[0] = 0x00  (HID report ID)                             |
    |   bufferW[1] = 0x08  (fixed protocol header)                     |
    |   bufferW[2..] = endpoint bytes + buffer bytes                    |
    \*-----------------------------------------------------------------*/
    std::vector<uint8_t>        Transfer(const std::vector<uint8_t>& endpoint,
                                         const std::vector<uint8_t>& buf = {});

    /*-----------------------------------------------------------------*\
    | Read endpoint: close-open-read-close sequence                     |
    \*-----------------------------------------------------------------*/
    std::vector<uint8_t>        ReadEndpoint(uint8_t mode);

    void                        ReadFirmware();
    void                        InitLedPorts();
};
