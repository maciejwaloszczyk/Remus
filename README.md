# Remus: Rufus ported to macOS

[![Latest Release](https://img.shields.io/github/release-pre/maciejwaloszczyk/Remus.svg?style=flat-square&label=Latest%20Release)](https://github.com/maciejwaloszczyk/Remus/releases)
[![Licence](https://img.shields.io/badge/license-GPLv3-blue.svg?style=flat-square&label=License)](https://www.gnu.org/licenses/gpl-3.0.en.html)
[![Status](https://img.shields.io/badge/status-alpha-orange.svg?style=flat-square&label=Status)](https://github.com/maciejwaloszczyk/Remus)
[![Download Stats](https://img.shields.io/github/downloads/maciejwaloszczyk/Remus/total.svg?label=Downloads&style=flat-square)](https://github.com/maciejwaloszczyk/Remus/releases)
[![macOS](https://img.shields.io/badge/platform-macOS-blue.svg?style=flat-square)](https://www.apple.com/macos)
[![Last Commit](https://img.shields.io/github/last-commit/maciejwaloszczyk/Remus.svg?style=flat-square&label=Last%20Commit)](https://github.com/maciejwaloszczyk/Remus/commits/master)

This is a macOS port of Rufus - The Reliable USB Formatting Utility.

## About

Rufus macOS is a command-line utility that brings Rufus functionality to macOS. It provides:

- USB device detection and enumeration
- Support for FAT32, ExFAT, and NTFS formatting
- Safe device identification with VID/PID detection
- Command-line interface optimized for macOS

## Features

### Currently Implemented:
- ✅ USB device detection using IOKit and DiskArbitration
- ✅ Device information display (size, vendor, product, etc.)
- ✅ Multiple filesystem support (FAT32, ExFAT, NTFS)
- ✅ Safe device validation (removable USB devices only)
- ✅ Volume labeling
- ✅ Device unmounting before formatting

### Planned Features:
- 🔄 ISO image writing to USB devices
- 🔄 Bootable USB creation
- 🔄 Progress reporting during operations
- 🔄 GUI interface using native macOS frameworks
- 🔄 Support for additional filesystems (ext2/ext3/ext4, HFS+)

## Building

### Prerequisites:
- macOS 10.12 or later
- Xcode Command Line Tools
- GCC or Clang

### Build Instructions:

```bash
cd src/macos
make
```

## Usage

### List USB Devices:
```bash
./rufus-macos -l
# or
./rufus-macos --list
```

### Format a USB Device:
```bash
# Format disk2 as FAT32 with label "MY_USB"
sudo ./rufus-macos -d disk2 -f FAT32 -n MY_USB

# Format disk3 as ExFAT
sudo ./rufus-macos -d disk3 -f ExFAT
```

### Command Line Options:
- `-l, --list`: List all USB devices
- `-d, --device DEVICE`: Select device to format (e.g., disk2)
- `-f, --filesystem TYPE`: Filesystem type (FAT32, ExFAT, NTFS)
- `-n, --name LABEL`: Volume label
- `-v, --verbose`: Verbose output
- `-h, --help`: Show help message

## Installation

```bash
make install
```

This will install `rufus-macos` to `/usr/local/bin/`.

## Safety Features

- Only lists and operates on removable USB devices
- Requires user confirmation before formatting
- Validates device existence before operations
- Uses native macOS disk management APIs

## Technical Implementation

### Core Components:

1. **Device Detection (`macos_device.c`)**:
   - Uses IOKit to enumerate storage devices
   - Filters for removable USB devices only
   - Extracts device properties (VID/PID, size, vendor/product names)

2. **Disk Operations**:
   - Uses DiskArbitration framework for device management
   - Leverages `diskutil` command for formatting operations
   - Handles device unmounting automatically

3. **Safety Mechanisms**:
   - Multiple validation layers to prevent system disk formatting
   - Clear user prompts and confirmations
   - Comprehensive error handling

### Key macOS APIs Used:
- **IOKit**: For low-level device enumeration and property access
- **DiskArbitration**: For disk management and mounting/unmounting
- **Core Foundation**: For handling Apple's data types and property lists

## Differences from Windows Rufus

### Architecture Changes:
- **No Win32 GUI**: Command-line interface instead of Windows dialogs
- **IOKit instead of WinAPI**: Native macOS hardware abstraction
- **DiskArbitration instead of DeviceIoControl**: macOS disk management
- **diskutil integration**: Leverages macOS native formatting utilities

### Limitations:
- No GUI (command-line only for now)
- Limited to basic formatting operations initially
- NTFS support depends on macOS NTFS drivers

## Development Roadmap

### Phase 1 (Current): ✅ Basic Functionality
- USB device detection and enumeration
- Basic formatting support
- Command-line interface

### Phase 2 (Next): 🔄 Enhanced Features
- ISO image writing capability
- Bootable USB creation
- Progress reporting and better error handling

### Phase 3 (Future): 🔄 Advanced Features
- Native macOS GUI using AppKit/SwiftUI
- Advanced formatting options
- Additional filesystem support

### Phase 4 (Long-term): 🔄 Feature Parity
- Feature parity with Windows Rufus where applicable
- macOS-specific enhancements
- Integration with macOS security model

## Contributing

This is a fork/port of the original Rufus project by Pete Batard. 

### Original Rufus:
- Repository: https://github.com/pbatard/rufus
- Author: Pete Batard
- License: GPL v3

### macOS Port:
- Maintained by: Maciej Wałoszczyk
- License: GPL v3 (same as original)

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

## Acknowledgments

- Pete Batard for the original Rufus application
- The macOS developer community for IOKit and DiskArbitration documentation
- All contributors to the original Rufus project

---

⚠️ **WARNING**: This software can erase data on your drives. Use with caution and always backup important data before formatting any device.
