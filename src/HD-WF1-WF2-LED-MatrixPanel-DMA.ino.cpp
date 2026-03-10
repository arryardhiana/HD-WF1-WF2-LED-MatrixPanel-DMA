// Parking gate LED matrix display firmware for WF1/WF2 HUB75 boards.
// Renders access status, ANPR result, plate number, and greeting
// on 4 chained 64x32 panels via MQTT JSON events.

#if defined(WF1)
  #include "hd-wf1-esp32s2-config.h"
#elif defined(WF2)
  #include "hd-wf2-esp32s3-config.h"
#else
  #error "Please define either WF1 or WF2"
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <ArduinoOTA.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

// WiFi credentials
static const char* WIFI_SSID = "IoT";
static const char* WIFI_PASS = "iotUnp4d";

// MQTT credentials
static const char* MQTT_HOST = "10.10.6.99";
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "app";
static const char* MQTT_PASS = "pB326ddRw46BjTjqRvSCWcnZZSO7i8gZ";

// OTA
static const char* OTA_PASSWORD = "ota-parking";

// Gate identity — configurable per device
static const char* GATE_ID   = "gate-01";
static const char* GATE_NAME = "Gate A";

// HUB75 panel setup
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 4
#define VIRTUAL_ROWS 4
#define VIRTUAL_COLS 1

constexpr bool CALIBRATION_MODE = false;
constexpr size_t LOCKED_DISPLAY_CONFIG_INDEX = 4; // CFG 5 selected during calibration.
constexpr uint8_t DISPLAY_ROTATION = 0;

struct DisplayConfig {
  PANEL_CHAIN_TYPE chainType;
  bool electricalFourScan;
  bool flipBottomPanels;
  const char* name;
};

static const DisplayConfig kDisplayConfigs[] = {
  {CHAIN_TOP_LEFT_DOWN, true,  true,  "TLD 4S FLIP"},
  {CHAIN_TOP_RIGHT_DOWN, true, true,  "TRD 4S FLIP"},
  {CHAIN_BOTTOM_LEFT_UP, true, true,  "BLU 4S FLIP"},
  {CHAIN_BOTTOM_RIGHT_UP, true, true, "BRU 4S FLIP"},
  {CHAIN_TOP_LEFT_DOWN_ZZ, true, true,  "TLDZZ 4S F"},
  {CHAIN_TOP_RIGHT_DOWN_ZZ, true, true, "TRDZZ 4S F"},
  {CHAIN_TOP_LEFT_DOWN, false, false, "TLD STD"},
  {CHAIN_TOP_RIGHT_DOWN, false, false,"TRD STD"},
};

constexpr uint8_t DISPLAY_PIXEL_BASE = 64;

// ---------------------------------------------------------------------------
// Gate state — populated from MQTT events
// ---------------------------------------------------------------------------

struct GateState {
  String status     = "idle";        // "granted" | "denied" | "scanning" | "idle"
  String plate      = "";            // nomor plat terbaca, e.g. "D 1234 ABC"
  String anprStatus = "";            // "detected" | "not_detected" | "low_confidence"
  String gateName   = GATE_NAME;     // nama gate (dapat dioverride dari topic config)
  String greeting       = "Selamat Datang";
  String greetingColor  = "white";           // "white" | "yellow" | "cyan"
  bool   greetingScroll = false;
};

static GateState gGate;

// ---------------------------------------------------------------------------
// Persisted config — loaded from NVS on boot, saved when changed via MQTT
// ---------------------------------------------------------------------------

static const char* NVS_NAMESPACE   = "gate-cfg";
static uint8_t     gBrightness     = 96; // 0-255, default matches setupDisplayForConfig
static uint8_t     gDisplayTimeout = 5;  // seconds before returning to idle (Phase 4)

// ---------------------------------------------------------------------------
// MQTT topic helpers — built from GATE_ID at runtime
// ---------------------------------------------------------------------------

static String topicEvent()    { return String("parking/") + GATE_ID + "/event"; }
static String topicGreeting() { return String("parking/") + GATE_ID + "/greeting"; }
static String topicConfig()   { return String("parking/") + GATE_ID + "/config"; }
static String topicReset()    { return String("parking/") + GATE_ID + "/reset"; }

