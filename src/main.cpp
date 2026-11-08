#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>

#include "BLEManager.h"
#include "BleCommandQueue.h"
#include "ConfigManager.h"
#include "WebAssets.h"
#include "WebAssetsGz.h"

#if CONFIG_IDF_TARGET_ESP32S3
#undef LED_BUILTIN
#define LED_BUILTIN 47
#define RGB_LED_PIN 48
#define BOOT_BUTTON_PIN 0
#elif !defined(LED_BUILTIN)
#define LED_BUILTIN 5
#define BOOT_BUTTON_PIN 0
#endif

enum SystemState { STATE_CONNECTING_WIFI, STATE_AP_MODE, STATE_RUN_MODE };

static SystemState g_state = STATE_CONNECTING_WIFI;
static AppConfig g_config;

static DNSServer g_dnsServer;
static AsyncWebServer g_server(80);
static AsyncWebSocket g_ws("/ws");

static bool g_lastBleConnected = false;
static unsigned long g_lastLedToggle = 0;
static bool g_ledState = false;

// Deferred reboot: never delay()/restart inside AsyncTCP callbacks, otherwise
// the HTTP/WS response may never be flushed. loop() performs the restart.
static volatile bool g_restartPending = false;
static volatile unsigned long g_restartAt = 0;

// Web OTA upload state (only one upload is expected at a time)
static bool g_otaReceived = false;
static bool g_otaSuccess = false;

// AP-mode fallback: periodically retry the saved Wi-Fi network so the board
// recovers on its own when it booted before the router (e.g. after a power cut)
static constexpr unsigned long STA_RETRY_INTERVAL_MS = 60000;
static constexpr unsigned long STA_RETRY_WINDOW_MS = 15000;
static unsigned long g_lastStaRetry = 0;
static bool g_staRetrying = false;

// Defer OTA image validation until the web server & OTA endpoint are up and
// running so any boot-time crash automatically rolls back to the previous slot
bool verifyRollbackLater() { return true; }

static void confirmOtaAppValid() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t otaState;
  if (esp_ota_get_state_partition(running, &otaState) == ESP_OK) {
    if (otaState == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
      Serial.println("[OTA] New firmware verified & marked valid.");
    }
  }
}

static void scheduleRestart(unsigned long delayMs) {
  g_restartAt = millis() + delayMs;
  g_restartPending = true;
}

static bool hasSavedWifi() {
  return g_config.configured && !g_config.ssid.isEmpty();
}

static uint8_t parseMouseButton(const char *btn) {
  return (strcmp(btn, "right") == 0) ? ESP32BLECombo::MOUSE_RIGHT
                                     : ESP32BLECombo::MOUSE_LEFT;
}

// Hardware LED Helper: drives both the ESP32-S3 WS2812 RGB LED (GPIO 48) and
// the monochrome status LED (GPIO 47 / GPIO 5 active-LOW) with state caching so
// RMT neopixelWrite() is only invoked on actual color transitions.
static int16_t s_lastR = -1, s_lastG = -1, s_lastB = -1;
static int8_t s_lastMono = -1;

void setRgbLed(uint8_t r, uint8_t g, uint8_t b, bool monoOn) {
  if (g_config.ledDisabled) {
    r = 0;
    g = 0;
    b = 0;
    monoOn = false;
  }
  int8_t monoVal = monoOn ? 1 : 0;
  if (s_lastMono != monoVal) {
    s_lastMono = monoVal;
    digitalWrite(LED_BUILTIN, monoOn ? LOW : HIGH);
  }
#if CONFIG_IDF_TARGET_ESP32S3 && defined(RGB_LED_PIN)
  if (s_lastR != r || s_lastG != g || s_lastB != b) {
    s_lastR = r;
    s_lastG = g;
    s_lastB = b;
    neopixelWrite(RGB_LED_PIN, r, g, b);
  }
#endif
}

void setStatusLed(bool on) {
  if (!on || g_config.ledDisabled) {
    setRgbLed(0, 0, 0, false);
    return;
  }
  if (g_state == STATE_AP_MODE || g_state == STATE_CONNECTING_WIFI) {
    setRgbLed(32, 18, 0, true); // Warm Amber for Wi-Fi / AP Setup
  } else if (BLEManager::instance().isConnected()) {
    setRgbLed(0, 24, 16, true); // Dim Green/Cyan when connected / USB ready
  } else {
    setRgbLed(0, 12, 40, true); // Soft Blue when BLE waiting/pairing
  }
}

// Status LED & user activity state variables
static volatile int g_blinkTransitionsLeft = 0;
static volatile unsigned long g_lastBlinkTransition = 0;
static const unsigned long BLINK_INTERVAL_MS = 60; // 60ms pulse = 3 clean blinks
static volatile unsigned long g_lastActivityMs = 0;
static bool g_zeroClientIdleActive = false;

void markUserActivity() {
  g_lastActivityMs = millis();
  // If we were in always-on zero-client 80 MHz idle and Power Saving Mode is OFF,
  // immediately boost back to 240 MHz on user activity
  if (g_zeroClientIdleActive && !g_config.powerSave) {
    g_zeroClientIdleActive = false;
    setCpuFrequencyMhz(240);
  }
}

void triggerCommandBlink() {
  markUserActivity();
  if (g_config.ledDisabled) return;
  // 3 blinks = 6 transitions (OFF, ON, OFF, ON, OFF, ON)
  g_blinkTransitionsLeft = 6;
  g_lastBlinkTransition = millis();
  setRgbLed(36, 0, 48, true); // Immediate Purple flash on command start
}

void updateStatusLed(unsigned long now) {
  if (g_config.ledDisabled) {
    setRgbLed(0, 0, 0, false);
    return;
  }

  // 1. If a command blink sequence is active, handle it with priority (Purple pulse)
  if (g_blinkTransitionsLeft > 0) {
    if (now - g_lastBlinkTransition >= BLINK_INTERVAL_MS) {
      g_lastBlinkTransition = now;
      g_blinkTransitionsLeft--;
      if (g_blinkTransitionsLeft > 0) {
        bool on = (g_blinkTransitionsLeft % 2 == 1);
        if (on) {
          setRgbLed(36, 0, 48, true); // Purple command pulse
        } else {
          setRgbLed(0, 0, 0, false);
        }
      }
    }
    return;
  }

  // 2. Normal status LED behavior
  if (g_state == STATE_AP_MODE) {
    // In setup AP provisioning mode, slow Amber blink (500ms)
    if (now - g_lastLedToggle >= 500) {
      g_lastLedToggle = now;
      g_ledState = !g_ledState;
      if (g_ledState) {
        setRgbLed(32, 18, 0, true);
      } else {
        setRgbLed(0, 0, 0, false);
      }
    }
  } else if (g_state == STATE_RUN_MODE) {
    if (BLEManager::instance().isConnected()) {
      // In Power Saving Mode, auto-sleep the steady status LED after 60s of inactivity
      if (g_config.powerSave && (now - g_lastActivityMs >= 60000)) {
        setRgbLed(0, 0, 0, false);
      } else {
        setRgbLed(0, 24, 16, true); // Solid dim Green/Cyan once BLE or USB connected
      }
    } else if (!BLEManager::instance().isRadioEnabled()) {
      setRgbLed(0, 0, 0, false); // Radio disabled & not USB mode
    } else {
      // Wi-Fi connected, waiting for BLE: slow Blue blink (1000ms)
      if (now - g_lastLedToggle >= 1000) {
        g_lastLedToggle = now;
        g_ledState = !g_ledState;
        if (g_ledState) {
          setRgbLed(0, 12, 40, true);
        } else {
          setRgbLed(0, 0, 0, false);
        }
      }
    }
  }
}

