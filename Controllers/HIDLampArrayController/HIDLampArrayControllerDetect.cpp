/*---------------------------------------------------------*\
| HIDLampArrayControllerDetect.cpp                          |
|                                                           |
|   Detector for HID LampArray Devices                      |
|                                                           |
|   Adam Honse (calcprogrammer1@gmail.com)      26 Mar 2024 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "DetectionManager.h"
#include "HIDLampArrayController.h"
#include "RGBController.h"
#include "RGBController_HIDLampArray.h"
#include <vector>
#include <hidapi.h>

#ifdef __linux__
#include <libusb.h>
#include "LogManager.h"
#endif

DetectedControllers DetectHIDLampArrayControllers(hid_device_info* info, const std::string& /*name*/)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    dev = hid_open_path(info->path);

    if(dev)
    {
        HIDLampArrayController* controller = new HIDLampArrayController(dev, info->path, info->vendor_id, info->product_id);

        /*-------------------------------------------------*\
        | Only create the RGBController if there are lamps  |
        | detected in the controller.  Otherwise, delete    |
        | the controller.                                   |
        \*-------------------------------------------------*/
        if(controller->GetLampCount() > 0)
        {
            RGBController_HIDLampArray* rgb_controller = new RGBController_HIDLampArray(controller);

            detected_controllers.push_back(rgb_controller);
        }
        else
        {
            delete controller;
        }
    }

    return(detected_controllers);
}

REGISTER_HID_DETECTOR_PU_ONLY("HID LampArray Device", DetectHIDLampArrayControllers, 0x59, 0x01);

#ifdef __linux__
/*---------------------------------------------------------*\
| Linux usbhid only binds HID interfaces that have an       |
| interrupt IN endpoint.  LampArray interfaces with only an |
| OUT endpoint (e.g. the wired 8BitDo Retro 87 keyboard)    |
| get no hidraw node, so hidapi cannot see them.  Find such |
| unbound interfaces with libusb and check their report     |
| descriptor for a LampArray application collection.       |
\*---------------------------------------------------------*/
static bool IsLampArrayDescriptor(const unsigned char* descriptor, int size)
{
    unsigned int usage_page = 0;
    unsigned int usage      = 0;
    int          pos        = 0;

    while(pos < size)
    {
        unsigned char key       = descriptor[pos];
        int           data_len  = (key & 0x03) == 3 ? 4 : (key & 0x03);
        unsigned int  value     = 0;

        if(key == 0xFE || pos + 1 + data_len > size)
        {
            break;
        }

        for(int byte_idx = 0; byte_idx < data_len; byte_idx++)
        {
            value |= descriptor[pos + 1 + byte_idx] << (8 * byte_idx);
        }

        switch(key & 0xFC)
        {
            case 0x04:
                usage_page = value;
                break;

            case 0x08:
                usage = (data_len == 4) ? (value & 0xFFFF) : value;
                if(data_len == 4)
                {
                    usage_page = value >> 16;
                }
                break;

            case 0xA0:
                if(value == 0x01 && usage_page == 0x59 && usage == 0x01)
                {
                    return(true);
                }
                break;
        }

        pos += 1 + data_len;
    }

    return(false);
}

