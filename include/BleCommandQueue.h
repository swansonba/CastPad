#pragma once

#include <Arduino.h>
#include <new>

#include "BLEManager.h"

// Forward declaration of LED blink trigger for transmitted BLE commands
void triggerCommandBlink();

// All BLE HID work (key taps, typing, clicks, re-pairing) involves blocking
// delays. Running it inside AsyncWebServer / AsyncTCP callbacks stalls the web
// server and can trip the async_tcp task watchdog when typing long strings.
// Instead, WebSocket handlers post commands here and a dedicated FreeRTOS task
// executes them sequentially (which also keeps key ordering intact).

enum class BleCmdType : uint8_t {
  Key,          // text = action name (UP, HOME, F1, C:0x..., ...)
  TypeText,     // text = string to type (followed by Enter)
  TypeTextRaw,  // text = string to type without Enter
  MouseMove,    // a = dx, b = dy
  MouseScroll,  // a = amount
  MouseClick,   // a = button mask
  MousePress,   // a = button mask
  MouseRelease, // a = button mask
  Disconnect,   // drop current host, keep bonds
  ForgetBonds,  // drop current host and erase all bonds
  SetRadio,     // a = 1 (on) or 0 (off)
  Sequence      // text = sequence macro string (e.g. K:HOME|W:500|K:OK)
};

struct BleCmd {
  BleCmdType type;
  int32_t a;
  int32_t b;
  String *text; // heap-owned; only used by Key / TypeText, freed by the worker
};

class BleCommandQueue {
public:
  // Must be called once after BLEManager::begin().
  static bool begin(size_t depth = 64) {
    if (queue() != nullptr)
      return true;
    queue() = xQueueCreate(depth, sizeof(BleCmd));
    if (queue() == nullptr) {
      Serial.println("[BLEQ] Failed to create command queue");
      return false;
    }
    // Core 1 alongside loop(); priority 2 so it runs ahead of loop() but well
    // below the NimBLE host and AsyncTCP tasks. delay() inside BLEManager
    // yields, so it never starves anything.
    BaseType_t ok = xTaskCreatePinnedToCore(taskMain, "ble_cmd", 6144, nullptr,
                                            2, nullptr, 1);
    if (ok != pdPASS) {
      Serial.println("[BLEQ] Failed to start command task");
      return false;
    }
    return true;
  }

  static bool post(BleCmdType type, int32_t a = 0, int32_t b = 0) {
    if (queue() == nullptr)
      return false;
    BleCmd cmd{type, a, b, nullptr};
    if (xQueueSend(queue(), &cmd, 0) != pdTRUE) {
      logDrop(type);
      return false;
    }
    return true;
  }

  static bool postText(BleCmdType type, const String &text) {
    if (queue() == nullptr || text.isEmpty())
      return false;
    String *copy = new (std::nothrow) String(text);
    if (copy == nullptr)
      return false;
    BleCmd cmd{type, 0, 0, copy};
    if (xQueueSend(queue(), &cmd, 0) != pdTRUE) {
      delete copy;
      logDrop(type);
      return false;
    }
    return true;
  }

private:
  static QueueHandle_t &queue() {
    static QueueHandle_t q = nullptr;
    return q;
  }

  static void logDrop(BleCmdType type) {
    // Mouse moves are high-rate; dropping one under backpressure is harmless.
    if (type != BleCmdType::MouseMove && type != BleCmdType::MouseScroll) {
      Serial.printf("[BLEQ] Queue full, dropped command %u\n",
                    static_cast<unsigned>(type));
    }
  }

