/*---------------------------------------------------------*\
| EightBitDoRetro87Controller.cpp                           |
|                                                           |
|   Driver for 8BitDo Retro 87 keyboard (2.4 GHz dongle)    |
|                                                           |
|   JoseStud                                    27 Sep 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <chrono>
#include <cstring>
#include <thread>
#include "EightBitDoRetro87Controller.h"
#include "LogManager.h"
#include "StringUtils.h"

using namespace std::chrono_literals;

EightBitDoRetro87Controller::EightBitDoRetro87Controller(hid_device* dev_handle, const char* path, std::string dev_name)
{
    dev         = dev_handle;
    location    = path;
    name        = dev_name;

    /*-----------------------------------------------------*\
    | Read the stored settings so that unchanged bytes are  |
    | not rewritten and the current effect can be shown     |
    \*-----------------------------------------------------*/
    custom_known = false;
    profile_read = Read(RETRO87_CMD_READ_PROFILE, profile, RETRO87_PROFILE_SIZE);

    if(!profile_read)
    {
        memset(profile, 0xFF, sizeof(profile));
        LOG_WARNING("[%s] Could not read the keyboard profile", name.c_str());
    }
    else if(!IsProfileValid())
    {
        LOG_WARNING("[%s] No profile stored on the keyboard; lighting settings other than the effect are ignored until one is created with the vendor software", name.c_str());
    }
}

EightBitDoRetro87Controller::~EightBitDoRetro87Controller()
{
    hid_close(dev);
}

void EightBitDoRetro87Controller::SetConnection(std::string connection_name)
{
    connection = connection_name;
}

std::string EightBitDoRetro87Controller::GetConnection()
{
    return(connection);
}

bool EightBitDoRetro87Controller::IsProfileRead()
{
    return(profile_read);
}

std::string EightBitDoRetro87Controller::GetDeviceLocation()
{
    return("HID: " + location);
}

std::string EightBitDoRetro87Controller::GetDeviceName()
{
    return(name);
}

std::string EightBitDoRetro87Controller::GetSerialString()
{
    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(serial_string));
}

bool EightBitDoRetro87Controller::IsProfileValid()
{
    unsigned int marker = profile[0] | (profile[1] << 8) | (profile[2] << 16) | ((unsigned int)profile[3] << 24);

    return(marker == RETRO87_PROFILE_MARKER);
}

bool EightBitDoRetro87Controller::IsProfileActive()
{
    /*-----------------------------------------------------*\
    | Toggled by the Profile button on the keyboard. While  |
    | inactive, the keyboard ignores stored brightness,     |
    | speed and colors and uses its onboard (Fn) settings.  |
    \*-----------------------------------------------------*/
    return(profile[RETRO87_PROFILE_ACTIVE] == 1);
}

unsigned char EightBitDoRetro87Controller::GetCurrentMode()
{
    if(profile[RETRO87_PROFILE_CUSTOM_ENABLE] == 1)
    {
        return(RETRO87_MODE_CUSTOM);
    }

    return(profile[RETRO87_PROFILE_CURRENT_MODE]);
}

unsigned char* EightBitDoRetro87Controller::GetProfile()
{
    return(profile);
}

bool EightBitDoRetro87Controller::ReadCustom(unsigned char* block)
{
    std::lock_guard<std::mutex> lock(transfer_mutex);

    if(!Read(RETRO87_CMD_READ_CUSTOM, block, RETRO87_CUSTOM_SIZE))
    {
        return(false);
    }

    memcpy(custom, block, RETRO87_CUSTOM_SIZE);
    custom_known = true;
    return(true);
}

