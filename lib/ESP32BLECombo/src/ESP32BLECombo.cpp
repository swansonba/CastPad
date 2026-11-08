#include "ESP32BLECombo.h"
#include <string>

namespace {
constexpr uint8_t KEYBOARD_ID = 0x01;
constexpr uint8_t MOUSE_ID = 0x02;
constexpr uint8_t GAMEPAD_ID = 0x03;
constexpr uint8_t CONSUMER_ID = 0x04;
constexpr uint8_t SHIFT = 0x80;

static const uint8_t kKeyboardMap[] = {
    USAGE_PAGE(1), 0x01, USAGE(1), 0x06, COLLECTION(1), 0x01,
    REPORT_ID(1), KEYBOARD_ID,
    USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0xE0, USAGE_MAXIMUM(1), 0xE7,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x08, HIDINPUT(1), 0x02,
    REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x08, HIDINPUT(1), 0x01,
    REPORT_COUNT(1), 0x05, REPORT_SIZE(1), 0x01,
    USAGE_PAGE(1), 0x08, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x05,
    HIDOUTPUT(1), 0x02,
    REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x03, HIDOUTPUT(1), 0x01,
    REPORT_COUNT(1), 0x06, REPORT_SIZE(1), 0x08,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(2), 0xFF, 0x00,
    USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(2), 0xFF, 0x00,
    HIDINPUT(1), 0x00, END_COLLECTION(0),

    USAGE_PAGE(1), 0x0C, USAGE(1), 0x01, COLLECTION(1), 0x01,
    REPORT_ID(1), CONSUMER_ID,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(2), 0xFF, 0x03,
    USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(2), 0xFF, 0x03,
    REPORT_SIZE(1), 0x10, REPORT_COUNT(1), 0x01,
    HIDINPUT(1), 0x00, END_COLLECTION(0),
};

static const uint8_t kMouseMap[] = {
    USAGE_PAGE(1), 0x01, USAGE(1), 0x02, COLLECTION(1), 0x01,
    REPORT_ID(1), MOUSE_ID,
    USAGE(1), 0x01, COLLECTION(1), 0x00,
    USAGE_PAGE(1), 0x09, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x03,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x03, HIDINPUT(1), 0x02,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x05, HIDINPUT(1), 0x01,
    USAGE_PAGE(1), 0x01, USAGE(1), 0x30, USAGE(1), 0x31, USAGE(1), 0x38,
    LOGICAL_MINIMUM(1), 0x81, LOGICAL_MAXIMUM(1), 0x7F,
    REPORT_SIZE(1), 0x08, REPORT_COUNT(1), 0x03, HIDINPUT(1), 0x06,
    END_COLLECTION(0), END_COLLECTION(0),
};

static const uint8_t kGamepadMap[] = {
    USAGE_PAGE(1), 0x01, USAGE(1), 0x05, COLLECTION(1), 0x01,
    REPORT_ID(1), GAMEPAD_ID,
    USAGE_PAGE(1), 0x09, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x10,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x10, HIDINPUT(1), 0x02,
    USAGE_PAGE(1), 0x01,
    USAGE(1), 0x30,
    USAGE(1), 0x31,
    USAGE(1), 0x32,
    USAGE(1), 0x35,
    LOGICAL_MINIMUM(2), 0x00, 0x80,
    LOGICAL_MAXIMUM(2), 0xFF, 0x7F,
    REPORT_SIZE(1), 0x10, REPORT_COUNT(1), 0x04, HIDINPUT(1), 0x02,
    USAGE(1), 0x39,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x08,
    PHYSICAL_MINIMUM(1), 0x00, PHYSICAL_MAXIMUM(2), 0x3B, 0x01,
    UNIT(1), 0x14,
    REPORT_SIZE(1), 0x04, REPORT_COUNT(1), 0x01, HIDINPUT(1), 0x42,
    REPORT_SIZE(1), 0x04, REPORT_COUNT(1), 0x01, HIDINPUT(1), 0x01,
    END_COLLECTION(0),
};

static const uint8_t kComboMap[] = {
    USAGE_PAGE(1), 0x01, USAGE(1), 0x06, COLLECTION(1), 0x01,
    REPORT_ID(1), KEYBOARD_ID,
    USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0xE0, USAGE_MAXIMUM(1), 0xE7,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x08, HIDINPUT(1), 0x02,
    REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x08, HIDINPUT(1), 0x01,
    REPORT_COUNT(1), 0x05, REPORT_SIZE(1), 0x01,
    USAGE_PAGE(1), 0x08, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x05,
    HIDOUTPUT(1), 0x02,
    REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x03, HIDOUTPUT(1), 0x01,
    REPORT_COUNT(1), 0x06, REPORT_SIZE(1), 0x08,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x65,
    USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(1), 0x65,
    HIDINPUT(1), 0x00, END_COLLECTION(0),

    USAGE_PAGE(1), 0x01, USAGE(1), 0x02, COLLECTION(1), 0x01,
    REPORT_ID(1), MOUSE_ID,
    USAGE(1), 0x01, COLLECTION(1), 0x00,
    USAGE_PAGE(1), 0x09, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x03,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x03, HIDINPUT(1), 0x02,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x05, HIDINPUT(1), 0x01,
    USAGE_PAGE(1), 0x01, USAGE(1), 0x30, USAGE(1), 0x31, USAGE(1), 0x38,
    LOGICAL_MINIMUM(1), 0x81, LOGICAL_MAXIMUM(1), 0x7F,
    REPORT_SIZE(1), 0x08, REPORT_COUNT(1), 0x03, HIDINPUT(1), 0x06,
    END_COLLECTION(0), END_COLLECTION(0),

    USAGE_PAGE(1), 0x0C, USAGE(1), 0x01, COLLECTION(1), 0x01,
    REPORT_ID(1), CONSUMER_ID,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(2), 0xFF, 0x03,
    USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(2), 0xFF, 0x03,
    REPORT_SIZE(1), 0x10, REPORT_COUNT(1), 0x01,
    HIDINPUT(1), 0x00, END_COLLECTION(0),
};

static const uint8_t kComboAllMap[] = {
    USAGE_PAGE(1), 0x01, USAGE(1), 0x06, COLLECTION(1), 0x01,
    REPORT_ID(1), KEYBOARD_ID,
    USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0xE0, USAGE_MAXIMUM(1), 0xE7,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x08, HIDINPUT(1), 0x02,
    REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x08, HIDINPUT(1), 0x01,
    REPORT_COUNT(1), 0x05, REPORT_SIZE(1), 0x01,
    USAGE_PAGE(1), 0x08, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x05,
    HIDOUTPUT(1), 0x02,
    REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x03, HIDOUTPUT(1), 0x01,
    REPORT_COUNT(1), 0x06, REPORT_SIZE(1), 0x08,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(2), 0xFF, 0x00,
    USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(2), 0xFF, 0x00,
    HIDINPUT(1), 0x00, END_COLLECTION(0),

    USAGE_PAGE(1), 0x0C, USAGE(1), 0x01, COLLECTION(1), 0x01,
    REPORT_ID(1), CONSUMER_ID,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(2), 0xFF, 0x03,
    USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(2), 0xFF, 0x03,
    REPORT_SIZE(1), 0x10, REPORT_COUNT(1), 0x01,
    HIDINPUT(1), 0x00, END_COLLECTION(0),

    USAGE_PAGE(1), 0x01, USAGE(1), 0x02, COLLECTION(1), 0x01,
    REPORT_ID(1), MOUSE_ID,
    USAGE(1), 0x01, COLLECTION(1), 0x00,
    USAGE_PAGE(1), 0x09, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x03,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x03, HIDINPUT(1), 0x02,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x05, HIDINPUT(1), 0x01,
    USAGE_PAGE(1), 0x01, USAGE(1), 0x30, USAGE(1), 0x31, USAGE(1), 0x38,
    LOGICAL_MINIMUM(1), 0x81, LOGICAL_MAXIMUM(1), 0x7F,
    REPORT_SIZE(1), 0x08, REPORT_COUNT(1), 0x03, HIDINPUT(1), 0x06,
    END_COLLECTION(0), END_COLLECTION(0),

    USAGE_PAGE(1), 0x01, USAGE(1), 0x05, COLLECTION(1), 0x01,
    REPORT_ID(1), GAMEPAD_ID,
    USAGE_PAGE(1), 0x09, USAGE_MINIMUM(1), 0x01, USAGE_MAXIMUM(1), 0x10,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x10, HIDINPUT(1), 0x02,
    USAGE_PAGE(1), 0x01,
    USAGE(1), 0x30, USAGE(1), 0x31, USAGE(1), 0x32, USAGE(1), 0x35,
    LOGICAL_MINIMUM(2), 0x00, 0x80,
    LOGICAL_MAXIMUM(2), 0xFF, 0x7F,
    REPORT_SIZE(1), 0x10, REPORT_COUNT(1), 0x04, HIDINPUT(1), 0x02,
    USAGE(1), 0x39,
    LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x08,
    PHYSICAL_MINIMUM(1), 0x00, PHYSICAL_MAXIMUM(2), 0x3B, 0x01,
    UNIT(1), 0x14,
    REPORT_SIZE(1), 0x04, REPORT_COUNT(1), 0x01, HIDINPUT(1), 0x42,
    REPORT_SIZE(1), 0x04, REPORT_COUNT(1), 0x01, HIDINPUT(1), 0x01,
    END_COLLECTION(0),
};

static const uint8_t asciimap[128] PROGMEM = {
    0,0,0,0,0,0,0,0,0x2a,0x2b,0x28,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0x2c,0x1e|SHIFT,0x34|SHIFT,0x20|SHIFT,0x21|SHIFT,0x22|SHIFT,0x24|SHIFT,0x34,
    0x26|SHIFT,0x27|SHIFT,0x25|SHIFT,0x2e|SHIFT,0x36,0x2d,0x37,0x38,
    0x27,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
    0x33|SHIFT,0x33,0x36|SHIFT,0x2e,0x37|SHIFT,0x38|SHIFT,0x1f|SHIFT,
    0x04|SHIFT,0x05|SHIFT,0x06|SHIFT,0x07|SHIFT,0x08|SHIFT,0x09|SHIFT,0x0a|SHIFT,
    0x0b|SHIFT,0x0c|SHIFT,0x0d|SHIFT,0x0e|SHIFT,0x0f|SHIFT,0x10|SHIFT,0x11|SHIFT,
    0x12|SHIFT,0x13|SHIFT,0x14|SHIFT,0x15|SHIFT,0x16|SHIFT,0x17|SHIFT,0x18|SHIFT,
    0x19|SHIFT,0x1a|SHIFT,0x1b|SHIFT,0x1c|SHIFT,0x1d|SHIFT,
    0x2f,0x31,0x30,0x23|SHIFT,0x2d|SHIFT,0x35,
    0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,
    0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,
    0x2f|SHIFT,0x31|SHIFT,0x30|SHIFT,0x35|SHIFT,0
};
}

