#include <ESP32BLECombo.h>

ESP32BLECombo ble;

String trimCopy(String s) {
  s.trim();
  return s;
}

bool handleCommand(const String& raw) {
  String input = raw;
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
    Serial.println("Moved");
    return true;
  }

  if (input.startsWith("/scroll ")) {
    int n = input.substring(8).toInt();
    ble.mouseScroll(n);
    Serial.println("Scrolled");
    return true;
  }

  if (input.startsWith("/click ")) {
    String which = trimCopy(input.substring(7));
    which.toLowerCase();
    if (which == "left") ble.mouseClick(ESP32BLECombo::MOUSE_LEFT);
    else if (which == "right") ble.mouseClick(ESP32BLECombo::MOUSE_RIGHT);
    else if (which == "middle") ble.mouseClick(ESP32BLECombo::MOUSE_MIDDLE);
    else Serial.println("Usage: /click left|right|middle");
    return true;
  }

  if (input.startsWith("/battery ")) {
    int pct = constrain(input.substring(9).toInt(), 0, 100);
    ble.setBatteryLevel((uint8_t)pct);
    Serial.println("Battery updated");
    return true;
  }

  return true;
}

void setup() {
  Serial.begin(115200);

  ESP32BLEComboConfig cfg;
  cfg.mode = ESP32BLEComboMode::KEYBOARD_MOUSE;
  cfg.deviceName = "ESP32 Combo";
  cfg.manufacturer = "Espressif";
  cfg.batteryLevel = 70;
  cfg.appearance = ESP32BLEComboAppearance::AUTO;
  cfg.keyPressDelayMs = 20;
  cfg.keyReleaseDelayMs = 20;
  cfg.keyIntervalDelayMs = 10;

  // Optional static random BLE address override.
  // cfg.useStaticAddress = true;
  // cfg.staticAddress = "C0:FF:EE:12:34:56";

  ble.begin(cfg);
  ble.setKeyPressDelay(20);
  ble.setKeyReleaseDelay(20);
  ble.setKeyIntervalDelay(10);

  Serial.println("Ready. Re-pair after descriptor changes.");
}

void loop() {
  if (!Serial.available()) return;

  String input = Serial.readStringUntil('\n');
  input.trim();

  if (!ble.isConnected()) {
    Serial.println("Not connected");
    return;
  }

  if (!handleCommand(input)) {
    ble.print(input);
    Serial.println("Typed");
  }
}
