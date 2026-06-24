#include "CorsairCapellixXTController.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

CorsairCapellixXTController::CorsairCapellixXTController(hid_device* dev, const char* path, uint16_t pid)
    : dev(dev)
    , product_id(pid)
    , device_path(path)
    , total_leds(0)
{
    /*-------------------------------------------------------------*\
    | Buffer sizes based on product ID                              |
    |   0x0C32 (Commander Core 2 / ST)  = 64-byte buffers           |
    |   0x0C2A (Commander Core XT)      = 384-byte buffers          |
    |   All others                      = 96-byte buffers           |
    \*-------------------------------------------------------------*/
    if(product_id == COMMANDER_CORE2_PID)
    {
        buffer_size         = 64;
        write_buffer_size   = 65;
        max_buf_per_request = 61;
    }
    else if(product_id == COMMANDER_CORE_XT_PID)
    {
        buffer_size         = 384;
        write_buffer_size   = 385;
        max_buf_per_request = 381;
    }
    else
    {
        buffer_size         = 96;
        write_buffer_size   = 97;
        max_buf_per_request = 93;
    }

    /*-------------------------------------------------------------*\
    | Read device strings                                           |
    \*-------------------------------------------------------------*/
    wchar_t buf[256];

    if(hid_get_serial_number_string(dev, buf, 256) == 0)
    {
        std::wstring ws(buf);
        serial = std::string(ws.begin(), ws.end());
    }

    if(hid_get_product_string(dev, buf, 256) == 0)
    {
        std::wstring ws(buf);
        device_name = std::string(ws.begin(), ws.end());
    }

    /*-------------------------------------------------------------*\
    | Default pump curve: liquid temperature (C) -> pump duty (%).   |
    | Quiet (~1150 rpm) up to ~42C, ramping to full by ~58C.         |
    | Floor is clamped to PUMP_DUTY_MIN so the pump never stops.     |
    \*-------------------------------------------------------------*/
    pump_curve =
    {
        { 50.0f,  30 },     // idle / light use: Silent floor (~1130 rpm) up to 50C
        { 53.0f,  50 },
        { 56.0f,  75 },
        { 58.0f, 100 },     // sustained load: full
    };

    fan_curve =
    {
        { 50.0f,  FAN_DUTY_MIN },   // idle / light use: Silent floor (~560 rpm) up to 50C
        { 53.0f,  45 },
        { 56.0f,  75 },
        { 58.0f, 100 },             // sustained load: full
    };

    LoadPumpMode();
}

CorsairCapellixXTController::~CorsairCapellixXTController()
{
    StopKeepalive();

    /*-----------------------------------------------------------------*\
    | NOTE: we deliberately do NOT send the hardware-mode command here. |
    | On this controller switching to hardware mode triggers a USB      |
    | re-enumeration, and the re-enumeration sometimes drops interface   |
    | 0's hidraw node, after which OpenRGB (hidraw backend) can no       |
    | longer detect the device until a physical replug / USB reset.     |
    | Once the keepalive stops the device reverts to its hardware        |
    | lighting on its own, so this is both safe and more robust.         |
    \*-----------------------------------------------------------------*/

    if(dev)
    {
        hid_close(dev);
        dev = nullptr;
    }
}

/*---------------------------------------------------------------------*\
| Keepalive thread — resend colors every 10s so the device doesn't      |
| revert to hardware lighting mode                                      |
\*---------------------------------------------------------------------*/

void CorsairCapellixXTController::StartKeepalive()
{
    keepalive_thread_run = true;
    last_commit_time     = std::chrono::steady_clock::now();
    keepalive_thread     = new std::thread(&CorsairCapellixXTController::KeepaliveThread, this);
}

void CorsairCapellixXTController::StopKeepalive()
{
    if(keepalive_thread)
    {
        keepalive_thread_run = false;
        keepalive_thread->join();
        delete keepalive_thread;
        keepalive_thread = nullptr;
    }
}

