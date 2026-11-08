#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr int QUICK_LINKS_COUNT = 60;
static constexpr int QUICK_LINKS_PAGE_SIZE = 12;

struct AppConfig {
  String ssid = "";
  String password = "";
  String hostname = "CastPad";
  String btnRed = "F1";
  String btnGreen = "F2";
  String btnYellow = "F3";
  String btnBlue = "F4";
  String btnCustom1 = "";
  String btnCustom2 = "";
  String btnCustom3 = "";
  String btnCustom4 = "";
  String lblRed = "";
  String lblGreen = "";
  String lblYellow = "";
  String lblBlue = "";
  String lblCustom1 = "";
  String lblCustom2 = "";
  String lblCustom3 = "";
  String lblCustom4 = "";
  String iconRed = "";
  String iconGreen = "";
  String iconYellow = "";
  String iconBlue = "";
  String iconCustom1 = "";
  String iconCustom2 = "";
  String iconCustom3 = "";
  String iconCustom4 = "";
  String colRed = "";
  String colGreen = "";
  String colYellow = "";
  String colBlue = "";
  String colCustom1 = "";
  String colCustom2 = "";
  String colCustom3 = "";
  String colCustom4 = "";
  int qlPages = 3;
  String qlLabel[QUICK_LINKS_COUNT];
  String qlNum[QUICK_LINKS_COUNT];
  String qlColor[QUICK_LINKS_COUNT];
  String qlIcon[QUICK_LINKS_COUNT];
  float mouseSensitivity = 1.3f;
  bool bleRadioEnabled = true;
  bool usbMode = false;
  bool apMode = false;
  bool powerSave = false;
  bool ledDisabled = false;
  String themeColor = "cyan";
  String themeMode = "dark";
  bool configured = false;
};

class ConfigManager {
public:
  static String sanitizeHostname(const String &input) {
    String clean = "";
    clean.reserve(input.length());
    for (size_t i = 0; i < input.length() && clean.length() < 32; ++i) {
      char c = input.charAt(i);
      if (isalnum((unsigned char)c) || c == '-') {
        clean += c;
      }
    }
    while (clean.startsWith("-")) {
      clean.remove(0, 1);
    }
    while (clean.endsWith("-")) {
      clean.remove(clean.length() - 1);
    }
    if (clean.isEmpty()) {
      clean = "CastPad";
    }
    return clean;
  }

  static bool savePowerSave(bool enable) {
    bool ok = false;
    Preferences prefs;
    if (prefs.begin("vkeyb", false)) {
      size_t w = prefs.putBool("pwr_save", enable);
      prefs.end();
      if (w > 0) ok = true;
    }
    // Isolated hardware namespace backup immune to partition churn
    Preferences sysPrefs;
    if (sysPrefs.begin("cp_hw", false)) {
      size_t w = sysPrefs.putBool("pwr_save", enable);
      sysPrefs.end();
      if (w > 0) ok = true;
    }
    Serial.printf("[Config] savePowerSave(%d) -> ok=%d\n", enable ? 1 : 0, ok ? 1 : 0);
    return ok;
  }

  static bool saveLedDisabled(bool disabled) {
    bool ok = false;
    Preferences prefs;
    if (prefs.begin("vkeyb", false)) {
      size_t w = prefs.putBool("led_dis", disabled);
      prefs.end();
      if (w > 0) ok = true;
    }
    // Backup in cp_hw namespace
    Preferences sysPrefs;
    if (sysPrefs.begin("cp_hw", false)) {
      size_t w = sysPrefs.putBool("led_dis", disabled);
      sysPrefs.end();
      if (w > 0) ok = true;
    }
    Serial.printf("[Config] saveLedDisabled(%d) -> ok=%d\n", disabled ? 1 : 0, ok ? 1 : 0);
    return ok;
  }

