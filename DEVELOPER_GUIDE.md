# Rufus macOS Fork - Developer Documentation

## Overview

This document provides technical details for developers working on the Rufus macOS fork.

## Architecture

### Original Rufus Architecture (Windows)
```
rufus.exe
├── Windows GUI (Win32 API)
├── Device Detection (SetupAPI, WMI)
├── Disk Operations (DeviceIoControl)
├── File System Support (Native Windows)
└── Boot Sector Creation (Custom implementations)
```

### macOS Port Architecture
```
rufus-macos
├── Command Line Interface (getopt, stdio)
├── Device Detection (IOKit, DiskArbitration)
├── Disk Operations (diskutil, ioctl)
├── File System Support (Native macOS)
└── Boot Sector Creation (Future: custom/dd-based)
```

## Key Technical Changes

### 1. Device Detection
**Windows (Original)**:
- Uses `SetupDiGetClassDevs()` with `GUID_DEVINTERFACE_DISK`
- Enumerates through `SetupDiEnumDeviceInfo()`
- Gets device properties via `SetupDiGetDeviceRegistryProperty()`
- USB detection via device enumerator checking

**macOS (Port)**:
- Uses `IOServiceGetMatchingServices()` with `kIOMediaClass`
- Filters by `kIOMediaWholeKey` for whole disks
- Gets properties via `IORegistryEntryCreateCFProperties()`
- USB detection via DiskArbitration bus name checking

### 2. Disk Operations
**Windows (Original)**:
- Direct device access via `CreateFile()` with `\\.\PhysicalDriveX`
- Low-level operations using `DeviceIoControl()` 
- Custom partition table manipulation
- Direct sector-level access

**macOS (Port)**:
- Device access via `/dev/diskX` paths
- High-level operations using `diskutil` command
- Native macOS disk management through DiskArbitration
- IOKit for device size and block information

### 3. File System Support
**Windows (Original)**:
- FAT32: Custom boot record writing
- NTFS: Native Windows formatting + custom boot sectors
- Direct MBR/GPT manipulation

**macOS (Port)**:
- FAT32: Native macOS formatting via `diskutil`
- ExFAT: Native macOS support
- NTFS: macOS NTFS driver (limited)
- Uses macOS native partition schemes

## Code Structure

### Core Files

#### `src/macos/macos_device.h`
- Defines macOS-specific data structures
- Function prototypes for device operations
- Constants and enums adapted for macOS

#### `src/macos/macos_device.c`  
- Core device detection and enumeration
- IOKit integration for hardware access
- DiskArbitration integration for disk management
- Device property extraction

#### `src/macos/rufus_macos.c`
- Main application entry point
- Command-line interface implementation
- User interaction and workflow control
- High-level operation coordination

### Key Functions

#### Device Detection
```c
bool macos_get_usb_devices(macos_rufus_drive drives[], int *num_drives)
```
- Primary function for USB device enumeration
- Uses IOKit to find IOMedia objects
- Filters for removable USB storage devices
- Populates drive information structures

#### Device Properties
```c
bool macos_get_device_properties(const char *device_path, macos_device_props *props)
```
- Extracts device vendor/product information
- Gets USB VID/PID when available
- Determines removability and bus type

#### Formatting Operations
```c
bool macos_format_device(const char *device_path, const char *fs_type, const char *label)
```
- Coordinates device unmounting
- Calls appropriate formatting utilities
- Handles error reporting

## macOS-Specific Considerations

### 1. Security and Permissions
- **System Integrity Protection (SIP)**: Limits low-level disk access
- **Disk Access Permissions**: May require Full Disk Access permission
- **Admin Privileges**: Required for disk formatting operations
- **Code Signing**: Recommended for distribution

### 2. Framework Dependencies
- **IOKit**: Hardware abstraction and device enumeration
- **DiskArbitration**: High-level disk management
- **Core Foundation**: Apple's fundamental data types and utilities