  static void executeSequence(const String &seq) {
    int start = 0;
    int len = seq.length();
    BLEManager &ble = BLEManager::instance();
    while (start < len) {
      int nextDelim = seq.indexOf('|', start);
      if (nextDelim < 0)
        nextDelim = len;
      String step = seq.substring(start, nextDelim);
      step.trim();
      if (!step.isEmpty()) {
        if (step.startsWith("W:") || step.startsWith("w:")) {
          int ms = step.substring(2).toInt();
          if (ms > 0) {
            if (ms > 10000)
              ms = 10000;
            vTaskDelay(pdMS_TO_TICKS(ms));
          }
        } else if (step.startsWith("K:") || step.startsWith("k:")) {
          ble.sendKey(step.substring(2));
          vTaskDelay(pdMS_TO_TICKS(40));
        } else if (step.startsWith("T:") || step.startsWith("t:")) {
          ble.typeText(step.substring(2), false);
          vTaskDelay(pdMS_TO_TICKS(40));
        } else if (step.startsWith("N:") || step.startsWith("n:")) {
          ble.typeText(step.substring(2), true);
          vTaskDelay(pdMS_TO_TICKS(40));
        } else {
          ble.sendKey(step);
          vTaskDelay(pdMS_TO_TICKS(40));
        }
      }
      start = nextDelim + 1;
    }
  }

  static void taskMain(void *) {
    BleCmd cmd;
    for (;;) {
      if (xQueueReceive(queue(), &cmd, portMAX_DELAY) != pdTRUE)
        continue;

      BLEManager &ble = BLEManager::instance();
      switch (cmd.type) {
      case BleCmdType::Key:
        if (cmd.text)
          ble.sendKey(*cmd.text);
        if (ble.isConnected())
          triggerCommandBlink();
        break;
      case BleCmdType::TypeText:
        if (cmd.text)
          ble.typeText(*cmd.text, true);
        if (ble.isConnected())
          triggerCommandBlink();
        break;
      case BleCmdType::TypeTextRaw:
        if (cmd.text)
          ble.typeText(*cmd.text, false);
        if (ble.isConnected())
          triggerCommandBlink();
        break;
      case BleCmdType::MouseMove: {
        int32_t totalDx = cmd.a;
        int32_t totalDy = cmd.b;
        BleCmd nextCmd;
        // Coalesce consecutive pending MouseMove events up to HID int8_t bounds
        while (xQueuePeek(queue(), &nextCmd, 0) == pdTRUE &&
               nextCmd.type == BleCmdType::MouseMove &&
               abs(totalDx + nextCmd.a) <= 127 &&
               abs(totalDy + nextCmd.b) <= 127) {
          if (xQueueReceive(queue(), &nextCmd, 0) == pdTRUE) {
            totalDx += nextCmd.a;
            totalDy += nextCmd.b;
          } else {
            break;
          }
        }
        ble.mouseMove(totalDx, totalDy);
        break;
      }
      case BleCmdType::MouseScroll: {
        int32_t totalScroll = cmd.a;
        BleCmd nextCmd;
        // Coalesce consecutive pending MouseScroll events up to HID int8_t bounds
        while (xQueuePeek(queue(), &nextCmd, 0) == pdTRUE &&
               nextCmd.type == BleCmdType::MouseScroll &&
               abs(totalScroll + nextCmd.a) <= 127) {
          if (xQueueReceive(queue(), &nextCmd, 0) == pdTRUE) {
            totalScroll += nextCmd.a;
          } else {
            break;
          }
        }
        ble.mouseScroll(totalScroll);
        break;
      }
      case BleCmdType::MouseClick:
        ble.mouseClick(static_cast<uint8_t>(cmd.a));
        if (ble.isConnected())
          triggerCommandBlink();
        break;
      case BleCmdType::MousePress:
        ble.mousePress(static_cast<uint8_t>(cmd.a));
        break;
      case BleCmdType::MouseRelease:
        ble.mouseRelease(static_cast<uint8_t>(cmd.a));
        break;
      case BleCmdType::Disconnect:
        ble.disconnectAndRepair(false);
        break;
      case BleCmdType::ForgetBonds:
        ble.disconnectAndRepair(true);
        break;
      case BleCmdType::SetRadio:
        ble.setRadioEnabled(cmd.a != 0);
        break;
      case BleCmdType::Sequence:
        if (cmd.text)
          executeSequence(*cmd.text);
        if (ble.isConnected())
          triggerCommandBlink();
        break;
      }

      delete cmd.text;
    }
  }
};