void CorsairCapellixXTController::KeepaliveThread()
{
    using namespace std::chrono_literals;

    while(keepalive_thread_run.load())
    {
        if((std::chrono::steady_clock::now() - last_commit_time) > 10s)
        {
            SendKeepalive();
        }

        /*-------------------------------------------------------------*\
        | Re-evaluate the pump curve every PUMP_UPDATE_INTERVAL_SEC.     |
        | Re-sending every cycle also keeps the pump in software-speed   |
        | mode so it can't revert to the loud hardware default.          |
        \*-------------------------------------------------------------*/
        if(++pump_update_counter >= PUMP_UPDATE_INTERVAL_SEC)
        {
            pump_update_counter = 0;
            UpdatePumpFromCurve();
        }

        std::this_thread::sleep_for(1s);
    }
}

void CorsairCapellixXTController::SendKeepalive()
{
    std::vector<uint8_t> colors_copy;

    {
        std::lock_guard<std::mutex> lock(color_mutex);
        colors_copy = last_colors;
    }

    if(!colors_copy.empty())
    {
        /*-------------------------------------------------------------*\
        | Resend last colors to keep device in software mode            |
        \*-------------------------------------------------------------*/
        SendColors(colors_copy);
    }
    else
    {
        /*-------------------------------------------------------------*\
        | No colors sent yet — send firmware query as keepalive ping    |
        \*-------------------------------------------------------------*/
        Transfer({CMD_GET_FIRMWARE_0, CMD_GET_FIRMWARE_1});
        last_commit_time = std::chrono::steady_clock::now();
    }
}

/*---------------------------------------------------------------------*\
| Public getters                                                        |
\*---------------------------------------------------------------------*/

std::string CorsairCapellixXTController::GetDevicePath()
{
    return device_path;
}

std::string CorsairCapellixXTController::GetFirmwareVersion()
{
    return firmware_version;
}

std::string CorsairCapellixXTController::GetSerialString()
{
    return serial;
}

std::string CorsairCapellixXTController::GetDeviceName()
{
    return device_name;
}

uint16_t CorsairCapellixXTController::GetProductID()
{
    return product_id;
}

unsigned int CorsairCapellixXTController::GetTotalLEDCount()
{
    return total_leds;
}

std::vector<ChannelInfo>& CorsairCapellixXTController::GetChannels()
{
    return channels;
}

/*---------------------------------------------------------------------*\
| Core transfer — matches OpenLinkHub cc.go transfer() exactly          |
|                                                                       |
| Write packet layout:                                                  |
|   [0] = 0x00  (HID report ID)                                        |
|   [1] = 0x08  (fixed protocol header)                                 |
|   [2..2+len(endpoint)] = endpoint/command bytes                       |
|   [2+len(endpoint)..] = buffer/payload bytes                          |
|   ... zero-padded to write_buffer_size                                |
\*---------------------------------------------------------------------*/

std::vector<uint8_t> CorsairCapellixXTController::Transfer(
    const std::vector<uint8_t>& endpoint,
    const std::vector<uint8_t>& buf)
{
    std::lock_guard<std::recursive_mutex> lock(io_mutex);

    std::vector<uint8_t> pkt(write_buffer_size, 0x00);

    pkt[0] = 0x00;                // HID report ID
    pkt[1] = CC_PROTOCOL_HEADER;  // 0x08

    size_t ep_start = CC_HEADER_SIZE;
    memcpy(&pkt[ep_start], endpoint.data(), endpoint.size());

    if(!buf.empty())
    {
        size_t buf_start = ep_start + endpoint.size();
        memcpy(&pkt[buf_start], buf.data(), buf.size());
    }

    hid_write(dev, pkt.data(), write_buffer_size);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    std::vector<uint8_t> resp(buffer_size);
    int bytes_read = hid_read_timeout(dev, resp.data(), buffer_size, 2000);

    if(bytes_read > 0)
    {
        resp.resize(bytes_read);
        return resp;
    }

    return {};
}

/*---------------------------------------------------------------------*\
| Read endpoint: close-open-read-close (matches OpenLinkHub read())     |
\*---------------------------------------------------------------------*/

