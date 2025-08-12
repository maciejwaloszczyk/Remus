/*
 * Remus: The Reliable USB Formattin    printf("Example:\n");
    printf("  %s -l                                    # List USB devices\n", progname);
    printf("  %s -d disk2 -f FAT32 -n MY_USB          # Format disk2 as FAT32\n", progname);
    printf("  %s -d disk2 -f FAT32 -n MY_USB -y       # Format without prompts\n", progname);
    printf("  %s -d disk2 -i ubuntu.iso -y            # Write ISO to disk2\n", progname);ility for macOS
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
#include <errno.h>
#include <getopt.h>
#include "macos/macos_device.h"

#ifdef REMUS_DEBUG
#define DBG(fmt, ...) printf("DEBUG: " fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...) do { } while(0)
#endif

#define VERSION "v0.1.0-alpha"
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
    printf("  -i, --iso IMAGE         ISO image to write to device\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -y, --yes               Answer yes to all prompts\n");
    printf("  -h, --help              Show this help message\n");
    printf("\nExample:\n");
    printf("  %s -l                                    # List USB devices\n", progname);
    printf("  %s -d disk2 -f FAT32 -n MY_USB          # Format disk2 as FAT32\n", progname);
    printf("  %s -d disk2 -f FAT32 -n MY_USB -y       # Format without prompts\n", progname);
    printf("  %s -d disk2 -i ubuntu.iso -y            # Write ISO to disk2\n", progname);
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

bool format_device(const char *device_name, const char *fs_type, const char *label, bool auto_yes) {
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
    
    if (!auto_yes) {
        printf("\nDo you want to continue? (y/N): ");
        fflush(stdout);
        
        char response[10];
        if (!fgets(response, sizeof(response), stdin) || 
            (response[0] != 'y' && response[0] != 'Y')) {
            printf("Operation cancelled.\n");
            return false;
        }
    } else {
        printf("\nProceeding automatically (--yes flag used)...\n");
    }
    
    printf("\nFormatting device...\n");
    if (!macos_format_device(drive->device_path, fs_type, label)) {
        printf("Error: Failed to format device\n");
        return false;
    }
    
    printf("Device formatted successfully!\n");
    return true;
}

bool write_iso_to_device(const char *device_name, const char *iso_path, bool auto_yes) {
    // Ciche (release) – brak jawnych DEBUG linii
    macos_remus_drive *drive = find_device_by_name(device_name);
    if (!drive) {
        printf("Error: Device '%s' not found or not a USB device\n", device_name);
        fflush(stdout);
        return false;
    }
    // Sprawdzenie ISO
    FILE *iso_f = fopen(iso_path, "rb");
    if (!iso_f) {
        printf("Error: Cannot open ISO file '%s': %s\n", iso_path, strerror(errno));
        return false;
    }
    fseek(iso_f, 0, SEEK_END);
    long iso_size = ftell(iso_f);
    fclose(iso_f);
    printf("\nWarning: This will erase all data on device '%s'\n", drive->display_name);
    printf("Device: %s\n", drive->device_path);
    printf("Device Size: %.2f GB\n", (double)drive->size / (1024.0 * 1024.0 * 1024.0));
    printf("ISO File: %s\n", iso_path);
    printf("ISO Size: %.2f MB\n", (double)iso_size / (1024.0 * 1024.0));
    if (iso_size > (long)drive->size) {
        printf("Error: ISO file (%.2f MB) is larger than device (%.2f GB)\n",
               (double)iso_size / (1024.0 * 1024.0),
               (double)drive->size / (1024.0 * 1024.0 * 1024.0));
        return false;
    }
    if (!auto_yes) {
        printf("\nDo you want to continue? (y/N): ");
        fflush(stdout);
        char response[10];
        if (!fgets(response, sizeof(response), stdin) || (response[0] != 'y' && response[0] != 'Y')) {
            printf("Operation cancelled.\n");
            return false;
        }
    } else {
        printf("\nProceeding automatically (--yes flag used)...\n");
    }
    printf("\nWriting ISO to device...\n");
    fflush(stdout);
    if (!macos_write_iso_to_device(iso_path, drive->device_path)) {
        printf("Error: Failed to write ISO to device\n");
        fflush(stdout);
        return false;
    }
    printf("ISO written successfully!\n");
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
    bool auto_yes = false;  // New flag for --yes
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
        {"yes", no_argument, 0, 'y'},  // New yes flag
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Check if running as root for write operations
    if (getuid() != 0) {
        printf("Note: Some operations may require root privileges.\n");
        printf("Run with 'sudo %s' if you encounter permission errors.\n\n", argv[0]);
    }
    
    // Force unbuffered stdout for real-time output in GUI
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("%s %s\n", APPLICATION_NAME, VERSION);
    fflush(stdout);
    printf("Copyright © 2025 Maciej Wałoszczyk\n\n");
    fflush(stdout);
    
    if (argc == 1) {
        list_devices = true;
    }
    
    // Proste parsowanie argumentów (z zachowaniem funkcjonalności) bez głośnych printf DEBUG
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if ((strcmp(arg, "--device") == 0 || strcmp(arg, "-d") == 0) && i + 1 < argc) {
            device_name = argv[++i];
            DBG("device_name set to %s\n", device_name);
        } else if ((strcmp(arg, "--iso") == 0 || strcmp(arg, "-i") == 0) && i + 1 < argc) {
            iso_file = argv[++i];
            DBG("iso_file set to %s\n", iso_file);
        } else if (strcmp(arg, "--yes") == 0 || strcmp(arg, "-y") == 0) {
            auto_yes = true;
            DBG("auto_yes enabled\n");
        } else if (strcmp(arg, "--list") == 0 || strcmp(arg, "-l") == 0) {
            list_devices = true;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            return 0;
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
        // Check if ISO writing is requested
        if (iso_file) {
            printf("Writing ISO to device (formatting will be skipped)\n");
            bool success = write_iso_to_device(device_name, iso_file, auto_yes);
            cleanup_drives();
            return success ? 0 : 1;
        } else {
            // Validate filesystem type for formatting
            if (strcmp(fs_type, "FAT32") != 0 && strcmp(fs_type, "ExFAT") != 0 && strcmp(fs_type, "NTFS") != 0) {
                printf("Error: Unsupported filesystem type '%s'\n", fs_type);
                printf("Supported types: FAT32, ExFAT, NTFS\n");
                cleanup_drives();
                return 1;
            }
            bool success = format_device(device_name, fs_type, label, auto_yes);
            cleanup_drives();
            return success ? 0 : 1;
        }
    }
    
    // Handle ISO writing without device specified
    if (iso_file && !device_name) {
        printf("Error: ISO file specified but no target device selected\n");
        printf("Use -d DEVICE to specify target device\n");
        cleanup_drives();
        return 1;
    }
    
    print_usage(argv[0]);
    cleanup_drives();
    return 0;
}
