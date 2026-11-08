#pragma once

#include <Arduino.h>
#include <ESP32BLECombo.h>
#include <new>
#ifdef CONFIG_BT_ENABLED
#include <esp_bt.h>
#endif
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
#include <USB.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#undef MOUSE_LEFT
#undef MOUSE_RIGHT
#undef MOUSE_MIDDLE
#endif

class BLEManager {
public:
  static BLEManager &instance() {
    static BLEManager mgr;
    return mgr;
  }

  bool beginUsb(const String &deviceName = "CastPad USB Remote") {
    if (m_initialized)
      return true;
    m_usbMode = true;
    m_radioEnabled = false;
#ifdef CONFIG_BT_ENABLED
    // Completely power down the Bluetooth baseband hardware and release its
    // ~36 KB internal SRAM back to the heap when running in Wired USB mode
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#endif
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
    // Zero-allocate via calloc before placement-new because Espressif's
    // USBHIDKeyboard constructor does not zero-initialize its private
    // _keyReport struct, which otherwise leaves non-zero heap bytes in
    // _keyReport.keys[0..5] and causes pressRaw() to drop all keystrokes.
    if (!m_usbKbd) {
      void *mem = calloc(1, sizeof(USBHIDKeyboard));
      m_usbKbd = new (mem) USBHIDKeyboard();
    }
    if (!m_usbMouse) {
      void *mem = calloc(1, sizeof(USBHIDMouse));
      m_usbMouse = new (mem) USBHIDMouse();
    }
    if (!m_usbConsumer) {
      void *mem = calloc(1, sizeof(USBHIDConsumerControl));
      m_usbConsumer = new (mem) USBHIDConsumerControl();
    }
    m_usbKbd->begin();
    m_usbMouse->begin();
    m_usbConsumer->begin();
    USB.manufacturerName("Espressif");
    USB.productName(deviceName.c_str());
    USB.begin();
#endif
    m_initialized = true;
    return true;
  }

  bool begin(const String &deviceName = "CastPad BLE Remote") {
    if (m_initialized)
      return true;

    ESP32BLEComboConfig cfg;
    cfg.mode = ESP32BLEComboMode::KEYBOARD_MOUSE;
    cfg.appearance =
        ESP32BLEComboAppearance::KEYBOARD; // 0x03C1 so Fire TV / Android
                                           // recognizes it as a keyboard
    cfg.deviceName = deviceName;
    cfg.manufacturer = "Espressif";
    cfg.batteryLevel = 100;
    cfg.enableSecurity = true; // REQUIRED by Fire TV / Android for HID pairing
    // Bump PnP ID/version so Android/FireOS refreshes cached HID report map and
    // device name
    cfg.productId = 0xA114;
    cfg.productVersion = 0x0230;
    // Press hold and release gap so FireOS / Android reliably registers
    // typed keys & repeated chars without dropping composition or spacebars
    cfg.keyPressDelayMs = 25;
    cfg.keyReleaseDelayMs = 20;
    cfg.keyIntervalDelayMs = 15;

    m_initialized = m_combo.begin(cfg);
    m_combo.setKeyPressDelay(25);
    m_combo.setKeyReleaseDelay(20);
    m_combo.setKeyIntervalDelay(15);

    // Configure NimBLE security & advertising specifically for FireOS / Android
    // compatibility: ESP32BLECombo::startAdvertising() does not call
    // adv->setName() and disables Scan Response, causing Fire TV to filter out
    // the unnamed BLE device during scanning.
    NimBLEDevice::setSecurityAuth(
        true, false, true); // Bonding=true, MITM=false, SecureConnections=true
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC |
                                     BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC |
                                     BLE_SM_PAIR_KEY_DIST_ID);

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    if (adv) {
      adv->stop();
      // Put the device name directly in the main advertisement packet AND
      // enable scan response
      adv->enableScanResponse(false);
      adv->setName(deviceName.c_str());
      adv->enableScanResponse(true);
      adv->setName(deviceName.c_str());
      adv->start();
    }