std::vector<uint8_t> CorsairCapellixXTController::ReadEndpoint(uint8_t mode)
{
    std::lock_guard<std::recursive_mutex> lock(io_mutex);

    std::vector<uint8_t> mode_buf = { mode };

    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2}, mode_buf);
    Transfer({CMD_OPEN_ENDPOINT_0, CMD_OPEN_ENDPOINT_1}, mode_buf);
    std::vector<uint8_t> resp = Transfer({CMD_READ_0, CMD_READ_1}, mode_buf);
    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2}, mode_buf);

    return resp;
}

/*---------------------------------------------------------------------*\
| Protocol commands                                                     |
\*---------------------------------------------------------------------*/

void CorsairCapellixXTController::ReadFirmware()
{
    std::vector<uint8_t> resp = Transfer({CMD_GET_FIRMWARE_0, CMD_GET_FIRMWARE_1});

    if(resp.size() >= 7)
    {
        uint16_t patch = resp[5] | (resp[6] << 8);  // little-endian
        firmware_version = "v" + std::to_string(resp[3]) + "."
                               + std::to_string(resp[4]) + "."
                               + std::to_string(patch);
    }
    else
    {
        firmware_version = "unknown";
    }
}

void CorsairCapellixXTController::SetSoftwareMode()
{
    Transfer({CMD_SOFTWARE_MODE_0, CMD_SOFTWARE_MODE_1,
              CMD_SOFTWARE_MODE_2, CMD_SOFTWARE_MODE_3});
}

void CorsairCapellixXTController::SetHardwareMode()
{
    Transfer({CMD_HARDWARE_MODE_0, CMD_HARDWARE_MODE_1,
              CMD_HARDWARE_MODE_2, CMD_HARDWARE_MODE_3});
}

void CorsairCapellixXTController::InitLedPorts()
{
    for(int i = 0; i < CC_MAX_LED_CHANNELS; i++)
    {
        Transfer({0x14, (uint8_t)i, 0x01});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void CorsairCapellixXTController::QueryLEDConfig()
{
    channels.clear();
    total_leds = 0;

    std::vector<uint8_t> resp = ReadEndpoint(MODE_GET_LEDS);

    if(resp.size() < CC_LED_START_INDEX + CC_LED_BYTES_PER_CHANNEL)
    {
        channels.push_back({0, 33, "Pump Head"});
        channels.push_back({1,  8, "Fan 1"});
        channels.push_back({2,  8, "Fan 2"});
        channels.push_back({3,  8, "Fan 3"});
        total_leds = 57;
        return;
    }

    for(unsigned int ch = 0; ch < CC_MAX_LED_CHANNELS; ch++)
    {
        unsigned int off = CC_LED_START_INDEX + ch * CC_LED_BYTES_PER_CHANNEL;

        if(off + 3 >= resp.size())
        {
            break;
        }

        uint8_t  status   = resp[off];
        uint16_t num_leds = resp[off + 2] | (resp[off + 3] << 8);  // LE uint16

        if(status == LED_STATUS_CONNECTED && num_leds > 0)
        {
            std::string name;
            if(ch == 0)
            {
                name = "Pump Head";
            }
            else
            {
                name = "Fan/Port " + std::to_string(ch);
            }

            channels.push_back({ch, num_leds, name});
            total_leds += num_leds;
        }
    }

    if(channels.empty())
    {
        channels.push_back({0, 33, "Pump Head"});
        channels.push_back({1,  8, "Fan 1"});
        channels.push_back({2,  8, "Fan 2"});
        channels.push_back({3,  8, "Fan 3"});
        total_leds = 57;
    }
}

void CorsairCapellixXTController::Initialize()
{
    ReadFirmware();
    SetSoftwareMode();
    InitLedPorts();
    QueryLEDConfig();

    /*-----------------------------------------------------------------*\
    | Open color endpoint: close then open (stays open for writes)      |
    \*-----------------------------------------------------------------*/
    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2},
             {MODE_SET_COLOR});
    Transfer({CMD_OPEN_COLOR_ENDPOINT_0, CMD_OPEN_COLOR_ENDPOINT_1},
             {MODE_SET_COLOR});

    /*-----------------------------------------------------------------*\
    | Apply the pump curve once immediately so the pump goes quiet at    |
    | startup instead of waiting for the first keepalive tick           |
    \*-----------------------------------------------------------------*/
    UpdatePumpFromCurve();

    /*-----------------------------------------------------------------*\
    | Start keepalive thread to prevent revert to hardware lighting     |
    \*-----------------------------------------------------------------*/
    StartKeepalive();
}

