/*
 * Remus: The Reliable USB Formatting Utility for macOS  
 * Device detection and enumeration for macOS
 * Copyright © 2025 Maciej Wałoszczyk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "macos_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/disk.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/usb/USBSpec.h>
#include <DiskArbitration/DiskArbitration.h>

/*
 * Get list of USB storage devices on macOS
 */
bool macos_get_usb_devices(macos_remus_drive drives[], int *num_drives) {
    CFMutableDictionaryRef matching_dict;
    io_iterator_t iter;
    io_object_t media;
    kern_return_t kr;
    int drive_count = 0;
    
    *num_drives = 0;
    
    printf("DEBUG: Starting USB device enumeration\n");
    printf("DEBUG: Running as UID: %d, GID: %d\n", getuid(), getgid());
    
    // Create a matching dictionary for IOMedia objects
    matching_dict = IOServiceMatching(kIOMediaClass);
    if (matching_dict == NULL) {
        printf("Error: Could not create matching dictionary\n");
        return false;
    }
    
    // Add property to match only whole disks (not partitions)
    CFDictionarySetValue(matching_dict, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    
    // Get an iterator for IOMedia objects
    kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching_dict, &iter);
    if (kr != KERN_SUCCESS) {
        printf("Error: Could not get matching services\n");
        return false;
    }
    
    // Iterate through the matching services
    printf("DEBUG: Starting device iteration\n");
    while ((media = IOIteratorNext(iter)) && (drive_count < MAX_DRIVES)) {
        CFMutableDictionaryRef properties = NULL;
        
        kr = IORegistryEntryCreateCFProperties(media, &properties, 
                                               kCFAllocatorDefault, kNilOptions);
        if (kr != KERN_SUCCESS) {
            printf("DEBUG: Could not get properties for media object\n");
            IOObjectRelease(media);
            continue;
        }
        
        // Get the BSD name (device path) first for debugging
        CFStringRef bsd_name = (CFStringRef)CFDictionaryGetValue(properties, 
                                                                 CFSTR(kIOBSDNameKey));
        if (!bsd_name) {
            printf("DEBUG: Media object has no BSD name\n");
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        
        char device_path[256];
        const char *bsd_cstr = CFStringGetCStringPtr(bsd_name, kCFStringEncodingUTF8);
        if (!bsd_cstr) {
            // Try to get string using CFStringGetCString if direct pointer fails
            char temp_buffer[256];
            if (!CFStringGetCString(bsd_name, temp_buffer, sizeof(temp_buffer), kCFStringEncodingUTF8)) {
                printf("DEBUG: Could not convert BSD name to string\n");
                CFRelease(properties);
                IOObjectRelease(media);
                continue;
            }
            snprintf(device_path, sizeof(device_path), "/dev/%s", temp_buffer);
        } else {
            snprintf(device_path, sizeof(device_path), "/dev/%s", bsd_cstr);
        }
        
        printf("DEBUG: Found device: %s\n", device_path);
        // Check if this is a removable device
        CFBooleanRef removable = (CFBooleanRef)CFDictionaryGetValue(properties, 
                                                                    CFSTR(kIOMediaRemovableKey));
        if (!removable || !CFBooleanGetValue(removable)) {
            printf("DEBUG: Device %s is not removable\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        
        printf("DEBUG: Device %s is removable, checking if USB\n", device_path);
        
        // Check if this is a USB device
        if (!macos_is_usb_device(device_path)) {
            printf("DEBUG: Device %s is not USB\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        
        printf("DEBUG: Device %s passed USB check, adding to list\n", device_path);
        // Initialize the drive structure
        memset(&drives[drive_count], 0, sizeof(struct macos_remus_drive));
        
        // Get device properties
        printf("DEBUG: Getting device properties for %s\n", device_path);
        if (!macos_get_device_properties(device_path, &drives[drive_count].props)) {
            printf("DEBUG: Failed to get device properties for %s\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        printf("DEBUG: Successfully got device properties for %s\n", device_path);
        
        // Get device size
        printf("DEBUG: Getting device size for %s\n", device_path);
        drives[drive_count].size = macos_get_device_size(device_path);
        printf("DEBUG: Device size for %s: %llu bytes\n", device_path, drives[drive_count].size);
        
        if (drives[drive_count].size == 0) {
            printf("DEBUG: Device %s has zero size, skipping\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        
        // Set device path
        drives[drive_count].device_path = strdup(device_path);
        
        // Get device name from properties
        CFStringRef device_name = (CFStringRef)CFDictionaryGetValue(properties,
                                                                    CFSTR(kIOBSDNameKey));
        if (device_name) {
            const char *name_cstr = CFStringGetCStringPtr(device_name, kCFStringEncodingUTF8);
            if (name_cstr) {
                drives[drive_count].name = strdup(name_cstr);
            }
        }
        
        // Create display name
        char display_name[512];
        if (drives[drive_count].props.vendor_name[0] && drives[drive_count].props.product_name[0]) {
            snprintf(display_name, sizeof(display_name), "%s %s (%s)",
                     drives[drive_count].props.vendor_name,
                     drives[drive_count].props.product_name,
                     drives[drive_count].name ? drives[drive_count].name : "Unknown");
        } else {
            snprintf(display_name, sizeof(display_name), "USB Storage Device (%s)",
                     drives[drive_count].name ? drives[drive_count].name : "Unknown");
        }
        drives[drive_count].display_name = strdup(display_name);
        
        // Get volume label
        drives[drive_count].label = macos_get_device_label(device_path);
        
        printf("Found USB device: %s (%s) - %.2f GB\n", 
               drives[drive_count].display_name,
               device_path,
               (double)drives[drive_count].size / (1024.0 * 1024.0 * 1024.0));
        
        drive_count++;
        CFRelease(properties);
        IOObjectRelease(media);
    }
    
    IOObjectRelease(iter);
    *num_drives = drive_count;
    return true;
}

/*
 * Check if a device is a USB device
 */
bool macos_is_usb_device(const char *device_path) {
    DASessionRef session;
    DADiskRef disk;
    CFDictionaryRef description;
    bool is_usb = false;
    
    
    session = DASessionCreate(kCFAllocatorDefault);
    if (!session) {
        return false;
    }
    
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, 
                                   strrchr(device_path, '/') + 1);
    if (!disk) {
        CFRelease(session);
        return false;
    }
    
    description = DADiskCopyDescription(disk);
    if (description) {
        CFStringRef bus = (CFStringRef)CFDictionaryGetValue(description, 
                                                            kDADiskDescriptionBusNameKey);
        if (bus) {
            char bus_name[256];
            if (CFStringGetCString(bus, bus_name, sizeof(bus_name), kCFStringEncodingUTF8)) {
                printf("DEBUG: Device %s has bus name: %s\n", device_path, bus_name);
            }
            // Check if the bus name contains "usb" (case insensitive)
            if (bus && (CFStringFind(bus, CFSTR("usb"), kCFCompareCaseInsensitive).location != kCFNotFound ||
                       CFStringFind(bus, CFSTR("USB"), 0).location != kCFNotFound)) {
                is_usb = true;
                printf("DEBUG: Device %s identified as USB\n", device_path);
            } else {
                printf("DEBUG: Device %s NOT identified as USB\n", device_path);
            }
        } else {
            printf("DEBUG: Device %s has no bus information\n", device_path);
        }
        CFRelease(description);
    } else {
    }
    
    CFRelease(disk);
    CFRelease(session);
    
    return is_usb;
}

/*
 * Get device properties (VID, PID, vendor, product names, etc.)
 */
bool macos_get_device_properties(const char *device_path, macos_device_props *props) {
    DASessionRef session;
    DADiskRef disk;
    CFDictionaryRef description;
    bool success = false;
    
    printf("DEBUG: macos_get_device_properties called for %s\n", device_path);
    
    memset(props, 0, sizeof(macos_device_props));
    strncpy(props->device_path, device_path, sizeof(props->device_path) - 1);
    
    session = DASessionCreate(kCFAllocatorDefault);
    if (!session) {
        printf("DEBUG: Could not create DA session for %s\n", device_path);
        return false;
    }
    
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
                                   strrchr(device_path, '/') + 1);
    if (!disk) {
        printf("DEBUG: Could not create DA disk for %s\n", device_path);
        CFRelease(session);
        return false;
    }
    
    description = DADiskCopyDescription(disk);
    if (description) {
        printf("DEBUG: Got disk description for %s\n", device_path);
        // Check if USB
        CFStringRef bus = (CFStringRef)CFDictionaryGetValue(description, 
                                                            kDADiskDescriptionBusNameKey);
        props->is_USB = (bus && CFStringCompare(bus, CFSTR("USB"), 0) == kCFCompareEqualTo);
        
        // Check if removable
        CFBooleanRef removable = (CFBooleanRef)CFDictionaryGetValue(description,
                                                                    kDADiskDescriptionMediaRemovableKey);
        props->is_Removable = (removable && CFBooleanGetValue(removable));
        
        // Get vendor ID and Product ID - these may not be available via DiskArbitration
        // We'll use IOKit directly for USB device properties
        CFStringRef device_path_cf = (CFStringRef)CFDictionaryGetValue(description,
                                                                       kDADiskDescriptionDevicePathKey);
        if (device_path_cf) {
            // For now, set VID/PID to 0 - we'll enhance this later with IOKit
            props->vid = 0;
            props->pid = 0;
        }
        
        // Get vendor and product names from device model/name
        CFStringRef device_model = (CFStringRef)CFDictionaryGetValue(description,
                                                                     kDADiskDescriptionDeviceModelKey);
        if (device_model) {
            CFStringGetCString(device_model, props->product_name,
                              sizeof(props->product_name), kCFStringEncodingUTF8);
        }
        
        // Try to get vendor from device internal name or use generic
        strncpy(props->vendor_name, "USB", sizeof(props->vendor_name) - 1);
        
        success = true;
        printf("DEBUG: Device properties successfully retrieved for %s\n", device_path);
        CFRelease(description);
    } else {
        printf("DEBUG: Could not get disk description for %s\n", device_path);
    }
    
    CFRelease(disk);
    CFRelease(session);
    
    printf("DEBUG: macos_get_device_properties returning %s for %s\n", 
           success ? "true" : "false", device_path);
    return success;
}

/*
 * Get device size in bytes using IOKit (no root privileges needed)
 */
uint64_t macos_get_device_size(const char *device_path) {
    printf("DEBUG: macos_get_device_size called for %s\n", device_path);
    
    // Create DiskArbitration session
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (!session) {
        printf("DEBUG: Failed to create DA session\n");
        return 0;
    }
    
    // Create disk reference from device path
    DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device_path + 5); // Skip "/dev/"
    if (!disk) {
        printf("DEBUG: Failed to create disk reference for %s\n", device_path);
        CFRelease(session);
        return 0;
    }
    
    // Get disk description
    CFDictionaryRef description = DADiskCopyDescription(disk);
    if (!description) {
        printf("DEBUG: Failed to get disk description for %s\n", device_path);
        CFRelease(disk);
        CFRelease(session);
        return 0;
    }
    
    // Get media size
    CFNumberRef media_size = (CFNumberRef)CFDictionaryGetValue(description, kDADiskDescriptionMediaSizeKey);
    uint64_t size = 0;
    
    if (media_size && CFNumberGetValue(media_size, kCFNumberSInt64Type, &size)) {
        printf("DEBUG: Got media size for %s: %llu bytes\n", device_path, size);
    } else {
        printf("DEBUG: Failed to get media size for %s\n", device_path);
        size = 0;
    }
    
    // Cleanup
    CFRelease(description);
    CFRelease(disk);
    CFRelease(session);
    
    return size;
}

/*
 * Get device volume label
 */
char *macos_get_device_label(const char *device_path) {
    DASessionRef session;
    DADiskRef disk;
    CFDictionaryRef description;
    char *label = NULL;
    
    session = DASessionCreate(kCFAllocatorDefault);
    if (!session) return NULL;
    
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
                                   strrchr(device_path, '/') + 1);
    if (!disk) {
        CFRelease(session);
        return NULL;
    }
    
    description = DADiskCopyDescription(disk);
    if (description) {
        CFStringRef volume_name = (CFStringRef)CFDictionaryGetValue(description,
                                                                    kDADiskDescriptionVolumeNameKey);
        if (volume_name) {
            const char *name_cstr = CFStringGetCStringPtr(volume_name, kCFStringEncodingUTF8);
            if (name_cstr && strlen(name_cstr) > 0) {
                label = strdup(name_cstr);
            }
        }
        CFRelease(description);
    }
    
    CFRelease(disk);
    CFRelease(session);
    return label ? label : strdup("NO_LABEL");
}

/*
 * Check if device is removable
 */
bool macos_is_device_removable(const char *device_path) {
    DASessionRef session;
    DADiskRef disk;
    CFDictionaryRef description;
    bool is_removable = false;
    
    session = DASessionCreate(kCFAllocatorDefault);
    if (!session) return false;
    
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
                                   strrchr(device_path, '/') + 1);
    if (!disk) {
        CFRelease(session);
        return false;
    }
    
    description = DADiskCopyDescription(disk);
    if (description) {
        CFBooleanRef removable = (CFBooleanRef)CFDictionaryGetValue(description,
                                                                    kDADiskDescriptionMediaRemovableKey);
        is_removable = (removable && CFBooleanGetValue(removable));
        CFRelease(description);
    }
    
    CFRelease(disk);
    CFRelease(session);
    return is_removable;
}

/*
 * Unmount device before formatting
 */
bool macos_unmount_device(const char *device_path) {
    DASessionRef session;
    DADiskRef disk;
    
    session = DASessionCreate(kCFAllocatorDefault);
    if (!session) return false;
    
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
                                   strrchr(device_path, '/') + 1);
    if (!disk) {
        CFRelease(session);
        return false;
    }
    
    DADiskUnmount(disk, kDADiskUnmountOptionForce, NULL, NULL);
    
    CFRelease(disk);
    CFRelease(session);
    return true;
}

/*
 * Format device using diskutil command
 */
bool macos_format_device(const char *device_path, const char *fs_type, const char *label) {
    char command[1024];
    const char *device_name = strrchr(device_path, '/') + 1;
    
    if (!device_name) return false;
    
    // First unmount the device
    if (!macos_unmount_device(device_path)) {
        printf("Warning: Could not unmount device %s\n", device_path);
    }
    
    // Map common filesystem types
    const char *macos_fs_type = "ExFAT"; // Default
    if (strcmp(fs_type, "FAT32") == 0 || strcmp(fs_type, "fat32") == 0) {
        macos_fs_type = "FAT32";
    } else if (strcmp(fs_type, "NTFS") == 0 || strcmp(fs_type, "ntfs") == 0) {
        macos_fs_type = "NTFS";
    } else if (strcmp(fs_type, "ExFAT") == 0 || strcmp(fs_type, "exfat") == 0) {
        macos_fs_type = "ExFAT";
    }
    
    // Use diskutil to format the device
    if (label && strlen(label) > 0 && strcmp(label, "NO_LABEL") != 0) {
        snprintf(command, sizeof(command), 
                 "diskutil eraseDisk %s \"%s\" %s", 
                 macos_fs_type, label, device_name);
    } else {
        snprintf(command, sizeof(command), 
                 "diskutil eraseDisk %s \"USB_DRIVE\" %s", 
                 macos_fs_type, device_name);
    }
    
    printf("Executing: %s\n", command);
    int result = system(command);
    return (result == 0);
}
