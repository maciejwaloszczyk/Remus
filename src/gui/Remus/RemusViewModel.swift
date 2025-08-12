import SwiftUI
import Foundation
import Security
import ServiceManagement
import AppKit

// MARK: - Data Models

struct RemusDevice: Hashable {
    let devicePath: String
    let name: String
    let displayName: String
    let size: UInt64
    let label: String?
    let props: DeviceProperties
    
    struct DeviceProperties: Hashable {
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
    
    // Bufor do strumieniowego parsowania wyjścia ISO
    private var isoStreamBuffer: String = ""
    
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
    
    private func requestDiskAccess() {
        DispatchQueue.main.async {
            let alert = NSAlert()
            alert.messageText = "Disk Access Required"
            alert.informativeText = """
            Remus needs Full Disk Access to detect USB storage devices.
            
            Please:
            1. Open System Settings (System Preferences)
            2. Go to Privacy & Security → Full Disk Access
            3. Click the + button and add this Remus app
            4. Restart Remus after granting access
            
            Would you like to open System Settings now?
            """
            alert.alertStyle = .informational
            alert.addButton(withTitle: "Open System Settings")
            alert.addButton(withTitle: "Cancel")
            
            let response = alert.runModal()
            if response == .alertFirstButtonReturn {
                // Open System Settings to Privacy & Security
                if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles") {
                    NSWorkspace.shared.open(url)
                }
            }
        }
    }
    
    private func runWithAuthorization(command: String, arguments: [String]) -> (output: String, error: String, success: Bool) {
        let devAccess = FileManager.default.isReadableFile(atPath: "/dev/disk0")
        if !devAccess {
            return (output: "", error: "Disk access may be limited", success: false)
        }
        
        // If we have access, try the command directly
        let task = Process()
        task.executableURL = URL(fileURLWithPath: command)
        task.arguments = arguments
        task.environment = ProcessInfo.processInfo.environment
        
        let outputPipe = Pipe()
        let errorPipe = Pipe()
        
        task.standardOutput = outputPipe
        task.standardError = errorPipe
        
        do {
            try task.run()
            task.waitUntilExit()
            
            let outputData = outputPipe.fileHandleForReading.readDataToEndOfFile()
            let errorData = errorPipe.fileHandleForReading.readDataToEndOfFile()
            
            let output = String(data: outputData, encoding: .utf8) ?? ""
            let error = String(data: errorData, encoding: .utf8) ?? ""
            
            addLogMessage("Direct execution exit status: \(task.terminationStatus)")
            addLogMessage("Direct execution output: \(output)")
            if !error.isEmpty {
                addLogMessage("Direct execution error: \(error)")
            }
            
            return (output: output, error: error, success: task.terminationStatus == 0)
        } catch {
            addLogMessage("Failed to execute command directly: \(error.localizedDescription)")
            return (output: "", error: error.localizedDescription, success: false)
        }
    }
    