ESP32BLECombo::ESP32BLECombo()
    : m_connected(false), m_started(false), m_mode(ESP32BLEComboMode::KEYBOARD_MOUSE),
      m_keyPressDelayMs(20), m_keyReleaseDelayMs(20), m_keyIntervalDelayMs(10),
      m_server(nullptr), m_hid(nullptr), m_inputKeyboard(nullptr), m_outputKeyboard(nullptr),
      m_inputConsumer(nullptr), m_inputMouse(nullptr), m_inputGamepad(nullptr) {
    resetReports();
}

void ESP32BLEComboServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    NimBLEDevice::startSecurity(connInfo.getConnHandle());
    if (pServer) {
        // 7.5ms-11.25ms interval (6-9) with slave latency = 6 so ESP32-S3 skips
        // up to 6 empty connection ticks when idle (~85% fewer radio wakeups)
        // while still transmitting on the very next 7.5ms tick on input
        pServer->updateConnParams(connInfo.getConnHandle(), 6, 9, 6, 400);
        pServer->updatePhy(connInfo.getConnHandle(), BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_CODED_ANY);
        pServer->sendServiceChangedIndication();
    }
    m_owner->handleConnected(true);
}

void ESP32BLEComboServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    (void)pServer; (void)connInfo; (void)reason;
    m_owner->handleConnected(false);
}