// Physical BOOT button (GPIO 0, active-LOW) handler:
// Hold for 4 seconds to toggle Standalone AP Mode on/off and reboot.
static unsigned long g_bootBtnPressStart = 0;
static bool g_bootBtnPressed = false;
static bool g_bootBtnTriggered = false;

void handleBootButton(unsigned long now) {
#ifdef BOOT_BUTTON_PIN
  bool isDown = (digitalRead(BOOT_BUTTON_PIN) == LOW);
  if (isDown) {
    if (!g_bootBtnPressed) {
      g_bootBtnPressed = true;
      g_bootBtnTriggered = false;
      g_bootBtnPressStart = now;
    } else if (!g_bootBtnTriggered && (now - g_bootBtnPressStart >= 4000)) {
      g_bootBtnTriggered = true;
      g_config.apMode = !g_config.apMode;
      if (g_config.apMode) {
        g_config.configured = true;
      } else if (g_config.ssid.isEmpty()) {
        g_config.configured = false;
      }
      ConfigManager::save(g_config);
      Serial.printf("\n[BOOT Button] 4s long-press detected! Toggling Standalone AP Mode -> %s. Rebooting...\n",
                    g_config.apMode ? "ENABLED" : "DISABLED");
      // Visual confirmation flash (even if LED is normally disabled, or respecting setting)
      for (int i = 0; i < 6; ++i) {
        if (i % 2 == 0) {
          setRgbLed(g_config.apMode ? 48 : 0, g_config.apMode ? 24 : 48, 0, true);
        } else {
          setRgbLed(0, 0, 0, false);
        }
        delay(80);
      }
      scheduleRestart(200);
    }
  } else {
    g_bootBtnPressed = false;
    g_bootBtnTriggered = false;
  }
#endif
}

// Helper to broadcast BLE & Wi-Fi status to web clients
void broadcastStatus() {
  JsonDocument doc;
  doc["type"] = "status";
  doc["ble_connected"] = BLEManager::instance().isConnected();
  doc["ble_radio"] = BLEManager::instance().isRadioEnabled();
  doc["usb_mode"] = g_config.usbMode;
  doc["hostname"] = g_config.hostname;
  doc["ap_mode"] = g_config.apMode;
  doc["rssi"] = g_config.apMode ? 0 : (int)WiFi.RSSI();

  String json;
  serializeJson(doc, json);
  g_ws.textAll(json);
}

static wifi_power_t s_lastTxPower = WIFI_POWER_19_5dBm;

void updateAdaptiveWifiTxPower() {
  if (g_state != STATE_RUN_MODE) return;
  wifi_power_t target = WIFI_POWER_19_5dBm;
  if (g_config.apMode) {
    target = g_config.powerSave ? WIFI_POWER_13dBm : WIFI_POWER_17dBm;
  } else if (WiFi.status() == WL_CONNECTED) {
    int8_t rssi = WiFi.RSSI();
    if (g_config.powerSave) {
      // Power Saving Mode ON: aggressive RSSI-based TX power reduction
      if (rssi > -65 && rssi != 0) {
        target = WIFI_POWER_11dBm;
      } else if (rssi > -72 && rssi != 0) {
        target = WIFI_POWER_13dBm;
      } else {
        target = WIFI_POWER_15dBm;
      }
    } else {
      // Always-On zero-compromise ceiling: trim TX power to 15 dBm when close
      // to the router (RSSI > -60 dBm) to prevent unnecessary heat spikes
      if (rssi > -60 && rssi != 0) {
        target = WIFI_POWER_15dBm;
      } else {
        target = WIFI_POWER_19_5dBm;
      }
    }
  }
  if (target != s_lastTxPower) {
    s_lastTxPower = target;
    WiFi.setTxPower(target);
  }
}

void applyPowerSave(bool enable) {
  markUserActivity();
  g_zeroClientIdleActive = false;
  if (enable) {
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    setCpuFrequencyMhz(80);
    Serial.println("[POWER] Power saving ENABLED (Modem-sleep MAX, CPU 80 MHz, Adaptive Low TX, 60s LED Sleep)");
  } else {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    setCpuFrequencyMhz(240);
    Serial.println("[POWER] Power saving DISABLED (Full Performance 240 MHz when active, 80 MHz auto-idle when 0 clients)");
  }
  updateAdaptiveWifiTxPower();
}

void updatePowerManagement(unsigned long now) {
  if (g_state != STATE_RUN_MODE) return;

  // Keep activity timestamp fresh while any browser WebSocket client is connected
  if (g_ws.count() > 0) {
    if (g_zeroClientIdleActive && !g_config.powerSave) {
      g_zeroClientIdleActive = false;
      setCpuFrequencyMhz(240);
    }
  } else if (!g_config.powerSave) {
    // Always-On Zero-Client Auto-Idle: when 0 WebSocket clients are open for 30s,
    // scale CPU down to 80 MHz while keeping WIFI_PS_MIN_MODEM for instant wake
    if (!g_zeroClientIdleActive && (now - g_lastActivityMs >= 30000)) {
      g_zeroClientIdleActive = true;
      setCpuFrequencyMhz(80);
    }
  }

  // Re-evaluate RSSI-adaptive Wi-Fi TX power every 15 seconds
  static unsigned long s_lastTxCheck = 0;
  static int8_t s_lastBroadcastRssi = 0;
  if (now - s_lastTxCheck >= 15000) {
    s_lastTxCheck = now;
    updateAdaptiveWifiTxPower();
    if (g_ws.count() > 0 && !g_config.apMode && WiFi.status() == WL_CONNECTED) {
      int8_t curRssi = WiFi.RSSI();
      if (abs((int)curRssi - (int)s_lastBroadcastRssi) >= 2) {
        s_lastBroadcastRssi = curRssi;
        broadcastStatus();
      }
    }
  }
}

