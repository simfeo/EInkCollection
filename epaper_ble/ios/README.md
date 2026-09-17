# iOS app

SwiftUI client for the e-paper display. Same job as the Android app and the
same BLE protocol — pick a photo, dither it, push 4736 bytes to the C3.

```
ios/
├── EpaperBLE.xcodeproj
└── EpaperBLE/
    ├── EpaperBLEApp.swift      entry point
    ├── ContentView.swift       UI: preview, settings, scan/connect/upload
    ├── Dither.swift            image pipeline (port of the Android/browser one)
    └── EpaperBleClient.swift   CoreBluetooth central, PROTOCOL.md
```

## Build

```bash
open EpaperBLE.xcodeproj
```

Or from the command line:

```bash
xcodebuild -project EpaperBLE.xcodeproj -scheme EpaperBLE \
  -destination 'platform=iOS Simulator,name=iPhone 17 Pro' build
```

Deployment target is iOS 17. The project uses Xcode 16+ file-system
synchronized groups, so new files in `EpaperBLE/` are picked up without editing
the project.

## Running on a real device

**Bluetooth does not work in the Simulator.** The UI and the whole image
pipeline do, so the Simulator is fine for everything except the transfer — but
Scan will never find the display there.

For a real iPhone you need a signing team: select the EpaperBLE target →
Signing & Capabilities → check *Automatically manage signing* → pick your Apple
ID. A free account works; the build expires after seven days.

`NSBluetoothAlwaysUsageDescription` is set through build settings
(`INFOPLIST_KEY_…`) rather than a checked-in `Info.plist`, which is why there
isn't one in the folder.

## Parity with the Android app

`Dither.swift` mirrors `Dither.kt` stage for stage — stepwise downscale, auto
levels, brightness, contrast, unsharp mask, then serpentine error diffusion with
unclamped error, or an ordered Bayer matrix. Same eight modes, same defaults
(Atkinson, auto levels on, sharpen 0.6).

The Bayer matrix is built least-significant-bit first. Reversing that bit order
still yields a plausible-looking matrix but clusters the low thresholds into one
quadrant, which dithers in visible blocks — the Android project has a unit test
pinning this; there is no test target here.

## Status

Built and run in the Simulator: compiles clean, launches, loads a photo, and
renders the dithered preview. The BLE path is **not** exercised — the Simulator
has no Bluetooth, so it needs a real iPhone plus the C3.
