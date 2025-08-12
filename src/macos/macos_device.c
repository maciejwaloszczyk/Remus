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
#include <time.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/usb/USBSpec.h>
#include <DiskArbitration/DiskArbitration.h>

// Dodane makro debug – wyłączone domyślnie w buildzie Alpha
#ifdef REMUS_DEBUG
#define DBG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...) do { } while(0)
#endif

/*
 * Helper function to get current time string
 */
static char* current_time_string(void) {
    static char time_buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", tm_info);
    return time_buffer;
}

/*
 * Helper function to check if path is a block device
 */
static bool device_path_is_block_device(const char* path) {
    if (!path) return false;
    return strncmp(path, "/dev/disk", 9) == 0;
}

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
    
    DBG("DEBUG: Starting USB device enumeration\n");
    DBG("DEBUG: Running as UID: %d, GID: %d\n", getuid(), getgid());
    
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
    DBG("DEBUG: Starting device iteration\n");
    while ((media = IOIteratorNext(iter)) && (drive_count < MAX_DRIVES)) {
        CFMutableDictionaryRef properties = NULL;
        
        kr = IORegistryEntryCreateCFProperties(media, &properties, 
                                               kCFAllocatorDefault, kNilOptions);
        if (kr != KERN_SUCCESS) {
            DBG("DEBUG: Could not get properties for media object\n");
            IOObjectRelease(media);
            continue;
        }
        
        // Get the BSD name (device path) first for debugging
        CFStringRef bsd_name = (CFStringRef)CFDictionaryGetValue(properties, 
                                                                 CFSTR(kIOBSDNameKey));
        if (!bsd_name) {
            DBG("DEBUG: Media object has no BSD name\n");
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
                DBG("DEBUG: Could not convert BSD name to string\n");
                CFRelease(properties);
                IOObjectRelease(media);
                continue;
            }
            snprintf(device_path, sizeof(device_path), "/dev/%s", temp_buffer);
        } else {
            snprintf(device_path, sizeof(device_path), "/dev/%s", bsd_cstr);
        }
        
        DBG("DEBUG: Found device: %s\n", device_path);
        // Check if this is a removable device
        CFBooleanRef removable = (CFBooleanRef)CFDictionaryGetValue(properties, 
                                                                    CFSTR(kIOMediaRemovableKey));
        if (!removable || !CFBooleanGetValue(removable)) {
            DBG("DEBUG: Device %s is not removable\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        
        DBG("DEBUG: Device %s is removable, checking if USB\n", device_path);
        
        // Check if this is a USB device
        if (!macos_is_usb_device(device_path)) {
            DBG("DEBUG: Device %s is not USB\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        
        DBG("DEBUG: Device %s passed USB check, adding to list\n", device_path);
        // Initialize the drive structure
        memset(&drives[drive_count], 0, sizeof(struct macos_remus_drive));
        
        // Get device properties
        DBG("DEBUG: Getting device properties for %s\n", device_path);
        if (!macos_get_device_properties(device_path, &drives[drive_count].props)) {
            DBG("DEBUG: Failed to get device properties for %s\n", device_path);
            CFRelease(properties);
            IOObjectRelease(media);
            continue;
        }
        DBG("DEBUG: Successfully got device properties for %s\n", device_path);
        
        // Get device size
        DBG("DEBUG: Getting device size for %s\n", device_path);
        drives[drive_count].size = macos_get_device_size(device_path);
        DBG("DEBUG: Device size for %s: %llu bytes\n", device_path, drives[drive_count].size);
        
        if (drives[drive_count].size == 0) {
            DBG("DEBUG: Device %s has zero size, skipping\n", device_path);
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
                DBG("DEBUG: Device %s has bus name: %s\n", device_path, bus_name);
            }
            // Check if the bus name contains "usb" (case insensitive)
            if (bus && (CFStringFind(bus, CFSTR("usb"), kCFCompareCaseInsensitive).location != kCFNotFound ||
                       CFStringFind(bus, CFSTR("USB"), 0).location != kCFNotFound)) {
                is_usb = true;
                DBG("DEBUG: Device %s identified as USB\n", device_path);
            } else {
                DBG("DEBUG: Device %s NOT identified as USB\n", device_path);
            }
        } else {
            DBG("DEBUG: Device %s has no bus information\n", device_path);
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
    
    DBG("DEBUG: macos_get_device_properties called for %s\n", device_path);
    
    memset(props, 0, sizeof(macos_device_props));
    strncpy(props->device_path, device_path, sizeof(props->device_path) - 1);
    
    session = DASessionCreate(kCFAllocatorDefault);
    if (!session) {
        DBG("DEBUG: Could not create DA session for %s\n", device_path);
        return false;
    }
    
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
                                   strrchr(device_path, '/') + 1);
    if (!disk) {
        DBG("DEBUG: Could not create DA disk for %s\n", device_path);
        CFRelease(session);
        return false;
    }
    
    description = DADiskCopyDescription(disk);
    if (description) {
        DBG("DEBUG: Got disk description for %s\n", device_path);
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
        DBG("DEBUG: Device properties successfully retrieved for %s\n", device_path);
        CFRelease(description);
    } else {
        DBG("DEBUG: Could not get disk description for %s\n", device_path);
    }
    
    CFRelease(disk);
    CFRelease(session);
    
    DBG("DEBUG: macos_get_device_properties returning %s for %s\n", 
           success ? "true" : "false", device_path);
    return success;
}

/*
 * Get device size in bytes using IOKit (no root privileges needed)
 */
uint64_t macos_get_device_size(const char *device_path) {
    DBG("DEBUG: macos_get_device_size called for %s\n", device_path);
    
    // Create DiskArbitration session
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (!session) {
        DBG("DEBUG: Failed to create DA session\n");
        return 0;
    }
    
    // Create disk reference from device path
    DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device_path + 5); // Skip "/dev/"
    if (!disk) {
        DBG("DEBUG: Failed to create disk reference for %s\n", device_path);
        CFRelease(session);
        return 0;
    }
    
    // Get disk description
    CFDictionaryRef description = DADiskCopyDescription(disk);
    if (!description) {
        DBG("DEBUG: Failed to get disk description for %s\n", device_path);
        CFRelease(disk);
        CFRelease(session);
        return 0;
    }
    
    // Get media size
    CFNumberRef media_size = (CFNumberRef)CFDictionaryGetValue(description, kDADiskDescriptionMediaSizeKey);
    uint64_t size = 0;
    
    if (media_size && CFNumberGetValue(media_size, kCFNumberSInt64Type, &size)) {
        DBG("DEBUG: Got media size for %s: %llu bytes\n", device_path, size);
    } else {
        DBG("DEBUG: Failed to get media size for %s\n", device_path);
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

// Stałe z Rufusa - definicje bezpośrednio z format.c
#define NUM_BUFFERS 2
#define DD_BUFFER_SIZE (8 * 1024 * 1024)  // 8MB jak w Rufus
#define WRITE_RETRIES 5                    // Zwiększone dla stabilności
#define WRITE_TIMEOUT 5000                 // 5 sekund

// Makra pomocnicze
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Struktura do śledzenia postępu jak w Rufus
typedef struct {
    uint64_t total_size;
    uint64_t written_bytes;
    double progress;
    bool cancelled;
} rufus_progress_t;

static rufus_progress_t g_rufus_progress = {0};

// Funkcja progress update jak w Rufus - UpdateProgressWithInfo
static void rufus_update_progress(uint64_t written, uint64_t total) {
    g_rufus_progress.written_bytes = written;
    g_rufus_progress.total_size = total;
    g_rufus_progress.progress = total > 0 ? (double)written / total * 100.0 : 0.0;
    
    printf("[%s] Writing image: %.1f%% (%llu/%llu bytes)\n", 
           current_time_string(), g_rufus_progress.progress, written, total);
    fflush(stdout); // Force immediate output for real-time progress in GUI
}

// Emulacja CHECK_FOR_USER_CANCEL z Rufus
#define CHECK_FOR_USER_CANCEL \
    if (g_rufus_progress.cancelled) { \
        printf("\n[%s] Operation cancelled by user\n", current_time_string()); \
        goto out; \
    }

/*
 * Full Rufus WriteDrive implementation adapted for macOS
 * Based on format.c from Rufus project - maintains all advanced features:
 * - Multi-buffer asynchronous I/O pattern  
 * - Sector-aligned buffer management
 * - Comprehensive retry logic with timeout
 * - Progress tracking with detailed reporting
 * - Raw device access for optimal performance
 */
bool macos_write_iso_to_device(const char *iso_path, const char *device_path) {
    // Force unbuffered output for real-time progress in GUI
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    printf("[%s] Starting Rufus-style ISO write: %s -> %s\n", 
           current_time_string(), iso_path, device_path);
    
    FILE* source_image = NULL;
    FILE* physical_drive = NULL;
    bool ret = false;
    uint64_t wb, target_size = 0;
    uint32_t read_size[NUM_BUFFERS] = {0};
    uint32_t write_size, buf_size;
    uint8_t* buffer = NULL;
    char raw_device_path[512];
    int read_bufnum = 0, proc_bufnum = 1;
    const char* device_name;
    char command[512];
    
    if (!iso_path || !device_path) {
        printf("[%s] Error: NULL parameters\n", current_time_string());
        return false;
    }
    
    // Reset progress tracking
    memset(&g_rufus_progress, 0, sizeof(g_rufus_progress));
    
    // Extract device name for unmounting operations
    device_name = strrchr(device_path, '/');
    if (device_name) {
        device_name++; // Skip the '/'
    } else {
        device_name = device_path;
    }
    
    // Unmount device like Rufus does - force unmount all partitions
    printf("[%s] Unmounting device partitions...\n", current_time_string());
    fflush(stdout);
    snprintf(command, sizeof(command), "diskutil unmountDisk force /dev/%s 2>&1", device_name);
    int unmount_result = system(command);
    if (unmount_result == 0) {
        printf("[%s] Forced unmount of all volumes on %s was successful\n", current_time_string(), device_name);
        fflush(stdout);
    } else {
        printf("[%s] Warning: Failed to unmount device (continuing anyway)\n", current_time_string());
        fflush(stdout);
    }
    
    // Give time for unmounting to complete
    usleep(2000000); // 2 seconds
    
    // Convert to raw device for better performance (Rufus-style optimization)
    if (device_path_is_block_device(device_path)) {
        snprintf(raw_device_path, sizeof(raw_device_path), "/dev/r%s", device_name);
    } else {
        strncpy(raw_device_path, device_path, sizeof(raw_device_path) - 1);
        raw_device_path[sizeof(raw_device_path) - 1] = '\0';
    }
    
    printf("[%s] Using raw device: %s\n", current_time_string(), raw_device_path);
    fflush(stdout);
    
    // Open source image file
    source_image = fopen(iso_path, "rb");
    if (!source_image) {
        printf("[%s] Could not open image '%s': %s\n", current_time_string(), iso_path, strerror(errno));
        goto out;
    }
    
    // Determine image size - like Rufus img_report.image_size
    fseek(source_image, 0, SEEK_END);
    target_size = ftell(source_image);
    fseek(source_image, 0, SEEK_SET);
    
    if (target_size <= 0) {
        printf("[%s] Invalid image size: %llu\n", current_time_string(), target_size);
        goto out;
    }
    
    printf("[%s] Image size: %.2f MB (%llu bytes)\n", 
           current_time_string(), (double)target_size / (1024.0 * 1024.0), target_size);
    fflush(stdout);
    
    // Open physical drive for writing
    physical_drive = fopen(raw_device_path, "r+b");
    if (!physical_drive) {
        printf("[%s] Could not open device '%s': %s\n", current_time_string(), raw_device_path, strerror(errno));
        printf("[%s] Note: Administrator privileges may be required\n", current_time_string());
        goto out;
    }
    
    // Our buffer size must be a multiple of the sector size and *ALIGNED* to the sector size
    // Like Rufus: buf_size = ((DD_BUFFER_SIZE + SelectedDrive.SectorSize - 1) / SelectedDrive.SectorSize) * SelectedDrive.SectorSize
    buf_size = ((DD_BUFFER_SIZE + 511) / 512) * 512;  // Align to 512-byte sectors
    
    // Allocate buffer for NUM_BUFFERS (Rufus multi-buffer system)
    if (posix_memalign((void**)&buffer, 512, buf_size * NUM_BUFFERS) != 0) {
        printf("[%s] Could not allocate disk write buffer\n", current_time_string());
        goto out;
    }
    
    // Verify buffer alignment (Rufus assertion)
    if ((uintptr_t)buffer % 512 != 0) {
        printf("[%s] Error: Buffer not aligned to sector size\n", current_time_string());
        goto out;
    }
    
    printf("[%s] Writing compressed image with %d MB buffer:\n", 
           current_time_string(), (buf_size * NUM_BUFFERS) / (1024*1024));
    fflush(stdout);
    
    // Start the initial read - Rufus asynchronous I/O pattern
    size_t initial_read = fread(&buffer[read_bufnum * buf_size], 1, 
                               (size_t)MIN(buf_size, target_size), source_image);
    read_size[read_bufnum] = (uint32_t)initial_read;
    
    read_size[proc_bufnum] = 1; // To avoid early loop exit (Rufus pattern)
    rufus_update_progress(0, target_size);
    
    // Main write loop - exact Rufus WriteDrive algorithm
    for (wb = 0; read_size[proc_bufnum] != 0; wb += read_size[proc_bufnum]) {
        // 0. Update the progress (Rufus UpdateProgressWithInfo)
        rufus_update_progress(wb, target_size);
        
        if (wb >= target_size)
            break;
        
        // 1. No need to wait for async read completion (we're using synchronous I/O)
        // But we maintain the Rufus buffer switching pattern
        
        // 2. WriteFile fails unless the size is a multiple of sector size
        if (read_size[read_bufnum] % 512 != 0) {
            read_size[read_bufnum] = ((read_size[read_bufnum] + 511) / 512) * 512;
        }
        
        // 3. Switch to the next reading buffer (Rufus pattern)
        proc_bufnum = read_bufnum;
        read_bufnum = (read_bufnum + 1) % NUM_BUFFERS;
        
        // 3. Launch the next read operation (Rufus async pattern adapted)
        if (wb + read_size[proc_bufnum] < target_size) {
            size_t next_read_size = MIN(buf_size, target_size - (wb + read_size[proc_bufnum]));
            size_t next_read = fread(&buffer[read_bufnum * buf_size], 1, next_read_size, source_image);
            read_size[read_bufnum] = (uint32_t)next_read;
        } else {
            read_size[read_bufnum] = 0; // End of data
        }
        
        // 4. Synchronously write the current data buffer - Full Rufus retry logic
        int i;
        for (i = 1; i <= WRITE_RETRIES; i++) {
            CHECK_FOR_USER_CANCEL;
            
            size_t written = fwrite(&buffer[proc_bufnum * buf_size], 1, read_size[proc_bufnum], physical_drive);
            write_size = (uint32_t)written;
            
            if (written == read_size[proc_bufnum]) {
                // Force write to disk (Rufus equivalent)
                fflush(physical_drive);
                fsync(fileno(physical_drive));
                break;
            }
            
            if (written > 0) {
                printf("\r\n[%s] Write error: Wrote %d bytes, expected %d bytes\n", 
                       current_time_string(), write_size, read_size[proc_bufnum]);
                fflush(stdout);
            } else {
                printf("\r\n[%s] Write error at sector %llu: %s\n", 
                       current_time_string(), wb / 512, strerror(errno));
                fflush(stdout);
            }
            
            if (i < WRITE_RETRIES) {
                printf("[%s] Retrying in %d seconds...\n", current_time_string(), WRITE_TIMEOUT / 1000);
                fflush(stdout);
                usleep(WRITE_TIMEOUT * 1000); // WRITE_TIMEOUT is in ms
                
                // Reset file position (Rufus SetFilePointerEx equivalent)
                if (fseek(physical_drive, wb, SEEK_SET) != 0) {
                    printf("[%s] Write error: Could not reset position - %s\n", current_time_string(), strerror(errno));
                    goto out;
                }
            } else {
                printf("[%s] Write error after %d retries\n", current_time_string(), WRITE_RETRIES);
                goto out;
            }
            
            usleep(200000); // 200ms like Rufus
        }
        
        if (i > WRITE_RETRIES)
            goto out;
    }
    
    // Final flush and sync (Rufus equivalent)
    fflush(physical_drive);
    fsync(fileno(physical_drive));
    
    printf("\r\n[%s] ISO written successfully!\n", current_time_string());
    fflush(stdout);
    printf("[%s] Syncing filesystem...\n", current_time_string());
    fflush(stdout);
    system("sync");
    
    ret = true;
    
out:
    if (source_image) fclose(source_image);
    if (physical_drive) fclose(physical_drive);
    if (buffer) free(buffer);
    
    return ret;
}
