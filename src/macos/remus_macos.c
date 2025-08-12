/*
 * Remus: The Reliable USB Formatting Utility for macOS
 * Main application for macOS * Rufus macOS: The Reliable USB Formatting Utility for macOS
 * Main application file for macOS
 * Copyright © 2025 Maciej Wałoszczyk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
#include "macos/macos_device.h"

#define VERSION "4.10-macOS"
#define APPLICATION_NAME "Remus"

static macos_remus_drive drives[MAX_DRIVES];
static int num_drives = 0;

void print_usage(const char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\nRemus - The Reliable USB Formatting Utility for macOS\n");
    printf("Version %s\n\n", VERSION);
    printf("Options:\n");
    printf("  -l, --list              List all USB devices\n");
    printf("  -d, --device DEVICE     Select device to format (e.g., disk2)\n");
    printf("  -f, --filesystem TYPE   Filesystem type (FAT32, ExFAT, NTFS)\n");
    printf("  -n, --name LABEL        Volume label\n");
    printf("  -i, --iso IMAGE         ISO image to write (future feature)\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -h, --help              Show this help message\n");
    printf("\nExample:\n");
    printf("  %s -l                                    # List USB devices\n", progname);
    printf("  %s -d disk2 -f FAT32 -n MY_USB          # Format disk2 as FAT32\n", progname);
    printf("\nWARNING: This will erase all data on the selected device!\n");
}

void list_usb_devices() {
    printf("\nScanning for USB storage devices...\n");
    
    if (!macos_get_usb_devices(drives, &num_drives)) {
        printf("Error: Could not enumerate USB devices\n");
        return;
    }
    
    if (num_drives == 0) {
        printf("No USB storage devices found.\n");
        return;
    }
    
    printf("\nFound %d USB storage device(s):\n", num_drives);
    printf("==================================================\n");
    
    for (int i = 0; i < num_drives; i++) {
        printf("[%d] %s\n", i, drives[i].display_name);
        printf("    Device: %s\n", drives[i].device_path);
        printf("    Size: %.2f GB\n", (double)drives[i].size / (1024.0 * 1024.0 * 1024.0));
        printf("    Label: %s\n", drives[i].label ? drives[i].label : "NO_LABEL");
        if (drives[i].props.vid && drives[i].props.pid) {
            printf("    VID:PID: %04X:%04X\n", drives[i].props.vid, drives[i].props.pid);
        }
        printf("    Removable: %s\n", drives[i].props.is_Removable ? "Yes" : "No");
        printf("\n");
    }
}

macos_remus_drive *find_device_by_name(const char *device_name) {
    // Refresh device list
    if (!macos_get_usb_devices(drives, &num_drives)) {
        return NULL;
    }
    
    for (int i = 0; i < num_drives; i++) {
        const char *dev_name = strrchr(drives[i].device_path, '/');
        if (dev_name && strcmp(dev_name + 1, device_name) == 0) {
            return &drives[i];
        }
    }
    return NULL;
}

bool format_device(const char *device_name, const char *fs_type, const char *label) {
    macos_remus_drive *drive = find_device_by_name(device_name);
    
    if (!drive) {
        printf("Error: Device '%s' not found or not a USB device\n", device_name);
        return false;
    }
    
    printf("\nWarning: This will erase all data on device '%s'\n", drive->display_name);
    printf("Device: %s\n", drive->device_path);
    printf("Size: %.2f GB\n", (double)drive->size / (1024.0 * 1024.0 * 1024.0));
    printf("Filesystem: %s\n", fs_type);
    printf("Label: %s\n", label ? label : "USB_DRIVE");
    
    printf("\nDo you want to continue? (y/N): ");
    fflush(stdout);
    
    char response[10];
    if (!fgets(response, sizeof(response), stdin) || 
        (response[0] != 'y' && response[0] != 'Y')) {
        printf("Operation cancelled.\n");
        return false;
    }
    
    printf("\nFormatting device...\n");
    if (!macos_format_device(drive->device_path, fs_type, label)) {
        printf("Error: Failed to format device\n");
        return false;
    }
    
    printf("Device formatted successfully!\n");
    return true;
}

void cleanup_drives() {
    for (int i = 0; i < num_drives; i++) {
        if (drives[i].device_path) free(drives[i].device_path);
        if (drives[i].name) free(drives[i].name);
        if (drives[i].display_name) free(drives[i].display_name);
        if (drives[i].label) free(drives[i].label);
    }
    memset(drives, 0, sizeof(drives));
    num_drives = 0;
}

int main(int argc, char *argv[]) {
    int opt;
    bool list_devices = false;
    bool verbose = false;
    char *device_name = NULL;
    char *fs_type = "FAT32";  // Default filesystem
    char *label = NULL;
    char *iso_file = NULL;
    
    static struct option long_options[] = {
        {"list", no_argument, 0, 'l'},
        {"device", required_argument, 0, 'd'},
        {"filesystem", required_argument, 0, 'f'},
        {"name", required_argument, 0, 'n'},
        {"iso", required_argument, 0, 'i'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Check if running as root for write operations
    if (getuid() != 0) {
        printf("Note: Some operations may require root privileges.\n");
        printf("Run with 'sudo %s' if you encounter permission errors.\n\n", argv[0]);
    }
    
    printf("%s %s\n", APPLICATION_NAME, VERSION);
    printf("Copyright © 2025 Maciej Wałoszczyk\n\n");
    
    // Parse command line arguments
    while ((opt = getopt_long(argc, argv, "ld:f:n:i:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'l':
                list_devices = true;
                break;
            case 'd':
                device_name = optarg;
                break;
            case 'f':
                fs_type = optarg;
                break;
            case 'n':
                label = optarg;
                break;
            case 'i':
                iso_file = optarg;
                printf("Note: ISO writing is not yet implemented.\n");
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // If no arguments provided, show device list
    if (argc == 1) {
        list_devices = true;
    }
    
    // List devices
    if (list_devices) {
        list_usb_devices();
        cleanup_drives();
        return 0;
    }
    
    // Format device if specified
    if (device_name) {
        // Validate filesystem type
        if (strcmp(fs_type, "FAT32") != 0 && 
            strcmp(fs_type, "ExFAT") != 0 && 
            strcmp(fs_type, "NTFS") != 0) {
            printf("Error: Unsupported filesystem type '%s'\n", fs_type);
            printf("Supported types: FAT32, ExFAT, NTFS\n");
            cleanup_drives();
            return 1;
        }
        
        bool success = format_device(device_name, fs_type, label);
        cleanup_drives();
        return success ? 0 : 1;
    }
    
    print_usage(argv[0]);
    cleanup_drives();
    return 0;
}