std::vector<HIDLampArrayController*> OpenHIDLampArrayUSBControllers(unsigned short vid, unsigned short pid, bool exclude)
{
    std::vector<HIDLampArrayController*> controllers;
    libusb_context*                      raw_context = NULL;

    if(libusb_init(&raw_context) < 0)
    {
        return(controllers);
    }

    /*-----------------------------------------------------*\
    | Controllers keep the context alive while they use it  |
    \*-----------------------------------------------------*/
    std::shared_ptr<libusb_context> context(raw_context, libusb_exit);

    libusb_device** devices;
    ssize_t         device_count = libusb_get_device_list(raw_context, &devices);

    for(ssize_t device_idx = 0; device_idx < device_count; device_idx++)
    {
        libusb_device*              device = devices[device_idx];
        libusb_device_descriptor    device_descriptor;
        libusb_config_descriptor*   config;

        if(libusb_get_device_descriptor(device, &device_descriptor) != 0)
        {
            continue;
        }

        bool match = (device_descriptor.idVendor == vid && device_descriptor.idProduct == pid);

        if(match == exclude || libusb_get_active_config_descriptor(device, &config) != 0)
        {
            continue;
        }

        for(uint8_t interface_idx = 0; interface_idx < config->bNumInterfaces; interface_idx++)
        {
            if(config->interface[interface_idx].num_altsetting < 1)
            {
                continue;
            }

            const libusb_interface_descriptor* interface = &config->interface[interface_idx].altsetting[0];
            bool                               has_in    = false;

            for(uint8_t endpoint_idx = 0; endpoint_idx < interface->bNumEndpoints; endpoint_idx++)
            {
                if(interface->endpoint[endpoint_idx].bEndpointAddress & LIBUSB_ENDPOINT_IN)
                {
                    has_in = true;
                }
            }

            /*---------------------------------------------*\
            | Interfaces with an IN endpoint are handled by |
            | usbhid and the hidapi detector                |
            \*---------------------------------------------*/
            if(interface->bInterfaceClass != LIBUSB_CLASS_HID || has_in)
            {
                continue;
            }

            libusb_device_handle* handle = NULL;

            if(libusb_open(device, &handle) != 0)
            {
                continue;
            }

            unsigned char report_descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
            int           size = -1;

            int claim = LIBUSB_ERROR_BUSY;

            if(libusb_kernel_driver_active(handle, interface->bInterfaceNumber) == 0)
            {
                claim = libusb_claim_interface(handle, interface->bInterfaceNumber);

                if(claim == LIBUSB_ERROR_BUSY)
                {
                    LOG_WARNING("[HID LampArray] USB device %04X:%04X interface %d is in use by another program; not added",
                                device_descriptor.idVendor, device_descriptor.idProduct, interface->bInterfaceNumber);
                }
            }

            if(claim == 0)
            {
                size = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
                                               LIBUSB_REQUEST_GET_DESCRIPTOR, 0x22 << 8, interface->bInterfaceNumber,
                                               report_descriptor, sizeof(report_descriptor), 1000);

                if(size <= 0 || !IsLampArrayDescriptor(report_descriptor, size))
                {
                    libusb_release_interface(handle, interface->bInterfaceNumber);
                    size = -1;
                }
            }

            if(size <= 0)
            {
                libusb_close(handle);
                continue;
            }

            uint8_t     ports[8];
            int         port_count = libusb_get_port_numbers(device, ports, sizeof(ports));
            std::string path       = std::to_string(libusb_get_bus_number(device)) + "-";

            for(int port_idx = 0; port_idx < port_count; port_idx++)
            {
                path += (port_idx ? "." : "") + std::to_string(ports[port_idx]);
            }

            path += ":" + std::to_string(config->bConfigurationValue) + "." + std::to_string(interface->bInterfaceNumber);

            /*---------------------------------------------*\
            | The controller owns the handle and releases   |
            | the interface when it is deleted              |
            \*---------------------------------------------*/
            HIDLampArrayController* controller = new HIDLampArrayController(context, handle, interface->bInterfaceNumber, path,
                                                                            device_descriptor.idVendor, device_descriptor.idProduct);

            if(controller->GetLampCount() > 0)
            {
                controllers.push_back(controller);
            }
            else
            {
                delete controller;
            }
        }

        libusb_free_config_descriptor(config);
    }

    if(device_count >= 0)
    {
        libusb_free_device_list(devices, 1);
    }

    return(controllers);
}

DetectedControllers DetectHIDLampArrayUSBControllers()
{
    DetectedControllers detected_controllers;

    /*-----------------------------------------------------*\
    | The wired 8BitDo Retro 87's LampArray is part of its  |
    | 8BitDo Retro 87 device (Direct mode)                  |
    \*-----------------------------------------------------*/
    for(HIDLampArrayController* controller : OpenHIDLampArrayUSBControllers(0x2DC8, 0x2028, true))
    {
        detected_controllers.push_back(new RGBController_HIDLampArray(controller));
    }

    return(detected_controllers);
}

REGISTER_DETECTOR("HID LampArray Device (USB)", DetectHIDLampArrayUSBControllers);
#endif