void EightBitDoRetro87Controller::SetMode(unsigned char mode_value, unsigned char brightness, unsigned char speed, RGBColor color, RGBColor echo_color)
{
    /*-----------------------------------------------------*\
    | Each effect has its own record; the whole record is   |
    | written, keeping fields OpenRGB does not expose       |
    | (color ripple direction, starlight density)           |
    \*-----------------------------------------------------*/
    /*-----------------------------------------------------*\
    | Responses are matched to requests by reading the      |
    | device, so only one transaction may run at a time     |
    \*-----------------------------------------------------*/
    std::lock_guard<std::mutex> lock(transfer_mutex);

    unsigned int    offset      = 0;
    unsigned char   size        = 0;
    int             color_idx   = -1;
    int             echo_idx    = -1;
    bool            has_speed   = true;

    switch(mode_value)
    {
        case RETRO87_MODE_SOLID:
            offset      = RETRO87_PROFILE_SOLID;
            size        = 5;
            color_idx   = 2;
            has_speed   = false;
            break;

        case RETRO87_MODE_CYCLE:
            offset      = RETRO87_PROFILE_CYCLE;
            size        = 2;
            break;

        case RETRO87_MODE_COLOR_RIPPLE:
            offset      = RETRO87_PROFILE_COLOR_RIPPLE;
            size        = 3;
            break;

        case RETRO87_MODE_BREATHING:
            offset      = RETRO87_PROFILE_BREATHING;
            size        = 6;
            color_idx   = 3;
            break;

        case RETRO87_MODE_RIPPLE:
            offset      = RETRO87_PROFILE_RIPPLE;
            size        = 6;
            color_idx   = 3;
            break;

        case RETRO87_MODE_RESONANCE:
            offset      = RETRO87_PROFILE_RESONANCE;
            size        = 9;
            color_idx   = 3;
            echo_idx    = 6;
            break;

        case RETRO87_MODE_STARLIGHT:
            offset      = RETRO87_PROFILE_STARLIGHT;
            size        = 10;
            color_idx   = 4;
            echo_idx    = 7;
            break;
    }

    if(size > 0)
    {
        unsigned char record[10];

        memcpy(record, &profile[offset], size);

        record[0] = brightness;

        if(has_speed)
        {
            record[1] = speed;
        }

        if(mode_value == RETRO87_MODE_COLOR_RIPPLE && record[2] > 5)
        {
            record[2] = 0;
        }

        if(mode_value == RETRO87_MODE_STARLIGHT && (record[2] < 5 || record[2] > 100))
        {
            record[2] = 25;
        }

        if(color_idx >= 0)
        {
            record[color_idx - 1]   = 1;    /* Use stored color */
            record[color_idx]       = RGBGetRValue(color);
            record[color_idx + 1]   = RGBGetGValue(color);
            record[color_idx + 2]   = RGBGetBValue(color);
        }

        if(echo_idx >= 0)
        {
            record[echo_idx]        = RGBGetRValue(echo_color);
            record[echo_idx + 1]    = RGBGetGValue(echo_color);
            record[echo_idx + 2]    = RGBGetBValue(echo_color);
        }

        WriteProfile(offset, record, size);
    }

    /*-----------------------------------------------------*\
    | Vendor sequence: leave per-key mode, then select the  |
    | effect                                                |
    \*-----------------------------------------------------*/
    unsigned char value = 0;
    WriteProfile(RETRO87_PROFILE_CUSTOM_ENABLE, &value, 1);

    WriteProfile(RETRO87_PROFILE_CURRENT_MODE, &mode_value, 1);
}