  static bool saveUsbMode(bool usb) {
    bool ok = false;
    Preferences prefs;
    if (prefs.begin("vkeyb", false)) {
      size_t w = prefs.putBool("usb_mode", usb);
      prefs.end();
      if (w > 0) ok = true;
    }
    Preferences sysPrefs;
    if (sysPrefs.begin("cp_hw", false)) {
      size_t w = sysPrefs.putBool("usb_mode", usb);
      sysPrefs.end();
      if (w > 0) ok = true;
    }
    Serial.printf("[Config] saveUsbMode(%d) -> ok=%d\n", usb ? 1 : 0, ok ? 1 : 0);
    return ok;
  }

  static bool load(AppConfig &config) {
    Preferences prefs;
    if (!prefs.begin("vkeyb", true)) {
      return false;
    }

    config.configured = prefs.getBool("cfg", false);
    config.powerSave = prefs.getBool("pwr_save", false);
    if (!config.powerSave) {
      Preferences sysPrefs;
      if (sysPrefs.begin("cp_hw", true)) {
        config.powerSave = sysPrefs.getBool("pwr_save", false);
        sysPrefs.end();
      }
    }
    Serial.printf("[Config] Loaded powerSave: %d\n", config.powerSave ? 1 : 0);

    config.ledDisabled = prefs.getBool("led_dis", false);
    if (!config.ledDisabled) {
      Preferences sysPrefs;
      if (sysPrefs.begin("cp_hw", true)) {
        config.ledDisabled = sysPrefs.getBool("led_dis", false);
        sysPrefs.end();
      }
    }
    Serial.printf("[Config] Loaded ledDisabled: %d\n", config.ledDisabled ? 1 : 0);

    config.usbMode = prefs.getBool("usb_mode", false);
    if (!config.usbMode) {
      Preferences sysPrefs;
      if (sysPrefs.begin("cp_hw", true)) {
        config.usbMode = sysPrefs.getBool("usb_mode", false);
        sysPrefs.end();
      }
    }
    Serial.printf("[Config] Loaded usbMode: %d\n", config.usbMode ? 1 : 0);
    config.ssid = prefs.getString("ssid", "");
    config.password = prefs.getString("pass", "");
    config.hostname = sanitizeHostname(prefs.getString("host", "CastPad"));
    if (config.hostname == "VirtualKeyboard") {
      config.hostname = "CastPad";
    }
    config.btnRed =
        prefs.isKey("b_red") ? prefs.getString("b_red", "F1") : String("F1");
    if (config.btnRed.isEmpty())
      config.btnRed = "F1";
    config.btnGreen = prefs.isKey("b_grn") ? prefs.getString("b_grn", "F2")
                                           : String("F2");
    if (config.btnGreen.isEmpty())
      config.btnGreen = "F2";
    config.btnYellow =
        prefs.isKey("b_yel") ? prefs.getString("b_yel", "F3") : String("F3");
    if (config.btnYellow.isEmpty())
      config.btnYellow = "F3";
    config.btnBlue =
        prefs.isKey("b_blu") ? prefs.getString("b_blu", "F4") : String("F4");
    if (config.btnBlue.isEmpty())
      config.btnBlue = "F4";
    config.btnCustom1 =
        prefs.isKey("b_c1") ? prefs.getString("b_c1", "") : String("");
    config.btnCustom2 =
        prefs.isKey("b_c2") ? prefs.getString("b_c2", "") : String("");
    config.btnCustom3 =
        prefs.isKey("b_c3") ? prefs.getString("b_c3", "") : String("");
    config.btnCustom4 =
        prefs.isKey("b_c4") ? prefs.getString("b_c4", "") : String("");

    config.lblRed =
        prefs.isKey("l_red") ? prefs.getString("l_red", "") : String("");
    config.lblGreen =
        prefs.isKey("l_grn") ? prefs.getString("l_grn", "") : String("");
    config.lblYellow =
        prefs.isKey("l_yel") ? prefs.getString("l_yel", "") : String("");
    config.lblBlue =
        prefs.isKey("l_blu") ? prefs.getString("l_blu", "") : String("");
    config.lblCustom1 =
        prefs.isKey("l_c1") ? prefs.getString("l_c1", "") : String("");
    config.lblCustom2 =
        prefs.isKey("l_c2") ? prefs.getString("l_c2", "") : String("");
    config.lblCustom3 =
        prefs.isKey("l_c3") ? prefs.getString("l_c3", "") : String("");
    config.lblCustom4 =
        prefs.isKey("l_c4") ? prefs.getString("l_c4", "") : String("");

    config.iconRed = prefs.isKey("i_red") ? prefs.getString("i_red", "") : String("");
    config.iconGreen = prefs.isKey("i_grn") ? prefs.getString("i_grn", "") : String("");
    config.iconYellow = prefs.isKey("i_yel") ? prefs.getString("i_yel", "") : String("");
    config.iconBlue = prefs.isKey("i_blu") ? prefs.getString("i_blu", "") : String("");
    config.iconCustom1 = prefs.isKey("i_c1") ? prefs.getString("i_c1", "") : String("");
    config.iconCustom2 = prefs.isKey("i_c2") ? prefs.getString("i_c2", "") : String("");
    config.iconCustom3 = prefs.isKey("i_c3") ? prefs.getString("i_c3", "") : String("");
    config.iconCustom4 = prefs.isKey("i_c4") ? prefs.getString("i_c4", "") : String("");

    config.colRed = prefs.isKey("c_red") ? prefs.getString("c_red", "") : String("");
    config.colGreen = prefs.isKey("c_grn") ? prefs.getString("c_grn", "") : String("");
    config.colYellow = prefs.isKey("c_yel") ? prefs.getString("c_yel", "") : String("");
    config.colBlue = prefs.isKey("c_blu") ? prefs.getString("c_blu", "") : String("");
    config.colCustom1 = prefs.isKey("c_c1") ? prefs.getString("c_c1", "") : String("");
    config.colCustom2 = prefs.isKey("c_c2") ? prefs.getString("c_c2", "") : String("");
    config.colCustom3 = prefs.isKey("c_c3") ? prefs.getString("c_c3", "") : String("");
    config.colCustom4 = prefs.isKey("c_c4") ? prefs.getString("c_c4", "") : String("");

    config.qlPages = prefs.getInt("ql_pages", 3);
    if (config.qlPages < 1) config.qlPages = 1;
    if (config.qlPages > 5) config.qlPages = 5;

    for (int i = 0; i < QUICK_LINKS_COUNT; ++i) {
      char keyL[12], keyN[12], keyC[12], keyI[12];
      snprintf(keyL, sizeof(keyL), "ql_l%d", i);
      snprintf(keyN, sizeof(keyN), "ql_n%d", i);
      snprintf(keyC, sizeof(keyC), "ql_c%d", i);
      snprintf(keyI, sizeof(keyI), "ql_i%d", i);
      config.qlLabel[i] =
          prefs.isKey(keyL) ? prefs.getString(keyL, "") : String("");
      config.qlNum[i] =
          prefs.isKey(keyN) ? prefs.getString(keyN, "") : String("");
      config.qlColor[i] =
          prefs.isKey(keyC) ? prefs.getString(keyC, "") : String("");
      config.qlIcon[i] =
          prefs.isKey(keyI) ? prefs.getString(keyI, "") : String("");
    }

    config.mouseSensitivity = prefs.getFloat("sens", 1.3f);
    config.bleRadioEnabled = prefs.getBool("ble_en", true);
    config.apMode = prefs.getBool("ap_mode", false);
    config.themeColor = prefs.isKey("theme") ? prefs.getString("theme", "cyan") : String("cyan");
    config.themeMode = prefs.isKey("th_mode") ? prefs.getString("th_mode", "dark") : String("dark");

    prefs.end();
    return config.configured;
  }

