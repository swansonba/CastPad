# ESP32BLECombo

Arduino-style ESP32 BLE HID combo library built on NimBLE-Arduino. It can expose:
- keyboard only
- mouse only
- keyboard + mouse

## Features
- Configurable device name and manufacturer
- Configurable battery level at runtime
- Keyboard appearance, mouse appearance, or generic HID appearance
- Optional static random BLE address override before init
- Keyboard text/key send helpers
- Mouse move, click, press/release, and scroll helpers
- Single class with mode selection

## Notes
- Requires re-pairing after HID report map changes.
- Static address override is optional and platform/core dependent; if the core rejects the address, initialization continues with the default address.
- Appearance affects advertising metadata, but the host primarily determines capabilities from the HID report map.

## Basic use
```cpp
#include <ESP32BLECombo.h>

ESP32BLECombo ble;

void setup() {
  Serial.begin(115200);

  ESP32BLEComboConfig cfg;
  cfg.mode = ESP32BLEComboMode::KEYBOARD_MOUSE;
  cfg.deviceName = "ESP32 Combo";
  cfg.manufacturer = "Espressif";
  cfg.batteryLevel = 87;
  cfg.appearance = ESP32BLEComboAppearance::AUTO;

  ble.begin(cfg);
}

void loop() {
  if (ble.isConnected()) {
    ble.print("hello");
    delay(1000);
  }
}
```


## Typing reliability

Some hosts drop BLE HID key events when reports are sent too quickly. This library now exposes configurable typing delays through `keyPressDelayMs`, `keyReleaseDelayMs`, `keyIntervalDelayMs`, and the runtime setters `setKeyPressDelay`, `setKeyReleaseDelay`, and `setKeyIntervalDelay`.


## Gamepad support

The library now supports `ESP32BLEComboMode::GAMEPAD_ONLY` with a generic BLE HID gamepad report containing 16 buttons, four 16-bit axes (`x`, `y`, `z`, `rz`), and one hat switch. Use `gamepadPress`, `gamepadRelease`, `gamepadSetButtons`, `gamepadSetAxes`, and `gamepadSetHat`.


## Combined keyboard, mouse, and gamepad

Use `ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD` to expose all three HID functions at once through separate report IDs in one HID report descriptor. Re-pair the device after changing modes so the host reloads the descriptor.


## Xbox-style gamepad layout

Set `cfg.gamepadLayout = ESP32BLEGamepadLayout::XBOX_STYLE;` to expose a gamepad report shaped for the browser and app standard gamepad mapping expectation: buttons 0-3 align to A/B/X/Y order, the main axes are left stick on `x/y` and right stick on `z/rz`, and the hat switch remains the D-pad. This is still generic BLE HID, not true XInput, but it improves odds that gamepad tester sites render an Xbox-like preview when the host/browser applies standard mapping.
