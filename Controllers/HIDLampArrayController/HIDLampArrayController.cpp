/*---------------------------------------------------------*\
| HIDLampArrayController.cpp                                |
|                                                           |
|   Driver for HID LampArray Devices                        |
|                                                           |
|   Adam Honse (calcprogrammer1@gmail.com)      26 Mar 2024 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include "hid_util.h"
#include "HIDLampArrayController.h"
#include "StringUtils.h"

#ifdef __linux__
#include <libusb.h>

#define HID_LAMPARRAY_USB_TIMEOUT   1000
#define HID_REQUEST_GET_REPORT      0x01
#define HID_REQUEST_SET_REPORT      0x09
#define HID_REPORT_TYPE_FEATURE     0x03
#define HID_DESCRIPTOR_TYPE_REPORT  0x22
#endif

/*---------------------------------------------------------*\
| 8BitDo Retro 87 Keyboard X (wired): the firmware reports  |
| some X positions 10x or 100x too large, the right arrow   |
| at (0, 0), backspace with the backslash usage, and no     |
| usage for pause, four of the five space bar lamps and the |
| A and B keys.                                             |
\*---------------------------------------------------------*/
#define EIGHTBITDO_VID                  0x2DC8
#define EIGHTBITDO_RETRO87_WIRED_PID    0x2028
#define EIGHTBITDO_RETRO87_LAMP_COUNT   91

HIDLampArrayController::HIDLampArrayController(hid_device *dev_handle, const char *path, unsigned short vid, unsigned short pid)
{
    dev                                 = dev_handle;
    location                            = "HID: " + std::string(path);
    vendor_id                           = vid;
    product_id                          = pid;
#ifdef __linux__
    usb_dev                             = NULL;
    usb_interface                       = -1;
#endif

    Initialize();
}

#ifdef __linux__
static std::string GetUSBString(libusb_device_handle *handle, uint8_t index)
{
    unsigned char string[128];

    if(index == 0 || libusb_get_string_descriptor_ascii(handle, index, string, sizeof(string)) < 0)
    {
        return("");
    }

    return(std::string((const char *)string));
}

HIDLampArrayController::HIDLampArrayController(std::shared_ptr<libusb_context> context, libusb_device_handle *usb_handle, int interface_number, const std::string& path, unsigned short vid, unsigned short pid)
{
    dev                                 = NULL;
    location                            = "USB: " + path;
    vendor_id                           = vid;
    product_id                          = pid;
    usb_context                         = context;
    usb_dev                             = usb_handle;
    usb_interface                       = interface_number;

    libusb_device_descriptor descriptor;

    if(libusb_get_device_descriptor(libusb_get_device(usb_dev), &descriptor) == 0)
    {
        usb_name                        = GetUSBString(usb_dev, descriptor.iProduct);
        usb_serial                      = GetUSBString(usb_dev, descriptor.iSerialNumber);
        usb_vendor                      = GetUSBString(usb_dev, descriptor.iManufacturer);
    }

    Initialize();
}
#endif