/*---------------------------------------------------------------------*\
| Color output — matches OpenLinkHub writeColor()                       |
|                                                                       |
| Write buffer:                                                         |
|   [0..1] = LE uint16 size (len(color_data) + 2)                      |
|   [2..3] = 0x00 0x00  (padding to headerWriteSize=4)                 |
|   [4..5] = dataTypeSetColor (0x12, 0x00)                             |
|   [6..]  = RGB color data                                            |
|                                                                       |
| Chunked into max_buf_per_request-sized pieces and sent via            |
| CMD_WRITE_COLOR (first chunk) / CMD_WRITE_COLOR_NEXT (rest).         |
\*---------------------------------------------------------------------*/

void CorsairCapellixXTController::SendColors(const std::vector<uint8_t>& color_data)
{
    if(color_data.empty())
    {
        return;
    }

    /*-----------------------------------------------------------------*\
    | Hold the device lock for the whole multi-chunk write so a pump     |
    | update on the keepalive thread can't interleave on the HID pipe    |
    \*-----------------------------------------------------------------*/
    std::lock_guard<std::recursive_mutex> io_lock(io_mutex);

    /*-----------------------------------------------------------------*\
    | Store last colors for keepalive resend                            |
    \*-----------------------------------------------------------------*/
    {
        std::lock_guard<std::mutex> lock(color_mutex);
        last_colors = color_data;
    }

    /*-----------------------------------------------------------------*\
    | Build write buffer                                                |
    \*-----------------------------------------------------------------*/
    uint16_t size = (uint16_t)(color_data.size() + 2);

    std::vector<uint8_t> write_buf;
    write_buf.push_back(size & 0xFF);           // LE size low
    write_buf.push_back((size >> 8) & 0xFF);    // LE size high
    write_buf.push_back(0x00);                  // padding
    write_buf.push_back(0x00);                  // padding
    write_buf.push_back(DATA_TYPE_SET_COLOR_0); // 0x12
    write_buf.push_back(DATA_TYPE_SET_COLOR_1); // 0x00
    write_buf.insert(write_buf.end(), color_data.begin(), color_data.end());

    /*-----------------------------------------------------------------*\
    | Chunk and send                                                    |
    \*-----------------------------------------------------------------*/
    size_t offset = 0;
    int chunk_num = 0;

    while(offset < write_buf.size())
    {
        size_t chunk_size = write_buf.size() - offset;
        if(chunk_size > max_buf_per_request)
        {
            chunk_size = max_buf_per_request;
        }

        std::vector<uint8_t> chunk(write_buf.begin() + offset,
                                   write_buf.begin() + offset + chunk_size);

        if(chunk_num == 0)
        {
            Transfer({CMD_WRITE_COLOR_0, CMD_WRITE_COLOR_1}, chunk);
        }
        else
        {
            Transfer({CMD_WRITE_COLOR_NEXT_0, CMD_WRITE_COLOR_NEXT_1}, chunk);
        }

        offset += chunk_size;
        chunk_num++;
    }

    last_commit_time = std::chrono::steady_clock::now();
}

/*---------------------------------------------------------------------*\
| Pump / fan speed control (endpoint 0x18, dataType 0x07 0x00)          |
|                                                                       |
| Speed buffer (one channel here, the pump):                            |
|   [0]    = channel count (1)                                          |
|   [1..4] = { channel, mode(0=percent), duty, 0x00 }                   |
| Wrapped like writeColor: LE16 size, 0x00 0x00 pad, dataType, data.    |
\*---------------------------------------------------------------------*/

