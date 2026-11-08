# CastPad — ESP32-S3 BLE 5.0 & Wired USB HID Web Remote

> ✨ **Vibe-coded with Gemini Argon** — Built end-to-end through conversational AI pair-programming using **Gemini Argon**.

Turn an **ESP32-S3 Super Mini** into a responsive **Bluetooth 5.0 (BLE)** or **Wired USB HID** composite Keyboard, Mouse, and Media Controller for **Amazon Fire TV**, Android TV, PCs, and tablets — controlled from any phone, tablet, or desktop browser over low-latency WebSockets with zero app installation required.

---

## Key Features

### 1. Dual Controller Output Modes
* **Bluetooth (BLE 5.0)**: Connects wirelessly as a composite HID Keyboard, Mouse, and Consumer Control device using NimBLE 2M PHY and adaptive connection parameters.
* **Wired USB HID**: Plugs directly into a host PC or TV USB port via the ESP32-S3 native TinyUSB stack — completely disabling the Bluetooth radio to save power.

### 2. Mobile-First Web Remote App
* **Remote Tab**:
  * Tactile 5-way D-Pad (`Up`, `Down`, `Left`, `Right`, `OK`) with auto-repeating directional hold.
  * Dedicated navigation & media rows (`Back`, `Home`, `Menu`, `Rewind`, `Play/Pause`, `Fast-Forward`, `Vol -`, `Vol +`, `Mute`).
  * **4 Color / Custom Macro Buttons**: Assign single actions or build multi-step **Macro Sequences** (key presses, modifier combos like `Ctrl+Alt+Del` or `Win+L`, text typing, and timed delays up to `5.0s`) with custom labels and emojis.
* **Quick Links Tab**:
  * Up to **5 customizable pages** (60 total slots) with custom page tabs, 8 color accents, and rich SVG presets (Netflix, Prime Video, YouTube, Disney+, Hulu, Max, Plex, Spotify, and more).
  * Supports `URL / App Launch`, `Text String`, `Key Press`, and multi-step `Macro Sequence` actions.
  * **Desktop Batch Edit Table**: Edit all links on a page side-by-side from a desktop browser.
* **Mouse & Trackpad Tab**:
  * Smooth sub-pixel touch trackpad with adjustable sensitivity (`0.5x – 3.0x`), tap-to-click, two-finger tap for Right-Click, and double-tap & hold to drag.
  * Dedicated vertical **Scroll Strip** on the right edge with coalesced 60 Hz HID reporting.
  * **Desktop PC Keyboard Passthrough**: Type directly on your physical computer keyboard while the Mouse tab is active on a desktop browser.
* **Dual-Mode Keyboard Tab**:
  * **`⚡ Live` Mode**: Real-time keystroke streaming with full mobile **Voice Dictation (Speech-to-Text)** and swipe-typing diff reconciliation.
  * **`📋 Batch` Mode**: Compose or paste long text/passwords locally with Show/Hide toggle (`👁️`) and send all at once.
  * **Quick-Edit Toolbar**: One-tap `Select All`, `Cut`, `Copy`, `Paste`, `Undo`, `Clear All`, URL domain chips (`www.`, `.com`, `.org`, `.net`), and a collapsible mini D-Pad.
* **Settings, Backup & Diagnostics**:
  * **Live Status Bar**: Real-time BLE/USB connection badge, active WebSocket client counter, and settings lock indicator.
  * **Configuration Backup & Restore**: Export or import all settings, custom buttons, and Quick Links as a single `.json` file.
  * **Smart Power Management**: Always-on 30s Zero-Client Auto-Idle CPU scaling (`240 MHz ↔ 80 MHz`), close-range Wi-Fi TX capping, and an optional **Power Saving Mode** (`WIFI_PS_MAX_MODEM`, continuous `80 MHz` lock, RSSI-adaptive TX power, and 60s LED sleep).
  * **Settings Lock**: Optional password protection for the Settings tab and Batch Edit mode.

---

## Hardware & Onboard Controls (ESP32-S3 Super Mini)

| Hardware | GPIO | Description |
| :--- | :--- | :--- |
| **WS2812 RGB LED** | `GPIO 48` | Color-coded status indicator (`🟢 Green` = BLE Connected, `🔵 Cyan` = Wired USB Mode, `🔵 Blue Blink` = BLE Waiting, `🟣 Purple Pulse` = Command Activity, `🟠 Amber Blink` = AP Mode) |
| **BOOT Button** | `GPIO 0` | Hold for **4 seconds** (`🟣 Magenta` flash) to toggle **Standalone AP Mode** (`CastPad-Setup` at `http://192.168.4.1`) without erasing saved Wi-Fi credentials |
| **Native USB Port** | `GPIO 19 / 20` | Hardware USB D- / D+ used for **Wired USB HID** mode and initial USB flashing |

---

## Getting Started

### 1. Build & Flash via PlatformIO
1. Clone this repository and open it in **VS Code** with the **PlatformIO** extension installed.
2. Build the firmware and compress the embedded web UI assets:
   ```powershell
   python scripts/compress.py
   pio run
   ```
3. Flash the firmware over USB (first-time setup):
   ```powershell
   pio run -t upload
   ```

### 2. Initial Wi-Fi Setup
1. On first boot (or when no saved Wi-Fi network is reachable), CastPad starts a Wi-Fi Access Point named **`CastPad-Setup`** (`🟢 Amber` slow blink).
2. Connect your phone or laptop to **`CastPad-Setup`** and open **`http://192.168.4.1`**.
3. Select your 2.4 GHz Wi-Fi network, enter the password, and click **Save & Connect**.
4. Once connected, access CastPad from any browser on your network via:
   * **mDNS URL**: [http://castpad.local](http://castpad.local)
   * **Direct IP**: Check your router or the serial monitor for the assigned IP address.

### 3. Pairing with Your Target Device
* **Bluetooth Mode**: On your Fire TV, Android TV, or PC, open **Bluetooth Settings → Pair New Device** and select **`CastPad`**.
* **Wired USB Mode**: Enable **Wired USB Mode** in **Settings → Controller Output Mode**, then plug the ESP32-S3 USB port directly into your target PC or TV.

---

## Over-The-Air (OTA) Firmware Updates

CastPad includes **automatic OTA rollback protection** — if a newly flashed firmware fails to boot or initialize the web server, the bootloader automatically reverts to the previous working slot.

### Option A: Web Browser OTA
1. Compile the firmware:
   ```powershell
   python scripts/compress.py
   pio run
   ```
2. Open **Settings → Firmware Update (OTA)** in the CastPad web app and select:
   `.pio/build/esp32-s3-supermini/firmware.bin`
3. Click **Upload & Flash Firmware**.

### Option B: Command-Line HTTP OTA
```powershell
curl.exe -F "firmware=@.pio/build/esp32-s3-supermini/firmware.bin" http://castpad.local/update
```

### Option C: PlatformIO ArduinoOTA
```powershell
pio run -t upload --upload-port castpad.local
```

---

## Credits

This entire project — including the dual-core ESP32-S3 firmware, BLE 5.0 / TinyUSB composite HID engine, and single-page glassmorphic web application — was **vibe-coded using Gemini Argon**.

