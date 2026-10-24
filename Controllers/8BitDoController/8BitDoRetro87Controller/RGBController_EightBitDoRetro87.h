/*---------------------------------------------------------*\
| RGBController_EightBitDoRetro87.h                         |
|                                                           |
|   RGBController for 8BitDo Retro 87 keyboard              |
|                                                           |
|   JoseStud                                    27 Sep 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "EightBitDoRetro87Controller.h"
#include "HIDLampArrayController.h"
#include "RGBController.h"

class RGBController_EightBitDoRetro87 : public RGBController
{
public:
    RGBController_EightBitDoRetro87(EightBitDoRetro87Controller* controller_ptr, HIDLampArrayController* lamps_ptr = nullptr);
    ~RGBController_EightBitDoRetro87();

    void        SetupZones();

    void        DeviceUpdateLEDs();
    void        DeviceUpdateZoneLEDs(int zone);
    void        DeviceUpdateSingleLED(int led);

    void        DeviceUpdateMode();

private:
    EightBitDoRetro87Controller*    controller;

    /*-----------------------------------------------------*\
    | Over the USB cable: HID LampArray for Direct mode     |
    \*-----------------------------------------------------*/
    HIDLampArrayController*         lamps;
    bool                            direct_active;
    int                             last_mode;
    int                             resume_mode;
    mode                            resume_settings;
    std::vector<RGBColor>           resume_colors;

    /*-----------------------------------------------------*\
    | Saved writes (effects, Custom) are coalesced so that  |
    | slider drags or fast SDK clients cannot wear flash    |
    \*-----------------------------------------------------*/
    std::thread                     save_thread;
    std::mutex                      save_mutex;
    std::condition_variable         save_cv;
    bool                            save_running;
    bool                            save_pending;
    mode                            save_settings;
    std::vector<RGBColor>           save_colors;
    std::chrono::steady_clock::time_point last_save;

    void        LoadStoredSettings();
    void        UpdateLamps();
    void        SetAutonomous(bool autonomous);
    void        ScheduleSave();
    void        SaveThread();
    void        Save(const mode& settings, const std::vector<RGBColor>& led_colors);
};