// Helper to build settings JSON
void buildSettingsJson(String &json) {
  JsonDocument doc;
  doc["type"] = "settings";
  doc["sens"] = g_config.mouseSensitivity;
  doc["bleRadio"] = BLEManager::instance().isRadioEnabled();
  doc["btnRed"] = g_config.btnRed;
  doc["btnGreen"] = g_config.btnGreen;
  doc["btnYellow"] = g_config.btnYellow;
  doc["btnBlue"] = g_config.btnBlue;
  doc["btnCustom1"] = g_config.btnCustom1;
  doc["btnCustom2"] = g_config.btnCustom2;
  doc["btnCustom3"] = g_config.btnCustom3;
  doc["btnCustom4"] = g_config.btnCustom4;
  doc["lblRed"] = g_config.lblRed;
  doc["lblGreen"] = g_config.lblGreen;
  doc["lblYellow"] = g_config.lblYellow;
  doc["lblBlue"] = g_config.lblBlue;
  doc["lblCustom1"] = g_config.lblCustom1;
  doc["lblCustom2"] = g_config.lblCustom2;
  doc["lblCustom3"] = g_config.lblCustom3;
  doc["lblCustom4"] = g_config.lblCustom4;
  doc["iconRed"] = g_config.iconRed;
  doc["iconGreen"] = g_config.iconGreen;
  doc["iconYellow"] = g_config.iconYellow;
  doc["iconBlue"] = g_config.iconBlue;
  doc["iconCustom1"] = g_config.iconCustom1;
  doc["iconCustom2"] = g_config.iconCustom2;
  doc["iconCustom3"] = g_config.iconCustom3;
  doc["iconCustom4"] = g_config.iconCustom4;
  doc["colRed"] = g_config.colRed;
  doc["colGreen"] = g_config.colGreen;
  doc["colYellow"] = g_config.colYellow;
  doc["colBlue"] = g_config.colBlue;
  doc["colCustom1"] = g_config.colCustom1;
  doc["colCustom2"] = g_config.colCustom2;
  doc["colCustom3"] = g_config.colCustom3;
  doc["colCustom4"] = g_config.colCustom4;
  doc["qlPages"] = g_config.qlPages;

  JsonArray qlArr = doc["quickLinks"].to<JsonArray>();
  int totalSlots = g_config.qlPages * QUICK_LINKS_PAGE_SIZE;
  if (totalSlots > QUICK_LINKS_COUNT)
    totalSlots = QUICK_LINKS_COUNT;
  for (int i = 0; i < totalSlots; ++i) {
    JsonObject item = qlArr.add<JsonObject>();
    item["label"] = g_config.qlLabel[i];
    item["num"] = g_config.qlNum[i];
    item["color"] = g_config.qlColor[i];
    item["icon"] = g_config.qlIcon[i];
  }

  doc["host"] = g_config.hostname;
  doc["ssid"] =
      g_config.apMode
          ? (g_config.hostname + " (AP Mode)")
          : (WiFi.SSID().length() > 0 ? WiFi.SSID() : String("Connected"));
  doc["ip"] =
      g_config.apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["rssi"] = g_config.apMode ? 0 : (int)WiFi.RSSI();
  doc["uptime"] = (uint32_t)(millis() / 1000);
  doc["sens"] = g_config.mouseSensitivity;
  doc["bleRadio"] = BLEManager::instance().isRadioEnabled();
  doc["usbMode"] = g_config.usbMode;
  doc["apMode"] = g_config.apMode;
  doc["powerSave"] = g_config.powerSave;
  doc["ledDisabled"] = g_config.ledDisabled;
  doc["theme"] =
      g_config.themeColor.length() > 0 ? g_config.themeColor : "cyan";
  doc["themeMode"] =
      g_config.themeMode.length() > 0 ? g_config.themeMode : "dark";
  doc["version"] = "2.8.3";

  serializeJson(doc, json);
}