// ---------------------------------------------------------------------------
// OTA setup
// ---------------------------------------------------------------------------

void setupOTA() {
  const String hostname = String("parking-") + GATE_ID;
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    const String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("[ota] start: " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[ota] end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[ota] progress: %u%%\r", progress * 100 / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ota] error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("auth failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("begin failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("connect failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("receive failed");
    else if (error == OTA_END_ERROR)     Serial.println("end failed");
  });

  ArduinoOTA.begin();
  Serial.printf("[ota] ready — hostname: %s\n", hostname.c_str());
}

// ---------------------------------------------------------------------------
// NTP / clock
// ---------------------------------------------------------------------------

static String        gClockStr          = "--:--:--";
static unsigned long gLastClockUpdateMs = 0;

static int16_t       gGreetingScrollX = PANEL_RES_X; // starts at right edge of P4
static unsigned long gLastScrollMs    = 0;

static unsigned long gLastEventMs = 0;          // millis() of last non-idle event (for display timeout)
static bool          gMqttWasConnected   = false; // tracks prior MQTT state for offline indicator
static unsigned long gMqttDisconnectedMs = 0;     // millis() when MQTT first lost connection
static bool          gOtaInitialized     = false; // ArduinoOTA.begin() called once after WiFi

// ---------------------------------------------------------------------------
// Hardware pin configuration
// ---------------------------------------------------------------------------

#if defined(WF1)
HUB75_I2S_CFG::i2s_pins _pins_x1 = {
  WF1_R1_PIN, WF1_G1_PIN, WF1_B1_PIN,
  WF1_R2_PIN, WF1_G2_PIN, WF1_B2_PIN,
  WF1_A_PIN, WF1_B_PIN, WF1_C_PIN, WF1_D_PIN, WF1_E_PIN,
  WF1_LAT_PIN, WF1_OE_PIN, WF1_CLK_PIN
};
#else
HUB75_I2S_CFG::i2s_pins _pins_x1 = {
  WF2_X1_R1_PIN, WF2_X1_G1_PIN, WF2_X1_B1_PIN,
  WF2_X1_R2_PIN, WF2_X1_G2_PIN, WF2_X1_B2_PIN,
  WF2_A_PIN, WF2_B_PIN, WF2_C_PIN, WF2_D_PIN, WF2_X1_E_PIN,
  WF2_LAT_PIN, WF2_OE_PIN, WF2_CLK_PIN
};
#endif

MatrixPanel_I2S_DMA* dma_display = nullptr;
VirtualMatrixPanel* virtual_display = nullptr;
size_t activeDisplayConfigIndex = 0;
bool gFlipBottomPanels = true;
unsigned long lastCalibSwitchMs = 0;

// ---------------------------------------------------------------------------
// Virtual panel with coordinate transform for serpentine (ZZ) layout.
// P1 (index 0) and P3 (index 2) are rotated 180° to match physical install.
// ---------------------------------------------------------------------------

class MqttDisplayVirtualPanel : public VirtualMatrixPanel {
public:
  using VirtualMatrixPanel::VirtualMatrixPanel;

  VirtualCoords getCoords(int16_t x, int16_t y) override {
    if (PANEL_CHAIN >= 4 && VIRTUAL_ROWS == 4 && VIRTUAL_COLS == 1) {
      const int16_t panelCol = x / PANEL_RES_X;
      const int16_t panelRow = y / PANEL_RES_Y;
      const int16_t panelIndex = panelRow;
      if (panelIndex == 0 || panelIndex == 2) { // P1 and P3 (rotate 180)
        const int16_t localX = x % PANEL_RES_X;
        const int16_t localY = y % PANEL_RES_Y;
        x = panelCol * PANEL_RES_X + (PANEL_RES_X - 1 - localX);
        y = panelRow * PANEL_RES_Y + (PANEL_RES_Y - 1 - localY);
      }
    }

    // Bottom-panel flip calibration only applies to the 2x2 arrangement.
    if (gFlipBottomPanels && PANEL_CHAIN >= 4 && VIRTUAL_ROWS == 2 && VIRTUAL_COLS == 2) {
      const int16_t panelCol = x / PANEL_RES_X;
      const int16_t panelRow = y / PANEL_RES_Y;
      const int16_t panelIndex = panelRow * VIRTUAL_COLS + panelCol;
      if (panelIndex == 2 || panelIndex == 3) {
        const int16_t localX = x % PANEL_RES_X;
        const int16_t localY = y % PANEL_RES_Y;
        x = panelCol * PANEL_RES_X + (PANEL_RES_X - 1 - localX);
        y = panelRow * PANEL_RES_Y + (PANEL_RES_Y - 1 - localY);
      }
    }
    return VirtualMatrixPanel::getCoords(x, y);
  }
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
bool redrawNeeded = true;
bool wifiConnectedShown = false;
bool mqttConnectedShown = false;

// ---------------------------------------------------------------------------
// NVS config load / save
// ---------------------------------------------------------------------------

void loadConfig() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true); // read-only
  gGate.gateName  = prefs.getString("gate_name",   GATE_NAME);
  gBrightness     = prefs.getUChar("brightness",   96);
  gDisplayTimeout = prefs.getUChar("disp_timeout", 5);
  prefs.end();
  Serial.printf("[config] loaded: gate=%s bright=%u timeout=%u\n",
                gGate.gateName.c_str(), gBrightness, gDisplayTimeout);
}

