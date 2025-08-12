import SwiftUI

// Przenieśmy @main do oddzielnego pliku
import SwiftUI

@main
struct RemusApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .windowToolbarStyle(UnifiedWindowToolbarStyle(showsTitle: false))
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
    }
}