void ESP32BLECombo::handleConnected(bool connected) { m_connected = connected; }

bool ESP32BLECombo::keyboardEnabled() const {
    return m_mode == ESP32BLEComboMode::KEYBOARD_ONLY || m_mode == ESP32BLEComboMode::KEYBOARD_MOUSE || m_mode == ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD;
}

bool ESP32BLECombo::mouseEnabled() const {
    return m_mode == ESP32BLEComboMode::MOUSE_ONLY || m_mode == ESP32BLEComboMode::KEYBOARD_MOUSE || m_mode == ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD;
}

bool ESP32BLECombo::gamepadEnabled() const {
    return m_mode == ESP32BLEComboMode::GAMEPAD_ONLY || m_mode == ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD;
}

bool ESP32BLECombo::comboAllEnabled() const { return m_mode == ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD; }

void ESP32BLECombo::resetReports() {
    memset(&m_keyReport, 0, sizeof(m_keyReport));
    memset(&m_mouseReport, 0, sizeof(m_mouseReport));
    memset(&m_gamepadReport, 0, sizeof(m_gamepadReport));
    m_gamepadReport.hat = HAT_CENTERED;
}

const uint8_t* ESP32BLECombo::keyboardReportMap(size_t& len) { len = sizeof(kKeyboardMap); return kKeyboardMap; }
const uint8_t* ESP32BLECombo::mouseReportMap(size_t& len) { len = sizeof(kMouseMap); return kMouseMap; }
const uint8_t* ESP32BLECombo::comboReportMap(size_t& len) { len = sizeof(kComboMap); return kComboMap; }
const uint8_t* ESP32BLECombo::gamepadReportMap(size_t& len) { len = sizeof(kGamepadMap); return kGamepadMap; }