void saveConfig() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false); // read-write
  prefs.putString("gate_name",   gGate.gateName);
  prefs.putUChar("brightness",   gBrightness);
  prefs.putUChar("disp_timeout", gDisplayTimeout);
  prefs.end();
  Serial.println("[config] saved to NVS");
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void targetFillScreen(uint16_t color) {
  if (virtual_display) virtual_display->fillScreen(color);
  else dma_display->fillScreen(color);
}

void targetSetTextColor(uint16_t color) {
  if (virtual_display) virtual_display->setTextColor(color);
  else dma_display->setTextColor(color);
}

void targetSetCursor(int16_t x, int16_t y) {
  if (virtual_display) virtual_display->setCursor(x, y);
  else dma_display->setCursor(x, y);
}

void targetPrint(const String& text) {
  if (virtual_display) virtual_display->print(text);
  else dma_display->print(text);
}

void targetDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (virtual_display) virtual_display->drawRect(x, y, w, h, color);
  else dma_display->drawRect(x, y, w, h, color);
}

void targetSetTextDefaults() {
  if (virtual_display) {
    virtual_display->setFont(nullptr);
    virtual_display->setTextSize(1);
    virtual_display->setTextWrap(false);
  } else {
    dma_display->setFont(nullptr);
    dma_display->setTextSize(1);
    dma_display->setTextWrap(false);
  }
}

void targetSetTextSize(uint8_t size) {
  if (virtual_display) virtual_display->setTextSize(size);
  else dma_display->setTextSize(size);
}

void drawCenteredText(const String& text, int16_t x, int16_t y, int16_t w, int16_t h) {
  const int textWidth = static_cast<int>(text.length()) * 6;
  const int textX = x + (w - textWidth) / 2;
  const int textY = y + (h - 8) / 2;
  targetSetCursor(textX, textY);
  targetPrint(text);
}

// Centered text for textSize(2): 12px per char width, ~16px height.
void drawCenteredText2(const String& text, int16_t x, int16_t y, int16_t w) {
  const int textWidth = static_cast<int>(text.length()) * 12;
  const int textX = x + max(0, (w - textWidth) / 2);
  targetSetCursor(textX, y);
  targetPrint(text);
}

String sanitizePayload(const String& raw) {
  String out = raw;
  out.trim();
  if (out.startsWith("\"") && out.endsWith("\"") && out.length() >= 2) {
    out = out.substring(1, out.length() - 1);
    out.trim();
  }
  return out;
}