    return m_initialized;
  }

  bool isConnected() const {
    if (m_usbMode)
      return m_initialized;
    return m_radioEnabled && m_combo.isConnected();
  }
  bool isRadioEnabled() const { return !m_usbMode && m_radioEnabled; }
  bool isUsbMode() const { return m_usbMode; }

  void setRadioEnabled(bool enable) {
    if (m_usbMode || m_radioEnabled == enable)
      return;
    m_radioEnabled = enable;
    NimBLEServer *srv = NimBLEDevice::getServer();
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();

    if (!enable) {
      if (srv) {
        srv->advertiseOnDisconnect(false);
        std::vector<uint16_t> peers = srv->getPeerDevices();
        for (uint16_t connHandle : peers) {
          srv->disconnect(connHandle);
        }
      }
      if (adv && adv->isAdvertising()) {
        adv->stop();
      }
    } else {
      if (srv) {
        srv->advertiseOnDisconnect(true);
      }
      if (adv && !adv->isAdvertising()) {
        adv->start();
      }
    }
  }

  // Disconnect the currently active BLE host and resume advertising so another
  // device can connect
  void disconnectAndRepair(bool clearBonds = false) {
    NimBLEServer *srv = NimBLEDevice::getServer();
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();

    if (srv) {
      srv->advertiseOnDisconnect(false);
    }
    if (adv && adv->isAdvertising()) {
      adv->stop();
    }
    if (srv) {
      std::vector<uint16_t> peers = srv->getPeerDevices();
      for (uint16_t connHandle : peers) {
        srv->disconnect(connHandle);
      }
    }
    delay(200);
    if (clearBonds) {
      NimBLEDevice::deleteAllBonds();
      if (srv) {
        srv->sendServiceChangedIndication();
      }
    }
    if (m_radioEnabled) {
      if (srv) {
        srv->advertiseOnDisconnect(true);
      }
      if (adv && !adv->isAdvertising()) {
        adv->start();
      }
    }
  }

  void sendKey(const String &rawAction) {
    if (!isConnected())
      return;

    // Handle Spacebar immediately before any string trimming strips whitespace.
    // FireOS IME requires a distinct hold duration for space (0x2C) so word
    // composition commits cleanly and the space glyph is inserted.
    if (rawAction == " " || rawAction.equalsIgnoreCase("SPACE")) {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbKbd) {
        m_usbKbd->pressRaw(0x2C);
        delay(25);
        m_usbKbd->releaseRaw(0x2C);
        delay(15);
        return;
      }
#endif
      m_combo.pressRaw(0x2C);
      delay(45);
      m_combo.releaseRaw(0x2C);
      delay(25);
      return;
    }

    String action = rawAction;
    action.trim();
    if (action.isEmpty() || action.equalsIgnoreCase("NONE"))
      return;

    // Strip optional K: / k: prefix (e.g. from sequences or custom input)
    bool hasKPrefix = false;
    if (action.startsWith("K:") || action.startsWith("k:")) {
      hasKPrefix = true;
      action = action.substring(2);
      action.trim();
      if (action.isEmpty() || action.equalsIgnoreCase("NONE"))
        return;
    }

    // Check for Function Keys F1-F12 (and KEY_F1-KEY_F12, F13-F24)
    // USB HID Usage Page 0x07 (Keyboard/Keypad):
    // F1  = 0x3A (58)
    // F2  = 0x3B (59)
    // F3  = 0x3C (60)
    // F4  = 0x3D (61)
    // F5  = 0x3E (62)
    // F6  = 0x3F (63)
    // F7  = 0x40 (64)
    // F8  = 0x41 (65)
    // F9  = 0x42 (66)
    // F10 = 0x43 (67)
    // F11 = 0x44 (68)
    // F12 = 0x45 (69)
    String upper = action;
    upper.toUpperCase();
    int fn = 0;
    if (upper.startsWith("KEY_F") && upper.length() >= 6 && upper.length() <= 7) {
      fn = upper.substring(5).toInt();
    } else if (upper.startsWith("F") && upper.length() >= 2 && upper.length() <= 3 && isDigit(upper.charAt(1))) {
      fn = upper.substring(1).toInt();
    }
    if (fn >= 1 && fn <= 12) {
      tapRawKey(static_cast<uint8_t>(0x3A + (fn - 1)));
      return;
    } else if (fn >= 13 && fn <= 24) {
      tapRawKey(static_cast<uint8_t>(0x68 + (fn - 13)));
      return;
    }

    if (action.equalsIgnoreCase("UP")) {
      tapKey(0xDA); // KEY_UP_ARROW
    } else if (action.equalsIgnoreCase("DOWN")) {
      tapKey(0xD9); // KEY_DOWN_ARROW
    } else if (action.equalsIgnoreCase("LEFT")) {
      tapKey(0xD8); // KEY_LEFT_ARROW
    } else if (action.equalsIgnoreCase("RIGHT")) {
      tapKey(0xD7); // KEY_RIGHT_ARROW
    } else if (action.equalsIgnoreCase("ENTER") ||
               action.equalsIgnoreCase("OK")) {
      tapKey(0xB0); // KEY_RETURN
    } else if (action.equalsIgnoreCase("BACK")) {
      tapKey(0xB1); // KEY_ESC (Android / Fire TV Back)
    } else if (action.equalsIgnoreCase("HOME")) {
      // Send Keyboard Meta+Enter (triggers launchHomeFromHotKey in
      // Android/FireOS PhoneWindowManager over Report ID 1 even without
      // re-pairing) AND Consumer AC Home (0x0223 -> KEYCODE_HOME)
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbKbd) {
        m_usbKbd->pressRaw(0xE3); // HID Left GUI (Meta)
        delay(10);
        m_usbKbd->press(0xB0); // KEY_RETURN (Enter)
        delay(20);
        m_usbKbd->releaseAll();
        tapConsumer(0x0223);
      } else
