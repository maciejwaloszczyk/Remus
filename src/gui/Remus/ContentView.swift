import SwiftUI
import Foundation

struct ContentView: View {
    @StateObject private var viewModel = RemusViewModel()
    @State private var sheetType: ActiveSheet? = nil
    
    var body: some View {
        VStack(spacing: 0) {
            // Main content
            ScrollView {
                VStack(spacing: 20) {
                    // Header with logo and title
                    HeaderView(sheetType: $sheetType)
                    
                    // Drive Properties Section
                    DrivePropertiesSection(viewModel: viewModel)
                    
                    // Boot Selection Section
                    BootSelectionSection(viewModel: viewModel)
                    
                    // Format Options Section
                    FormatOptionsSection(viewModel: viewModel)
                    
                    // Advanced Options (collapsible)
                    AdvancedOptionsSection(viewModel: viewModel)
                }
                .padding()
            }
            
            Divider()
            
            // Status and Progress Section
            StatusProgressSection(viewModel: viewModel)
            
            // Action Buttons
            ActionButtonsSection(viewModel: viewModel, sheetType: $sheetType)
        }
        .background(Color(.windowBackgroundColor))
        .sheet(item: $sheetType) { item in
            switch item {
            case .about: AboutView(isPresented: Binding(get: { sheetType == .about }, set: { if !$0 { sheetType = nil } }))
            case .settings: SettingsView(isPresented: Binding(get: { sheetType == .settings }, set: { if !$0 { sheetType = nil } }), viewModel: viewModel)
            case .log: LogView(isPresented: Binding(get: { sheetType == .log }, set: { if !$0 { sheetType = nil } }), viewModel: viewModel)
            }
        }
        .onAppear { viewModel.refreshDevices() }
    }
}

enum ActiveSheet: Identifiable { case about, settings, log; var id: Int { hashValue } }

struct HeaderView: View {
    @Binding var sheetType: ActiveSheet?
    var body: some View {
        HStack {
            Image(systemName: "externaldrive.fill")
                .font(.system(size: 32))
                .foregroundColor(.blue)
            
            VStack(alignment: .leading, spacing: 4) {
                Text("Remus")
                    .font(.title.bold())
                Text("The Reliable USB Formatting Utility")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            Spacer()
            
            // Settings and info buttons
            HStack(spacing: 8) {
                Button { sheetType = .about } label: {
                    Image(systemName: "info.circle")
                }
                .buttonStyle(.plain)
                
                Button { sheetType = .settings } label: {
                    Image(systemName: "gear")
                }
                .buttonStyle(.plain)
            }
        }
    }
}

struct DrivePropertiesSection: View {
    @ObservedObject var viewModel: RemusViewModel
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SectionHeader(title: "Drive Properties", icon: "externaldrive")
            
            VStack(spacing: 10) {
                HStack {
                    Text("Device:")
                        .frame(width: 100, alignment: .leading)
                    
                    Picker("Device", selection: $viewModel.selectedDevice) {
                        ForEach(viewModel.devices, id: \.devicePath) { device in
                            Text(device.displayName).tag(Optional(device))
                        }
                    }
                    .pickerStyle(.menu)
                    .frame(maxWidth: .infinity)
                    
                    Button(action: viewModel.refreshDevices) {
                        Image(systemName: "arrow.clockwise")
                    }
                    .buttonStyle(.borderless)
                }
                
                if let device = viewModel.selectedDevice {
                    VStack(spacing: 6) {
                        InfoRow(label: "Size:", value: String(format: "%.2f GB", Double(device.size) / (1024*1024*1024)))
                        InfoRow(label: "Label:", value: device.label ?? "NO_LABEL")
                        if device.props.vid != 0 && device.props.pid != 0 {
                            InfoRow(label: "VID:PID:", value: String(format: "%04X:%04X", device.props.vid, device.props.pid))
                        }
                        InfoRow(label: "Removable:", value: device.props.isRemovable ? "Yes" : "No")
                    }
                    .font(.caption)
                    .padding(8)
                    .background(Color(.controlBackgroundColor))
                    .cornerRadius(6)
                }
            }
        }
    }
}