    private func runWithSudoAskpass(command: String, arguments: [String], progressCallback: @escaping (String) -> Void) -> (output: String, error: String, success: Bool) {
        addLogMessage("Running with sudo askpass: \(command) \(arguments.joined(separator: " "))")
        
        // Get the path to our askpass helper from the bundle
        let bundlePath = Bundle.main.bundlePath
        let askpassPath = bundlePath + "/Contents/Resources/askpass_helper.sh"
        
        // If askpass helper doesn't exist in bundle, use the one from source
        let finalAskpassPath: String
        if FileManager.default.fileExists(atPath: askpassPath) {
            finalAskpassPath = askpassPath
        } else {
            // Fallback to source location for development
            finalAskpassPath = "/Users/maciejwaloszczyk/Programowanie/GitHub/Remus/src/gui/askpass_helper.sh"
        }
        
        addLogMessage("Using askpass helper at: \(finalAskpassPath)")
        
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/sudo")
        task.arguments = ["-A"] + [command] + arguments
        
        // Set SUDO_ASKPASS environment variable
        var environment = ProcessInfo.processInfo.environment
        environment["SUDO_ASKPASS"] = finalAskpassPath
        task.environment = environment
        
        let outputPipe = Pipe()
        let errorPipe = Pipe()
        task.standardOutput = outputPipe
        task.standardError = errorPipe
        
        var fullOutput = ""
        var fullError = ""
        do {
            try task.run()
            let outputHandle = outputPipe.fileHandleForReading
            let errorHandle = errorPipe.fileHandleForReading
            // Ustawiamy readabilityHandler dla strumieniowego odczytu
            outputHandle.readabilityHandler = { [weak self] handle in
                guard let self = self else { return }
                let data = handle.availableData
                if data.isEmpty { return }
                if let chunk = String(data: data, encoding: .utf8), !chunk.isEmpty {
                    DispatchQueue.main.async {
                        self.isoStreamBuffer += chunk
                        // Podziel na pełne linie
                        let parts = self.isoStreamBuffer.components(separatedBy: "\n")
                        // Wszystkie oprócz ostatniego (może być niekompletna)
                        for i in 0..<(parts.count - 1) {
                            let line = parts[i] + "\n" // zachowaj newline
                            fullOutput += line
                            progressCallback(line) // parsujemy natychmiast pełną linię
                        }
                        // Ostatni fragment zostawiamy w buforze
                        self.isoStreamBuffer = parts.last ?? ""
                    }
                }
            }
            // Obsługa błędów (opcjonalnie strumieniowo)
            errorHandle.readabilityHandler = { handle in
                let data = handle.availableData
                if data.isEmpty { return }
                if let chunk = String(data: data, encoding: .utf8), !chunk.isEmpty {
                    DispatchQueue.main.async {
                        fullError += chunk
                        self.addLogMessage("Error stream: \(chunk)")
                    }
                }
            }
            // Czekamy na zakończenie
            task.waitUntilExit()
            // Wyłącz handlery
            outputHandle.readabilityHandler = nil
            errorHandle.readabilityHandler = nil
            // Jeśli w buforze została niekompletna linia – przetwarzamy
            if !isoStreamBuffer.isEmpty {
                let remaining = isoStreamBuffer
                isoStreamBuffer = ""
                fullOutput += remaining
                progressCallback(remaining)
            }
            // Dozbieraj resztę (jeśli coś zostało)
            let remainingOut = outputHandle.readDataToEndOfFile()
            if !remainingOut.isEmpty, let s = String(data: remainingOut, encoding: .utf8) {
                fullOutput += s
                progressCallback(s)
            }
            let remainingErr = errorHandle.readDataToEndOfFile()
            if !remainingErr.isEmpty, let s = String(data: remainingErr, encoding: .utf8) {
                fullError += s
                addLogMessage("Error stream: \(s)")
            }
            addLogMessage("Sudo askpass exit status: \(task.terminationStatus)")
            addLogMessage("Sudo askpass output: \(fullOutput)")
            if !fullError.isEmpty { addLogMessage("Sudo askpass error: \(fullError)") }
            return (output: fullOutput, error: fullError, success: task.terminationStatus == 0)
        } catch {
            addLogMessage("Failed to execute command with sudo askpass: \(error.localizedDescription)")
            return (output: "", error: error.localizedDescription, success: false)
        }
    }
    
    // MARK: - Device Operations
    