bool tryParseNumber(const String& text, float& valueOut) {
  char* endPtr = nullptr;
  const char* c = text.c_str();
  valueOut = strtof(c, &endPtr);
  return (endPtr != c) && (*endPtr == '\0');
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// Returns color565 for access status. Returns 0 (black) for "idle" or unknown.
uint16_t getStatusColor(const String& status) {
  if (!dma_display) return 0;
  if (status == "granted")  return dma_display->color565(0,   220, 0);
  if (status == "denied")   return dma_display->color565(220, 0,   0);
  if (status == "scanning") return dma_display->color565(220, 180, 0);
  return 0; // idle or unknown — black
}

// Returns color565 for greeting color name. Defaults to white.
uint16_t greetingColorToColor565(const String& colorName) {
  if (!dma_display) return 0;
  if (colorName == "yellow") return dma_display->color565(255, 220, 0);
  if (colorName == "cyan")   return dma_display->color565(0, 220, 220);
  return dma_display->color565(255, 255, 255); // white (default)
}

void renderStatus(const char* line1, const char* line2, uint16_t color) {
  if (!dma_display) return;
  const int totalWidth = PANEL_RES_X * VIRTUAL_COLS;
  const int totalHeight = PANEL_RES_Y * VIRTUAL_ROWS;
  targetFillScreen(0);
  targetSetTextDefaults();
  targetSetTextColor(color);
  drawCenteredText(line1, 0, (totalHeight / 2) - 10, totalWidth, 10);
  drawCenteredText(line2, 0, (totalHeight / 2) + 2, totalWidth, 10);
}

void renderPanels() {
  if (!dma_display) return;

  const uint16_t labelColor = dma_display->color565(0, 180, 255);
  const uint16_t valueColor = dma_display->color565(0, 255, 80);

  targetFillScreen(0);

  // P1 (row 0): traffic lamp — 3 circles horizontal (red | yellow | green)
  {
    const uint16_t dimColor    = dma_display->color565(25,  25,  25);
    const uint16_t redColor    = dma_display->color565(220,  0,   0);
    const uint16_t yellowColor = dma_display->color565(220, 180,  0);
    const uint16_t greenColor  = dma_display->color565(0,   220,  0);

    const int cy = PANEL_RES_Y / 2;   // 16 — vertical center of P1
    const int r  = 9;
    // Three circles evenly spaced in 64px: centers at x=11, 32, 53
    const int cxRed    = 11;
    const int cxYellow = 32;
    const int cxGreen  = 53;

    const uint16_t colRed    = (gGate.status == "denied")   ? redColor    : dimColor;
    const uint16_t colYellow = (gGate.status == "scanning") ? yellowColor : dimColor;
    const uint16_t colGreen  = (gGate.status == "granted")  ? greenColor  : dimColor;

    if (virtual_display) {
      virtual_display->fillCircle(cxRed,    cy, r, colRed);
      virtual_display->fillCircle(cxYellow, cy, r, colYellow);
      virtual_display->fillCircle(cxGreen,  cy, r, colGreen);
    } else {
      dma_display->fillCircle(cxRed,    cy, r, colRed);
      dma_display->fillCircle(cxYellow, cy, r, colYellow);
      dma_display->fillCircle(cxGreen,  cy, r, colGreen);
    }
    // idle: all 3 circles dim — already handled by dimColor above
  }

  // P2 (row 1): gate name / clock / ANPR status
  targetSetTextDefaults();
  targetSetTextColor(labelColor);
  drawCenteredText(gGate.gateName, 0, PANEL_RES_Y + 1, PANEL_RES_X, 8);
  targetSetTextColor(valueColor);
  drawCenteredText(gClockStr, 0, PANEL_RES_Y + 12, PANEL_RES_X, 8);
  {
    const bool mqttOffline = gMqttWasConnected && gMqttDisconnectedMs > 0 &&
                             (millis() - gMqttDisconnectedMs >= 5000UL);
    if (mqttOffline) {
      targetSetTextColor(dma_display->color565(180, 0, 0)); // dim red
      drawCenteredText("OFFLINE", 0, PANEL_RES_Y + 23, PANEL_RES_X, 8);
    } else {
      targetSetTextColor(valueColor);
      drawCenteredText(gGate.anprStatus.length() ? gGate.anprStatus : "--",
                       0, PANEL_RES_Y + 23, PANEL_RES_X, 8);
    }
  }

  // P3 (row 2): plate number — textSize(2) if lines fit, else textSize(1) single line
  {
    const String plate = gGate.plate.length() ? gGate.plate : "------";
    const int firstSpace  = plate.indexOf(' ');
    const int secondSpace = (firstSpace >= 0) ? plate.indexOf(' ', firstSpace + 1) : -1;
    const String line1 = (secondSpace > 0) ? plate.substring(0, secondSpace) : plate;
    const String line2 = (secondSpace > 0) ? plate.substring(secondSpace + 1) : String();

    targetSetTextDefaults();
    targetSetTextColor(dma_display->color565(255, 255, 255));

    const int p3MidY = PANEL_RES_Y * 2 + 12;  // center line for single-line fallback
    const int p3TopY = PANEL_RES_Y * 2 + 2;
    const int p3BotY = PANEL_RES_Y * 2 + 18;

    const int maxLineLen = max((int)line1.length(), (int)line2.length());
    if (maxLineLen * 12 <= PANEL_RES_X) {
      // Both lines fit at textSize(2)
      targetSetTextSize(2);
      drawCenteredText2(line1, 0, p3TopY, PANEL_RES_X);
      if (line2.length() > 0) drawCenteredText2(line2, 0, p3BotY, PANEL_RES_X);
    } else {
      // Falls back to textSize(1) — full plate on one centered line
      targetSetTextSize(1);
      drawCenteredText(plate, 0, p3MidY, PANEL_RES_X, 8);
    }
    targetSetTextSize(1);
  }

  // P4 (row 3): denied override OR scanning (blank) OR greeting — color + optional scroll
  {
    targetSetTextDefaults();
    if (gGate.status == "denied") {
      // 3 lines centered in P4 (each word fits: 7×6=42px, 6×6=36px, 7×6=42px < 64px)
      targetSetTextColor(dma_display->color565(255, 80, 0)); // orange-red
      drawCenteredText("Gunakan", 0, PANEL_RES_Y * 3 + 3,  PANEL_RES_X, 8);
      drawCenteredText("QRCODE",  0, PANEL_RES_Y * 3 + 13, PANEL_RES_X, 8);
      drawCenteredText("SAUNPAD", 0, PANEL_RES_Y * 3 + 23, PANEL_RES_X, 8);
    } else if (gGate.status == "scanning") {
      // P4 cleared during scanning — area stays black from targetFillScreen(0) above
    } else {
      targetSetTextColor(greetingColorToColor565(gGate.greetingColor));
      if (gGate.greetingScroll) {
        targetSetCursor(gGreetingScrollX, PANEL_RES_Y * 3 + 12);
        targetPrint(gGate.greeting);
      } else {
        const String& text = gGate.greeting;
        const int textW = static_cast<int>(text.length()) * 6;
        if (textW <= PANEL_RES_X) {
          // Fits on one line — center it
          const int greetX = (PANEL_RES_X - textW) / 2;
          targetSetCursor(greetX, PANEL_RES_Y * 3 + 12);
          targetPrint(text);
        } else {
          // Split at last space at or before midpoint → 2 centered lines
          int splitIdx = -1;
          const int mid = text.length() / 2;
          for (int i = mid; i >= 0; i--) {
            if (text[i] == ' ') { splitIdx = i; break; }
          }
          if (splitIdx < 0) {
            // No space — left-align single line (clipped rather than off-screen)
            targetSetCursor(0, PANEL_RES_Y * 3 + 12);
            targetPrint(text);
          } else {
            const String line1 = text.substring(0, splitIdx);
            const String line2 = text.substring(splitIdx + 1);
            const int x1 = max(0, (PANEL_RES_X - (int)line1.length() * 6) / 2);
            const int x2 = max(0, (PANEL_RES_X - (int)line2.length() * 6) / 2);
            targetSetCursor(x1, PANEL_RES_Y * 3 + 6);
            targetPrint(line1);
            targetSetCursor(x2, PANEL_RES_Y * 3 + 20);
            targetPrint(line2);
          }
        }
      }
    }
  }
}

void renderCalibrationPattern() {
  if (!dma_display) return;
  const DisplayConfig& cfg = kDisplayConfigs[activeDisplayConfigIndex];
  const uint16_t borderColor = dma_display->color565(255, 255, 255);
  const uint16_t panelColors[4] = {
    dma_display->color565(220, 30, 30),
    dma_display->color565(30, 220, 30),
    dma_display->color565(30, 120, 255),
    dma_display->color565(240, 170, 20)
  };

  targetFillScreen(0);
  targetSetTextDefaults();

  for (int i = 0; i < 4; i++) {
    const int row = i / VIRTUAL_COLS;
    const int col = i % VIRTUAL_COLS;
    const int panelX = col * PANEL_RES_X;
    const int panelY = row * PANEL_RES_Y;
    if (virtual_display) virtual_display->fillRect(panelX + 1, panelY + 1, PANEL_RES_X - 2, PANEL_RES_Y - 2, panelColors[i]);
    else dma_display->fillRect(panelX + 1, panelY + 1, PANEL_RES_X - 2, PANEL_RES_Y - 2, panelColors[i]);
    targetDrawRect(panelX, panelY, PANEL_RES_X, PANEL_RES_Y, borderColor);
    targetSetTextColor(0);
    drawCenteredText(String("P") + String(i + 1), panelX, panelY + 10, PANEL_RES_X, 12);
  }

  targetSetTextColor(borderColor);
  drawCenteredText(String("CFG ") + String(activeDisplayConfigIndex + 1), 0, 0, PANEL_RES_X * VIRTUAL_COLS, 8);
  drawCenteredText(cfg.name, 0, (PANEL_RES_Y * VIRTUAL_ROWS) - 10, PANEL_RES_X * VIRTUAL_COLS, 8);
}

// ---------------------------------------------------------------------------
// MQTT topic handlers
// JSON parsing will be implemented in plan 01-02 (requires ArduinoJson).
// For now, handlers accept raw payload and populate GateState with placeholders.
// ---------------------------------------------------------------------------

void handleEventTopic(const String& payload) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[event] JSON parse error: %s | payload: %s\n",
                  err.c_str(), payload.c_str());
    gGate.status = "idle";
    redrawNeeded = true;
    return;
  }

  // Ekstrak fields — gunakan default jika field tidak ada
  const char* status = doc["status"]      | "idle";
  const char* plate  = doc["plate"]       | "";
  const char* anpr   = doc["anpr_status"] | "";
  const char* gate   = doc["gate_name"]   | gGate.gateName.c_str();

  // Validasi nilai status enum
  const String s(status);
  if (s == "granted" || s == "denied" || s == "scanning" || s == "idle") {
    gGate.status = s;
  } else {
    gGate.status = "idle";
    Serial.printf("[event] unknown status value: %s\n", status);
  }

  // Track timestamp for display timeout (only non-idle events reset the timer)
  if (gGate.status != "idle") {
    gLastEventMs = millis();
  }

  gGate.plate      = String(plate);
  gGate.anprStatus = String(anpr);
  if (strlen(gate) > 0) gGate.gateName = String(gate);

  redrawNeeded = true;
  Serial.printf("[event] status=%s plate=%s anpr=%s gate=%s\n",
                gGate.status.c_str(), gGate.plate.c_str(),
                gGate.anprStatus.c_str(), gGate.gateName.c_str());
}

