// Minimal WF1/WF2 HUB75 firmware: render MQTT sensor values on 4 chained 64x32 panels.

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

struct SensorBinding {
  const char* topic;
  const char* label;
  const char* unit;
};

static const SensorBinding kSensors[4] = {
  {"am-102/sensor/temperature/state", "TEMP", "C"},
  {"am-102/sensor/humidity/state", "HUM", "%"},
  {"am-102/sensor/co2_level/state", "CO2", "ppm"},
  {"am-102/sensor/pm2_5_concentration/state", "PM2.5", "ug/m3"},
};

String sensorValues[4] = {"--", "--", "--", "--"};

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

String formatSensorValue(int index, const String& rawPayload) {
  String payload = sanitizePayload(rawPayload);
  if (payload.length() == 0) return "--";

  float numeric = 0.0f;
  if (tryParseNumber(payload, numeric)) {
    char buff[24];
    if (index == 0 || index == 1) {
      snprintf(buff, sizeof(buff), "%.1f %s", static_cast<double>(numeric), kSensors[index].unit);
    } else {
      snprintf(buff, sizeof(buff), "%.0f %s", static_cast<double>(numeric), kSensors[index].unit);
    }
    return String(buff);
  }

  return payload + " " + kSensors[index].unit;
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

  for (int i = 0; i < 4; i++) {
    const int row = i / VIRTUAL_COLS;
    const int col = i % VIRTUAL_COLS;
    const int panelX = col * PANEL_RES_X;
    const int panelY = row * PANEL_RES_Y;

    targetSetTextDefaults();
    targetSetTextSize(1);
    targetSetTextColor(labelColor);
    drawCenteredText(String("P") + String(i + 1) + " " + kSensors[i].label, panelX, panelY + 1, PANEL_RES_X, 8);

    String value = sensorValues[i];
    if (value.length() > 9) value = value.substring(0, 9);
    targetSetTextSize(1);
    targetSetTextColor(valueColor);
    drawCenteredText(value, panelX, panelY + 12, PANEL_RES_X, 12);
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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    value += static_cast<char>(payload[i]);
  }
  value.trim();

  for (int i = 0; i < 4; i++) {
    if (strcmp(topic, kSensors[i].topic) == 0) {
      sensorValues[i] = formatSensorValue(i, value);
      redrawNeeded = true;
      Serial.printf("MQTT update [%s] = %s\n", topic, sensorValues[i].c_str());
      return;
    }
  }
}

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectedShown = true;
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
  for (int i = 0; i < 4; i++) {
    mqttClient.subscribe(kSensors[i].topic);
    Serial.printf("Subscribed: %s\n", kSensors[i].topic);
  }
  mqttConnectedShown = false;
}

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

void setup() {
  Serial.begin(115200);
  setupDisplayForConfig(LOCKED_DISPLAY_CONFIG_INDEX);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  if (CALIBRATION_MODE) {
    renderCalibrationPattern();
  } else {
    renderPanels();
  }
}

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

  if (redrawNeeded) {
    renderPanels();
    redrawNeeded = false;
  }

  delay(5);
}