void EightBitDoRetro87Controller::SetCustom(unsigned char brightness, RGBColor* led_colors)
{
    std::lock_guard<std::mutex> lock(transfer_mutex);

    unsigned char block[RETRO87_CUSTOM_SIZE];

    memset(block, 0, sizeof(block));

    block[0] = brightness;
    block[1] = 1;
    block[2] = RETRO87_CUSTOM_TYPE_STATIC;
    block[3] = RETRO87_SPEED_DEFAULT;
    block[4] = 25;

    for(unsigned int led_idx = 0; led_idx < RETRO87_CUSTOM_LED_COUNT; led_idx++)
    {
        block[9 + led_idx * 3]      = RGBGetRValue(led_colors[led_idx]);
        block[9 + led_idx * 3 + 1]  = RGBGetGValue(led_colors[led_idx]);
        block[9 + led_idx * 3 + 2]  = RGBGetBValue(led_colors[led_idx]);
    }

    block[RETRO87_CUSTOM_SIZE - 1] = 1;

    /*-----------------------------------------------------*\
    | Writing the per-key block erases flash; skip it when  |
    | the keyboard already stores exactly this block        |
    \*-----------------------------------------------------*/
    bool stored = custom_known && memcmp(custom, block, RETRO87_CUSTOM_SIZE) == 0;

    if(stored)
    {
        LOG_DEBUG("[%s] Per-key block unchanged; not rewritten", name.c_str());
    }
    else
    {
        LOG_DEBUG("[%s] Writing per-key block", name.c_str());
        custom_known = WriteCustom(block);

        if(custom_known)
        {
            memcpy(custom, block, RETRO87_CUSTOM_SIZE);
        }
    }

    if(stored || custom_known)
    {
        unsigned char value = 1;
        WriteProfile(RETRO87_PROFILE_CUSTOM_ENABLE, &value, 1);
    }
}

bool EightBitDoRetro87Controller::Transfer(unsigned char command, unsigned int offset, const unsigned char* data, unsigned char length, unsigned char* response)
{
    unsigned char   usb_buf[RETRO87_PACKET_SIZE];
    unsigned char   sum             = 0;
    bool            is_read         = (command == RETRO87_CMD_READ_PROFILE || command == RETRO87_CMD_READ_CUSTOM);
    bool            check_offset    = (command != RETRO87_CMD_PREPARE_CUSTOM && command != RETRO87_CMD_WRITE_CUSTOM);

    memset(usb_buf, 0, sizeof(usb_buf));

    usb_buf[0]  = RETRO87_REPORT_ID_OUT;
    usb_buf[1]  = 0x04;
    usb_buf[2]  = command;
    usb_buf[4]  = length;
    usb_buf[6]  = offset & 0xFF;
    usb_buf[7]  = (offset >> 8) & 0xFF;
    usb_buf[8]  = (offset >> 16) & 0xFF;
    usb_buf[9]  = (offset >> 24) & 0xFF;

    if(!is_read && data != NULL)
    {
        for(unsigned int i = 0; i < length; i++)
        {
            usb_buf[10 + i] = data[i];
            sum            += data[i];
        }
    }

    usb_buf[5]  = sum;

    if(hid_write(dev, usb_buf, RETRO87_PACKET_SIZE) != RETRO87_PACKET_SIZE)
    {
        return(false);
    }

    /*-----------------------------------------------------*\
    | Skip unrelated input reports until the response to    |
    | this command arrives                                  |
    \*-----------------------------------------------------*/
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(RETRO87_TIMEOUT_MS);

    while(std::chrono::steady_clock::now() < deadline)
    {
        int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        int ret       = hid_read_timeout(dev, response, RETRO87_PACKET_SIZE, remaining > 0 ? remaining : 1);

        if(ret < 0)
        {
            return(false);
        }

        if(ret < 10 || response[0] != RETRO87_REPORT_ID_IN || response[1] != 0x04 || response[2] != 0x03 || response[3] != command)
        {
            continue;
        }

        unsigned int returned_offset = response[6] | (response[7] << 8) | (response[8] << 16) | ((unsigned int)response[9] << 24);

        if(check_offset && returned_offset != offset)
        {
            continue;
        }

        return(true);
    }

    LOG_WARNING("[%s] No response to command 0x%02X at offset 0x%03X", name.c_str(), command, offset);

    return(false);
}

bool EightBitDoRetro87Controller::Read(unsigned char command, unsigned char* buf, unsigned int size)
{
    unsigned char response[RETRO87_PACKET_SIZE];
    unsigned int  offset = 0;

    while(offset < size)
    {
        unsigned char chunk = (size - offset) > RETRO87_MAX_DATA ? RETRO87_MAX_DATA : (unsigned char)(size - offset);

        if(!Transfer(command, offset, NULL, chunk, response))
        {
            return(false);
        }

        unsigned char returned = response[4];

        if(returned == 0 || returned > chunk)
        {
            return(false);
        }

        memcpy(&buf[offset], &response[10], returned);
        offset += returned;
    }

    return(true);
}