// Helper to send current settings to a client
void sendSettings(AsyncWebSocketClient *client) {
  String json;
  buildSettingsJson(json);
  client->text(json);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    markUserActivity();
    Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                  client->remoteIP().toString().c_str());

    JsonDocument doc;
    doc["type"] = "status";
    doc["ble_connected"] = BLEManager::instance().isConnected();
    doc["ble_radio"] = BLEManager::instance().isRadioEnabled();
    doc["usb_mode"] = g_config.usbMode;
    doc["hostname"] = g_config.hostname;
    doc["ap_mode"] = g_config.apMode;
    doc["rssi"] = g_config.apMode ? 0 : (int)WiFi.RSSI();
    String json;
    serializeJson(doc, json);
    client->text(json);

    sendSettings(client);
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
    if (client->_tempObject) {
      delete (String *)client->_tempObject;
      client->_tempObject = nullptr;
    }
  } else if (type == WS_EVT_DATA) {
    markUserActivity();
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->message_opcode != WS_TEXT)
      return;

    // Ultra-fast compact path for real-time mouse and key events (<10 microseconds)
    if (info->final && info->index == 0 && info->len == len && len >= 3 && len < 64 && data[0] != '{') {
      char buf[64];
      memcpy(buf, data, len);
      buf[len] = '\0';
      if (buf[0] == 'm' && buf[1] == ',') {
        int dx = 0, dy = 0;
        if (sscanf(buf, "m,%d,%d", &dx, &dy) == 2) {
          BleCommandQueue::post(BleCmdType::MouseMove, dx, dy);
        }
        return;
      } else if (buf[0] == 's' && buf[1] == ',') {
        int amt = 0;
        if (sscanf(buf, "s,%d", &amt) == 1) {
          BleCommandQueue::post(BleCmdType::MouseScroll, amt);
        }
        return;
      } else if (buf[0] == 'k' && buf[1] == ',') {
        String act = String(buf + 2);
        if (act != " ")
          act.trim();
        BleCommandQueue::postText(BleCmdType::Key, act);
        return;
      } else if (buf[0] == 't' && buf[1] == ',') {
        BleCommandQueue::postText(BleCmdType::TypeTextRaw, String(buf + 2));
        return;
      } else if (buf[0] == 'c' && buf[1] == ',') {
        char col = buf[2];
        String action = "";
        if (col == 'r')
          action = g_config.btnRed;
        else if (col == 'g')
          action = g_config.btnGreen;
        else if (col == 'y')
          action = g_config.btnYellow;
        else if (col == 'b')
          action = g_config.btnBlue;
        else if (col == '1')
          action = g_config.btnCustom1;
        else if (col == '2')
          action = g_config.btnCustom2;
        else if (col == '3')
          action = g_config.btnCustom3;
        else if (col == '4')
          action = g_config.btnCustom4;
        if (!action.isEmpty() && !action.equalsIgnoreCase("NONE")) {
          if (action.startsWith("SEQ:")) {
            BleCommandQueue::postText(BleCmdType::Sequence, action.substring(4));
          } else {
            BleCommandQueue::postText(BleCmdType::Key, action);
          }
        }
        return;
      } else if (buf[0] == 'q' && buf[1] == ',') {
        int idx = atoi(buf + 2);
        if (idx >= 0 && idx < QUICK_LINKS_COUNT) {
          String num = g_config.qlNum[idx];
          num.trim();
          if (!num.isEmpty()) {
            if (num.startsWith("SEQ:")) {
              BleCommandQueue::postText(BleCmdType::Sequence, num.substring(4));
            } else {
              BleCommandQueue::postText(BleCmdType::TypeText, num);
            }
          }
        }
        return;
      } else if (len >= 4 && buf[0] == 'm' && buf[1] == 'c') {
        BleCommandQueue::post(BleCmdType::MouseClick,
                              buf[3] == 'r' ? ESP32BLECombo::MOUSE_RIGHT
                                            : ESP32BLECombo::MOUSE_LEFT);
        return;
      } else if (len >= 4 && buf[0] == 'm' && buf[1] == 'd') {
        BleCommandQueue::post(BleCmdType::MousePress,
                              buf[3] == 'r' ? ESP32BLECombo::MOUSE_RIGHT
                                            : ESP32BLECombo::MOUSE_LEFT);
        return;
      } else if (len >= 4 && buf[0] == 'm' && buf[1] == 'u') {
        BleCommandQueue::post(BleCmdType::MouseRelease,
                              buf[3] == 'r' ? ESP32BLECombo::MOUSE_RIGHT
                                            : ESP32BLECombo::MOUSE_LEFT);
        return;
      }
    }

    // Accumulate fragmented or multi-packet frames
    const uint8_t *msgData = data;
    size_t msgLen = len;

    if (!info->final || info->opcode == WS_CONTINUATION || info->index > 0 || info->len != len) {
      if (info->opcode != WS_CONTINUATION && info->index == 0) {
        if (client->_tempObject) {
          delete (String *)client->_tempObject;
          client->_tempObject = nullptr;
        }
        client->_tempObject = new (std::nothrow) String();
      }
      String *accum = (String *)client->_tempObject;
      if (accum) {
        accum->concat((const char *)data, len);
        if (!info->final || (info->index + len < info->len)) {
          return; // Wait for complete frame / message
        }
        msgData = (const uint8_t *)accum->c_str();
        msgLen = accum->length();
      }
    }

    // Parse accumulated JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msgData, msgLen);

    if (client->_tempObject) {
      delete (String *)client->_tempObject;
      client->_tempObject = nullptr;
    }

    if (err)
      return;

      const char *typeStr = doc["type"] | "";

      if (strcmp(typeStr, "key") == 0) {
        const char *action = doc["action"] | "";
        if (strncmp(action, "SEQ:", 4) == 0) {
          BleCommandQueue::postText(BleCmdType::Sequence, action + 4);
        } else {
          BleCommandQueue::postText(BleCmdType::Key, action);
        }
      } else if (strcmp(typeStr, "sequence") == 0) {
        const char *seq = doc["seq"] | "";
        if (strlen(seq) > 0) {
          BleCommandQueue::postText(BleCmdType::Sequence, seq);
        }
      } else if (strcmp(typeStr, "color_btn") == 0) {
        const char *color = doc["color"] | "";
        String action = "";
        if (strcmp(color, "red") == 0)
          action = g_config.btnRed;
        else if (strcmp(color, "green") == 0)
          action = g_config.btnGreen;
        else if (strcmp(color, "yellow") == 0)
          action = g_config.btnYellow;
        else if (strcmp(color, "blue") == 0)
          action = g_config.btnBlue;
        else if (strcmp(color, "custom1") == 0)
          action = g_config.btnCustom1;
        else if (strcmp(color, "custom2") == 0)
          action = g_config.btnCustom2;
        else if (strcmp(color, "custom3") == 0)
          action = g_config.btnCustom3;
        else if (strcmp(color, "custom4") == 0)
          action = g_config.btnCustom4;

        if (!action.isEmpty() && !action.equalsIgnoreCase("NONE")) {
          if (action.startsWith("SEQ:")) {
            BleCommandQueue::postText(BleCmdType::Sequence, action.substring(4));
          } else {
            BleCommandQueue::postText(BleCmdType::Key, action);
          }
        }
      } else if (strcmp(typeStr, "quick_link") == 0) {
        int idx = doc["index"] | -1;
        if (idx >= 0 && idx < QUICK_LINKS_COUNT) {
          String num = g_config.qlNum[idx];
          num.trim();
          if (!num.isEmpty()) {
            if (num.startsWith("SEQ:")) {
              BleCommandQueue::postText(BleCmdType::Sequence, num.substring(4));
            } else {
              BleCommandQueue::postText(BleCmdType::TypeText, num);
            }
          }
        }
      } else if (strcmp(typeStr, "type_text") == 0) {
        const char *text = doc["text"] | "";
        bool withEnter = doc["enter"] | false;
        BleCommandQueue::postText(
            withEnter ? BleCmdType::TypeText : BleCmdType::TypeTextRaw, text);
      } else if (strcmp(typeStr, "mouse_move") == 0) {
        int dx = doc["dx"] | 0;
        int dy = doc["dy"] | 0;
        BleCommandQueue::post(BleCmdType::MouseMove, dx, dy);
      } else if (strcmp(typeStr, "mouse_scroll") == 0) {
        int amount = doc["amount"] | 0;
        BleCommandQueue::post(BleCmdType::MouseScroll, amount);
      } else if (strcmp(typeStr, "mouse_click") == 0) {
        BleCommandQueue::post(BleCmdType::MouseClick,
                              parseMouseButton(doc["button"] | "left"));
      } else if (strcmp(typeStr, "mouse_down") == 0) {
        BleCommandQueue::post(BleCmdType::MousePress,
                              parseMouseButton(doc["button"] | "left"));
      } else if (strcmp(typeStr, "mouse_up") == 0) {
        BleCommandQueue::post(BleCmdType::MouseRelease,
                              parseMouseButton(doc["button"] | "left"));
      } else if (strcmp(typeStr, "get_settings") == 0) {
        sendSettings(client);
      } else if (strcmp(typeStr, "save_button") == 0) {
        const char *id = doc["id"] | "";
        const char *act = doc["action"] | "";
        const char *lbl = doc["label"] | "";
        const char *icon = doc["icon"] | "";
        const char *col = doc["color"] | "";

        if (strcmp(id, "red") == 0) {
          if (strlen(act) > 0) g_config.btnRed = act;
          g_config.lblRed = lbl;
          g_config.iconRed = icon;
          g_config.colRed = col;
        } else if (strcmp(id, "green") == 0) {
          if (strlen(act) > 0) g_config.btnGreen = act;
          g_config.lblGreen = lbl;
          g_config.iconGreen = icon;
          g_config.colGreen = col;
        } else if (strcmp(id, "yellow") == 0) {
          if (strlen(act) > 0) g_config.btnYellow = act;
          g_config.lblYellow = lbl;
          g_config.iconYellow = icon;
          g_config.colYellow = col;
        } else if (strcmp(id, "blue") == 0) {
          if (strlen(act) > 0) g_config.btnBlue = act;
          g_config.lblBlue = lbl;
          g_config.iconBlue = icon;
          g_config.colBlue = col;
        } else if (strcmp(id, "custom1") == 0) {
          if (strlen(act) > 0) g_config.btnCustom1 = act;
          g_config.lblCustom1 = lbl;
          g_config.iconCustom1 = icon;
          g_config.colCustom1 = col;
        } else if (strcmp(id, "custom2") == 0) {
          if (strlen(act) > 0) g_config.btnCustom2 = act;
          g_config.lblCustom2 = lbl;
          g_config.iconCustom2 = icon;
          g_config.colCustom2 = col;
        } else if (strcmp(id, "custom3") == 0) {
          if (strlen(act) > 0) g_config.btnCustom3 = act;
          g_config.lblCustom3 = lbl;
          g_config.iconCustom3 = icon;
          g_config.colCustom3 = col;
        } else if (strcmp(id, "custom4") == 0) {
          if (strlen(act) > 0) g_config.btnCustom4 = act;
          g_config.lblCustom4 = lbl;
          g_config.iconCustom4 = icon;
          g_config.colCustom4 = col;
        }
        ConfigManager::save(g_config);
        String json;
        buildSettingsJson(json);
        g_ws.textAll(json);
      } else if (strcmp(typeStr, "save_quick_link") == 0) {
        int idx = doc["index"] | -1;
        if (idx >= 0 && idx < QUICK_LINKS_COUNT) {
          g_config.qlLabel[idx] = doc["label"] | "";
          g_config.qlNum[idx] = doc["num"] | "";
          g_config.qlColor[idx] = doc["color"] | "";
          g_config.qlIcon[idx] = doc["icon"] | "";
          ConfigManager::save(g_config);
          String json;
          buildSettingsJson(json);
          g_ws.textAll(json);
        }
      } else if (strcmp(typeStr, "save_settings") == 0) {
        if (doc["sens"].is<float>())
          g_config.mouseSensitivity = doc["sens"].as<float>();
        if (doc["btnRed"].is<const char *>())
          g_config.btnRed = doc["btnRed"].as<const char *>();
        if (doc["btnGreen"].is<const char *>())
          g_config.btnGreen = doc["btnGreen"].as<const char *>();
        if (doc["btnYellow"].is<const char *>())
          g_config.btnYellow = doc["btnYellow"].as<const char *>();
        if (doc["btnBlue"].is<const char *>())
          g_config.btnBlue = doc["btnBlue"].as<const char *>();
        if (doc["btnCustom1"].is<const char *>())
          g_config.btnCustom1 = doc["btnCustom1"].as<const char *>();
        if (doc["btnCustom2"].is<const char *>())
          g_config.btnCustom2 = doc["btnCustom2"].as<const char *>();
        if (doc["btnCustom3"].is<const char *>())
          g_config.btnCustom3 = doc["btnCustom3"].as<const char *>();
        if (doc["btnCustom4"].is<const char *>())
          g_config.btnCustom4 = doc["btnCustom4"].as<const char *>();
        if (doc["lblRed"].is<const char *>())
          g_config.lblRed = doc["lblRed"].as<const char *>();
        if (doc["lblGreen"].is<const char *>())
          g_config.lblGreen = doc["lblGreen"].as<const char *>();
        if (doc["lblYellow"].is<const char *>())
          g_config.lblYellow = doc["lblYellow"].as<const char *>();
        if (doc["lblBlue"].is<const char *>())
          g_config.lblBlue = doc["lblBlue"].as<const char *>();
        if (doc["lblCustom1"].is<const char *>())
          g_config.lblCustom1 = doc["lblCustom1"].as<const char *>();
        if (doc["lblCustom2"].is<const char *>())
          g_config.lblCustom2 = doc["lblCustom2"].as<const char *>();
        if (doc["lblCustom3"].is<const char *>())
          g_config.lblCustom3 = doc["lblCustom3"].as<const char *>();
        if (doc["lblCustom4"].is<const char *>())
          g_config.lblCustom4 = doc["lblCustom4"].as<const char *>();
        if (doc["iconRed"].is<const char *>())
          g_config.iconRed = doc["iconRed"].as<const char *>();
        if (doc["iconGreen"].is<const char *>())
          g_config.iconGreen = doc["iconGreen"].as<const char *>();
        if (doc["iconYellow"].is<const char *>())
          g_config.iconYellow = doc["iconYellow"].as<const char *>();
        if (doc["iconBlue"].is<const char *>())
          g_config.iconBlue = doc["iconBlue"].as<const char *>();
        if (doc["iconCustom1"].is<const char *>())
          g_config.iconCustom1 = doc["iconCustom1"].as<const char *>();
        if (doc["iconCustom2"].is<const char *>())
          g_config.iconCustom2 = doc["iconCustom2"].as<const char *>();
        if (doc["iconCustom3"].is<const char *>())
          g_config.iconCustom3 = doc["iconCustom3"].as<const char *>();
        if (doc["iconCustom4"].is<const char *>())
          g_config.iconCustom4 = doc["iconCustom4"].as<const char *>();
        if (doc["colRed"].is<const char *>())
          g_config.colRed = doc["colRed"].as<const char *>();
        if (doc["colGreen"].is<const char *>())
          g_config.colGreen = doc["colGreen"].as<const char *>();
        if (doc["colYellow"].is<const char *>())
          g_config.colYellow = doc["colYellow"].as<const char *>();
        if (doc["colBlue"].is<const char *>())
          g_config.colBlue = doc["colBlue"].as<const char *>();
        if (doc["colCustom1"].is<const char *>())
          g_config.colCustom1 = doc["colCustom1"].as<const char *>();
        if (doc["colCustom2"].is<const char *>())
          g_config.colCustom2 = doc["colCustom2"].as<const char *>();
        if (doc["colCustom3"].is<const char *>())
          g_config.colCustom3 = doc["colCustom3"].as<const char *>();
        if (doc["colCustom4"].is<const char *>())
          g_config.colCustom4 = doc["colCustom4"].as<const char *>();

        if (doc["qlPages"].is<int>()) {
          int p = doc["qlPages"].as<int>();
          if (p >= 1 && p <= 5)
            g_config.qlPages = p;
        }

        if (doc["quickLinks"].is<JsonArray>()) {
          JsonArray qlArr = doc["quickLinks"].as<JsonArray>();
          int idx = 0;
          for (JsonObject item : qlArr) {
            if (idx >= QUICK_LINKS_COUNT)
              break;
            g_config.qlLabel[idx] = item["label"] | "";
            g_config.qlNum[idx] = item["num"] | "";
            g_config.qlColor[idx] = item["color"] | "";
            g_config.qlIcon[idx] = item["icon"] | "";
            idx++;
          }
        }

        if (doc["host"].is<const char *>()) {
          String h = doc["host"].as<const char *>();
          g_config.hostname = ConfigManager::sanitizeHostname(h);
        }
        if (doc["theme"].is<const char *>()) {
          g_config.themeColor = doc["theme"].as<const char *>();
        }
        if (doc["themeMode"].is<const char *>()) {
          g_config.themeMode = doc["themeMode"].as<const char *>();
        }
        if (!doc["powerSave"].isNull()) {
          g_config.powerSave = doc["powerSave"] | false;
          ConfigManager::savePowerSave(g_config.powerSave);
          applyPowerSave(g_config.powerSave);
        }
        if (!doc["ledDisabled"].isNull()) {
          g_config.ledDisabled = doc["ledDisabled"] | false;
          ConfigManager::saveLedDisabled(g_config.ledDisabled);
        }
        bool usbChanged = false;
        if (!doc["usbMode"].isNull()) {
          bool newUsb = doc["usbMode"] | false;
          if (newUsb != g_config.usbMode) {
            g_config.usbMode = newUsb;
            ConfigManager::saveUsbMode(g_config.usbMode);
            usbChanged = true;
          }
        }
        ConfigManager::save(g_config);
        setStatusLed(BLEManager::instance().isConnected());
        String json;
        buildSettingsJson(json);
        g_ws.textAll(json);
        if (usbChanged) {
          scheduleRestart(500);
        }
      } else if (strcmp(typeStr, "set_usb_mode") == 0) {
        bool usb = doc["enabled"] | false;
        if (usb != g_config.usbMode) {
          g_config.usbMode = usb;
          ConfigManager::saveUsbMode(g_config.usbMode);
          ConfigManager::save(g_config);
          Serial.printf("[USB] Controller output mode changed to: %s. Rebooting...\n",
                        g_config.usbMode ? "Wired USB HID" : "Bluetooth (BLE)");
          String json;
          buildSettingsJson(json);
          g_ws.textAll(json);
          broadcastStatus();
          scheduleRestart(600);
        }
      } else if (strcmp(typeStr, "set_power_save") == 0) {
        bool en = doc["enabled"] | false;
        g_config.powerSave = en;
        ConfigManager::savePowerSave(g_config.powerSave);
        ConfigManager::save(g_config);
        applyPowerSave(g_config.powerSave);
        Serial.printf("[POWER] Power saving saved: %d\n", g_config.powerSave ? 1 : 0);
        String json;
        buildSettingsJson(json);
        g_ws.textAll(json);
      } else if (strcmp(typeStr, "set_led_disabled") == 0) {
        bool dis = doc["disabled"] | false;
        g_config.ledDisabled = dis;
        ConfigManager::saveLedDisabled(g_config.ledDisabled);
        ConfigManager::save(g_config);
        setStatusLed(BLEManager::instance().isConnected());
        Serial.printf("[LED] Status LED disabled: %d\n", g_config.ledDisabled ? 1 : 0);
        String json;
        buildSettingsJson(json);
        g_ws.textAll(json);
      } else if (strcmp(typeStr, "set_theme") == 0) {
        if (doc["theme"].is<const char *>()) {
          g_config.themeColor = doc["theme"].as<const char *>();
          ConfigManager::save(g_config);
          Serial.printf("[THEME] Accent color saved: %s\n",
                        g_config.themeColor.c_str());
        }
      } else if (strcmp(typeStr, "set_theme_mode") == 0) {
        if (doc["mode"].is<const char *>()) {
          g_config.themeMode = doc["mode"].as<const char *>();
          ConfigManager::save(g_config);
          Serial.printf("[THEME] Mode saved: %s\n", g_config.themeMode.c_str());
        }
      } else if (strcmp(typeStr, "ble_disconnect") == 0) {
        Serial.println("[BLE] User requested active BLE disconnect.");
        // Status is re-broadcast by loop() once the connection state changes
        BleCommandQueue::post(BleCmdType::Disconnect);
      } else if (strcmp(typeStr, "toggle_ble_radio") == 0 ||
                 strcmp(typeStr, "set_ble_radio") == 0) {
        bool targetState = !BLEManager::instance().isRadioEnabled();
        if (doc["enabled"].is<bool>()) {
          targetState = doc["enabled"].as<bool>();
        }
        Serial.printf("[BLE] User set BLE Radio: %s\n",
                      targetState ? "ENABLED" : "DISABLED");
        g_config.bleRadioEnabled = targetState;
        ConfigManager::save(g_config);
        BleCommandQueue::post(BleCmdType::SetRadio, targetState ? 1 : 0);
        if (!targetState) {
          setStatusLed(false);
        }
        broadcastStatus();
      } else if (strcmp(typeStr, "ble_forget_bonds") == 0) {
        Serial.println("[BLE] User requested BLE bond reset & re-pair.");
        BleCommandQueue::post(BleCmdType::ForgetBonds);
      } else if (strcmp(typeStr, "reboot_device") == 0) {
        Serial.println("[System] Reboot requested by user.");
        scheduleRestart(500);
      } else if (strcmp(typeStr, "factory_reset") == 0) {
        Serial.println("[System] Factory Reset requested by user.");
        ConfigManager::clear();
        BleCommandQueue::post(BleCmdType::ForgetBonds);
        scheduleRestart(500);
      } else if (strcmp(typeStr, "reset_wifi") == 0 ||
                 strcmp(typeStr, "forget_wifi") == 0) {
        Serial.println("[System] Forget Wi-Fi requested by user.");
        g_config.ssid = "";
        g_config.password = "";
        g_config.apMode = false;
        g_config.configured = false;
        ConfigManager::save(g_config);
        scheduleRestart(500);
      } else if (strcmp(typeStr, "set_ap_mode") == 0) {
        bool enableAp = doc["enabled"] | false;
        Serial.printf("[System] User set AP Mode: %s\n",
                      enableAp ? "ENABLED" : "DISABLED");
        g_config.apMode = enableAp;
        if (enableAp) {
          g_config.configured = true;
        } else {
          if (g_config.ssid.isEmpty()) {
            g_config.configured = false;
          }
        }
        ConfigManager::save(g_config);
        scheduleRestart(500);
      }
  }
}

