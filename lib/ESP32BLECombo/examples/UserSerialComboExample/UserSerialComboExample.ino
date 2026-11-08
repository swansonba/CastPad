#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <ESP32BLECombo.h>

#define DEVICE_NAME   "ESP32 Keyboard"
#define MANUFACTURER  "Espressif"

ESP32BLECombo ble;

String trimCopy(String s) {
    s.trim();
    return s;
}

bool handleCommand(String input) {
    input.trim();
    if (!input.startsWith("/")) return false;

    if (input.startsWith("/move ")) {
        String args = input.substring(6);
        int comma = args.indexOf(',');
        if (comma < 0) {
            Serial.println("Usage: /move x,y");
            return true;
        }

        int x = trimCopy(args.substring(0, comma)).toInt();
        int y = trimCopy(args.substring(comma + 1)).toInt();
        ble.mouseMove(x, y);
        Serial.println("Moved: " + String(x) + "," + String(y));
        return true;
    }

    if (input.startsWith("/scroll ")) {
        int n = input.substring(8).toInt();
        ble.mouseScroll(n);
        Serial.println("Scrolled: " + String(n));
        return true;
    }

    if (input.startsWith("/click ")) {
        String which = trimCopy(input.substring(7));
        which.toLowerCase();

        if (which == "left") {
            ble.mouseClick(ESP32BLECombo::MOUSE_LEFT);
            Serial.println("Clicked: left");
        } else if (which == "right") {
            ble.mouseClick(ESP32BLECombo::MOUSE_RIGHT);
            Serial.println("Clicked: right");
        } else if (which == "middle") {
            ble.mouseClick(ESP32BLECombo::MOUSE_MIDDLE);
            Serial.println("Clicked: middle");
        } else {
            Serial.println("Usage: /click left|right|middle");
        }
        return true;
    }

    if (input == "/release") {
        ble.mouseRelease(
            ESP32BLECombo::MOUSE_LEFT |
            ESP32BLECombo::MOUSE_RIGHT |
            ESP32BLECombo::MOUSE_MIDDLE
        );
        Serial.println("Released all mouse buttons");
        return true;
    }

    if (input.startsWith("/battery ")) {
        int pct = constrain(input.substring(9).toInt(), 0, 100);
        ble.setBatteryLevel((uint8_t)pct);
        Serial.println("Battery set to: " + String(pct) + "%");
        return true;
    }

    Serial.println("Unknown command");
    return true;
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    Serial.println("Starting BLE HID Keyboard + Mouse...");

    ESP32BLEComboConfig cfg;
    cfg.mode = ESP32BLEComboMode::KEYBOARD_MOUSE;
    cfg.deviceName = DEVICE_NAME;
    cfg.manufacturer = MANUFACTURER;
    cfg.batteryLevel = 70;
    cfg.appearance = ESP32BLEComboAppearance::KEYBOARD;
    cfg.enableSecurity = true;

    // Optional static BLE address override:
    // cfg.useStaticAddress = true;
    // cfg.staticAddress = "C0:FF:EE:12:34:56";

    ble.begin(cfg);

    Serial.println("Advertising as 'ESP32 Keyboard'");
    Serial.println("IMPORTANT: remove old Bluetooth pairing, then pair again.");
    Serial.println("Commands:");
    Serial.println("  /move x,y");
    Serial.println("  /scroll n");
    Serial.println("  /click left");
    Serial.println("  /click right");
    Serial.println("  /click middle");
    Serial.println("  /release");
    Serial.println("  /battery n");
    Serial.println("Any other line is typed as keyboard text.");
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (!ble.isConnected()) {
            Serial.println("Not connected — waiting for BLE pair");
            return;
        }

        if (!handleCommand(input)) {
            ble.print(input);
            delay(50);
            Serial.println("Sent: " + input);
        }
    }
}