void CorsairCapellixXTController::SetPumpDuty(uint8_t duty)
{
    /*-----------------------------------------------------------------*\
    | SAFETY: clamp to a floor so the pump never stops (no flow).        |
    | Measured: <=10% => 0 rpm, 20% => ~690 rpm, 30% => ~1150 rpm.       |
    \*-----------------------------------------------------------------*/
    if(duty < PUMP_DUTY_MIN) duty = PUMP_DUTY_MIN;
    if(duty > PUMP_DUTY_MAX) duty = PUMP_DUTY_MAX;

    std::vector<uint8_t> speed_data =
    {
        0x01,                       // channel count
        (uint8_t)PUMP_CHANNEL,      // channel id (pump = 0)
        SPEED_MODE_PERCENT,         // mode (fixed percent)
        duty,                       // duty %
        0x00,
    };

    uint16_t size = (uint16_t)(speed_data.size() + 2);

    std::vector<uint8_t> write_buf;
    write_buf.push_back(size & 0xFF);
    write_buf.push_back((size >> 8) & 0xFF);
    write_buf.push_back(0x00);
    write_buf.push_back(0x00);
    write_buf.push_back(DATA_TYPE_SET_SPEED_0);
    write_buf.push_back(DATA_TYPE_SET_SPEED_1);
    write_buf.insert(write_buf.end(), speed_data.begin(), speed_data.end());

    std::lock_guard<std::recursive_mutex> lock(io_mutex);
    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2}, {MODE_SET_SPEED});
    Transfer({CMD_OPEN_ENDPOINT_0, CMD_OPEN_ENDPOINT_1}, {MODE_SET_SPEED});
    Transfer({CMD_WRITE_0, CMD_WRITE_1}, write_buf);
    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2}, {MODE_SET_SPEED});

    last_pump_duty.store(duty);
}

/*---------------------------------------------------------------------*\
| Read AIO liquid temperature (channel 0 of the temperature endpoint)   |
|   response[6]   = status (0x00 == connected)                          |
|   response[7:9] = temperature * 10, little-endian                     |
\*---------------------------------------------------------------------*/

float CorsairCapellixXTController::ReadLiquidTemp()
{
    std::vector<uint8_t> resp = ReadEndpoint(MODE_GET_TEMPS);

    if(resp.size() < 9)
    {
        return -1.0f;
    }
    if(resp[6] != 0x00)
    {
        return -1.0f;
    }

    int16_t raw = (int16_t)(resp[7] | (resp[8] << 8));
    return (float)raw / 10.0f;
}

/*---------------------------------------------------------------------*\
| Read pump RPM (channel 0 of the speeds endpoint)                      |
\*---------------------------------------------------------------------*/

int CorsairCapellixXTController::ReadPumpRpm()
{
    std::vector<uint8_t> resp = ReadEndpoint(MODE_GET_SPEEDS);

    if(resp.size() < 8)
    {
        return -1;
    }

    return (int)(int16_t)(resp[6] | (resp[7] << 8));
}

/*---------------------------------------------------------------------*\
| Read first radiator fan RPM (speed channel 1)                         |
\*---------------------------------------------------------------------*/

int CorsairCapellixXTController::ReadFanRpm()
{
    std::vector<uint8_t> resp = ReadEndpoint(MODE_GET_SPEEDS);

    if(resp.size() < 10)
    {
        return -1;
    }

    // [6:8] = pump (ch0), [8:10] = fan1 (ch1)
    return (int)(int16_t)(resp[8] | (resp[9] << 8));
}

/*---------------------------------------------------------------------*\
| Drive pump (channel 0) and all radiator fans (channels 1..6) in a     |
| single speed write. Fans are clamped to FAN_DUTY_MIN so they never    |
| stall (~300 rpm floor); the pump is clamped to PUMP_DUTY_MIN.         |
\*---------------------------------------------------------------------*/

