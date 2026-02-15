#include "CorsairCapellixXTController.h"
#include <cstring>
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
}

CorsairCapellixXTController::~CorsairCapellixXTController()
{
    StopKeepalive();
    SetHardwareMode();

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
