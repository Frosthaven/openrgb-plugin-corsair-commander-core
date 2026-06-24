#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <hidapi.h>

// Commander Core USB identifiers
#define CORSAIR_VID                     0x1B1C
#define COMMANDER_CORE_PID              0x0C1C  // Commander Core        (96-byte buffer)
#define COMMANDER_CORE2_PID             0x0C32  // Commander Core 2 / ST (64-byte buffer)
#define COMMANDER_CORE3_PID             0x0C1D  // Commander Core 3      (96-byte buffer)
#define COMMANDER_CORE4_PID             0x0C3C  // Commander Core 4      (96-byte buffer)
#define COMMANDER_CORE5_PID             0x0C3D  // Commander Core 5      (96-byte buffer)
#define COMMANDER_CORE6_PID             0x0C3E  // Commander Core 6      (96-byte buffer)
#define COMMANDER_CORE_XT_PID           0x0C2A  // Commander Core XT     (384-byte buffer)

// Number of supported PIDs
static const uint16_t COMMANDER_CORE_PIDS[] =
{
    COMMANDER_CORE_PID,
    COMMANDER_CORE2_PID,
    COMMANDER_CORE3_PID,
    COMMANDER_CORE4_PID,
    COMMANDER_CORE5_PID,
    COMMANDER_CORE6_PID,
    COMMANDER_CORE_XT_PID
};
static const size_t COMMANDER_CORE_PID_COUNT = 7;

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

// Speed / temperature control (firmware 2.0 — ported from OpenLinkHub cc.go)
//   endpoint 0x18 = set speed, 0x17 = get speeds, 0x21 = get temperatures
#define CMD_WRITE_0                 0x06    // non-color write (color write is 0x06 0x00)
#define CMD_WRITE_1                 0x01

#define MODE_GET_SPEEDS             0x17
#define MODE_SET_SPEED              0x18
#define MODE_GET_TEMPS              0x21

#define DATA_TYPE_SET_SPEED_0       0x07
#define DATA_TYPE_SET_SPEED_1       0x00

#define SPEED_MODE_PERCENT          0x00    // per-channel mode byte: 0 = fixed percent

#define PUMP_CHANNEL                0       // pump is speed channel 0 (also carries liquid temp)
#define PUMP_DUTY_MIN               30      // SAFETY floor: <=10% stops the pump (no coolant flow)
#define PUMP_DUTY_MAX               100
#define PUMP_UPDATE_INTERVAL_SEC    3       // re-evaluate the curve every N seconds

// Selectable pump operating modes (exposed as radio buttons in the plugin pane).
// Fixed-mode duties are calibrated from the measured duty->RPM sweep on this pump.
enum CorsairPumpMode
{
    PUMP_MODE_AUTO        = 0,   // liquid-temperature curve (default; quiet idle)
    PUMP_MODE_SILENT      = 1,   // fixed, quietest safe speed (below iCUE Quiet)
    PUMP_MODE_QUIET       = 2,   // fixed, ~iCUE "Quiet"    band
    PUMP_MODE_BALANCED    = 3,   // fixed, ~iCUE "Balanced" band
    PUMP_MODE_PERFORMANCE = 4,   // fixed, ~iCUE "Extreme"  band
    PUMP_MODE_DISABLED    = 5,   // hands off — let the pump/fans run externally
};

#define PUMP_DUTY_SILENT            30      // ~1130 rpm (quietest safe flow)
#define PUMP_DUTY_QUIET             65      // ~2150 rpm
#define PUMP_DUTY_BALANCED          80      // ~2500 rpm
#define PUMP_DUTY_PERFORMANCE      100      // ~2800 rpm

// Radiator fans are speed channels 1..6 (channel 0 is the pump). Each mode
// drives the fans too, so "Silent" actually quiets the loudest component.
#define FAN_CHANNEL_FIRST           1
#define FAN_CHANNEL_LAST            6
#define FAN_DUTY_MIN                20      // floor: keeps fans above ~300 rpm (no stall) - tuned by test
#define FAN_DUTY_SILENT             20      // quietest (= floor)
#define FAN_DUTY_QUIET              40
#define FAN_DUTY_BALANCED           65
#define FAN_DUTY_PERFORMANCE       100

// A single (liquid temperature -> pump duty%) point on the control curve
struct CurvePoint
{
    float           tempC;
    uint8_t         duty;
};

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
    uint16_t                    GetProductID();

    unsigned int                GetTotalLEDCount();
    std::vector<ChannelInfo>&   GetChannels();

    void                        Initialize();
    void                        SetSoftwareMode();
    void                        SetHardwareMode();
    void                        QueryLEDConfig();
    void                        SendColors(const std::vector<uint8_t>& color_data);

    void                        StartKeepalive();
    void                        StopKeepalive();

    /*-----------------------------------------------------------------*\
    | Pump speed control (liquid-temp curve, runs in the keepalive      |
    | thread so it shares this process's exclusive device access)       |
    \*-----------------------------------------------------------------*/
    void                        SetPumpDuty(uint8_t duty);
    void                        SetCooling(uint8_t pump_duty, uint8_t fan_duty);
    float                       ReadLiquidTemp();
    int                         ReadPumpRpm();
    int                         ReadFanRpm();
    void                        SetPumpCurve(const std::vector<CurvePoint>& points);
    float                       GetLastLiquidTemp();
    uint8_t                     GetLastPumpDuty();

    /*-----------------------------------------------------------------*\
    | Pump mode: PUMP_MODE_AUTO (curve) or a fixed Quiet/Balanced/Perf   |
    | mode. Persisted so the choice survives restarts.                   |
    \*-----------------------------------------------------------------*/
    void                        SetPumpMode(int mode);
    int                         GetPumpMode();

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
    | Keepalive — resend colors every 10s to prevent hardware revert    |
    \*-----------------------------------------------------------------*/
    std::thread*                                keepalive_thread = nullptr;
    std::atomic<bool>                           keepalive_thread_run{false};
    std::mutex                                  color_mutex;
    std::vector<uint8_t>                        last_colors;
    std::chrono::steady_clock::time_point       last_commit_time;

    /*-----------------------------------------------------------------*\
    | Serializes ALL device I/O so the pump-curve updates and the color  |
    | writes (different threads) never interleave on the single HID pipe |
    \*-----------------------------------------------------------------*/
    std::recursive_mutex                        io_mutex;

    /*-----------------------------------------------------------------*\
    | Pump control state                                                 |
    \*-----------------------------------------------------------------*/
    std::vector<CurvePoint>                     pump_curve;
    std::vector<CurvePoint>                     fan_curve;
    int                                         pump_update_counter = 0;
    std::atomic<float>                          last_liquid_temp{0.0f};
    std::atomic<uint8_t>                        last_pump_duty{0};
    std::atomic<uint8_t>                        last_fan_duty{0};
    std::atomic<int>                            pump_mode{PUMP_MODE_AUTO};

    void                        KeepaliveThread();
    void                        SendKeepalive();

    uint8_t                     EvalCurve(const std::vector<CurvePoint>& curve, float tempC);
    void                        UpdatePumpFromCurve();
    void                        LoadPumpMode();
    void                        SavePumpMode();

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
