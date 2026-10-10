/*---------------------------------------------------------*\
| RGBController_EightBitDoRetro87.cpp                       |
|                                                           |
|   RGBController for 8BitDo Retro 87 keyboard              |
|                                                           |
|   JoseStud                                    27 Sep 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "KeyboardLayoutManager.h"
#include "LogManager.h"
#include "RGBController_EightBitDoRetro87.h"

/**------------------------------------------------------------------*\
    @name 8BitDo Retro 87
    @category Keyboard
    @type USB
    @save :white_check_mark:
    @direct :white_check_mark:
    @effects :white_check_mark:
    @detectors DetectEightBitDoRetro87
    @comment Tested with the Mecha BREAK edition over its 2.4 GHz
        dongle. The keyboard only uses stored brightness, speed and
        colors while its profile is active (Profile button). Stored
        per-key updates are slow and saved to flash, so Direct mode
        is only available over the USB cable, where it uses the
        keyboard's HID LampArray interface (Linux: via libusb).
\*-------------------------------------------------------------------*/

#define RETRO87_SPACE_LED               3
#define RETRO87_SPACE_LED_LAST          7
#define RETRO87_NO_LED                  0   /* ISO keys, removed by ANSI layout */

#define RETRO87_MODE_DIRECT             0x100   /* OpenRGB only, LampArray */

/*---------------------------------------------------------*\
| At most one saved write (effect or Custom) per interval;  |
| every Custom write erases flash on the keyboard           |
\*---------------------------------------------------------*/
#define RETRO87_SAVE_INTERVAL           std::chrono::milliseconds(1000)