#endif
      {
        m_combo.press(0x83); // KEY_LEFT_GUI (Meta)
        delay(10);
        m_combo.press(0xB0); // KEY_RETURN (Enter)
        delay(20);
        m_combo.releaseAll();
        tapConsumer(0x0223);
      }
    } else if (action.equalsIgnoreCase("MENU")) {
      tapKey(0xED); // Application / Menu Key (0x65 + 136)
    } else if (action.equalsIgnoreCase("PAGE_UP") ||
               action.equalsIgnoreCase("PG_UP") ||
               action.equalsIgnoreCase("CH_UP")) {
      tapKey(0xD3); // KEY_PAGE_UP (0x4B + 136)
    } else if (action.equalsIgnoreCase("PAGE_DOWN") ||
               action.equalsIgnoreCase("PG_DOWN") ||
               action.equalsIgnoreCase("CH_DOWN")) {
      tapKey(0xD6); // KEY_PAGE_DOWN (0x4E + 136)
    } else if (action.equalsIgnoreCase("REWIND")) {
      tapConsumer(0x00B4); // Consumer Rewind (0x00B4 -> KEY_REWIND ->
                           // KEYCODE_MEDIA_REWIND)
    } else if (action.equalsIgnoreCase("PLAY_PAUSE")) {
      tapKey(' '); // Spacebar action for reliable Play/Pause across Fire TV &
                   // STB apps
    } else if (action.equalsIgnoreCase("MEDIA_PLAY_PAUSE")) {
      tapConsumer(0x00CD); // Consumer Play/Pause (0x00CD)
    } else if (action.equalsIgnoreCase("FAST_FORWARD")) {
      tapConsumer(0x00B3); // Consumer Fast Forward (0x00B3 -> KEY_FASTFORWARD
                           // -> KEYCODE_MEDIA_FAST_FORWARD)
    } else if (action.equalsIgnoreCase("MUTE")) {
      tapConsumer(
          0x00E2); // Consumer Mute (0x00E2 -> KEY_MUTE -> KEYCODE_VOLUME_MUTE)
    } else if (action.equalsIgnoreCase("VOL_UP")) {
      tapConsumer(0x00E9); // Consumer Volume Increment (0x00E9 -> KEY_VOLUMEUP)
    } else if (action.equalsIgnoreCase("VOL_DOWN")) {
      tapConsumer(
          0x00EA); // Consumer Volume Decrement (0x00EA -> KEY_VOLUMEDOWN)
    } else if (action.equalsIgnoreCase("SEARCH")) {
      tapConsumer(0x0221); // Consumer AC Search (0x0221 -> KEY_SEARCH ->
                           // KEYCODE_SEARCH)
    } else if (action.equalsIgnoreCase("TAB")) {
      tapKey(0xB3); // KEY_TAB
    } else if (action.equalsIgnoreCase("BACKSPACE")) {
      tapKey(0xB2); // KEY_BACKSPACE
    } else if (action.equalsIgnoreCase("CLEAR_FIELD")) {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbKbd) {
        m_usbKbd->pressRaw(0xE0); // HID Left Ctrl
        m_usbKbd->press('a');
        delay(20);
        m_usbKbd->releaseAll();
        delay(20);
        for (int i = 0; i < 24; ++i) {
          m_usbKbd->pressRaw(0x2A); // HID Backspace
          delay(10);
          m_usbKbd->releaseRaw(0x2A);
          delay(8);
        }
      } else
