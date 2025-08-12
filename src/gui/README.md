# Remus GUI (SwiftUI)

Graficzny interfejs użytkownika dla Remus - portu Rufus na macOS.

## Funkcje

### Zaimplementowane:
- 📱 Natywny interfejs macOS w SwiftUI
- 🔍 Automatyczne wykrywanie urządzeń USB  
- 💾 Obsługa formatowania FAT32, exFAT, NTFS
- ⚙️ Opcje zaawansowane (rozwijane)
- 📊 Pasek postępu w czasie rzeczywistym
- 📝 Okno logów z historią operacji
- 🎨 Wspiera tryb ciemny macOS
- ⚠️ Okna dialogowe ostrzeżeń przed formatowaniem

### Interface podobny do Rufus:
- **Drive Properties** - sekcja właściwości urządzenia
- **Boot Selection** - wybór typu bootowania
- **Format Options** - opcje formatowania
- **Advanced Options** - zaawansowane ustawienia (rozwijane)
- **Progress/Status** - pasek postępu i status
- **Action Buttons** - przyciski akcji

## Budowanie

### Wymagania:
- macOS 12.0+
- Xcode 14.0+
- Swift 5.7+

### Instrukcje:

1. Otwórz projekt w Xcode:
   ```bash
   open src/gui/Remus.xcodeproj
   ```

2. Zbuduj aplikację:
   - Product → Build (⌘+B)

3. Uruchom aplikację:
   - Product → Run (⌘+R)

## Architektura

```
src/gui/
├── Remus.xcodeproj/          # Projekt Xcode
├── Remus/
│   ├── ContentView.swift     # Główny interfejs
│   ├── RemusViewModel.swift  # Logika aplikacji
│   ├── LogWindow.swift       # Okno logów
│   ├── Assets.xcassets/      # Zasoby aplikacji
│   └── Remus.entitlements    # Uprawnienia sandboxu
```

### Komponenty UI:
- `HeaderView` - Nagłówek z logo i tytułem
- `DrivePropertiesSection` - Lista i info o urządzeniach
- `BootSelectionSection` - Wybór typu bootowania
- `FormatOptionsSection` - Opcje systemu plików
- `AdvancedOptionsSection` - Zaawansowane opcje (rozwijane)
- `StatusProgressSection` - Pasek postępu
- `ActionButtonsSection` - Przyciski Start/Cancel/Log

### Model danych:
- `RemusDevice` - Struktura urządzenia USB
- `RemusViewModel` - ObservableObject z logiką
- Komunikacja z CLI przez `Process`

## Integracja z CLI

GUI komunikuje się z narzędziem command-line `remus`:

```swift
// Listowanie urządzeń
remus --list

// Formatowanie
remus --device disk2 --filesystem FAT32 --name "MY_USB"
```

## Uprawnienia (Sandbox)

Aplikacja wymaga następujących uprawnień:
- `com.apple.security.files.user-selected.read-write` - wybór plików
- `com.apple.security.device.usb` - dostęp do USB
- `com.apple.security.files.downloads.read-write` - zapis w Downloads

## Rozprowadzanie

### Dla deweloperów:
```bash
# Budowanie release
xcodebuild -project Remus.xcodeproj -scheme Remus -configuration Release build
```

### Dla użytkowników:
1. Pobierz Remus.app z Releases
2. Przeciągnij do Applications
3. Przy pierwszym uruchomieniu: System Preferences → Security & Privacy → Allow

## Różnice od Windows Rufus

### Dostosowania macOS:
- **SwiftUI** zamiast Win32 API
- **Natywne kontrolki** macOS (NSPicker, NSProgressIndicator)
- **Sandbox** - ograniczony dostęp do systemu
- **Okno logów** jako oddzielny sheet
- **SF Symbols** jako ikony

### Zachowane elementy:
- Układ sekcji podobny do oryginału
- Te same opcje formatowania
- Podobny flow operacji
- Ostrzeżenia przed usunięciem danych

## Screenshots

```
┌─────────────────────────────────────┐
│ 🔷 Remus                            │
│    The Reliable USB Formatting Utility │
├─────────────────────────────────────┤
│ 💾 Drive Properties                │  
│ Device: [SanDisk USB 3.0 ▼]    🔄  │
│ ┌─────────────────────────────────┐ │
│ │ Size: 32.00 GB                  │ │
│ │ Label: NO_LABEL                 │ │
│ │ VID:PID: 0781:5567              │ │
│ └─────────────────────────────────┘ │
├─────────────────────────────────────┤
│ 🔄 Boot Selection                  │
│ [Non bootable ▼]                    │
├─────────────────────────────────────┤
│ 💾 Format Options                  │
│ File system: [FAT32 ▼] Cluster: [Default ▼] │
│ Volume label: [USB_DRIVE          ] │
├─────────────────────────────────────┤
│ ▶ Advanced Options                  │
│   ☑ Quick format                   │
│   ☐ Check device for bad blocks    │
├─────────────────────────────────────┤
│ Ready                          0%   │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
├─────────────────────────────────────┤
│ [List Devices] [Show Log]    [START] │
└─────────────────────────────────────┘
```

## Contributing

Ten GUI jest częścią projektu Remus. Zobacz główny README.md dla instrukcji kontrybucji.

## Licencja

GPL v3 - jak cały projekt Remus.