struct BootSelectionSection: View {
    @ObservedObject var viewModel: RemusViewModel
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SectionHeader(title: "Boot Selection", icon: "restart")
            
            VStack(spacing: 10) {
                HStack {
                    Picker("Boot Type", selection: $viewModel.bootType) {
                        ForEach(BootType.allCases, id: \.self) { bt in
                            Text(bt.rawValue).tag(bt)
                        }
                    }
                    .pickerStyle(.menu)
                    .frame(maxWidth: .infinity)
                }
                
                if viewModel.bootType == .diskImage {
                    HStack {
                        Text("Image:")
                            .frame(width: 100, alignment: .leading)
                        
                        TextField("Select ISO file", text: $viewModel.imageFile)
                            .textFieldStyle(.roundedBorder)
                        
                        Button("SELECT") {
                            viewModel.selectImageFile()
                        }
                        .buttonStyle(.bordered)
                    }
                }
            }
        }
    }
}

struct FormatOptionsSection: View {
    @ObservedObject var viewModel: RemusViewModel
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SectionHeader(title: "Format Options", icon: "square.and.arrow.down")
            
            VStack(spacing: 10) {
                HStack {
                    Text("File system:")
                        .frame(width: 100, alignment: .leading)
                    
                    Picker("File System", selection: $viewModel.fileSystem) {
                        ForEach(FileSystem.allCases, id: \.self) { fs in
                            Text(fs.rawValue).tag(fs)
                        }
                    }
                    .pickerStyle(.menu)
                    
                    Spacer()
                    
                    Text("Cluster size:")
                        .frame(width: 80, alignment: .leading)
                    
                    Picker("Cluster Size", selection: $viewModel.clusterSize) {
                        ForEach(viewModel.availableClusterSizes, id: \.self) { size in
                            Text(size).tag(size)
                        }
                    }
                    .pickerStyle(.menu)
                    .frame(width: 100)
                }
                
                HStack {
                    Text("Volume label:")
                        .frame(width: 100, alignment: .leading)
                    
                    TextField("USB_DRIVE", text: $viewModel.volumeLabel)
                        .textFieldStyle(.roundedBorder)
                }
            }
        }
    }
}

struct AdvancedOptionsSection: View {
    @ObservedObject var viewModel: RemusViewModel
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            DisclosureGroup("Advanced Options", isExpanded: $viewModel.showAdvancedOptions) {
                VStack(alignment: .leading, spacing: 10) {
                    Toggle("Quick format", isOn: $viewModel.quickFormat)
                    Toggle("Check device for bad blocks", isOn: $viewModel.checkBadBlocks)
                    Toggle("Create extended label and icon files", isOn: $viewModel.createExtendedLabel)
                    
                    if viewModel.checkBadBlocks {
                        HStack {
                            Text("Bad block passes:")
                                .frame(width: 120, alignment: .leading)
                            
                            Picker("Passes", selection: $viewModel.badBlockPasses) {
                                Text("1").tag(1)
                                Text("2").tag(2)
                                Text("3").tag(3)
                                Text("4").tag(4)
                            }
                            .pickerStyle(.menu)
                            .frame(width: 60)
                            
                            Spacer()
                        }
                    }
                }
                .padding(.top, 8)
            }
            .font(.subheadline.weight(.medium))
        }
    }
}

struct StatusProgressSection: View {
    @ObservedObject var viewModel: RemusViewModel
    