#endif
      {
        m_combo.press(0x80); // KEY_LEFT_CTRL
        m_combo.press('a');
        delay(25);
        m_combo.releaseAll();
        delay(25);
        for (int i = 0; i < 24; ++i) {
          m_combo.pressRaw(0x2A); // HID Backspace
          delay(12);
          m_combo.releaseRaw(0x2A);
          delay(10);
        }
      }
    } else if (action.startsWith("C:") || action.startsWith("c:")) {
      String hexPart = action.substring(2);
      hexPart.trim();
      uint16_t code =
          static_cast<uint16_t>(strtoul(hexPart.c_str(), nullptr, 16));
      if (code > 0)
        tapConsumer(code);
    } else if (action.startsWith("0x") || action.startsWith("0X")) {
      unsigned long code = strtoul(action.c_str(), nullptr, 16);
      if (code > 0xFF || action.length() >= 6 ||
          (code >= 0xB0 && code <= 0xDF) || code >= 0xE8) {
        tapConsumer(static_cast<uint16_t>(code));
      } else if (code > 0) {
        tapRawKey(static_cast<uint8_t>(code));
      }
    } else if (hasKPrefix) {
      uint8_t code =
          static_cast<uint8_t>(strtoul(action.c_str(), nullptr, 16));
      if (code > 0)
        tapRawKey(code);
    } else if (action.length() == 1) {
      tapKey(action.charAt(0));
    } else {
      typeText(action);
    }
  }

    void typeText(const String &text, bool pressEnter = true) {
      if (!isConnected() || text.isEmpty())
        return;
      for (size_t i = 0; i < text.length(); ++i) {
        char c = text.charAt(i);
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
        if (m_usbMode && m_usbKbd) {
          if (c == ' ') {
            m_usbKbd->pressRaw(0x2C);
            delay(20);
            m_usbKbd->releaseRaw(0x2C);
            delay(15);
          } else {
            m_usbKbd->press(static_cast<uint8_t>(c));
            delay(15);
            m_usbKbd->release(static_cast<uint8_t>(c));
            delay(10);
          }
          continue;
        }
#endif
        if (c == ' ') {
          // FireOS IME requires an explicit hold duration and inter-key gap on Space
          // to commit word composition and insert the whitespace glyph
          m_combo.pressRaw(0x2C);
          delay(45);
          m_combo.releaseRaw(0x2C);
          delay(35);
        } else {
          m_combo.write(static_cast<uint8_t>(c));
        }
      }
      if (pressEnter) {
        delay(50);
        tapKey(0xB0); // KEY_RETURN
      }
    }

    // Zero-delay instantaneous mouse movement
    void mouseMove(int dx, int dy) {
      if (!isConnected())
        return;
      int8_t clampedX = dx > 127 ? 127 : (dx < -127 ? -127 : dx);
      int8_t clampedY = dy > 127 ? 127 : (dy < -127 ? -127 : dy);
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbMouse) {
        m_usbMouse->move(clampedX, clampedY, 0, 0);
        return;
      }