static void registerOtaEndpoint(AsyncWebServer &server) {
  server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        // Only report success if a firmware file was actually received and
        // written completely. Previously a request with no multipart file
        // (e.g. a raw body) reported OK and rebooted without flashing anything.
        bool ok = g_otaReceived && g_otaSuccess && !Update.hasError();
        const char *msg = ok ? "OK"
                          : !g_otaReceived
                              ? "FAIL: no firmware file received"
                              : "FAIL: update error (see serial log)";
        AsyncWebServerResponse *response =
            request->beginResponse(ok ? 200 : 500, "text/plain", msg);
        response->addHeader("Connection", "close");
        request->send(response);

        g_otaReceived = false;
        g_otaSuccess = false;
        if (ok) {
          Serial.println("[OTA] Firmware uploaded successfully. Rebooting...");
          scheduleRestart(1000);
        } else {
          Serial.printf("[OTA] %s\n", msg);
        }
      },
      [](AsyncWebServerRequest *request, String filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          Serial.printf("[OTA] Upload start: %s\n", filename.c_str());
          g_otaReceived = true;
          g_otaSuccess = false;
          // Clean up after a previously interrupted upload
          if (Update.isRunning()) {
            Update.abort();
          }
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
          }
        }
        if (Update.isRunning() && !Update.hasError()) {
          if (Update.write(data, len) != len) {
            Update.printError(Serial);
          }
        }
        if (final && Update.isRunning()) {
          if (Update.end(true)) {
            g_otaSuccess = true;
            Serial.printf("[OTA] Upload complete: %u bytes\n", index + len);
          } else {
            Update.printError(Serial);
          }
        }
      });
}