void HIDLampArrayController::Initialize()
{
    memset(&ids, 0, sizeof(ids));
    memset(&LampArray, 0, sizeof(LampArray));

    /*-----------------------------------------------------*\
    | Parse report IDs from descriptor                      |
    \*-----------------------------------------------------*/
    unsigned int  curr_collection_usage = 0;
    unsigned char data_len              = 0;
    unsigned char key                   = 0;
    unsigned char key_cmd               = 0;
    unsigned char key_size              = 0;
    unsigned int  pos                   = 0;
    unsigned char report_descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
    unsigned char report_id             = 0;
    int           size                  = GetReportDescriptor(report_descriptor, sizeof(report_descriptor));
    unsigned int  usage                 = 0;
    unsigned char usage_page            = 0;

    /*-----------------------------------------------------*\
    | Create a list of usages and their report IDs          |
    \*-----------------------------------------------------*/
    std::unordered_map<unsigned int, unsigned int> usage_to_report_id;

    /*-----------------------------------------------------*\
    | A failed descriptor read returns -1; treated as a     |
    | length it walks off the end of the buffer.            |
    \*-----------------------------------------------------*/
    if(size < 0)
    {
        LampArray.LampCount     = 0;
        LampArray.LampArrayKind = HID_LAMPARRAY_KIND_UNDEFINED;
        return;
    }

    while(pos < (unsigned int)size)
    {
        get_hid_item_size(report_descriptor, size, pos, &data_len, &key_size);

        key                             = report_descriptor[pos];
        key_cmd                         = key & 0xFC;

        switch(key)
        {
            case 0xA1:
                curr_collection_usage   = usage;
                if(usage_page == 0x59 && report_id)
                {
                    usage_to_report_id[curr_collection_usage] = report_id;
                }
                break;

            case 0xC0:
                curr_collection_usage   = 0;
                report_id               = 0;
                break;
        }

        switch(key_cmd)
        {
            case 0x4:
                usage_page              = get_hid_report_bytes(report_descriptor, size, data_len, pos);
                break;

            case 0x8:
                usage                   = get_hid_report_bytes(report_descriptor, size, data_len, pos);
                if(data_len == 4)
                {
                    int local_usage_page = usage >> 16;
                    usage &= 0x0000FFFF;
                    if (local_usage_page == 0x59 && report_id)
                    {
                        usage_to_report_id[usage] = report_id;
                    }
                }
                break;

            case 0x84:
                report_id               = get_hid_report_bytes(report_descriptor, size, data_len, pos);
                if (usage_page == 0x59 && curr_collection_usage)
                {
                    usage_to_report_id[curr_collection_usage] = report_id;
                }
                break;
        }
        pos += data_len + key_size;
    }

    /*-----------------------------------------------------*\
    | Get the report IDs for each report                    |
    \*-----------------------------------------------------*/
    for(const std::pair<const unsigned int, unsigned int>& pair : usage_to_report_id)
    {
        switch(pair.first)
        {
            case 0x02:
                ids.LampArrayAttributesReportID     = pair.second;
                break;

            case 0x20:
                ids.LampAttributesRequestReportID   = pair.second;
                break;

            case 0x22:
                ids.LampAttributesResponseReportID  = pair.second;
                break;

            case 0x50:
                ids.LampMultiUpdateReportID         = pair.second;
                break;

            case 0x60:
                ids.LampRangeUpdateReportID         = pair.second;
                break;

            case 0x70:
                ids.LampArrayControlReportID        = pair.second;
                break;
        }
    }

    /*-----------------------------------------------------*\
    | If reports are missing, return                        |
    \*-----------------------------------------------------*/
    bool report_missing                 = (ids.LampArrayAttributesReportID == 0)
                                       || (ids.LampAttributesRequestReportID == 0)
                                       || (ids.LampAttributesResponseReportID == 0)
                                       || (ids.LampMultiUpdateReportID == 0)
                                       || (ids.LampRangeUpdateReportID == 0)
                                       || (ids.LampArrayControlReportID == 0);

    if(report_missing)
    {
        LampArray.LampCount = 0;
        LampArray.LampArrayKind = HID_LAMPARRAY_KIND_UNDEFINED;
        return;
    }

    /*-----------------------------------------------------*\
    | Get LampArrayAttributesReport                         |
    \*-----------------------------------------------------*/
    GetLampArrayAttributesReport();

    /*-----------------------------------------------------*\
    | Set LampAttributesRequestReport for LampId 0          |
    \*-----------------------------------------------------*/
    SetLampAttributesRequestReport(0);

    /*-----------------------------------------------------*\
    | Get LampAttributesResponseReport for each LampId      |
    \*-----------------------------------------------------*/
    for(unsigned int LampId = 0; LampId < LampArray.LampCount; LampId++)
    {
        GetLampAttributesResponseReport();
    }

    ApplyQuirks();
}