void handleGreetingTopic(const String& payload) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[greeting] JSON parse error: %s | payload: %s\n",
                  err.c_str(), payload.c_str());
    return;
  }

  bool changed = false;

  // Update text if present and non-empty
  const char* text = doc["text"] | "";
  if (strlen(text) > 0 && gGate.greeting != text) {
    gGate.greeting = String(text);
    gGreetingScrollX = PANEL_RES_X; // reset scroll on new text
    changed = true;
    Serial.printf("[greeting] text=%s\n", gGate.greeting.c_str());
  }

  // Update color if present and recognized
  if (doc["color"].is<const char*>()) {
    const String c = doc["color"].as<String>();
    if ((c == "white" || c == "yellow" || c == "cyan") && gGate.greetingColor != c) {
      gGate.greetingColor = c;
      changed = true;
      Serial.printf("[greeting] color=%s\n", c.c_str());
    }
  }

  // Update scroll if present
  if (doc["scroll"].is<bool>()) {
    const bool scroll = doc["scroll"].as<bool>();
    if (scroll != gGate.greetingScroll) {
      gGate.greetingScroll = scroll;
      gGreetingScrollX = PANEL_RES_X; // reset position when toggled
      changed = true;
      Serial.printf("[greeting] scroll=%s\n", scroll ? "true" : "false");
    }
  }

  if (changed) redrawNeeded = true;
  else Serial.println("[greeting] no changes detected");
}

