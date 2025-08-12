/*
 * Rufus macOS: The Reliable USB Formatting Utility for macOS
 * Device detection and enumeration for macOS
 * Copyright © 2025 Maciej Wałoszczyk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MACOS_DEVICE_H
#define MACOS_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>

#define MAX_DRIVES 64
#define USB_SPEED_UNKNOWN           0
#define USB_SPEED_LOW               1
#define USB_SPEED_FULL              2
#define USB_SPEED_HIGH              3
#define USB_SPEED_SUPER             4
#define USB_SPEED_SUPER_PLUS        5
#define USB_SPEED_MAX               6

/* macOS equivalent of Windows usb_device_props */
typedef struct macos_device_props {
    uint32_t  vid;
    uint32_t  pid;
    uint32_t  speed;
    uint32_t  port;
    bool      is_USB;
    bool      is_Removable;
    bool      is_CARD;
    char      device_path[256];
    char      device_name[256];
    char      vendor_name[128];
    char      product_name[128];
} macos_device_props;

/* macOS equivalent of Windows RUFUS_DRIVE */
typedef struct macos_rufus_drive {
    uint64_t size;
    char *device_path;
    char *name;
    char *display_name;
    char *label;
    int partition_type;
    bool has_protective_mbr;
    macos_device_props props;
} macos_rufus_drive;

/* Function declarations */
bool macos_get_usb_devices(macos_rufus_drive drives[], int *num_drives);
bool macos_is_usb_device(const char *device_path);
bool macos_get_device_properties(const char *device_path, macos_device_props *props);
uint64_t macos_get_device_size(const char *device_path);
char *macos_get_device_label(const char *device_path);
bool macos_is_device_removable(const char *device_path);
bool macos_unmount_device(const char *device_path);
bool macos_format_device(const char *device_path, const char *fs_type, const char *label);

#endif // MACOS_DEVICE_H