void CorsairCapellixXTController::SetCooling(uint8_t pump_duty, uint8_t fan_duty)
{
    if(pump_duty < PUMP_DUTY_MIN) pump_duty = PUMP_DUTY_MIN;
    if(pump_duty > PUMP_DUTY_MAX) pump_duty = PUMP_DUTY_MAX;
    if(fan_duty  < FAN_DUTY_MIN)  fan_duty  = FAN_DUTY_MIN;
    if(fan_duty  > 100)           fan_duty  = 100;

    int channel_count = 1 + (FAN_CHANNEL_LAST - FAN_CHANNEL_FIRST + 1);  // pump + 6 fans

    std::vector<uint8_t> speed_data;
    speed_data.push_back((uint8_t)channel_count);

    // pump (channel 0)
    speed_data.push_back((uint8_t)PUMP_CHANNEL);
    speed_data.push_back(SPEED_MODE_PERCENT);
    speed_data.push_back(pump_duty);
    speed_data.push_back(0x00);

    // fans (channels 1..6 — unconnected ports are harmlessly ignored)
    for(int ch = FAN_CHANNEL_FIRST; ch <= FAN_CHANNEL_LAST; ch++)
    {
        speed_data.push_back((uint8_t)ch);
        speed_data.push_back(SPEED_MODE_PERCENT);
        speed_data.push_back(fan_duty);
        speed_data.push_back(0x00);
    }

    uint16_t size = (uint16_t)(speed_data.size() + 2);

    std::vector<uint8_t> write_buf;
    write_buf.push_back(size & 0xFF);
    write_buf.push_back((size >> 8) & 0xFF);
    write_buf.push_back(0x00);
    write_buf.push_back(0x00);
    write_buf.push_back(DATA_TYPE_SET_SPEED_0);
    write_buf.push_back(DATA_TYPE_SET_SPEED_1);
    write_buf.insert(write_buf.end(), speed_data.begin(), speed_data.end());

    std::lock_guard<std::recursive_mutex> lock(io_mutex);
    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2}, {MODE_SET_SPEED});
    Transfer({CMD_OPEN_ENDPOINT_0, CMD_OPEN_ENDPOINT_1}, {MODE_SET_SPEED});
    Transfer({CMD_WRITE_0, CMD_WRITE_1}, write_buf);
    Transfer({CMD_CLOSE_ENDPOINT_0, CMD_CLOSE_ENDPOINT_1, CMD_CLOSE_ENDPOINT_2}, {MODE_SET_SPEED});

    last_pump_duty.store(pump_duty);
    last_fan_duty.store(fan_duty);
}

/*---------------------------------------------------------------------*\
| Linear interpolation across the pump curve                            |
\*---------------------------------------------------------------------*/

uint8_t CorsairCapellixXTController::EvalCurve(const std::vector<CurvePoint>& curve, float tempC)
{
    if(curve.empty())
    {
        return PUMP_DUTY_MIN;
    }

    if(tempC <= curve.front().tempC)
    {
        return curve.front().duty;
    }
    if(tempC >= curve.back().tempC)
    {
        return curve.back().duty;
    }

    for(size_t i = 1; i < curve.size(); i++)
    {
        const CurvePoint& a = curve[i - 1];
        const CurvePoint& b = curve[i];
        if(tempC <= b.tempC)
        {
            float span = b.tempC - a.tempC;
            float frac = span > 0.0f ? (tempC - a.tempC) / span : 0.0f;
            float duty = a.duty + frac * ((float)b.duty - (float)a.duty);
            return (uint8_t)(duty + 0.5f);
        }
    }

    return curve.back().duty;
}

/*---------------------------------------------------------------------*\
| Read liquid temp, evaluate the curve, drive the pump                  |
\*---------------------------------------------------------------------*/

