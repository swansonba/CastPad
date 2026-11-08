#include <ESP32BLECombo.h>

ESP32BLECombo ble;

void setup() {
  Serial.begin(115200);

  ESP32BLEComboConfig cfg;
  cfg.mode = ESP32BLEComboMode::GAMEPAD_ONLY;
  cfg.deviceName = "ESP32 Gamepad";
  cfg.manufacturer = "Espressif";
  cfg.batteryLevel = 80;
  cfg.appearance = ESP32BLEComboAppearance::GAMEPAD;

  ble.begin(cfg);
  Serial.println("BLE gamepad ready");
  Serial.println("Commands:");
  Serial.println("  b1 on/off");
  Serial.println("  hat up/down/left/right/center");
  Serial.println("  axis x y z rz");
}

void loop() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();

  if (!ble.isConnected()) {
    Serial.println("Not connected");
    return;
  }

  if (line == "b1 on") ble.gamepadPress(1);
  else if (line == "b1 off") ble.gamepadRelease(1);
  else if (line == "hat up") ble.gamepadSetHat(ESP32BLECombo::HAT_UP);
  else if (line == "hat down") ble.gamepadSetHat(ESP32BLECombo::HAT_DOWN);
  else if (line == "hat left") ble.gamepadSetHat(ESP32BLECombo::HAT_LEFT);
  else if (line == "hat right") ble.gamepadSetHat(ESP32BLECombo::HAT_RIGHT);
  else if (line == "hat center") ble.gamepadSetHat(ESP32BLECombo::HAT_CENTERED);
  else if (line.startsWith("axis ")) {
    int x=0,y=0,z=0,rz=0;
    sscanf(line.c_str(), "axis %d %d %d %d", &x, &y, &z, &rz);
    ble.gamepadSetAxes((int16_t)x, (int16_t)y, (int16_t)z, (int16_t)rz);
  }
}
