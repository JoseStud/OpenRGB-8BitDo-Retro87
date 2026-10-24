/*---------------------------------------------------------*\
| EightBitDoRetro87Controller.h                             |
|                                                           |
|   Driver for 8BitDo Retro 87 keyboard (2.4 GHz dongle)    |
|                                                           |
|   JoseStud                                    27 Sep 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <mutex>
#include <string>
#include <hidapi.h>
#include "RGBController.h"

/*---------------------------------------------------------*\
| Report layout                                             |
|   Request:  81 04 CMD 00 LEN SUM OFFSET(LE32) DATA...     |
|   Response: 02 04 03 CMD LEN xx OFFSET(LE32) DATA...      |
\*---------------------------------------------------------*/
#define RETRO87_PACKET_SIZE             64
#define RETRO87_REPORT_ID_OUT           0x81
#define RETRO87_REPORT_ID_IN            0x02
#define RETRO87_MAX_DATA                53
#define RETRO87_TIMEOUT_MS              2000

/*---------------------------------------------------------*\
| Commands                                                  |
\*---------------------------------------------------------*/
enum
{
    RETRO87_CMD_WRITE_PROFILE           = 0x01,
    RETRO87_CMD_READ_PROFILE            = 0x02,
    RETRO87_CMD_CONFIG_MODE             = 0x08, /* Sent before writing  */
    RETRO87_CMD_PREPARE_CUSTOM          = 0x0D, /* Before custom writes */
    RETRO87_CMD_WRITE_CUSTOM            = 0x0E,
    RETRO87_CMD_READ_CUSTOM             = 0x0F,
};

/*---------------------------------------------------------*\
| Profile region (1532 bytes)                               |
\*---------------------------------------------------------*/
#define RETRO87_PROFILE_SIZE            0x5FC
#define RETRO87_PROFILE_MARKER          0x20200902
#define RETRO87_PROFILE_ACTIVE          0x024   /* 1 = active       */
#define RETRO87_PROFILE_CURRENT_MODE    0x5CA
#define RETRO87_PROFILE_SOLID           0x5D0   /* 5 bytes          */
#define RETRO87_PROFILE_CYCLE           0x5D5   /* 2 bytes          */
#define RETRO87_PROFILE_COLOR_RIPPLE    0x5D7   /* 3 bytes          */
#define RETRO87_PROFILE_BREATHING       0x5DA   /* 6 bytes          */
#define RETRO87_PROFILE_RIPPLE          0x5E0   /* 6 bytes          */
#define RETRO87_PROFILE_RESONANCE       0x5E6   /* 9 bytes          */
#define RETRO87_PROFILE_STARLIGHT       0x5EF   /* 10 bytes         */
#define RETRO87_PROFILE_CUSTOM_ENABLE   0x5F9

/*---------------------------------------------------------*\
| Custom (per-key) region (283 bytes)                       |
|   brightness, 1, type, speed, count, 4 x 0,               |
|   91 x R G B, 1                                           |
\*---------------------------------------------------------*/
#define RETRO87_CUSTOM_SIZE             0x11B
#define RETRO87_CUSTOM_LED_COUNT        91
#define RETRO87_CUSTOM_TYPE_STATIC      0x01

/*---------------------------------------------------------*\
| Built-in effects (value of RETRO87_PROFILE_CURRENT_MODE)  |
\*---------------------------------------------------------*/
enum
{
    RETRO87_MODE_OFF                    = 0x00,
    RETRO87_MODE_RESONANCE              = 0x01,
    RETRO87_MODE_STARLIGHT              = 0x02,
    RETRO87_MODE_SOLID                  = 0x03,
    RETRO87_MODE_CYCLE                  = 0x04,
    RETRO87_MODE_COLOR_RIPPLE           = 0x05,
    RETRO87_MODE_BREATHING              = 0x06,
    RETRO87_MODE_RIPPLE                 = 0x07,
    RETRO87_MODE_CUSTOM                 = 0xFF, /* OpenRGB only     */
};

/*---------------------------------------------------------*\
| Stored speed: 1 is fastest, 10 is slowest                 |
\*---------------------------------------------------------*/
#define RETRO87_SPEED_FASTEST           0x01
#define RETRO87_SPEED_SLOWEST           0x0A
#define RETRO87_SPEED_DEFAULT           0x06
#define RETRO87_BRIGHTNESS_MAX          0xFF

class EightBitDoRetro87Controller
{
public:
    EightBitDoRetro87Controller(hid_device* dev_handle, const char* path, std::string dev_name);
    ~EightBitDoRetro87Controller();

    std::string     GetDeviceLocation();
    std::string     GetDeviceName();
    std::string     GetSerialString();

    void            SetConnection(std::string connection_name);
    std::string     GetConnection();

    bool            IsProfileRead();
    bool            IsProfileValid();
    bool            IsProfileActive();
    unsigned char   GetCurrentMode();
    unsigned char*  GetProfile();
    bool            ReadCustom(unsigned char* block);

    void            SetMode(unsigned char mode_value, unsigned char brightness, unsigned char speed, RGBColor color, RGBColor echo_color);
    void            SetCustom(unsigned char brightness, RGBColor* led_colors);

private:
    hid_device*     dev;
    std::string     location;
    std::string     name;
    bool            profile_read;
    std::string     connection;
    std::mutex      transfer_mutex;
    unsigned char   profile[RETRO87_PROFILE_SIZE];
    unsigned char   custom[RETRO87_CUSTOM_SIZE];    /* Last per-key block read or written */
    bool            custom_known;

    bool            Transfer(unsigned char command, unsigned int offset, const unsigned char* data, unsigned char length, unsigned char* response);
    bool            Read(unsigned char command, unsigned char* buf, unsigned int size);
    bool            WriteProfile(unsigned int offset, const unsigned char* data, unsigned char length);
    bool            WriteCustom(const unsigned char* block);
    bool            StartConfiguration();
};