/*---------------------------------------------------------*\
| LampArray lamp ID for each configuration LED index        |
\*---------------------------------------------------------*/
static const unsigned short retro87_lamp_ids[RETRO87_CUSTOM_LED_COUNT] =
{
    76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 50, 51, 52, 53,
    54, 55, 56, 57, 58, 59, 60, 61, 62, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,  0,  1,  2,  3,  4,
     5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

static const char* KEY_EN_RETRO87_A = "Key: A (Super button)";
static const char* KEY_EN_RETRO87_B = "Key: B (Super button)";

/*---------------------------------------------------------------------*\
|  Retro 87 KLM layout: values are LED indices in the per-key block    |
\*---------------------------------------------------------------------*/
static const std::vector<unsigned int> retro87_values =
{
    /* ESC          F1    F2    F3    F4    F5    F6    F7    F8    F9   F10   F11   F12   PRSC  SCLK  PSBK */
        75,         76,   77,   78,   79,   80,   81,   82,   83,   84,   85,   86,   87,   88,   89,   90,
    /* BKTK    1     2     3     4     5     6     7     8     9     0     -     =   BSPC  INS   HOME  PGUP */
        58,   59,   60,   61,   62,   63,   64,   65,   66,   67,   68,   69,   70,   71,   72,   73,   74,
    /* TAB     Q     W     E     R     T     Y     U     I     O     P     [     ]     \   DEL    END  PGDN */
        41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,   57,
    /* CPLK    A     S     D     F     G     H     J     K     L     ;     '     #   ENTR                   */
        28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,    RETRO87_NO_LED,   40,
    /* LSFT  ISO\    Z     X     C     V     B     N     M     ,     .     /   RSFT              ARWU       */
        15,    RETRO87_NO_LED,    16,   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,
    /* LCTL  LWIN  LALT               SPC              RALT    A     B   RCTL              ARWL ARWD   ARWR */
         0,    1,    2,                3,                8,    9,   10,   11,               12,   13,   14,
};

static const keyboard_keymap_overlay_values retro87_layout =
{
    KEYBOARD_SIZE::KEYBOARD_SIZE_TKL,
    {
        retro87_values,
        {
            /* Add more regional layout fixes here */
        }
    },
    {
        /*-------------------------------------------------------------------------------------------------------------------------------------*\
        | Edit Keys - the right Fn and Menu positions hold the A and B buttons                                                                  |
        |   Zone,   Row,    Column,     Value,      Name,                       Alternate Name,             OpCode                              |
        \*-------------------------------------------------------------------------------------------------------------------------------------*/
        {   0,      5,      11,         9,          KEY_EN_RETRO87_A,           KEY_EN_UNUSED,              KEYBOARD_OPCODE_SWAP_ONLY,          },
        {   0,      5,      12,         10,         KEY_EN_RETRO87_B,           KEY_EN_UNUSED,              KEYBOARD_OPCODE_SWAP_ONLY,          },
    }
};

static mode CreateMode(const char* name, int value, unsigned int flags, unsigned int colors)
{
    mode new_mode;

    new_mode.name               = name;
    new_mode.value              = value;
    new_mode.flags              = flags | MODE_FLAG_AUTOMATIC_SAVE;
    new_mode.color_mode         = MODE_COLORS_NONE;

    if(flags & MODE_FLAG_HAS_BRIGHTNESS)
    {
        new_mode.brightness_min = 0;
        new_mode.brightness_max = RETRO87_BRIGHTNESS_MAX;
        new_mode.brightness     = RETRO87_BRIGHTNESS_MAX;
    }

    if(flags & MODE_FLAG_HAS_SPEED)
    {
        new_mode.speed_min      = RETRO87_SPEED_SLOWEST;
        new_mode.speed_max      = RETRO87_SPEED_FASTEST;
        new_mode.speed          = RETRO87_SPEED_DEFAULT;
    }

    if(flags & MODE_FLAG_HAS_PER_LED_COLOR)
    {
        new_mode.color_mode     = MODE_COLORS_PER_LED;
    }

    if(colors > 0)
    {
        new_mode.colors_min     = colors;
        new_mode.colors_max     = colors;
        new_mode.color_mode     = MODE_COLORS_MODE_SPECIFIC;
        new_mode.colors.resize(colors, ToRGBColor(0xFF, 0xA5, 0x00));
    }

    return(new_mode);
}

RGBController_EightBitDoRetro87::RGBController_EightBitDoRetro87(EightBitDoRetro87Controller* controller_ptr, HIDLampArrayController* lamps_ptr)
{
    controller          = controller_ptr;
    lamps               = (lamps_ptr && lamps_ptr->GetLampCount() == RETRO87_CUSTOM_LED_COUNT) ? lamps_ptr : nullptr;
    direct_active       = false;
    resume_mode         = -1;

    if(lamps_ptr && !lamps)
    {
        delete lamps_ptr;
    }

    name                = controller->GetDeviceName();
    vendor              = "8BitDo";
    type                = DEVICE_TYPE_KEYBOARD;
    description         = "8BitDo Retro 87 Keyboard Device (" + controller->GetConnection() + ")";
    location            = controller->GetDeviceLocation();
    serial              = controller->GetSerialString();

    const unsigned int color_flags = MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR;

    if(lamps)
    {
        mode Direct;
        Direct.name         = "Direct";
        Direct.value        = RETRO87_MODE_DIRECT;
        Direct.flags        = MODE_FLAG_HAS_PER_LED_COLOR;
        Direct.color_mode   = MODE_COLORS_PER_LED;
        modes.push_back(Direct);
    }

    modes.push_back(CreateMode("Custom",          RETRO87_MODE_CUSTOM,       MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS, 0));
    modes.push_back(CreateMode("Static",          RETRO87_MODE_SOLID,        color_flags,                                             1));
    modes.push_back(CreateMode("Breathing",       RETRO87_MODE_BREATHING,    color_flags | MODE_FLAG_HAS_SPEED,                       1));
    modes.push_back(CreateMode("Spectrum Cycle",  RETRO87_MODE_CYCLE,        MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_SPEED,          0));
    modes.push_back(CreateMode("Rainbow Wave",    RETRO87_MODE_COLOR_RIPPLE, MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_SPEED,          0));
    modes.push_back(CreateMode("Ripple",          RETRO87_MODE_RIPPLE,       color_flags | MODE_FLAG_HAS_SPEED,                       1));
    modes.push_back(CreateMode("Resonance",       RETRO87_MODE_RESONANCE,    color_flags | MODE_FLAG_HAS_SPEED,                       2));
    modes.push_back(CreateMode("Starlight",       RETRO87_MODE_STARLIGHT,    color_flags | MODE_FLAG_HAS_SPEED,                       2));
    modes.push_back(CreateMode("Off",             RETRO87_MODE_OFF,          0,                                                       0));

    SetupZones();

    LoadStoredSettings();

    last_mode       = active_mode;

    save_running    = true;
    save_pending    = false;
    save_thread     = std::thread(&RGBController_EightBitDoRetro87::SaveThread, this);
}

RGBController_EightBitDoRetro87::~RGBController_EightBitDoRetro87()
{
    Shutdown();

    /*-----------------------------------------------------*\
    | Write a pending change, then stop the save thread     |
    \*-----------------------------------------------------*/
    {
        std::lock_guard<std::mutex> lock(save_mutex);
        save_running = false;
    }
    save_cv.notify_all();
    save_thread.join();

    /*-----------------------------------------------------*\
    | Give the lighting back to the keyboard                |
    \*-----------------------------------------------------*/
    if(direct_active)
    {
        SetAutonomous(true);
    }

    delete lamps;
    delete controller;
}

void RGBController_EightBitDoRetro87::SetAutonomous(bool autonomous)
{
    lamps->SetLampArrayControlReport(autonomous ? 1 : 0);
    direct_active = !autonomous;
}

void RGBController_EightBitDoRetro87::UpdateLamps()
{
    LampArrayColor lamp_colors[RETRO87_CUSTOM_LED_COUNT] = {};

    for(unsigned int led_idx = 0; led_idx < leds.size(); led_idx++)
    {
        unsigned int  first = leds[led_idx].value;
        unsigned int  last  = (first == RETRO87_SPACE_LED) ? RETRO87_SPACE_LED_LAST : first;

        for(unsigned int config_idx = first; config_idx <= last && config_idx < RETRO87_CUSTOM_LED_COUNT; config_idx++)
        {
            LampArrayColor& color   = lamp_colors[retro87_lamp_ids[config_idx]];
            color.RedChannel        = RGBGetRValue(colors[led_idx]);
            color.GreenChannel      = RGBGetGValue(colors[led_idx]);
            color.BlueChannel       = RGBGetBValue(colors[led_idx]);
            color.IntensityChannel  = 0xFF;
        }
    }

    for(unsigned short first_lamp = 0; first_lamp < RETRO87_CUSTOM_LED_COUNT; first_lamp += LAMP_MULTI_UPDATE_LAMP_COUNT)
    {
        unsigned short  lamp_ids[LAMP_MULTI_UPDATE_LAMP_COUNT];
        unsigned char   count   = 0;

        for(; count < LAMP_MULTI_UPDATE_LAMP_COUNT && first_lamp + count < RETRO87_CUSTOM_LED_COUNT; count++)
        {
            lamp_ids[count] = first_lamp + count;
        }

        bool last = (first_lamp + count >= RETRO87_CUSTOM_LED_COUNT);

        lamps->SetLampMultiUpdateReport(count, last ? LAMP_UPDATE_FLAG_UPDATE_COMPLETE : 0, lamp_ids, &lamp_colors[first_lamp]);
    }
}

void RGBController_EightBitDoRetro87::SetupZones()
{
    KeyboardLayoutManager new_kb(KEYBOARD_LAYOUT_ANSI_QWERTY, retro87_layout.base_size, retro87_layout.key_values);
    new_kb.ChangeKeys(retro87_layout.edit_keys);

    zone keyboard_zone;

    keyboard_zone.name                      = "Keyboard";
    keyboard_zone.type                      = ZONE_TYPE_MATRIX;
    keyboard_zone.leds_count                = new_kb.GetKeyCount();
    keyboard_zone.leds_min                  = keyboard_zone.leds_count;
    keyboard_zone.leds_max                  = keyboard_zone.leds_count;
    keyboard_zone.matrix_map                = new_kb.GetKeyMap(KEYBOARD_MAP_FILL_TYPE_COUNT);

    zones.push_back(keyboard_zone);

    for(unsigned int led_idx = 0; led_idx < keyboard_zone.leds_count; led_idx++)
    {
        led new_led;

        new_led.name                = new_kb.GetKeyNameAt(led_idx);
        new_led.value               = new_kb.GetKeyValueAt(led_idx);

        leds.push_back(new_led);
    }

    SetupColors();
}

void RGBController_EightBitDoRetro87::LoadStoredSettings()
{
    /*-----------------------------------------------------*\
    | Show the effect and settings stored on the keyboard   |
    \*-----------------------------------------------------*/
    unsigned char* profile = controller->GetProfile();
    unsigned char  current = controller->GetCurrentMode();

    for(unsigned int mode_idx = 0; mode_idx < modes.size(); mode_idx++)
    {
        if(modes[mode_idx].value == current)
        {
            active_mode = mode_idx;
        }
    }

    unsigned char block[RETRO87_CUSTOM_SIZE];

    if(controller->ReadCustom(block) && block[1] == 1)
    {
        for(unsigned int led_idx = 0; led_idx < leds.size(); led_idx++)
        {
            unsigned int idx = 9 + leds[led_idx].value * 3;

            colors[led_idx] = ToRGBColor(block[idx], block[idx + 1], block[idx + 2]);
        }

        for(unsigned int mode_idx = 0; mode_idx < modes.size(); mode_idx++)
        {
            if(modes[mode_idx].value == RETRO87_MODE_CUSTOM)
            {
                modes[mode_idx].brightness = block[0];
            }
        }
    }

    for(unsigned int mode_idx = 0; mode_idx < modes.size(); mode_idx++)
    {
        unsigned int offset    = 0;
        int          color_idx = -1;

        switch(modes[mode_idx].value)
        {
            case RETRO87_MODE_SOLID:        offset = RETRO87_PROFILE_SOLID;        color_idx = 2; break;
            case RETRO87_MODE_CYCLE:        offset = RETRO87_PROFILE_CYCLE;                       break;
            case RETRO87_MODE_COLOR_RIPPLE: offset = RETRO87_PROFILE_COLOR_RIPPLE;                break;
            case RETRO87_MODE_BREATHING:    offset = RETRO87_PROFILE_BREATHING;    color_idx = 3; break;
            case RETRO87_MODE_RIPPLE:       offset = RETRO87_PROFILE_RIPPLE;       color_idx = 3; break;
            case RETRO87_MODE_RESONANCE:    offset = RETRO87_PROFILE_RESONANCE;    color_idx = 3; break;
            case RETRO87_MODE_STARLIGHT:    offset = RETRO87_PROFILE_STARLIGHT;    color_idx = 4; break;
            default:                                                                               continue;
        }

        /*-------------------------------------------------*\
        | 0xFF brightness with 0xFF speed means unset       |
        \*-------------------------------------------------*/
        if(profile[offset] == 0xFF && profile[offset + 1] == 0xFF)
        {
            continue;
        }

        modes[mode_idx].brightness = profile[offset];

        if((modes[mode_idx].flags & MODE_FLAG_HAS_SPEED) && profile[offset + 1] >= RETRO87_SPEED_FASTEST && profile[offset + 1] <= RETRO87_SPEED_SLOWEST)
        {
            modes[mode_idx].speed = profile[offset + 1];
        }

        for(unsigned int color = 0; color < modes[mode_idx].colors.size(); color++)
        {
            unsigned int idx = offset + color_idx + color * 3;

            modes[mode_idx].colors[color] = ToRGBColor(profile[idx], profile[idx + 1], profile[idx + 2]);
        }
    }
}

void RGBController_EightBitDoRetro87::DeviceUpdateLEDs()
{
    if(modes[active_mode].value == RETRO87_MODE_DIRECT)
    {
        if(direct_active)
        {
            UpdateLamps();
        }
        return;
    }

    if(modes[active_mode].value != RETRO87_MODE_CUSTOM)
    {
        return;
    }

    ScheduleSave();
}

void RGBController_EightBitDoRetro87::ScheduleSave()
{
    /*-----------------------------------------------------*\
    | Keep only the latest request; the save thread writes  |
    | it at once, or when the save interval has passed      |
    \*-----------------------------------------------------*/
    {
        std::lock_guard<std::mutex> lock(save_mutex);
        save_settings   = modes[active_mode];
        save_colors     = colors;
        save_pending    = true;
    }
    save_cv.notify_all();
}

void RGBController_EightBitDoRetro87::SaveThread()
{
    std::unique_lock<std::mutex> lock(save_mutex);

    while(true)
    {
        save_cv.wait(lock, [this]{ return(save_pending || !save_running); });

        if(!save_pending)
        {
            break;
        }

        /*-------------------------------------------------*\
        | Wait out the interval (new requests replace the   |
        | pending one); on shutdown, write immediately      |
        \*-------------------------------------------------*/
        save_cv.wait_until(lock, last_save + RETRO87_SAVE_INTERVAL, [this]{ return(!save_running); });

        mode                  settings   = save_settings;
        std::vector<RGBColor> led_colors = save_colors;
        save_pending                     = false;

        lock.unlock();
        Save(settings, led_colors);
        lock.lock();

        last_save = std::chrono::steady_clock::now();
    }
}

void RGBController_EightBitDoRetro87::Save(const mode& settings, const std::vector<RGBColor>& led_colors)
{
    LOG_DEBUG("[%s] Saving mode %s (brightness %d)", name.c_str(), settings.name.c_str(), settings.brightness);

    if(settings.value == RETRO87_MODE_CUSTOM)
    {
        RGBColor custom_colors[RETRO87_CUSTOM_LED_COUNT] = {};

        for(unsigned int led_idx = 0; led_idx < leds.size() && led_idx < led_colors.size(); led_idx++)
        {
            custom_colors[leds[led_idx].value] = led_colors[led_idx];

            /*---------------------------------------------*\
            | The space bar has five LEDs                   |
            \*---------------------------------------------*/
            if(leds[led_idx].value == RETRO87_SPACE_LED)
            {
                for(unsigned int space_idx = RETRO87_SPACE_LED; space_idx <= RETRO87_SPACE_LED_LAST; space_idx++)
                {
                    custom_colors[space_idx] = led_colors[led_idx];
                }
            }
        }

        controller->SetCustom(settings.brightness, custom_colors);
        return;
    }

    RGBColor     color      = settings.colors.size() > 0 ? settings.colors[0] : 0;
    RGBColor     echo_color = settings.colors.size() > 1 ? settings.colors[1] : 0;
    unsigned int speed      = settings.speed;

    /*-----------------------------------------------------*\
    | Never store a speed outside the range the keyboard    |
    | accepts                                               |
    \*-----------------------------------------------------*/
    if(speed < RETRO87_SPEED_FASTEST || speed > RETRO87_SPEED_SLOWEST)
    {
        speed = RETRO87_SPEED_DEFAULT;
    }

    controller->SetMode(settings.value, settings.brightness, speed, color, echo_color);
}

void RGBController_EightBitDoRetro87::DeviceUpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_EightBitDoRetro87::DeviceUpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_EightBitDoRetro87::DeviceUpdateMode()
{
    mode& current   = modes[active_mode];
    int   previous  = last_mode;

    last_mode       = active_mode;

    if(current.value == RETRO87_MODE_DIRECT)
    {
        if(!direct_active)
        {
            /*---------------------------------------------*\
            | Remember the stored effect to return to       |
            \*---------------------------------------------*/
            resume_mode     = previous;
            resume_settings = modes[previous];
            resume_colors   = colors;

            SetAutonomous(false);
        }

        UpdateLamps();
        return;
    }

    if(direct_active)
    {
        SetAutonomous(true);

        /*-------------------------------------------------*\
        | Back to the unchanged stored effect: the keyboard |
        | still has it, so nothing needs to be saved        |
        \*-------------------------------------------------*/
        const mode& stored = resume_settings;

        if(active_mode == resume_mode && current.brightness == stored.brightness && current.speed == stored.speed
        && current.colors == stored.colors)
        {
            if(current.value == RETRO87_MODE_CUSTOM)
            {
                colors = resume_colors;
            }
            return;
        }

        if(current.value == RETRO87_MODE_CUSTOM)
        {
            colors = resume_colors;
        }
    }

    ScheduleSave();
}