uint16_t ESP32BLECombo::resolveAppearance() const {
    if (m_config.appearance != ESP32BLEComboAppearance::AUTO) return static_cast<uint16_t>(m_config.appearance);
    switch (m_mode) {
        case ESP32BLEComboMode::KEYBOARD_ONLY: return static_cast<uint16_t>(ESP32BLEComboAppearance::KEYBOARD);
        case ESP32BLEComboMode::MOUSE_ONLY: return static_cast<uint16_t>(ESP32BLEComboAppearance::MOUSE);
        case ESP32BLEComboMode::GAMEPAD_ONLY: return static_cast<uint16_t>(ESP32BLEComboAppearance::GAMEPAD);
        case ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD: return static_cast<uint16_t>(ESP32BLEComboAppearance::GENERIC_HID_APPEARANCE);
        default: return static_cast<uint16_t>(ESP32BLEComboAppearance::GENERIC_HID_APPEARANCE);
    }
}

bool ESP32BLECombo::initAddressOverride() {
    if (!m_config.useStaticAddress || m_config.staticAddress.isEmpty()) return true;
    NimBLEAddress addr(std::string(m_config.staticAddress.c_str()), BLE_OWN_ADDR_RANDOM);
    return NimBLEDevice::setOwnAddr(addr);
}

