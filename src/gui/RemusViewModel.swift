import SwiftUI
import Foundation
import Security

// MARK: - Data Models

struct RemusDevice {
    let devicePath: String
    let name: String
    let displayName: String
    let size: UInt64
    let label: String?
    let props: DeviceProperties
    
    struct DeviceProperties {
        let vid: UInt16
        let pid: UInt16
        let isRemovable: Bool
    }
}

enum BootType: String, CaseIterable {
    case nonBootable = "Non bootable"
    case diskImage = "Disk or ISO image"
    case freeDOS = "FreeDOS"
}

enum FileSystem: String, CaseIterable {
    case fat32 = "FAT32"
    case exFAT = "exFAT" 
    case ntfs = "NTFS"
}

// MARK: - View Model

class RemusViewModel: ObservableObject {
    // Device properties
    @Published var devices: [RemusDevice] = []
    @Published var selectedDevice: RemusDevice?
    
    // Boot selection
    @Published var bootType: BootType = .nonBootable
    @Published var imageFile: String = ""
    
    // Format options
    @Published var fileSystem: FileSystem = .fat32
    @Published var clusterSize: String = "Default allocation size"
    @Published var volumeLabel: String = "USB_DRIVE"
    
    // Advanced options
    @Published var showAdvancedOptions: Bool = false
    @Published var quickFormat: Bool = true
    @Published var checkBadBlocks: Bool = false
    @Published var createExtendedLabel: Bool = false
    @Published var badBlockPasses: Int = 1
    
    // Operation state
    @Published var isOperationInProgress: Bool = false
    @Published var progress: Double = 0.0
    @Published var statusMessage: String = "Ready"
    
    // UI state
    @Published var showLog: Bool = false
    @Published var logMessages: [String] = []
    
    var availableClusterSizes: [String] {
        switch fileSystem {
        case .fat32:
            return ["Default allocation size", "512 bytes", "1024 bytes", "2048 bytes", "4096 bytes", "8192 bytes", "16 kilobytes", "32 kilobytes"]
        case .exFAT:
            return ["Default allocation size", "512 bytes", "1024 bytes", "2048 bytes", "4096 bytes", "8192 bytes", "16 kilobytes", "32 kilobytes", "64 kilobytes", "128 kilobytes", "256 kilobytes", "512 kilobytes", "1024 kilobytes", "2048 kilobytes", "4096 kilobytes", "8192 kilobytes", "16384 kilobytes", "32768 kilobytes"]
        case .ntfs:
            return ["Default allocation size", "512 bytes", "1024 bytes", "2048 bytes", "4096 bytes", "8192 bytes", "16 kilobytes", "32 kilobytes", "64 kilobytes"]
        }
    }
    
    var canStartFormatting: Bool {
        return selectedDevice != nil && !isOperationInProgress
    }
    
    // MARK: - Authorization
    
    private func runWithAuthorization(command: String, arguments: [String]) -> (output: String, error: String, success: Bool) {
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/sudo")
        task.arguments = ["-S", command] + arguments
        
        let inputPipe = Pipe()
        let outputPipe = Pipe()
        let errorPipe = Pipe()
        
        task.standardInput = inputPipe
        task.standardOutput = outputPipe
        task.standardError = errorPipe
        
        // Set environment
        task.environment = ProcessInfo.processInfo.environment
        
        do {
            try task.run()
            
            // Provide empty password (hoping sudo is already authenticated or will prompt)
            inputPipe.fileHandleForWriting.write("\n".data(using: .utf8) ?? Data())
            inputPipe.fileHandleForWriting.closeFile()
            
            task.waitUntilExit()
            
            let outputData = outputPipe.fileHandleForReading.readDataToEndOfFile()
            let errorData = errorPipe.fileHandleForReading.readDataToEndOfFile()
            
            let output = String(data: outputData, encoding: .utf8) ?? ""
            let error = String(data: errorData, encoding: .utf8) ?? ""
            
            return (output: output, error: error, success: task.terminationStatus == 0)
        } catch {
            return (output: "", error: error.localizedDescription, success: false)
        }
    }
    
    // MARK: - Device Operations
    