### 3. Device Paths
- **BSD Names**: Devices appear as `/dev/diskX` (disk identifiers)
- **IOKit Paths**: Internal paths like `IOService:/...` for device tree navigation  
- **Mount Points**: Volumes mount to `/Volumes/VolumeName`

### 4. API Differences

| Operation | Windows | macOS |
|-----------|---------|-------|
| Device Enumeration | SetupAPI | IOKit |
| Device Properties | Registry | IORegistry |
| Disk Access | DeviceIoControl | ioctl/diskutil |
| Formatting | Custom + Windows API | diskutil |
| Mounting | Win32 Volume API | DiskArbitration |

## Building and Development

### Build Requirements
- macOS 10.12+ (Sierra or later)
- Xcode Command Line Tools
- GCC or Clang compiler

### Build Process
```bash
cd src/macos
make clean
make
```

### Development Workflow
1. Make changes to source files
2. Test compile: `make`
3. Test functionality: `./rufus-macos -l`
4. Full test: `../../build_macos.sh test`
5. Create package: `../../build_macos.sh package`

## Testing

### Unit Testing Strategy
- **Device Detection**: Test with various USB device types
- **Safety Checks**: Ensure only removable devices are listed
- **Error Handling**: Test with permission restrictions
- **Cross-Platform**: Validate behavior across macOS versions

### Testing Commands
```bash
# List devices (should work without root)
./rufus-macos -l

# Test help system
./rufus-macos --help

# Test with dummy device (requires USB device)
sudo ./rufus-macos -d disk999 -f FAT32  # Should fail safely
```

## Future Development Areas

### Phase 2: Enhanced Features
- **ISO Writing**: Implement `dd`-based ISO image writing
- **Progress Reporting**: Add progress bars for long operations
- **Better Error Handling**: More descriptive error messages

### Phase 3: Advanced Features  
- **GUI Interface**: Native macOS GUI using AppKit or SwiftUI
- **Bootable USB Creation**: EFI boot support for macOS/Windows/Linux
- **Verification**: Hash verification after operations

### Phase 4: Polish
- **Code Signing**: Apple Developer certificates
- **App Store**: Consider sandboxed version
- **Automatic Updates**: Built-in update mechanism

## API Reference

### IOKit Integration
```c
// Get matching IOMedia services
CFMutableDictionaryRef matching_dict = IOServiceMatching(kIOMediaClass);
CFDictionarySetValue(matching_dict, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching_dict, &iter);
```

### DiskArbitration Usage
```c
// Create session and disk reference
DASessionRef session = DASessionCreate(kCFAllocatorDefault);
DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device_name);
CFDictionaryRef description = DADiskCopyDescription(disk);
```

## Troubleshooting

### Common Issues

#### "No USB storage devices found"
- **Cause**: No removable USB devices connected, or permission issues
- **Solution**: Connect a USB drive, try with `sudo`

#### "Permission denied" errors  
- **Cause**: Insufficient privileges for disk operations
- **Solution**: Use `sudo`, grant Full Disk Access in Security & Privacy

#### Build failures
- **Cause**: Missing Xcode Command Line Tools or wrong SDK
- **Solution**: Run `xcode-select --install`, update Xcode

### Debug Mode
Enable debug output by modifying source:
```c
#define DEBUG_DEVICE_DETECTION 1
#define DEBUG_DISK_OPERATIONS 1
```

## Contributing

### Code Style
- Follow existing C99 style
- Use clear, descriptive variable names
- Add comprehensive comments for complex operations
- Error handling for all system calls

### Pull Request Process
1. Fork the repository
2. Create feature branch
3. Implement changes with tests
4. Update documentation
5. Submit pull request with detailed description

### Areas for Contribution
- Additional filesystem support
- GUI interface development  
- Performance optimizations
- Extended hardware compatibility
- Localization/internationalization

---

This documentation is a living document and should be updated as the codebase evolves.