void CorsairCapellixXTController::UpdatePumpFromCurve()
{
    if(pump_mode.load() == PUMP_MODE_DISABLED)
    {
        /*-------------------------------------------------------------*\
        | Hands off: don't send any speed commands so the pump/fans run |
        | on their own (or under an external tool). RGB is unaffected.  |
        \*-------------------------------------------------------------*/
        printf("[CommanderCore] mode=Disabled (pump/fans not managed)\n");
        fflush(stdout);
        return;
    }

    float tempC = ReadLiquidTemp();
    if(tempC >= 0.0f)
    {
        last_liquid_temp.store(tempC);
    }

    int         mode = pump_mode.load();
    const char* mode_name;
    uint8_t     pump_duty;
    uint8_t     fan_duty;

    switch(mode)
    {
        case PUMP_MODE_SILENT:       pump_duty = PUMP_DUTY_SILENT;      fan_duty = FAN_DUTY_SILENT;      mode_name = "Silent";      break;
        case PUMP_MODE_QUIET:        pump_duty = PUMP_DUTY_QUIET;       fan_duty = FAN_DUTY_QUIET;       mode_name = "Quiet";       break;
        case PUMP_MODE_BALANCED:     pump_duty = PUMP_DUTY_BALANCED;    fan_duty = FAN_DUTY_BALANCED;    mode_name = "Balanced";    break;
        case PUMP_MODE_PERFORMANCE:  pump_duty = PUMP_DUTY_PERFORMANCE; fan_duty = FAN_DUTY_PERFORMANCE; mode_name = "Performance"; break;
        case PUMP_MODE_AUTO:
        default:
            mode_name = "Auto";
            if(tempC >= 0.0f)
            {
                pump_duty = EvalCurve(pump_curve, tempC);
                fan_duty  = EvalCurve(fan_curve,  tempC);
            }
            else
            {
                pump_duty = last_pump_duty.load();
                fan_duty  = last_fan_duty.load();
            }
            break;
    }

    SetCooling(pump_duty, fan_duty);

    int prpm = ReadPumpRpm();
    int frpm = ReadFanRpm();
    printf("[CommanderCore] mode=%s liquid=%.1fC | pump=%u%%/%drpm | fans=%u%%/%drpm\n",
           mode_name, tempC, (unsigned)pump_duty, prpm, (unsigned)fan_duty, frpm);
    fflush(stdout);
}

void CorsairCapellixXTController::SetPumpCurve(const std::vector<CurvePoint>& points)
{
    if(!points.empty())
    {
        pump_curve = points;
    }
}

float CorsairCapellixXTController::GetLastLiquidTemp()
{
    return last_liquid_temp.load();
}

uint8_t CorsairCapellixXTController::GetLastPumpDuty()
{
    return last_pump_duty.load();
}

/*---------------------------------------------------------------------*\
| Pump mode selection + persistence                                     |
\*---------------------------------------------------------------------*/

static std::string PumpModeConfigPath()
{
    const char* home = getenv("HOME");
    if(home == nullptr)
    {
        return "";
    }
    return std::string(home) + "/.config/OpenRGB/plugins/settings/CommanderCorePump.conf";
}

void CorsairCapellixXTController::SetPumpMode(int mode)
{
    if(mode < PUMP_MODE_AUTO || mode > PUMP_MODE_DISABLED)
    {
        mode = PUMP_MODE_AUTO;
    }
    pump_mode.store(mode);
    SavePumpMode();
    UpdatePumpFromCurve();      // apply the new mode immediately
}

int CorsairCapellixXTController::GetPumpMode()
{
    return pump_mode.load();
}

void CorsairCapellixXTController::LoadPumpMode()
{
    std::string path = PumpModeConfigPath();
    if(path.empty())
    {
        return;
    }
    FILE* f = fopen(path.c_str(), "r");
    if(f == nullptr)
    {
        return;                 // no saved file yet -> stays default (Auto)
    }
    int m = PUMP_MODE_AUTO;
    if(fscanf(f, "%d", &m) == 1 && m >= PUMP_MODE_AUTO && m <= PUMP_MODE_DISABLED)
    {
        pump_mode.store(m);
    }
    fclose(f);
}

void CorsairCapellixXTController::SavePumpMode()
{
    std::string path = PumpModeConfigPath();
    if(path.empty())
    {
        return;
    }
    FILE* f = fopen(path.c_str(), "w");
    if(f == nullptr)
    {
        return;
    }
    fprintf(f, "%d\n", pump_mode.load());
    fclose(f);
}