    func refreshDevices() {
        addLogMessage("Scanning for USB storage devices...")
        
        // First try without sudo
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/usr/local/bin/remus")
        task.arguments = ["--list"]
        task.environment = ProcessInfo.processInfo.environment
        
        let pipe = Pipe()
        task.standardOutput = pipe
        task.standardError = pipe
        
        do {
            try task.run()
            task.waitUntilExit()
            
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            let output = String(data: data, encoding: .utf8) ?? ""
            
            addLogMessage("CLI Output: \(output)")
            
            // If no devices found, we probably need sudo
            if output.contains("No USB storage devices found") {
                addLogMessage("No devices found without admin privileges. Requesting authorization...")
                
                // Try with sudo authorization
                let result = runWithAuthorization(command: "/usr/local/bin/remus", arguments: ["--list"])
                
                if result.success {
                    addLogMessage("Authorized CLI Output: \(result.output)")
                    parseDeviceOutput(result.output)
                } else {
                    addLogMessage("Authorization failed: \(result.error)")
                    DispatchQueue.main.async {
                        self.statusMessage = "Authorization required to scan devices"
                    }
                }
            } else {
                parseDeviceOutput(output)
            }
            
        } catch {
            addLogMessage("Error scanning devices: \(error.localizedDescription)")
            addLogMessage("Attempted to run: /usr/local/bin/remus --list")
            statusMessage = "Error scanning devices"
        }
    }
    
    private func parseDeviceOutput(_ output: String) {
        let lines = output.components(separatedBy: .newlines)
        var parsedDevices: [RemusDevice] = []
        var currentDevice: [String: String] = [:]
        
        for line in lines {
            if line.hasPrefix("[") && line.contains("]") {
                // Save previous device if we have one
                if !currentDevice.isEmpty {
                    if let device = createDeviceFromParsedData(currentDevice) {
                        parsedDevices.append(device)
                    }
                    currentDevice = [:]
                }
                
                // Extract device name from [0] Device Name
                if let range = line.range(of: "] ") {
                    let deviceName = String(line[range.upperBound...])
                    currentDevice["displayName"] = deviceName
                }
            } else if line.contains(":") {
                let components = line.components(separatedBy: ":")
                if components.count >= 2 {
                    let key = components[0].trimmingCharacters(in: .whitespaces)
                    let value = components[1...].joined(separator: ":").trimmingCharacters(in: .whitespaces)
                    currentDevice[key] = value
                }
            }
        }
        
        // Don't forget the last device
        if !currentDevice.isEmpty {
            if let device = createDeviceFromParsedData(currentDevice) {
                parsedDevices.append(device)
            }
        }
        
        DispatchQueue.main.async {
            self.devices = parsedDevices
            if !parsedDevices.isEmpty && self.selectedDevice == nil {
                self.selectedDevice = parsedDevices.first
            }
            
            self.addLogMessage("Found \(parsedDevices.count) USB storage device(s)")
            self.statusMessage = parsedDevices.isEmpty ? "No USB devices found" : "Ready"
        }
    }
    
    private func createDeviceFromParsedData(_ data: [String: String]) -> RemusDevice? {
        guard let displayName = data["displayName"],
              let devicePath = data["Device"],
              let sizeString = data["Size"] else {
            return nil
        }
        
        // Parse size (assume it's in GB format like "32.00 GB")
        let sizeComponents = sizeString.components(separatedBy: " ")
        let size = UInt64((Double(sizeComponents.first ?? "0") ?? 0.0) * 1024 * 1024 * 1024)
        
        // Parse VID:PID if available
        var vid: UInt16 = 0
        var pid: UInt16 = 0
        if let vidPidString = data["VID:PID"] {
            let components = vidPidString.components(separatedBy: ":")
            if components.count == 2 {
                vid = UInt16(components[0], radix: 16) ?? 0
                pid = UInt16(components[1], radix: 16) ?? 0
            }
        }
        
        let isRemovable = data["Removable"] == "Yes"
        let label = data["Label"] == "NO_LABEL" ? nil : data["Label"]
        
        return RemusDevice(
            devicePath: devicePath,
            name: devicePath.components(separatedBy: "/").last ?? devicePath,
            displayName: displayName,
            size: size,
            label: label,
            props: RemusDevice.DeviceProperties(vid: vid, pid: pid, isRemovable: isRemovable)
        )
    }
    