void startApMode() {
  g_state = STATE_AP_MODE;
  Serial.println("\n[AP] Starting Access Point 'CastPad' (Captive Portal)...");

  WiFi.disconnect(true);
  // Keep the STA interface available when credentials exist so loop() can
  // periodically retry the saved network.
  WiFi.mode(hasSavedWifi() ? WIFI_AP_STA : WIFI_AP);
  WiFi.setAutoReconnect(false);
  WiFi.softAP("CastPad");
  g_lastStaRetry = millis();

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("[AP] AP IP address: ");
  Serial.println(apIP);

  g_dnsServer.start(53, "*", apIP);

  if (MDNS.begin("CastPad")) {
    MDNS.addService("http", "tcp", 80);
  }

  g_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",
                  reinterpret_cast<const uint8_t *>(PORTAL_HTML),
                  sizeof(PORTAL_HTML) - 1);
  });

  g_server.on("/hotspot-detect.html", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->redirect("/"); });
  g_server.on("/generate_204", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->redirect("/"); });
  g_server.on("/canonical.html", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->redirect("/"); });
  g_server.on("/connecttest.txt", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->redirect("/"); });
  g_server.on("/ncsi.txt", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->redirect("/"); });

  g_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if (n == -2) {
      WiFi.scanNetworks(true);
      request->send(200, "application/json", "[]");
      return;
    } else if (n == -1) {
      request->send(200, "application/json", "[]");
      return;
    }

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    for (int i = 0; i < n && i < 15; ++i) {
      JsonObject net = array.add<JsonObject>();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true);

    String res;
    serializeJson(doc, res);
    request->send(200, "application/json", res);
  });

  AsyncCallbackJsonWebHandler *saveHandler = new AsyncCallbackJsonWebHandler(
      "/save", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        if (!obj.isNull()) {
          bool standaloneAp = obj["ap_mode"] | false;
          String host =
              ConfigManager::sanitizeHostname(obj["host"] | "CastPad");
          g_config.hostname = host;
          g_config.configured = true;

          if (standaloneAp) {
            g_config.apMode = true;
            g_config.ssid = "";
            g_config.password = "";
            Serial.printf("[Config] Standalone AP mode selected. Hostname: %s. "
                          "Rebooting...\n",
                          g_config.hostname.c_str());
          } else {
            g_config.apMode = false;
            g_config.ssid = obj["ssid"] | "";
            g_config.password = obj["pass"] | "";
            Serial.printf(
                "[Config] Saved SSID: %s, Hostname: %s. Rebooting...\n",
                g_config.ssid.c_str(), g_config.hostname.c_str());
          }

          ConfigManager::save(g_config);
          request->send(200, "text/plain", "OK");
          scheduleRestart(1000);
        } else {
          request->send(400, "text/plain", "Invalid JSON");
        }
      });
  g_server.addHandler(saveHandler);

  registerOtaEndpoint(g_server);

  g_server.onNotFound(
      [](AsyncWebServerRequest *request) { request->redirect("/"); });

  g_server.begin();
  confirmOtaAppValid();
  Serial.println("[AP] Web server started.");
}