void HIDLampArrayController::ApplyQuirks()
{
    if(vendor_id == EIGHTBITDO_VID && product_id == EIGHTBITDO_RETRO87_WIRED_PID
    && Lamps.size() == EIGHTBITDO_RETRO87_LAMP_COUNT)
    {
        for(LampAttributes& lamp : Lamps)
        {
            while(lamp.PositionXInMicrometers > LampArray.BoundingBoxWidthInMicrometers)
            {
                lamp.PositionXInMicrometers /= 10;
            }
        }

        Lamps[90].PositionXInMicrometers    = 360000;       /* Right arrow, after down  */
        Lamps[90].PositionYInMicrometers    = 144000;
        Lamps[29].LampKey                   = 0x2A;         /* Backspace                */
        Lamps[15].LampKey                   = 0x48;         /* Pause                    */

        const unsigned int space_lamps[]    = { 79, 80, 82, 83 };

        for(unsigned int lamp_idx : space_lamps)
        {
            Lamps[lamp_idx].LampKey         = 0x2C;         /* Space                    */
        }

        lamp_names.resize(Lamps.size());
        lamp_names[85]                      = "Key: A (Super button)";
        lamp_names[86]                      = "Key: B (Super button)";
    }
}

HIDLampArrayController::~HIDLampArrayController()
{
    if(dev)
    {
        hid_close(dev);
    }

#ifdef __linux__
    if(usb_dev)
    {
        libusb_release_interface(usb_dev, usb_interface);
        libusb_close(usb_dev);
    }
#endif
}

int HIDLampArrayController::GetReportDescriptor(unsigned char *data, size_t length)
{
#ifdef __linux__
    if(usb_dev)
    {
        return(libusb_control_transfer(usb_dev, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
                                       LIBUSB_REQUEST_GET_DESCRIPTOR, HID_DESCRIPTOR_TYPE_REPORT << 8, usb_interface,
                                       data, (uint16_t)length, HID_LAMPARRAY_USB_TIMEOUT));
    }
#endif

    return(hid_get_report_descriptor(dev, data, length));
}

int HIDLampArrayController::GetFeatureReport(unsigned char *data, size_t length)
{
#ifdef __linux__
    if(usb_dev)
    {
        /*-------------------------------------------------*\
        | As with hidapi, data[0] holds the report ID and   |
        | the returned report starts with it                |
        \*-------------------------------------------------*/
        return(libusb_control_transfer(usb_dev, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                       HID_REQUEST_GET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | data[0], usb_interface,
                                       data, (uint16_t)length, HID_LAMPARRAY_USB_TIMEOUT));
    }
#endif

    return(hid_get_feature_report(dev, data, length));
}

int HIDLampArrayController::SendFeatureReport(const unsigned char *data, size_t length)
{
#ifdef __linux__
    if(usb_dev)
    {
        return(libusb_control_transfer(usb_dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                       HID_REQUEST_SET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | data[0], usb_interface,
                                       (unsigned char *)data, (uint16_t)length, HID_LAMPARRAY_USB_TIMEOUT));
    }
#endif

    return(hid_send_feature_report(dev, data, length));
}

std::string HIDLampArrayController::GetDeviceLocation()
{
    return(location);
}

std::string HIDLampArrayController::GetDeviceName()
{
#ifdef __linux__
    if(usb_dev)
    {
        return(usb_name);
    }
#endif

    wchar_t name_string[128];
    int ret = hid_get_product_string(dev, name_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(name_string));
}

std::string HIDLampArrayController::GetDeviceSerial()
{
#ifdef __linux__
    if(usb_dev)
    {
        return(usb_serial);
    }
#endif

    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(serial_string));
}

std::string HIDLampArrayController::GetDeviceVendor()
{
#ifdef __linux__
    if(usb_dev)
    {
        return(usb_vendor);
    }
#endif

    wchar_t vendor_string[128];
    int ret = hid_get_manufacturer_string(dev, vendor_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(vendor_string));
}

unsigned int HIDLampArrayController::GetLampArrayKind()
{
    return(LampArray.LampArrayKind);
}

unsigned int HIDLampArrayController::GetLampCount()
{
    return(LampArray.LampCount);
}

std::vector<LampAttributes> HIDLampArrayController::GetLamps()
{
    return(Lamps);
}