bool EightBitDoRetro87Controller::StartConfiguration()
{
    unsigned char usb_buf[RETRO87_PACKET_SIZE];

    memset(usb_buf, 0, sizeof(usb_buf));

    usb_buf[0]  = RETRO87_REPORT_ID_OUT;
    usb_buf[1]  = 0x04;
    usb_buf[2]  = RETRO87_CMD_CONFIG_MODE;

    bool ok = hid_write(dev, usb_buf, RETRO87_PACKET_SIZE) == RETRO87_PACKET_SIZE;

    std::this_thread::sleep_for(10ms);

    return(ok);
}

bool EightBitDoRetro87Controller::WriteProfile(unsigned int offset, const unsigned char* data, unsigned char length)
{
    unsigned char response[RETRO87_PACKET_SIZE];
    unsigned char readback[RETRO87_MAX_DATA];

    if(length == 0 || length > RETRO87_MAX_DATA || offset + length > RETRO87_PROFILE_SIZE)
    {
        return(false);
    }

    /*-----------------------------------------------------*\
    | Skip writes that would not change the stored value    |
    \*-----------------------------------------------------*/
    if(profile_read && memcmp(&profile[offset], data, length) == 0)
    {
        return(true);
    }

    if(!StartConfiguration())
    {
        return(false);
    }

    if(!Transfer(RETRO87_CMD_WRITE_PROFILE, offset, data, length, response) || response[4] != length)
    {
        LOG_WARNING("[%s] Write at 0x%03X was not acknowledged", name.c_str(), offset);
        return(false);
    }

    /*-----------------------------------------------------*\
    | Verify by reading back                                |
    \*-----------------------------------------------------*/
    if(!Transfer(RETRO87_CMD_READ_PROFILE, offset, NULL, length, response) || response[4] != length)
    {
        return(false);
    }

    memcpy(readback, &response[10], length);

    if(memcmp(readback, data, length) != 0)
    {
        LOG_WARNING("[%s] Readback mismatch at 0x%03X", name.c_str(), offset);
        return(false);
    }

    memcpy(&profile[offset], data, length);

    return(true);
}

bool EightBitDoRetro87Controller::WriteCustom(const unsigned char* block)
{
    unsigned char response[RETRO87_PACKET_SIZE];
    unsigned int  offset = 0;

    if(!StartConfiguration())
    {
        return(false);
    }

    /*-----------------------------------------------------*\
    | The keyboard needs a prepare command. The vendor      |
    | waits 100 ms between chunks; each chunk is            |
    | acknowledged in about 7 ms, so 20 ms leaves a margin  |
    | and a full update takes about 0.2 s                   |
    \*-----------------------------------------------------*/
    if(!Transfer(RETRO87_CMD_PREPARE_CUSTOM, 0, NULL, 0, response))
    {
        return(false);
    }

    while(offset < RETRO87_CUSTOM_SIZE)
    {
        unsigned char chunk = (RETRO87_CUSTOM_SIZE - offset) > RETRO87_MAX_DATA ? RETRO87_MAX_DATA : (unsigned char)(RETRO87_CUSTOM_SIZE - offset);

        std::this_thread::sleep_for(20ms);

        if(!Transfer(RETRO87_CMD_WRITE_CUSTOM, offset, &block[offset], chunk, response))
        {
            return(false);
        }

        unsigned char accepted = response[4];

        if(accepted == 0 || accepted > chunk)
        {
            LOG_WARNING("[%s] Per-key write at 0x%03X accepted %d bytes", name.c_str(), offset, accepted);
            return(false);
        }

        offset += accepted;
    }

    return(true);
}