void startRunMode() {
  g_state = STATE_RUN_MODE;
  if (g_config.apMode) {
    Serial.println("\n[Run] Starting Standalone AP Mode (Open network)...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(g_config.hostname.c_str());
    g_dnsServer.start(53, "*", apIP);
    Serial.printf(
        "[Run] SoftAP '%s' started. IP Address: %s (Captive DNS active)\n",
        g_config.hostname.c_str(), WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("\n[Run] Connected to Wi-Fi successfully!");
    Serial.print("[Run] IP Address: ");
    Serial.println(WiFi.localIP());

    // Wi-Fi modem sleep MUST stay enabled when Bluetooth is used (coexistence).
    // Apply power saving profile (MAX_MODEM & 80 MHz, or MIN_MODEM & 240 MHz).
    applyPowerSave(g_config.powerSave);
    WiFi.setAutoReconnect(true);

    // Zero-polling event-driven Wi-Fi auto-reconnect watchdog
    WiFi.onEvent(
        [](WiFiEvent_t event, WiFiEventInfo_t info) {
          if (g_state == STATE_RUN_MODE && !g_config.apMode) {
            Serial.println(
                "[Wi-Fi] Disconnected from AP. Triggering auto-reconnect...");
            WiFi.reconnect();
          }
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  }

  WiFi.setHostname(g_config.hostname.c_str());
  if (MDNS.begin(g_config.hostname.c_str())) {
    Serial.printf("[mDNS] Responder started: http://%s.local\n",
                  g_config.hostname.c_str());
    MDNS.addService("http", "tcp", 80);
  }

  if (!g_config.apMode) {
    // Configure remote ArduinoOTA
    ArduinoOTA.setHostname(g_config.hostname.c_str());
    ArduinoOTA.onStart(
        []() { Serial.println("[ArduinoOTA] Start remote OTA update"); });
    ArduinoOTA.onEnd([]() { Serial.println("\n[ArduinoOTA] Flash complete"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("[ArduinoOTA] Progress: %u%%\r",
                    (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("[ArduinoOTA] Error[%u]\n", error);
    });
    ArduinoOTA.begin();
  }

  // Initialize Controller (Wired USB HID or BLE Combo Device)
  if (g_config.usbMode) {
    Serial.println("[USB] Starting Native USB HID controller 'CastPad USB Remote' (BLE disabled)...");
    BLEManager::instance().beginUsb("CastPad USB Remote");
  } else {
    Serial.println("[BLE] Starting BLE Combo controller 'CastPad BLE Remote'...");
    BLEManager::instance().begin("CastPad BLE Remote");
    if (!g_config.bleRadioEnabled) {
      Serial.println("[BLE] BLE radio disabled in saved configuration.");
      BLEManager::instance().setRadioEnabled(false);
    }
  }
  BleCommandQueue::begin();

  // Setup WebSockets
  g_ws.onEvent(onWsEvent);
  g_server.addHandler(&g_ws);

  // Serve main application with Gzip compression and ETag / Cache-Control
  static const char APP_ETAG[] = "\"castpad-2.8.3\"";
  g_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    markUserActivity();
    if (request->header("If-None-Match").indexOf("castpad-2.8.3") >= 0) {
      request->send(304);
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/html", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("ETag", APP_ETAG);
    response->addHeader("Cache-Control",
                        "public, max-age=604800, must-revalidate");
    request->send(response);
  });

  // Web App Manifest & App Icon with Cache-Control headers
  g_server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(
        200, "application/manifest+json",
        reinterpret_cast<const uint8_t *>(MANIFEST_JSON),
        sizeof(MANIFEST_JSON) - 1);
    response->addHeader("Cache-Control", "public, max-age=604800");
    request->send(response);
  });
  g_server.on("/icon.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(
        200, "image/svg+xml", reinterpret_cast<const uint8_t *>(ICON_SVG),
        sizeof(ICON_SVG) - 1);
    response->addHeader("Cache-Control", "public, max-age=604800");
    request->send(response);
  });

  // Register Web OTA Firmware Upload Endpoint
  registerOtaEndpoint(g_server);

  g_server.onNotFound(
      [](AsyncWebServerRequest *request) { request->redirect("/"); });

  g_server.begin();
  confirmOtaAppValid();
  Serial.printf("[Run] Web server running on http://%s.local (or http://%s)\n",
                g_config.hostname.c_str(),
                g_config.apMode ? WiFi.softAPIP().toString().c_str()
                                : WiFi.localIP().toString().c_str());

  // Reset LED state for run mode: blink slowly (1000ms) until BLE connects
  g_lastLedToggle = millis();
  g_ledState = false;
  setStatusLed(false);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
#ifdef BOOT_BUTTON_PIN
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
#endif

  ConfigManager::load(g_config);

  if (!g_config.usbMode) {
    Serial.begin(115200);
  }
  applyPowerSave(g_config.powerSave);

  // Rapid boot blink (100ms) - suppressed if LED disabled
  for (int i = 0; i < 10; ++i) {
    setStatusLed(i % 2 == 0);
    delay(100);
  }
  setStatusLed(false);
  Serial.println("\n======================================");
  Serial.println("           CastPad BLE Remote         ");
  Serial.println("======================================");

  Serial.printf(
      "[Config] Configured: %s, AP Mode: %s, USB Mode: %s, PowerSave: %s, LedDisabled: %s, SSID: '%s', Hostname: '%s'\n",
      g_config.configured ? "YES" : "NO", g_config.apMode ? "YES" : "NO",
      g_config.usbMode ? "YES" : "NO",
      g_config.powerSave ? "YES" : "NO", g_config.ledDisabled ? "YES" : "NO",
      g_config.ssid.c_str(), g_config.hostname.c_str());

  bool runDirect = false;

  if (g_config.configured && g_config.apMode) {
    Serial.println("[Wi-Fi] Standalone AP mode enabled. Skipping home Wi-Fi.");
    runDirect = true;
  } else if (g_config.configured && !g_config.ssid.isEmpty()) {
    Serial.printf("[Wi-Fi] Connecting to '%s' (15s timeout)...\n",
                  g_config.ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(g_config.hostname.c_str());
    WiFi.begin(g_config.ssid.c_str(), g_config.password.c_str());

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
      delay(100);
      g_ledState = !g_ledState;
      setStatusLed(g_ledState);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      runDirect = true;
    } else {
      Serial.println("[Wi-Fi] Connection timed out!");
    }
  }

  if (runDirect) {
    startRunMode();
  } else {
    startApMode();
  }
}

void loop() {
  unsigned long now = millis();

  if (g_restartPending && (long)(now - g_restartAt) >= 0) {
    Serial.println("[System] Restarting...");
    Serial.flush();
    ESP.restart();
  }

  if (g_state == STATE_AP_MODE ||
      (g_state == STATE_RUN_MODE && g_config.apMode)) {
    g_dnsServer.processNextRequest();
  }

  if (g_state == STATE_AP_MODE) {

    // Periodically retry the saved Wi-Fi network (router may have been slow to
    // boot). Skipped while a phone is connected to the setup AP, because STA
    // connection attempts can hop the AP channel and interrupt provisioning.
    if (hasSavedWifi()) {
      if (g_staRetrying) {
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("[Wi-Fi] Saved network is reachable again. "
                         "Rebooting into run mode...");
          g_staRetrying = false;
          scheduleRestart(500);
        } else if (now - g_lastStaRetry >= STA_RETRY_WINDOW_MS) {
          Serial.println("[Wi-Fi] Retry failed, staying in AP mode.");
          WiFi.disconnect(false, false);
          g_staRetrying = false;
          g_lastStaRetry = now;
        }
      } else if (!g_restartPending &&
                 now - g_lastStaRetry >= STA_RETRY_INTERVAL_MS &&
                 WiFi.softAPgetStationNum() == 0) {
        Serial.printf("[Wi-Fi] Retrying saved network '%s'...\n",
                      g_config.ssid.c_str());
        WiFi.begin(g_config.ssid.c_str(), g_config.password.c_str());
        g_staRetrying = true;
        g_lastStaRetry = now;
      }
    }
  } else if (g_state == STATE_RUN_MODE) {
    // Throttle WebSocket client cleanup to every 2 seconds instead of every loop tick
    static unsigned long s_lastWsCleanup = 0;
    if (now - s_lastWsCleanup >= 2000) {
      s_lastWsCleanup = now;
      g_ws.cleanupClients();
    }

    ArduinoOTA.handle(); // Handle remote wireless firmware flashes

    // Check if BLE connection status changed
    bool currentBle = BLEManager::instance().isConnected();
    if (currentBle != g_lastBleConnected) {
      g_lastBleConnected = currentBle;
      Serial.printf("[BLE] Connection state changed: %s\n",
                    currentBle ? "CONNECTED" : "DISCONNECTED/PAIRING");
      broadcastStatus();
    }
  }

  // Handle physical BOOT button (4s hold toggles Standalone AP mode)
  handleBootButton(now);

  // Handle smart zero-client auto-idle & RSSI-adaptive Wi-Fi TX power
  updatePowerManagement(now);

  // Handle status LED state machine (blinks on boot/waiting, steady on BLE connected, 3 blinks on commands)
  updateStatusLed(now);

  // 1ms cooperative yield allows FreeRTOS IDLE task to enter tickless idle
  delay(1);
}