void handleConfigTopic(const String& payload) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[config] JSON parse error: %s | payload: %s\n",
                  err.c_str(), payload.c_str());
    return;
  }

  bool changed = false;

  // Update hanya field yang ada dalam payload (partial update supported)
  if (doc["gate_name"].is<const char*>()) {
    const String name = doc["gate_name"].as<String>();
    if (name.length() > 0) {
      gGate.gateName = name;
      changed = true;
    }
  }

  if (doc["brightness"].is<int>()) {
    gBrightness = static_cast<uint8_t>(doc["brightness"].as<int>());
    if (dma_display) dma_display->setBrightness8(gBrightness);
    changed = true;
  }

  if (doc["display_timeout"].is<int>()) {
    gDisplayTimeout = static_cast<uint8_t>(doc["display_timeout"].as<int>());
    changed = true;
  }

  if (changed) {
    saveConfig();
    redrawNeeded = true;
    Serial.printf("[config] updated: gate=%s bright=%u timeout=%u\n",
                  gGate.gateName.c_str(), gBrightness, gDisplayTimeout);
  } else {
    Serial.println("[config] no recognized fields in payload — no change");
  }
}

void handleResetTopic(const String& /*payload*/) {
  gGate.status     = "idle";
  gGate.plate      = "";
  gGate.anprStatus = "";
  redrawNeeded     = true;
  Serial.println("[reset] display reset to idle");
}