    func refreshDevices() {
        addLogMessage("Scanning for USB storage devices...")
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
            // Bez dodatkowego fallbacku autoryzacji – po prostu parsuj wynik
            parseDeviceOutput(output)
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
            let trimmedLine = line.trimmingCharacters(in: .whitespaces)
            if trimmedLine.hasPrefix("[") && trimmedLine.contains("]") {
                if !currentDevice.isEmpty {
                    if let device = createDeviceFromParsedData(currentDevice) { parsedDevices.append(device) }
                    currentDevice = [:]
                }
                if let range = trimmedLine.range(of: "] ") {
                    let deviceName = String(trimmedLine[range.upperBound...])
                    currentDevice["displayName"] = deviceName
                }
            } else if trimmedLine.contains(":") {
                let components = trimmedLine.components(separatedBy: ":")
                if components.count >= 2 {
                    let key = components[0].trimmingCharacters(in: .whitespaces)
                    let value = components[1...].joined(separator: ":").trimmingCharacters(in: .whitespaces)
                    currentDevice[key] = value
                }
            }
        }
        if !currentDevice.isEmpty { if let device = createDeviceFromParsedData(currentDevice) { parsedDevices.append(device) } }
        DispatchQueue.main.async {
            self.devices = parsedDevices
            if !parsedDevices.isEmpty && self.selectedDevice == nil { self.selectedDevice = parsedDevices.first }
            self.statusMessage = parsedDevices.isEmpty ? "No USB devices found" : "Ready"
        }
    }
    
    private func createDeviceFromParsedData(_ data: [String: String]) -> RemusDevice? {
        guard let displayName = data["displayName"],
              let devicePath = data["Device"],
              let sizeString = data["Size"] else { return nil }
        let sizeComponents = sizeString.components(separatedBy: " ")
        let size = UInt64((Double(sizeComponents.first ?? "0") ?? 0.0) * 1024 * 1024 * 1024)
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
            "--filesystem", fileSystem.rawValue,
            "--yes"  // Add --yes flag for GUI mode (no prompts)
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
        
        DispatchQueue.global(qos: .userInitiated).async {
            // Use sudo with askpass for GUI authentication
            let result = self.runWithSudoAskpass(command: "/usr/local/bin/remus", arguments: arguments) { output in
                DispatchQueue.main.async {
                    self.parseFormatOutput(output)
                }
            }
            
            DispatchQueue.main.async {
                if result.success {
                    self.progress = 1.0
                    self.statusMessage = "Format completed successfully!"
                    self.addLogMessage("Format operation completed")
                    self.parseFormatOutput(result.output)
                } else {
                    self.statusMessage = "Format failed: \(result.error)"
                    self.addLogMessage("Format failed: \(result.error)")
                }
                
                self.isOperationInProgress = false
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
    
    // MARK: - ISO Writing Operations
    
    func startWritingISO() {
        guard let device = selectedDevice else { return }
        guard !imageFile.isEmpty else { return }
        
        let alert = NSAlert()
        alert.messageText = "Warning: Data Loss"
        alert.informativeText = "This will erase all data on device '\(device.displayName)' and write the ISO image. Are you sure you want to continue?"
        alert.alertStyle = .critical
        alert.addButton(withTitle: "Write ISO")
        alert.addButton(withTitle: "Cancel")
        
        let response = alert.runModal()
        
        if response == .alertFirstButtonReturn {
            performISOWrite(device)
        }
    }
    
    private func performISOWrite(_ device: RemusDevice) {
        isOperationInProgress = true
        progress = 0.0
        statusMessage = "Preparing to write ISO..."
        addLogMessage("Starting ISO write operation on \(device.displayName)")
        addLogMessage("ISO file: \(imageFile)")
        
        // Build command arguments
        let arguments = [
            "--device", device.name,
            "--iso", imageFile,
            "--yes"  // Add --yes flag for GUI mode (no prompts)
        ]
        
        DispatchQueue.global(qos: .userInitiated).async {
            // Use sudo with askpass for GUI authentication
            let result = self.runWithSudoAskpass(command: "/usr/local/bin/remus", arguments: arguments) { output in
                DispatchQueue.main.async {
                    self.parseISOOutput(output)
                }
            }
            
            DispatchQueue.main.async {
                if result.success {
                    self.progress = 1.0
                    self.statusMessage = "ISO written successfully!"
                    self.addLogMessage("ISO write operation completed")
                    self.parseISOOutput(result.output)
                } else {
                    self.statusMessage = "ISO write failed: \(result.error)"
                    self.addLogMessage("ISO write failed: \(result.error)")
                }
                
                self.isOperationInProgress = false
            }
        }
    }
    
    private func parseISOOutput(_ output: String) {
        // Minimalne logowanie w wersji Alpha – tylko kluczowe wydarzenia
        let lines = output.components(separatedBy: .newlines)
        for line in lines {
            let trimmedLine = line.trimmingCharacters(in: .whitespacesAndNewlines)
            if trimmedLine.isEmpty { continue }
            if trimmedLine.contains("Writing image:") {
                if let range = trimmedLine.range(of: #"\d+\.\d+%"#, options: .regularExpression) {
                    let percentText = String(trimmedLine[range]).replacingOccurrences(of: "%", with: "")
                    if let percent = Double(percentText) {
                        DispatchQueue.main.async {
                            self.progress = percent / 100.0
                            self.statusMessage = "Writing ISO: \(String(format: "%.1f%%", percent))"
                        }
                    }
                }
            } else if trimmedLine.contains("ISO written successfully") {
                DispatchQueue.main.async {
                    self.statusMessage = "ISO written successfully!"
                    self.progress = 1.0
                }
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
    
    func triggerManualDiskAccessRequest() {
        requestDiskAccess()
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