std::string HIDLampArrayController::GetLampName(unsigned int LampId)
{
    if(LampId < lamp_names.size())
    {
        return(lamp_names[LampId]);
    }

    return("");
}

void HIDLampArrayController::GetLampArrayAttributesReport()
{
    unsigned char   usb_buf[sizeof(LampArrayAttributes) + 1];

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | First byte is the report ID                           |
    \*-----------------------------------------------------*/
    usb_buf[0]          = ids.LampArrayAttributesReportID;

    /*-----------------------------------------------------*\
    | Get the report                                        |
    \*-----------------------------------------------------*/
    GetFeatureReport(usb_buf, sizeof(usb_buf));

    memcpy(&LampArray, &usb_buf[1], sizeof(LampArray));
}

void HIDLampArrayController::GetLampAttributesResponseReport()
{
    unsigned char   usb_buf[65];
    LampAttributes  attributes;

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | First byte is the report ID                           |
    \*-----------------------------------------------------*/
    usb_buf[0]      = ids.LampAttributesResponseReportID;

    /*-----------------------------------------------------*\
    | Get the report                                        |
    \*-----------------------------------------------------*/
    GetFeatureReport(usb_buf, sizeof(usb_buf));

    memcpy(&attributes, &usb_buf[1], sizeof(attributes));

    /*-----------------------------------------------------*\
    | Store the attributes                                  |
    \*-----------------------------------------------------*/
    Lamps.push_back(attributes);
}

void HIDLampArrayController::SetLampArrayControlReport(unsigned char AutonomousMode)
{
    unsigned char   usb_buf[sizeof(LampArrayControl) + 1];

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | First byte is the report ID                           |
    \*-----------------------------------------------------*/
    usb_buf[0] = ids.LampArrayControlReportID;

    /*-----------------------------------------------------*\
    | Fill in control data                                  |
    \*-----------------------------------------------------*/
    ((LampArrayControl *)&usb_buf[1])->AutonomousMode = AutonomousMode;

    /*-----------------------------------------------------*\
    | Send the report                                       |
    \*-----------------------------------------------------*/
    SendFeatureReport(usb_buf, sizeof(usb_buf));
}

void HIDLampArrayController::SetLampAttributesRequestReport(unsigned short LampId)
{
    unsigned char   usb_buf[sizeof(LampAttributesRequest) + 1];

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | First byte is the report ID                           |
    \*-----------------------------------------------------*/
    usb_buf[0] = ids.LampAttributesRequestReportID;

    /*-----------------------------------------------------*\
    | Fill in request data                                  |
    \*-----------------------------------------------------*/
    ((LampAttributesRequest *)&usb_buf[1])->LampId = LampId;

    /*-----------------------------------------------------*\
    | Send the report                                       |
    \*-----------------------------------------------------*/
    SendFeatureReport(usb_buf, sizeof(usb_buf));
}

void HIDLampArrayController::SetLampMultiUpdateReport(unsigned char LampCount, unsigned char LampUpdateFlags, unsigned short * LampIds, LampArrayColor * UpdateColors)
{
    unsigned char   usb_buf[sizeof(LampMultiUpdate) + 1];

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | First byte is the report ID                           |
    \*-----------------------------------------------------*/
    usb_buf[0] = ids.LampMultiUpdateReportID;

    /*-----------------------------------------------------*\
    | Fill in multi update data                             |
    \*-----------------------------------------------------*/
    ((LampMultiUpdate *)&usb_buf[1])->LampCount         = LampCount;
    ((LampMultiUpdate *)&usb_buf[1])->LampUpdateFlags   = LampUpdateFlags;

    for(unsigned char LampId = 0; LampId < LampCount; LampId++)
    {
        ((LampMultiUpdate *)&usb_buf[1])->LampIds[LampId] = LampIds[LampId];
        ((LampMultiUpdate *)&usb_buf[1])->UpdateColors[LampId] = UpdateColors[LampId];
    }

    /*-----------------------------------------------------*\
    | Send the report                                       |
    \*-----------------------------------------------------*/
    SendFeatureReport(usb_buf, sizeof(usb_buf));
}