bool ESP32BLECombo::begin(const ESP32BLEComboConfig& config) {
    if (m_started) return true;
    m_config = config;
    m_mode = config.mode;
    m_keyPressDelayMs = m_config.keyPressDelayMs;
    m_keyReleaseDelayMs = m_config.keyReleaseDelayMs;
    m_keyIntervalDelayMs = m_config.keyIntervalDelayMs;
    resetReports();

    if (m_config.useStaticAddress) initAddressOverride();
    NimBLEDevice::init(m_config.deviceName.c_str());
    if (m_config.enableSecurity) {
        NimBLEDevice::setSecurityAuth(true, false, true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
        NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    }

    m_server = NimBLEDevice::createServer();
    m_server->setCallbacks(new ESP32BLEComboServerCallbacks(this));
    m_server->advertiseOnDisconnect(true);

    m_hid = new NimBLEHIDDevice(m_server);
    if (keyboardEnabled()) {
        m_inputKeyboard = m_hid->getInputReport(KEYBOARD_ID);
        m_outputKeyboard = m_hid->getOutputReport(KEYBOARD_ID);
    }
    if (mouseEnabled()) m_inputMouse = m_hid->getInputReport(MOUSE_ID);
    if (keyboardEnabled()) m_inputConsumer = m_hid->getInputReport(CONSUMER_ID);
    if (gamepadEnabled()) m_inputGamepad = m_hid->getInputReport(GAMEPAD_ID);

    m_hid->setManufacturer(m_config.manufacturer.c_str());
    m_hid->setPnp(0x02, m_config.vendorId, m_config.productId, m_config.productVersion);
    m_hid->setHidInfo(m_config.countryCode, m_config.hidFlags);

    size_t mapLen = 0;
    const uint8_t* map = nullptr;
    switch (m_mode) {
        case ESP32BLEComboMode::KEYBOARD_ONLY: map = keyboardReportMap(mapLen); break;
        case ESP32BLEComboMode::MOUSE_ONLY: map = mouseReportMap(mapLen); break;
        case ESP32BLEComboMode::GAMEPAD_ONLY: map = gamepadReportMap(mapLen); break;
        case ESP32BLEComboMode::KEYBOARD_MOUSE_GAMEPAD: map = kComboAllMap; mapLen = sizeof(kComboAllMap); break;
        default: map = comboReportMap(mapLen); break;
    }

    m_hid->setReportMap(const_cast<uint8_t*>(map), mapLen);
    setBatteryLevel(m_config.batteryLevel);
    startAdvertising();
    m_started = true;
    return true;
}

void ESP32BLECombo::startAdvertising() {
    NimBLEAdvertising* adv = m_server->getAdvertising();
    adv->setAppearance(resolveAppearance());
    adv->addServiceUUID(m_hid->getHidService()->getUUID());
    adv->enableScanResponse(false);
    adv->start();
}

void ESP32BLECombo::end() {
    if (!m_started) return;
    NimBLEDevice::deinit(true);
    m_server = nullptr; m_hid = nullptr; m_inputKeyboard = nullptr; m_outputKeyboard = nullptr;
    m_inputConsumer = nullptr; m_inputMouse = nullptr; m_inputGamepad = nullptr; m_connected = false; m_started = false;
}

bool ESP32BLECombo::isConnected() const { return m_connected; }
ESP32BLEComboMode ESP32BLECombo::getMode() const { return m_mode; }
void ESP32BLECombo::setBatteryLevel(uint8_t level) { m_config.batteryLevel = level; if (m_hid) m_hid->setBatteryLevel(level); }
void ESP32BLECombo::setDeviceName(const String& name) { m_config.deviceName = name; }
void ESP32BLECombo::setManufacturer(const String& manufacturer) { m_config.manufacturer = manufacturer; if (m_hid) m_hid->setManufacturer(manufacturer.c_str()); }
void ESP32BLECombo::setKeyPressDelay(uint16_t ms) { m_keyPressDelayMs = ms; m_config.keyPressDelayMs = ms; }
void ESP32BLECombo::setKeyReleaseDelay(uint16_t ms) { m_keyReleaseDelayMs = ms; m_config.keyReleaseDelayMs = ms; }
void ESP32BLECombo::setKeyIntervalDelay(uint16_t ms) { m_keyIntervalDelayMs = ms; m_config.keyIntervalDelayMs = ms; }

void ESP32BLECombo::sendKeyboardReport() {
    if (!m_connected || !m_inputKeyboard || !keyboardEnabled()) return;
    m_inputKeyboard->setValue(reinterpret_cast<uint8_t*>(&m_keyReport), sizeof(m_keyReport));
    m_inputKeyboard->notify(reinterpret_cast<uint8_t*>(&m_keyReport), sizeof(m_keyReport));
}

void ESP32BLECombo::sendConsumerReport(uint16_t usage) {
    if (!m_connected || !m_inputConsumer || !keyboardEnabled()) return;
    uint8_t report[2] = {
        static_cast<uint8_t>(usage & 0xFF),
        static_cast<uint8_t>((usage >> 8) & 0xFF)
    };
    m_inputConsumer->setValue(report, sizeof(report));
    m_inputConsumer->notify(report, sizeof(report));
}

void ESP32BLECombo::sendMouseReport(int8_t x, int8_t y, int8_t wheel, uint8_t buttons) {
    if (!m_connected || !m_inputMouse || !mouseEnabled()) return;
    m_mouseReport.buttons = buttons; m_mouseReport.x = x; m_mouseReport.y = y; m_mouseReport.wheel = wheel;
    m_inputMouse->setValue(reinterpret_cast<uint8_t*>(&m_mouseReport), sizeof(m_mouseReport));
    m_inputMouse->notify(reinterpret_cast<uint8_t*>(&m_mouseReport), sizeof(m_mouseReport));
}

void ESP32BLECombo::sendGamepadReport() {
    if (!m_connected || !m_inputGamepad || !gamepadEnabled()) return;
    m_inputGamepad->setValue(reinterpret_cast<uint8_t*>(&m_gamepadReport), sizeof(m_gamepadReport));
    m_inputGamepad->notify(reinterpret_cast<uint8_t*>(&m_gamepadReport), sizeof(m_gamepadReport));
}

bool ESP32BLECombo::write(uint8_t c) {
    if (!keyboardEnabled()) return false;
    if (!press(c)) return false;
    delay(m_keyPressDelayMs);
    if (!release(c)) return false;
    delay(m_keyReleaseDelayMs);
    delay(m_keyIntervalDelayMs);
    return true;
}

bool ESP32BLECombo::print(const char* text) {
    if (!keyboardEnabled() || text == nullptr) return false;
    while (*text) if (!write(static_cast<uint8_t>(*text++))) return false;
    releaseAll();
    return true;
}

bool ESP32BLECombo::print(const String& text) { return print(text.c_str()); }

bool ESP32BLECombo::press(uint8_t k) {
    if (!keyboardEnabled()) return false;
    if (k >= 136) k -= 136;
    else if (k >= 128) { m_keyReport.modifiers |= (1 << (k - 128)); k = 0; }
    else {
        k = pgm_read_byte(asciimap + k);
        if (!k) return false;
        if (k & 0x80) { m_keyReport.modifiers |= 0x02; k &= 0x7F; }
    }
    for (int i = 0; i < 6; i++) if (m_keyReport.keys[i] == 0) { m_keyReport.keys[i] = k; break; }
    sendKeyboardReport();
    return true;
}

bool ESP32BLECombo::release(uint8_t k) {
    if (!keyboardEnabled()) return false;
    if (k >= 136) k -= 136;
    else if (k >= 128) { m_keyReport.modifiers &= ~(1 << (k - 128)); k = 0; }
    else {
        k = pgm_read_byte(asciimap + k);
        if (!k) return false;
        if (k & 0x80) { m_keyReport.modifiers &= ~0x02; k &= 0x7F; }
    }
    for (int i = 0; i < 6; i++) if (m_keyReport.keys[i] == k) m_keyReport.keys[i] = 0;
    sendKeyboardReport();
    return true;
}

bool ESP32BLECombo::pressRaw(uint8_t usage) {
    if (!keyboardEnabled() || usage == 0) return false;
    if (usage >= 0xE0 && usage <= 0xE7) {
        m_keyReport.modifiers |= (1 << (usage - 0xE0));
    } else {
        for (int i = 0; i < 6; i++) {
            if (m_keyReport.keys[i] == usage) break;
            if (m_keyReport.keys[i] == 0) {
                m_keyReport.keys[i] = usage;
                break;
            }
        }
    }
    sendKeyboardReport();
    return true;
}

bool ESP32BLECombo::releaseRaw(uint8_t usage) {
    if (!keyboardEnabled() || usage == 0) return false;
    if (usage >= 0xE0 && usage <= 0xE7) {
        m_keyReport.modifiers &= ~(1 << (usage - 0xE0));
    } else {
        for (int i = 0; i < 6; i++) {
            if (m_keyReport.keys[i] == usage) m_keyReport.keys[i] = 0;
        }
    }
    sendKeyboardReport();
    return true;
}

bool ESP32BLECombo::releaseAll() {
    if (!keyboardEnabled()) return false;
    memset(&m_keyReport, 0, sizeof(m_keyReport));
    sendKeyboardReport();
    return true;
}

bool ESP32BLECombo::mouseMove(int x, int y) {
    if (!mouseEnabled()) return false;
    while (x != 0 || y != 0) {
        int8_t dx = x > 127 ? 127 : (x < -127 ? -127 : x);
        int8_t dy = y > 127 ? 127 : (y < -127 ? -127 : y);
        sendMouseReport(dx, dy, 0, m_mouseReport.buttons);
        delay(8);
        sendMouseReport(0, 0, 0, m_mouseReport.buttons);
        delay(8);
        x -= dx; y -= dy;
    }
    return true;
}

bool ESP32BLECombo::mouseScroll(int amount) {
    if (!mouseEnabled()) return false;
    while (amount != 0) {
        int8_t step = amount > 127 ? 127 : (amount < -127 ? -127 : amount);
        sendMouseReport(0, 0, step, m_mouseReport.buttons);
        delay(8);
        sendMouseReport(0, 0, 0, m_mouseReport.buttons);
        delay(8);
        amount -= step;
    }
    return true;
}

bool ESP32BLECombo::mousePress(uint8_t buttons) {
    if (!mouseEnabled()) return false;
    m_mouseReport.buttons |= buttons;
    sendMouseReport(0, 0, 0, m_mouseReport.buttons);
    return true;
}

bool ESP32BLECombo::mouseRelease(uint8_t buttons) {
    if (!mouseEnabled()) return false;
    m_mouseReport.buttons &= ~buttons;
    sendMouseReport(0, 0, 0, m_mouseReport.buttons);
    return true;
}

bool ESP32BLECombo::mouseClick(uint8_t buttons) {
    if (!mouseEnabled()) return false;
    mousePress(buttons); delay(20); mouseRelease(buttons); delay(20); return true;
}

bool ESP32BLECombo::gamepadSetButtons(uint16_t buttons) {
    if (!gamepadEnabled()) return false;
    m_gamepadReport.buttons = buttons; sendGamepadReport(); return true;
}

bool ESP32BLECombo::gamepadPress(uint8_t buttonIndex) {
    if (!gamepadEnabled() || buttonIndex < 1 || buttonIndex > 16) return false;
    m_gamepadReport.buttons |= (1u << (buttonIndex - 1)); sendGamepadReport(); return true;
}

bool ESP32BLECombo::gamepadRelease(uint8_t buttonIndex) {
    if (!gamepadEnabled() || buttonIndex < 1 || buttonIndex > 16) return false;
    m_gamepadReport.buttons &= ~(1u << (buttonIndex - 1)); sendGamepadReport(); return true;
}

bool ESP32BLECombo::gamepadSetAxes(int16_t x, int16_t y, int16_t z, int16_t rz) {
    if (!gamepadEnabled()) return false;
    m_gamepadReport.x = x; m_gamepadReport.y = y; m_gamepadReport.z = z; m_gamepadReport.rz = rz; sendGamepadReport(); return true;
}

bool ESP32BLECombo::gamepadSetHat(uint8_t hat) {
    if (!gamepadEnabled()) return false;
    m_gamepadReport.hat = hat; sendGamepadReport(); return true;
}

bool ESP32BLECombo::gamepadReleaseAllButtons() {
    if (!gamepadEnabled()) return false;
    m_gamepadReport.buttons = 0; sendGamepadReport(); return true;
}

bool ESP32BLECombo::gamepadSetTriggers(int16_t leftTrigger, int16_t rightTrigger) {
    if (!gamepadEnabled()) return false;
    m_gamepadReport.z = leftTrigger;
    m_gamepadReport.rz = rightTrigger;
    sendGamepadReport();
    return true;
}

bool ESP32BLECombo::gamepadSetLeftStick(int16_t x, int16_t y) {
    if (!gamepadEnabled()) return false;
    m_gamepadReport.x = x;
    m_gamepadReport.y = y;
    sendGamepadReport();
    return true;
}

bool ESP32BLECombo::gamepadSetRightStick(int16_t x, int16_t y) {
    if (!gamepadEnabled()) return false;
    if (m_config.gamepadLayout == ESP32BLEGamepadLayout::XBOX_STYLE) {
        m_gamepadReport.z = x;
        m_gamepadReport.rz = y;
    } else {
        m_gamepadReport.z = x;
        m_gamepadReport.rz = y;
    }
    sendGamepadReport();
    return true;
}