  static bool save(const AppConfig &config) {
    savePowerSave(config.powerSave);
    saveLedDisabled(config.ledDisabled);
    saveUsbMode(config.usbMode);

    Preferences prefs;
    if (!prefs.begin("vkeyb", false)) {
      return false;
    }

    prefs.putBool("cfg", config.configured);
    prefs.putBool("ap_mode", config.apMode);
    prefs.putBool("pwr_save", config.powerSave);
    prefs.putBool("led_dis", config.ledDisabled);
    prefs.putBool("usb_mode", config.usbMode);
    prefs.putBool("ble_en", config.bleRadioEnabled);
    prefs.putFloat("sens", config.mouseSensitivity);
    prefs.putString("theme", config.themeColor);
    prefs.putString("th_mode", config.themeMode);
    prefs.putString("ssid", config.ssid);
    prefs.putString("pass", config.password);
    prefs.putString("host", sanitizeHostname(config.hostname));
    prefs.putString("b_red", config.btnRed);
    prefs.putString("b_grn", config.btnGreen);
    prefs.putString("b_yel", config.btnYellow);
    prefs.putString("b_blu", config.btnBlue);
    prefs.putString("b_c1", config.btnCustom1);
    prefs.putString("b_c2", config.btnCustom2);
    prefs.putString("b_c3", config.btnCustom3);
    prefs.putString("b_c4", config.btnCustom4);

    prefs.putString("l_red", config.lblRed);
    prefs.putString("l_grn", config.lblGreen);
    prefs.putString("l_yel", config.lblYellow);
    prefs.putString("l_blu", config.lblBlue);
    prefs.putString("l_c1", config.lblCustom1);
    prefs.putString("l_c2", config.lblCustom2);
    prefs.putString("l_c3", config.lblCustom3);
    prefs.putString("l_c4", config.lblCustom4);

    auto saveOrRemove = [&](const char *key, const String &val) {
      if (val.isEmpty()) {
        if (prefs.isKey(key)) prefs.remove(key);
      } else {
        prefs.putString(key, val);
      }
    };

    saveOrRemove("i_red", config.iconRed);
    saveOrRemove("i_grn", config.iconGreen);
    saveOrRemove("i_yel", config.iconYellow);
    saveOrRemove("i_blu", config.iconBlue);
    saveOrRemove("i_c1", config.iconCustom1);
    saveOrRemove("i_c2", config.iconCustom2);
    saveOrRemove("i_c3", config.iconCustom3);
    saveOrRemove("i_c4", config.iconCustom4);

    saveOrRemove("c_red", config.colRed);
    saveOrRemove("c_grn", config.colGreen);
    saveOrRemove("c_yel", config.colYellow);
    saveOrRemove("c_blu", config.colBlue);
    saveOrRemove("c_c1", config.colCustom1);
    saveOrRemove("c_c2", config.colCustom2);
    saveOrRemove("c_c3", config.colCustom3);
    saveOrRemove("c_c4", config.colCustom4);

    prefs.putInt("ql_pages", config.qlPages);

    for (int i = 0; i < QUICK_LINKS_COUNT; ++i) {
      char keyL[12], keyN[12], keyC[12], keyI[12];
      snprintf(keyL, sizeof(keyL), "ql_l%d", i);
      snprintf(keyN, sizeof(keyN), "ql_n%d", i);
      snprintf(keyC, sizeof(keyC), "ql_c%d", i);
      snprintf(keyI, sizeof(keyI), "ql_i%d", i);
      saveOrRemove(keyL, config.qlLabel[i]);
      saveOrRemove(keyN, config.qlNum[i]);
      saveOrRemove(keyC, config.qlColor[i]);
      saveOrRemove(keyI, config.qlIcon[i]);
    }

    prefs.end();
    return true;
  }

  static void clear() {
    Preferences prefs;
    if (prefs.begin("vkeyb", false)) {
      prefs.clear();
      prefs.end();
    }
  }
};
