import SwiftUI

struct LogWindow: View {
    @ObservedObject var viewModel: RemusViewModel
    @Binding var isPresented: Bool
    
    var body: some View {
        VStack(spacing: 0) {
            // Header
            HStack {
                Text("Remus Log")
                    .font(.headline)
                
                Spacer()
                
                Button("Clear") {
                    viewModel.logMessages.removeAll()
                }
                .buttonStyle(.bordered)
                
                Button("Close") {
                    isPresented = false
                }
                .buttonStyle(.borderedProminent)
            }
            .padding()
            .background(Color(.controlBackgroundColor))
            
            Divider()
            
            // Log content
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 2) {
                        ForEach(Array(viewModel.logMessages.enumerated()), id: \.offset) { index, message in
                            Text(message)
                                .font(.system(.caption, design: .monospaced))
                                .textSelection(.enabled)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(.horizontal, 8)
                                .padding(.vertical, 1)
                                .background(index % 2 == 0 ? Color.clear : Color(.controlBackgroundColor).opacity(0.3))
                                .id(index)
                        }
                    }
                }
                .onChange(of: viewModel.logMessages.count) { _ in
                    if let lastIndex = viewModel.logMessages.indices.last {
                        withAnimation(.easeOut(duration: 0.3)) {
                            proxy.scrollTo(lastIndex, anchor: .bottom)
                        }
                    }
                }
            }
        }
        .frame(width: 600, height: 400)
        .background(Color(.textBackgroundColor))
    }
}
