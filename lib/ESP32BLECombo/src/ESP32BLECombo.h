#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEHIDDevice.h>
#include <NimBLECharacteristic.h>
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"

class ESP32BLECombo;

enum class ESP32BLEComboMode : uint8_t {
    KEYBOARD_ONLY,
    MOUSE_ONLY,
    KEYBOARD_MOUSE,
    GAMEPAD_ONLY,
    KEYBOARD_MOUSE_GAMEPAD
};

enum class ESP32BLEComboAppearance : uint16_t {
    AUTO = 0,
    KEYBOARD = 0x03C1,
    MOUSE = 0x03C2,
    GENERIC_HID_APPEARANCE = 0x03C0,
    GAMEPAD = 0x03C4
};

enum class ESP32BLEGamepadLayout : uint8_t {
    GENERIC,
    XBOX_STYLE
};

struct ESP32BLEComboConfig {
    ESP32BLEComboMode mode = ESP32BLEComboMode::KEYBOARD_MOUSE;
    ESP32BLEGamepadLayout gamepadLayout = ESP32BLEGamepadLayout::GENERIC;
    String deviceName = "ESP32 Combo";
    String manufacturer = "Espressif";
    uint8_t batteryLevel = 100;
    ESP32BLEComboAppearance appearance = ESP32BLEComboAppearance::AUTO;
    bool enableSecurity = true;
    bool useStaticAddress = false;
    String staticAddress = "";
    uint16_t vendorId = 0xE502;
    uint16_t productId = 0xA111;
    uint16_t productVersion = 0x0210;
    uint8_t countryCode = 0x00;
    uint8_t hidFlags = 0x01;
    uint16_t keyPressDelayMs = 20;
    uint16_t keyReleaseDelayMs = 20;
    uint16_t keyIntervalDelayMs = 10;
};

class ESP32BLEComboServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ESP32BLEComboServerCallbacks(ESP32BLECombo* owner) : m_owner(owner) {}
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
private:
    ESP32BLECombo* m_owner;
};

class ESP32BLECombo {
public:
    ESP32BLECombo();

    bool begin(const ESP32BLEComboConfig& config = ESP32BLEComboConfig());
    void end();

    bool isConnected() const;
    ESP32BLEComboMode getMode() const;

    void setBatteryLevel(uint8_t level);
    void setDeviceName(const String& name);
    void setManufacturer(const String& manufacturer);
    void setKeyPressDelay(uint16_t ms);
    void setKeyReleaseDelay(uint16_t ms);
    void setKeyIntervalDelay(uint16_t ms);

    bool print(const char* text);
    bool print(const String& text);
    bool write(uint8_t c);
    bool press(uint8_t k);
    bool release(uint8_t k);
    bool pressRaw(uint8_t usage);
    bool releaseRaw(uint8_t usage);
    bool releaseAll();
    void sendConsumerReport(uint16_t usage);

    bool mouseMove(int x, int y);
    bool mouseScroll(int amount);
    bool mousePress(uint8_t buttons);
    bool mouseRelease(uint8_t buttons);
    bool mouseClick(uint8_t buttons);
    void sendMouseReport(int8_t x, int8_t y, int8_t wheel, uint8_t buttons);

    bool gamepadSetButtons(uint16_t buttons);
    bool gamepadPress(uint8_t buttonIndex);
    bool gamepadRelease(uint8_t buttonIndex);
    bool gamepadSetAxes(int16_t x, int16_t y, int16_t z = 0, int16_t rz = 0);
    bool gamepadSetHat(uint8_t hat);
    bool gamepadReleaseAllButtons();
    bool gamepadSetTriggers(int16_t leftTrigger, int16_t rightTrigger);
    bool gamepadSetLeftStick(int16_t x, int16_t y);
    bool gamepadSetRightStick(int16_t x, int16_t y);

    static constexpr uint8_t MOUSE_LEFT   = 0x01;
    static constexpr uint8_t MOUSE_RIGHT  = 0x02;
    static constexpr uint8_t MOUSE_MIDDLE = 0x04;
    static constexpr uint8_t HAT_CENTERED   = 0x08;
    static constexpr uint8_t HAT_UP         = 0x00;
    static constexpr uint8_t HAT_UP_RIGHT   = 0x01;
    static constexpr uint8_t HAT_RIGHT      = 0x02;
    static constexpr uint8_t HAT_DOWN_RIGHT = 0x03;
    static constexpr uint8_t HAT_DOWN       = 0x04;
    static constexpr uint8_t HAT_DOWN_LEFT  = 0x05;
    static constexpr uint8_t HAT_LEFT       = 0x06;
    static constexpr uint8_t HAT_UP_LEFT    = 0x07;

private:
    friend class ESP32BLEComboServerCallbacks;

    struct KeyReport {
        uint8_t modifiers;
        uint8_t reserved;
        uint8_t keys[6];
    };

    struct MouseReport {
        uint8_t buttons;
        int8_t x;
        int8_t y;
        int8_t wheel;
    };

    struct GamepadReport {
        uint16_t buttons;
        int16_t x;
        int16_t y;
        int16_t z;
        int16_t rz;
        uint8_t hat;
        uint8_t padding;
    };

    bool m_connected;
    bool m_started;
    ESP32BLEComboConfig m_config;
    ESP32BLEComboMode m_mode;
    KeyReport m_keyReport;
    MouseReport m_mouseReport;
    GamepadReport m_gamepadReport;
    uint16_t m_keyPressDelayMs;
    uint16_t m_keyReleaseDelayMs;
    uint16_t m_keyIntervalDelayMs;

    NimBLEServer* m_server;
    NimBLEHIDDevice* m_hid;
    NimBLECharacteristic* m_inputKeyboard;
    NimBLECharacteristic* m_outputKeyboard;
    NimBLECharacteristic* m_inputConsumer;
    NimBLECharacteristic* m_inputMouse;
    NimBLECharacteristic* m_inputGamepad;

    void handleConnected(bool connected);
    bool initAddressOverride();
    uint16_t resolveAppearance() const;
    void startAdvertising();

    void sendKeyboardReport();
    void sendGamepadReport();
    void resetReports();

    bool keyboardEnabled() const;
    bool mouseEnabled() const;
    bool gamepadEnabled() const;
    bool comboAllEnabled() const;

    static const uint8_t* keyboardReportMap(size_t& len);
    static const uint8_t* mouseReportMap(size_t& len);
    static const uint8_t* comboReportMap(size_t& len);
    static const uint8_t* gamepadReportMap(size_t& len);
};