// ---------------------------------------------------------------------------
// MQTT callback — routes by topic to handler functions
// ---------------------------------------------------------------------------

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    value += static_cast<char>(payload[i]);
  }
  value.trim();

  const String t(topic);
  if      (t == topicEvent())    handleEventTopic(value);
  else if (t == topicGreeting()) handleGreetingTopic(value);
  else if (t == topicConfig())   handleConfigTopic(value);
  else if (t == topicReset())    handleResetTopic(value);
  else Serial.printf("[mqtt] unknown topic: %s\n", topic);
}

// ---------------------------------------------------------------------------
// Connectivity
// ---------------------------------------------------------------------------

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectedShown = true;
    if (!gOtaInitialized) {
      setupOTA();
      gOtaInitialized = true;
    }
    return;
  }

  const unsigned long now = millis();
  if (now - lastWifiAttemptMs < 3000) return;
  lastWifiAttemptMs = now;

  Serial.printf("Connecting WiFi to %s ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiConnectedShown = false;
  mqttConnectedShown = false;
}

void connectMqttIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (mqttClient.connected()) {
    mqttConnectedShown = true;
    return;
  }

  const unsigned long now = millis();
  if (now - lastMqttAttemptMs < 3000) return;
  lastMqttAttemptMs = now;

  const String clientId = String("wf2-matrix-") + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  Serial.printf("Connecting MQTT to %s:%u ...\n", MQTT_HOST, MQTT_PORT);

  if (!mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.printf("MQTT connect failed, state=%d\n", mqttClient.state());
    return;
  }

  Serial.println("MQTT connected");
  mqttClient.subscribe(topicEvent().c_str());
  mqttClient.subscribe(topicGreeting().c_str());
  mqttClient.subscribe(topicConfig().c_str());
  mqttClient.subscribe(topicReset().c_str());
  Serial.printf("Subscribed to parking/%s/* topics\n", GATE_ID);
  mqttConnectedShown = false;
}

// ---------------------------------------------------------------------------
// Display initialisation
// ---------------------------------------------------------------------------