#endif
      m_combo.sendMouseReport(clampedX, clampedY, 0, m_mouseButtons);
    }

    // Zero-delay scroll
    void mouseScroll(int amount) {
      if (!isConnected())
        return;
      int8_t clamped = amount > 127 ? 127 : (amount < -127 ? -127 : amount);
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbMouse) {
        m_usbMouse->move(0, 0, clamped, 0);
        return;
      }
#endif
      m_combo.sendMouseReport(0, 0, clamped, m_mouseButtons);
    }

    void mouseClick(uint8_t button = ESP32BLECombo::MOUSE_LEFT) {
      if (!isConnected())
        return;
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbMouse) {
        m_usbMouse->press(button);
        delay(15);
        m_usbMouse->release(button);
        return;
      }
#endif
      m_combo.sendMouseReport(0, 0, 0, m_mouseButtons | button);
      delay(15);
      m_combo.sendMouseReport(0, 0, 0, m_mouseButtons);
    }

    void mousePress(uint8_t button = ESP32BLECombo::MOUSE_LEFT) {
      if (!isConnected())
        return;
      m_mouseButtons |= button;
      m_lastMousePressMs = millis();
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbMouse) {
        m_usbMouse->press(button);
        return;
      }
#endif
      m_combo.sendMouseReport(0, 0, 0, m_mouseButtons);
    }

    void mouseRelease(uint8_t button = ESP32BLECombo::MOUSE_LEFT) {
      if (!isConnected())
        return;
      unsigned long elapsed = millis() - m_lastMousePressMs;
      if (elapsed < 15) {
        delay(15 - elapsed);
      }
      m_mouseButtons &= ~button;
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbMouse) {
        m_usbMouse->release(button);
        return;
      }
#endif
      m_combo.sendMouseReport(0, 0, 0, m_mouseButtons);
    }

  private:
    BLEManager()
        : m_initialized(false), m_radioEnabled(true), m_usbMode(false),
          m_mouseButtons(0), m_lastMousePressMs(0) {}
    ESP32BLECombo m_combo;
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
    USBHIDKeyboard *m_usbKbd = nullptr;
    USBHIDMouse *m_usbMouse = nullptr;
    USBHIDConsumerControl *m_usbConsumer = nullptr;
#endif
    bool m_initialized;
    bool m_radioEnabled;
    bool m_usbMode;
    uint8_t m_mouseButtons;
    unsigned long m_lastMousePressMs;

    void tapKey(uint8_t key) {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbKbd) {
        m_usbKbd->press(key);
        delay(15);
        m_usbKbd->release(key);
        delay(12);
        return;
      }
#endif
      m_combo.press(key);
      delay(25);
      m_combo.release(key);
      delay(15);
    }

    void tapRawKey(uint8_t usage) {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbKbd) {
        m_usbKbd->pressRaw(usage);
        delay(15);
        m_usbKbd->releaseRaw(usage);
        delay(12);
        return;
      }
#endif
      m_combo.pressRaw(usage);
      delay(25);
      m_combo.releaseRaw(usage);
      delay(15);
    }

    void tapConsumer(uint16_t usage) {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_TINYUSB_HID_ENABLED
      if (m_usbMode && m_usbConsumer) {
        m_usbConsumer->press(usage);
        delay(15);
        m_usbConsumer->release();
        return;
      }
#endif
      m_combo.sendConsumerReport(usage);
      delay(15);
      m_combo.sendConsumerReport(0x0000);
    }
  };
