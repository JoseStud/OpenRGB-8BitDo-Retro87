/*---------------------------------------------------------*\
| EightBitDoRetro87ControllerDetect.cpp                     |
|                                                           |
|   Detector for 8BitDo Retro 87 keyboard                   |
|                                                           |
|   JoseStud                                    27 Sep 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <hidapi.h>
#include "DetectionManager.h"
#include "LogManager.h"
#include "EightBitDoRetro87Controller.h"
#include "RGBController_EightBitDoRetro87.h"

/*---------------------------------------------------------*\
| 8BitDo vendor ID                                          |
\*---------------------------------------------------------*/
#define EIGHTBITDO_VID                              0x2DC8

/*---------------------------------------------------------*\
| 8BitDo keyboard product IDs                               |
\*---------------------------------------------------------*/
#define EIGHTBITDO_RETRO87_DONGLE_PID               0x202E
#define EIGHTBITDO_RETRO87_WIRED_PID                0x2028

DetectedControllers DetectEightBitDoRetro87(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    dev = hid_open_path(info->path);

    if(dev)
    {
        EightBitDoRetro87Controller* controller = new EightBitDoRetro87Controller(dev, info->path, name);

        controller->SetConnection(info->product_id == EIGHTBITDO_RETRO87_WIRED_PID ? "USB cable" : "2.4 GHz dongle");

        /*-------------------------------------------------*\
        | The dongle enumerates even when the keyboard is   |
        | off or connected by USB cable, where it cannot    |
        | reach it.  Only add the keyboard if it answered.  |
        \*-------------------------------------------------*/
        if(controller->IsProfileRead())
        {
            HIDLampArrayController* lamps = nullptr;

#ifdef __linux__
            /*---------------------------------------------*\
            | Over the cable, the keyboard's LampArray       |
            | interface provides Direct mode                 |
            \*---------------------------------------------*/
            if(info->product_id == EIGHTBITDO_RETRO87_WIRED_PID)
            {
                std::vector<HIDLampArrayController*> found = OpenHIDLampArrayUSBControllers(EIGHTBITDO_VID, EIGHTBITDO_RETRO87_WIRED_PID, false);

                for(std::size_t found_idx = 0; found_idx < found.size(); found_idx++)
                {
                    if(found_idx == 0)
                    {
                        lamps = found[found_idx];
                    }
                    else
                    {
                        delete found[found_idx];
                    }
                }
            }
#endif

            detected_controllers.push_back(new RGBController_EightBitDoRetro87(controller, lamps));
        }
        else
        {
            LOG_INFO("[%s] Keyboard not reachable through the dongle (switched off, or connected by USB cable); not added", name.c_str());
            delete controller;
        }
    }

    return(detected_controllers);
}

/*---------------------------------------------------------*\
| Configuration interface: 2, usage page 0xFFA0, usage 1    |
\*---------------------------------------------------------*/
REGISTER_HID_DETECTOR_IPU("8BitDo Retro 87 Mecha BREAK", DetectEightBitDoRetro87, EIGHTBITDO_VID, EIGHTBITDO_RETRO87_DONGLE_PID, 2, 0xFFA0, 0x01);

/*---------------------------------------------------------*\
| The keyboard's own configuration interface over the USB   |
| cable uses the same protocol.  While the cable is in, the |
| dongle no longer reaches the keyboard.  Per-key streaming |
| over the cable is the separate HID LampArray device.      |
\*---------------------------------------------------------*/
REGISTER_HID_DETECTOR_IPU("8BitDo Retro 87 Mecha BREAK (USB)", DetectEightBitDoRetro87, EIGHTBITDO_VID, EIGHTBITDO_RETRO87_WIRED_PID, 2, 0xFFA0, 0x01);