void setupDisplayForConfig(size_t configIndex) {
  if (configIndex >= (sizeof(kDisplayConfigs) / sizeof(kDisplayConfigs[0]))) configIndex = 0;
  activeDisplayConfigIndex = configIndex;
  const DisplayConfig& cfg = kDisplayConfigs[activeDisplayConfigIndex];

  if (virtual_display) {
    delete virtual_display;
    virtual_display = nullptr;
  }
  if (dma_display) {
    delete dma_display;
    dma_display = nullptr;
  }

  const int mxWidth = cfg.electricalFourScan ? (PANEL_RES_X * 2) : PANEL_RES_X;
  const int mxHeight = cfg.electricalFourScan ? (PANEL_RES_Y / 2) : PANEL_RES_Y;

  HUB75_I2S_CFG mxconfig(mxWidth, mxHeight, PANEL_CHAIN, _pins_x1);
  mxconfig.i2sspeed = cfg.electricalFourScan ? HUB75_I2S_CFG::HZ_8M : HUB75_I2S_CFG::HZ_20M;
  mxconfig.latch_blanking = cfg.electricalFourScan ? 6 : 4;
  mxconfig.min_refresh_rate = 120;
  mxconfig.clkphase = cfg.electricalFourScan ? false : true;
  mxconfig.double_buff = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setRotation(DISPLAY_ROTATION);
  dma_display->setBrightness8(96);
  dma_display->clearScreen();

  gFlipBottomPanels = cfg.flipBottomPanels;
  virtual_display = new MqttDisplayVirtualPanel(*dma_display, VIRTUAL_ROWS, VIRTUAL_COLS, PANEL_RES_X, PANEL_RES_Y, cfg.chainType);
  if (cfg.electricalFourScan) {
    virtual_display->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH, DISPLAY_PIXEL_BASE);
  }
  virtual_display->setRotation(DISPLAY_ROTATION);

  Serial.printf("Display cfg #%u: %s\n", static_cast<unsigned>(activeDisplayConfigIndex + 1), cfg.name);
  redrawNeeded = true;
}

// ---------------------------------------------------------------------------
// NTP clock update — returns true if gClockStr changed
// ---------------------------------------------------------------------------

bool updateClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false; // Not synced yet — leave gClockStr as "--:--:--"
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  if (gClockStr != buf) {
    gClockStr = buf;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  setupDisplayForConfig(LOCKED_DISPLAY_CONFIG_INDEX);
  loadConfig();
  dma_display->setBrightness8(gBrightness);

  mqttClient.setBufferSize(512);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.println("[ntp] configured (UTC+7)");

  if (CALIBRATION_MODE) {
    renderCalibrationPattern();
  } else {
    renderPanels();
  }

}
// ArduinoOTA is initialized in loop() after WiFi first connects (see gOtaInitialized flag)

void loop() {
  if (CALIBRATION_MODE) {
    const unsigned long now = millis();
    if (now - lastCalibSwitchMs > 4000) {
      lastCalibSwitchMs = now;
      size_t next = activeDisplayConfigIndex + 1;
      if (next >= (sizeof(kDisplayConfigs) / sizeof(kDisplayConfigs[0]))) next = 0;
      setupDisplayForConfig(next);
      renderCalibrationPattern();
    }
    delay(5);
    return;
  }

  connectWiFiIfNeeded();
  connectMqttIfNeeded();

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }

  // Track MQTT connection state for offline indicator
  if (mqttClient.connected()) {
    gMqttWasConnected   = true;
    gMqttDisconnectedMs = 0;
  } else if (gMqttWasConnected && gMqttDisconnectedMs == 0) {
    gMqttDisconnectedMs = millis(); // record when connection was lost
    redrawNeeded = true;
  }

  // Update clock every second
  const unsigned long clockNowMs = millis();
  if (clockNowMs - gLastClockUpdateMs >= 1000) {
    gLastClockUpdateMs = clockNowMs;
    if (updateClock()) {
      redrawNeeded = true;
    }
  }

  // Advance P4 greeting scroll if enabled
  if (gGate.greetingScroll) {
    const unsigned long scrollNowMs = millis();
    if (scrollNowMs - gLastScrollMs >= 40) { // ~25fps
      gLastScrollMs = scrollNowMs;
      gGreetingScrollX--;
      const int textWidth = static_cast<int>(gGate.greeting.length()) * 6;
      if (gGreetingScrollX < -textWidth) {
        gGreetingScrollX = PANEL_RES_X; // wrap: restart from right edge
      }
      redrawNeeded = true;
    }
  }

  // Auto-timeout: reset to idle after gDisplayTimeout seconds from last event
  if (gGate.status != "idle" && gDisplayTimeout > 0) {
    const unsigned long nowMs = millis();
    if (nowMs - gLastEventMs >= static_cast<unsigned long>(gDisplayTimeout) * 1000UL) {
      gGate.status     = "idle";
      gGate.plate      = "";
      gGate.anprStatus = "";
      redrawNeeded     = true;
      Serial.printf("[timeout] display reset to idle after %us\n", gDisplayTimeout);
    }
  }

  if (redrawNeeded) {
    renderPanels();
    redrawNeeded = false;
  }

  delay(5);
}