    // MARK: - Format Operations
    
    func startFormatting() {
        guard let device = selectedDevice else { return }
        
        let alert = NSAlert()
        alert.messageText = "Warning: Data Loss"
        alert.informativeText = "This will erase all data on device '\(device.displayName)'. Are you sure you want to continue?"
        alert.alertStyle = .critical
        alert.addButton(withTitle: "Format")
        alert.addButton(withTitle: "Cancel")
        
        let response = alert.runModal()
        
        if response == .alertFirstButtonReturn {
            performFormat(device)
        }
    }
    
    private func performFormat(_ device: RemusDevice) {
        isOperationInProgress = true
        progress = 0.0
        statusMessage = "Preparing to format..."
        addLogMessage("Starting format operation on \(device.displayName)")
        
        // Build command arguments
        var arguments = [
            "--device", device.name,
            "--filesystem", fileSystem.rawValue
        ]
        
        if !volumeLabel.isEmpty {
            arguments.append(contentsOf: ["--name", volumeLabel])
        }
        
        if !quickFormat {
            arguments.append("--slow-format")
        }
        
        if checkBadBlocks {
            arguments.append("--check-bad-blocks")
        }
        
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/usr/local/bin/remus")
        task.arguments = arguments
        
        let pipe = Pipe()
        task.standardOutput = pipe
        task.standardError = pipe
        
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try task.run()
                
                // Monitor output for progress updates
                let outputHandle = pipe.fileHandleForReading
                
                while task.isRunning {
                    let data = outputHandle.availableData
                    if !data.isEmpty {
                        let output = String(data: data, encoding: .utf8) ?? ""
                        DispatchQueue.main.async {
                            self.parseFormatOutput(output)
                        }
                    }
                    Thread.sleep(forTimeInterval: 0.1)
                }
                
                task.waitUntilExit()
                
                DispatchQueue.main.async {
                    if task.terminationStatus == 0 {
                        self.progress = 100.0
                        self.statusMessage = "Format completed successfully"
                        self.addLogMessage("Device formatted successfully!")
                    } else {
                        self.statusMessage = "Format failed"
                        self.addLogMessage("Format operation failed")
                    }
                    
                    self.isOperationInProgress = false
                }
                
            } catch {
                DispatchQueue.main.async {
                    self.statusMessage = "Error: \(error.localizedDescription)"
                    self.addLogMessage("Error during format: \(error.localizedDescription)")
                    self.isOperationInProgress = false
                }
            }
        }
    }
    
    private func parseFormatOutput(_ output: String) {
        addLogMessage(output)
        
        // Simple progress estimation based on output
        if output.contains("Formatting") {
            progress = 25.0
            statusMessage = "Formatting device..."
        } else if output.contains("Writing") {
            progress = 50.0
            statusMessage = "Writing file system..."
        } else if output.contains("Verifying") {
            progress = 75.0
            statusMessage = "Verifying..."
        }
    }
    
    func cancelOperation() {
        // In a real implementation, we'd need to track and terminate the running process
        isOperationInProgress = false
        statusMessage = "Operation cancelled"
        addLogMessage("Operation cancelled by user")
    }
    
    // MARK: - Helper Functions
    
    func listDevices() {
        showLog = true
        refreshDevices()
    }
    
    func selectImageFile() {
        let openPanel = NSOpenPanel()
        openPanel.allowedContentTypes = [.init(filenameExtension: "iso")!, .init(filenameExtension: "img")!]
        openPanel.allowsMultipleSelection = false
        openPanel.canChooseDirectories = false
        openPanel.canChooseFiles = true
        
        if openPanel.runModal() == .OK {
            if let url = openPanel.url {
                imageFile = url.path
                addLogMessage("Selected ISO image: \(url.lastPathComponent)")
            }
        }
    }
    
    private func addLogMessage(_ message: String) {
        let timestamp = DateFormatter.timeFormatter.string(from: Date())
        let logEntry = "[\(timestamp)] \(message)"
        
        DispatchQueue.main.async {
            self.logMessages.append(logEntry)
        }
    }
}

// MARK: - Extensions

extension DateFormatter {
    static let timeFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        return formatter
    }()
}