    var body: some View {
        VStack(spacing: 8) {
            if viewModel.isOperationInProgress {
                VStack(spacing: 4) {
                    // progress jest w 0..1 więc total = 1
                    ProgressView(value: viewModel.progress, total: 1.0)
                        .progressViewStyle(.linear)
                    HStack {
                        Text(viewModel.statusMessage)
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Spacer()
                        if viewModel.progress > 0 {
                            Text(String(format: "%.1f%%", viewModel.progress * 100.0))
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }
            } else {
                Text(viewModel.statusMessage)
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color(.controlBackgroundColor))
    }
}

struct ActionButtonsSection: View {
    @ObservedObject var viewModel: RemusViewModel
    @Binding var sheetType: ActiveSheet?
    private var buttonText: String { viewModel.bootType == BootType.diskImage && !viewModel.imageFile.isEmpty ? "WRITE ISO" : "START" }
    
    var body: some View {
        HStack(spacing: 12) {
            Button("List Devices") {
                viewModel.listDevices()
            }
            .buttonStyle(.bordered)
            .disabled(viewModel.isOperationInProgress)
            
            Button("Show Log") {
                sheetType = .log
            }
            .buttonStyle(.bordered)
            
            Spacer()
            
            if viewModel.isOperationInProgress {
                Button("Cancel") {
                    viewModel.cancelOperation()
                }
                .buttonStyle(.bordered)
                .foregroundColor(.red)
            } else {
                Button(buttonText) {
                    if viewModel.bootType == .diskImage && !viewModel.imageFile.isEmpty {
                        viewModel.startWritingISO()
                    } else {
                        viewModel.startFormatting()
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(!viewModel.canStartFormatting)
            }
        }
        .padding()
    }
}

struct SectionHeader: View {
    let title: String
    let icon: String
    
    var body: some View {
        HStack {
            Image(systemName: icon)
                .foregroundColor(.blue)
            
            Text(title)
                .font(.subheadline.weight(.medium))
                .foregroundColor(.primary)
            
            Spacer()
            
            Rectangle()
                .fill(Color(.separatorColor))
                .frame(height: 1)
                .frame(maxWidth: .infinity)
        }
    }
}

struct InfoRow: View {
    let label: String
    let value: String
    
    var body: some View {
        HStack {
            Text(label)
                .foregroundColor(.secondary)
                .frame(width: 60, alignment: .leading)
            
            Text(value)
                .foregroundColor(.primary)
            
            Spacer()
        }
    }
}

struct AboutView: View {
    @Binding var isPresented: Bool
    private var versionString: String {
        let v = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
        let b = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "?"
        return "Version \(v) (Build \(b))"
    }
    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "externaldrive.fill")
                .font(.system(size: 48))
                .foregroundColor(.blue)
            Text("Remus")
                .font(.title.bold())
            Text(versionString)
                .font(.subheadline)
                .foregroundColor(.secondary)
            Text("Copyright © 2025 Maciej Wałoszczyk")
                .font(.footnote)
            Text("Licensed under GPLv3")
                .font(.footnote)
                .foregroundColor(.secondary)
            Button("Close") { isPresented = false }
                .keyboardShortcut(.cancelAction)
        }
        .frame(minWidth: 320, minHeight: 300)
        .padding()
    }
}

struct SettingsView: View {
    @Binding var isPresented: Bool
    @ObservedObject var viewModel: RemusViewModel
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Settings")
                .font(.headline)
            Form {
                Section("General") {
                    Toggle("Quick format by default", isOn: $viewModel.quickFormat)
                }
                Section("Advanced") {
                    Toggle("Show advanced format options", isOn: $viewModel.showAdvancedOptions)
                    Button("Request Full Disk Access…") { viewModel.triggerManualDiskAccessRequest() }
                        .help("Use only if device detection fails.")
                }
            }
            .frame(minWidth: 420, minHeight: 320)
            HStack { Spacer(); Button("Close") { isPresented = false } }
        }
        .padding()
    }
}

struct LogView: View {
    @Binding var isPresented: Bool
    @ObservedObject var viewModel: RemusViewModel
    var body: some View {
        VStack(alignment: .leading) {
            Text("Log").font(.headline)
            ScrollView {
                VStack(alignment: .leading, spacing: 4) {
                    ForEach(viewModel.logMessages, id: \.self) { Text($0).font(.caption.monospaced()) }
                }
            }
            HStack { Spacer(); Button("Close") { isPresented = false } }
        }.padding().frame(minWidth: 500, minHeight: 400)
    }
}

// Preview
#Preview {
    ContentView()
}
