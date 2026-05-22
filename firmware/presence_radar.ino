#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <xpt2046.h>

#define PWR_EN_PIN 10
#define PWR_ON_PIN 14
#define BK_LIGHT_PIN 38
#define BUTTON_PIN 0
#define TOUCHSCREEN_SCLK_PIN 1
#define TOUCHSCREEN_MISO_PIN 4
#define TOUCHSCREEN_MOSI_PIN 3
#define TOUCHSCREEN_CS_PIN 2
#define TOUCHSCREEN_IRQ_PIN 9

TFT_eSPI tft = TFT_eSPI();
XPT2046 touch = XPT2046(SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN);
BLEScan *bleScan = nullptr;

#define C_BG 0x0004
#define C_PANEL 0x1082
#define C_PANEL2 0x18E3
#define C_GRID 0x0320
#define C_GRID2 0x0200
#define C_TEXT 0xE73C
#define C_MUTED 0x6B4D
#define C_ACCENT 0x07FF
#define C_WARN 0xFD20
#define C_HOT 0xF800
#define C_BLE 0xA01F
#define C_WIFI 0x07D9
#define C_COOL 0x035F
#define C_WARM 0xFFE0

struct SignalPoint {
  int16_t rssi;
  uint8_t channel;
  uint8_t age;
};

SignalPoint points[24];
uint32_t seenHashes[32];
uint32_t prevHashes[32];
int16_t seenRssi[32];
int16_t prevRssi[32];
uint32_t bleHashes[48];
uint32_t prevBleHashes[48];
int16_t bleRssi[48];
int16_t prevBleRssi[48];
int seenCount = 0;
int prevCount = 0;
int bleCount = 0;
int prevBleCount = 0;
int pointCount = 0;
int baselineCount = -1;
int baselineStrong = -100;
int baselineRssiSum = 0;
float rfBaseCount = -1;
float rfBaseStrong = -100;
float rfBaseAvg = -90;
float rfBaseStrongCount = 0;
float rfBaseZone[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float bleBaseCount = 0;
float bleBaseStrong = -100;
float bleBaseStrongCount = 0;
float rfNoiseFloor = 18;
int scanSamples = 0;
int motionStreak = 0;
int quietStreak = 0;
int multiMoverStreak = 0;
int presenceStreak = 0;
int absenceStreak = 0;
int lastNetworkCount = 0;
int lastStrongest = -100;
int lastRssiAvg = -90;
int lastBleCount = 0;
int lastBleStrongest = -100;
int networkCount = 0;
int strongCount = 0;
int strongest = -100;
int rssiSum = 0;
int bleStrongCount = 0;
int bleStrongest = -100;
int bleRssiSum = 0;
int presenceScore = 0;
int motionScore = 0;
int roomScore = 0;
int newDeviceCount = 0;
int vanishedDeviceCount = 0;
int newBleCount = 0;
int vanishedBleCount = 0;
int zoneEnergy[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float zoneHeat[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float zoneTrail[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int topZoneA = 0;
int topZoneB = 4;
int possibleHumanCount = 0;
int silhouetteScore = 0;
int humanConfidence = 0;
int meshScore = 0;
int anchorRssi = -100;
int anchorZone = 0;
int anchorDelta = 0;
int anchorQuality = 0;
uint32_t anchorHash = 0;
String shapeLabel = "No clear shape";
bool humanDetected = false;
bool lastHumanDetected = false;
uint32_t humanPresentSince = 0;
uint32_t lastPresenceDurationSec = 0;
uint32_t lastPresenceEventAt = 0;
int sensitivity = 2;
int sweepDeg = 0;
uint32_t lastScan = 0;
uint32_t lastFrame = 0;
String statusLine = "Calibrating";
String inferenceLine = "Learning this room";
String tacticalLine = "Adaptive RF baseline";

enum ViewMode {
  VIEW_RADAR,
  VIEW_HUMAN,
  VIEW_ANALYTICS
};

ViewMode viewMode = VIEW_RADAR;
int navSelection = 0;
int historyConfidence[30] = {0};
int historyMotion[30] = {0};
int historyRoom[30] = {0};
int historySilhouette[30] = {0};
int historyIndex = 0;
int historyCount = 0;

static void setBrightness(uint8_t value) {
  static uint8_t steps = 16;
  static uint8_t brightness = 0;
  if (brightness == value) return;
  if (value > 16) value = 16;
  if (value == 0) {
    digitalWrite(BK_LIGHT_PIN, LOW);
    brightness = 0;
    delay(3);
    return;
  }
  if (brightness == 0) {
    digitalWrite(BK_LIGHT_PIN, HIGH);
    brightness = steps;
    delayMicroseconds(30);
  }
  int from = steps - brightness;
  int to = steps - value;
  int pulses = (steps + to - from) % steps;
  for (int i = 0; i < pulses; i++) {
    digitalWrite(BK_LIGHT_PIN, LOW);
    digitalWrite(BK_LIGHT_PIN, HIGH);
  }
  brightness = value;
}

static String clip(String value, int maxLen) {
  value.trim();
  if (value.length() <= maxLen) return value;
  return value.substring(0, maxLen - 1) + "~";
}

static void drawButton(int x, int y, int w, int h, const String &label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 4, C_PANEL2);
  tft.drawRoundRect(x, y, w, h, 4, color);
  tft.drawFastHLine(x + 6, y + 3, w - 12, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_TEXT, C_PANEL2);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
}

static void drawNavButton(int x, int y, int w, const String &label, uint16_t color, bool active) {
  uint16_t bg = active ? color : C_PANEL2;
  uint16_t fg = active ? C_BG : C_TEXT;
  tft.fillRoundRect(x, y, w, 34, 4, bg);
  tft.drawRoundRect(x, y, w, 34, 4, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, x + w / 2, y + 17, 2);
}

static void drawTopTab(int x, int w, const String &label, bool active, uint16_t color) {
  uint16_t bg = active ? color : C_PANEL;
  uint16_t fg = active ? C_BG : C_TEXT;
  tft.fillRoundRect(x, 4, w, 29, 4, bg);
  tft.drawRoundRect(x, 4, w, 29, 4, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, x + w / 2, 18, 2);
}

static void drawTopTabs() {
  drawTopTab(2, 76, "RADAR", viewMode == VIEW_RADAR, C_ACCENT);
  drawTopTab(82, 76, "HUMAN", viewMode == VIEW_HUMAN, C_WARN);
  drawTopTab(162, 76, "ANALY", viewMode == VIEW_ANALYTICS, C_BLE);
}

static String viewButtonLabel() {
  if (viewMode == VIEW_RADAR) return "HUMAN";
  if (viewMode == VIEW_HUMAN) return "ANALY";
  return "RADAR";
}

static String formatDuration(uint32_t seconds) {
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;
  seconds %= 60;
  minutes %= 60;
  if (hours > 0) return String(hours) + "h " + String(minutes) + "m";
  if (minutes > 0) return String(minutes) + "m " + String(seconds) + "s";
  return String(seconds) + "s";
}

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_SHORT,
  BUTTON_LONG
};

static int readButtonEvent() {
  static bool last = false;
  static uint32_t downAt = 0;
  bool now = digitalRead(BUTTON_PIN) == LOW;
  if (now && !last) downAt = millis();
  if (!now && last) {
    uint32_t held = millis() - downAt;
    last = now;
    if (held > 800) return BUTTON_LONG;
    if (held > 25) return BUTTON_SHORT;
  }
  last = now;
  return BUTTON_NONE;
}

static bool touchTap(int &x, int &y) {
  static bool last = false;
  bool now = touch.pressed();
  if (now && !last) {
    x = constrain((int)touch.Y(), 0, 239);
    y = constrain((int)touch.X(), 0, 319);
    last = now;
    return true;
  }
  last = now;
  return false;
}

static uint16_t scoreColor() {
  if (presenceScore >= 70) return TFT_RED;
  if (presenceScore >= 40) return TFT_ORANGE;
  if (presenceScore >= 20) return TFT_YELLOW;
  return TFT_GREEN;
}

static void recordAnalyticsSample() {
  historyConfidence[historyIndex] = humanConfidence;
  historyMotion[historyIndex] = motionScore;
  historyRoom[historyIndex] = roomScore;
  historySilhouette[historyIndex] = silhouetteScore;
  historyIndex = (historyIndex + 1) % 30;
  historyCount = min(30, historyCount + 1);
}

static uint32_t fnv1a(const uint8_t *data, int len) {
  uint32_t hash = 2166136261UL;
  for (int i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619UL;
  }
  return hash;
}

static uint32_t hashString(const String &value) {
  return fnv1a((const uint8_t *)value.c_str(), value.length());
}

static bool hashIn(uint32_t hash, uint32_t *list, int count) {
  for (int i = 0; i < count; i++) {
    if (list[i] == hash) return true;
  }
  return false;
}

static int prevRssiFor(uint32_t hash) {
  for (int i = 0; i < prevCount; i++) {
    if (prevHashes[i] == hash) return prevRssi[i];
  }
  return -127;
}

static int prevBleRssiFor(uint32_t hash) {
  for (int i = 0; i < prevBleCount; i++) {
    if (prevBleHashes[i] == hash) return prevBleRssi[i];
  }
  return -127;
}

static void findTopZones() {
  topZoneA = 0;
  topZoneB = 4;
  for (int i = 1; i < 8; i++) {
    if (zoneEnergy[i] > zoneEnergy[topZoneA]) topZoneA = i;
  }
  for (int i = 0; i < 8; i++) {
    if (i == topZoneA) continue;
    if (zoneEnergy[i] > zoneEnergy[topZoneB] || topZoneB == topZoneA) topZoneB = i;
  }
}

static void updateMotionField() {
  int total = 0;
  for (int i = 0; i < 8; i++) {
    int excess = max(0, zoneEnergy[i] - (int)rfBaseZone[i]);
    int blended = zoneEnergy[i] / 2 + excess;
    zoneHeat[i] = zoneHeat[i] * 0.72f + blended * 0.28f;
    zoneTrail[i] = max(zoneTrail[i] * 0.90f, zoneHeat[i]);
    total += constrain((int)zoneHeat[i], 0, 80);
  }
  silhouetteScore = constrain(total / 4 + motionStreak * 4, 0, 100);
}

static void updateSignalMesh() {
  int meshTotal = 0;
  int peak = 0;
  int opposite = (anchorZone + 4) % 8;
  int left = (anchorZone + 7) % 8;
  int right = (anchorZone + 1) % 8;
  int anchorPush = constrain(anchorQuality / 5 + anchorDelta * 3, 0, 42);

  zoneEnergy[anchorZone] += anchorPush / 2;
  zoneEnergy[opposite] += anchorPush;
  zoneEnergy[left] += anchorPush / 3;
  zoneEnergy[right] += anchorPush / 3;

  for (int i = 0; i < 8; i++) {
    int neighbor = (zoneEnergy[(i + 7) % 8] + zoneEnergy[(i + 1) % 8]) / 4;
    int bridge = (i == opposite || i == anchorZone) ? anchorPush / 2 : 0;
    int fused = constrain(zoneEnergy[i] + neighbor + bridge, 0, 130);
    zoneEnergy[i] = (zoneEnergy[i] * 2 + fused) / 3;
    meshTotal += zoneEnergy[i];
    peak = max(peak, zoneEnergy[i]);
  }
  meshScore = constrain(meshTotal / 6 + peak / 2 + anchorQuality / 8, 0, 100);
}

static void classifyRfShape() {
  int spread = 0;
  for (int i = 0; i < 8; i++) {
    if (zoneHeat[i] > 18) spread++;
  }

  if (humanConfidence < 42 && silhouetteScore < 32) {
    shapeLabel = "No clear body";
  } else if (motionScore >= 70 && spread >= 4) {
    shapeLabel = "Wide moving shape";
  } else if (motionScore >= 52) {
    shapeLabel = "Moving RF body";
  } else if (silhouetteScore >= 62 && motionScore < 38) {
    shapeLabel = "Stable human blob";
  } else if (meshScore >= 55) {
    shapeLabel = "Partial RF shape";
  } else {
    shapeLabel = "Weak human trace";
  }
}

static void updateAdaptiveBaseline(int avg, int rawMotion) {
  if (rfBaseCount < 0) {
    rfBaseCount = networkCount;
    rfBaseStrong = strongest;
    rfBaseAvg = avg;
    rfBaseStrongCount = strongCount;
    for (int i = 0; i < 8; i++) rfBaseZone[i] = zoneEnergy[i];
    bleBaseCount = bleCount;
    bleBaseStrong = bleStrongest;
    bleBaseStrongCount = bleStrongCount;
    rfNoiseFloor = max(10, rawMotion);
    return;
  }

  bool learning = scanSamples < 8;
  bool quiet = rawMotion < rfNoiseFloor * 1.6f && motionScore < 38;
  float alpha = learning ? 0.28f : (quiet ? 0.08f : 0.015f);
  rfBaseCount = rfBaseCount * (1.0f - alpha) + networkCount * alpha;
  rfBaseStrong = rfBaseStrong * (1.0f - alpha) + strongest * alpha;
  rfBaseAvg = rfBaseAvg * (1.0f - alpha) + avg * alpha;
  rfBaseStrongCount = rfBaseStrongCount * (1.0f - alpha) + strongCount * alpha;
  bleBaseCount = bleBaseCount * (1.0f - alpha) + bleCount * alpha;
  bleBaseStrong = bleBaseStrong * (1.0f - alpha) + bleStrongest * alpha;
  bleBaseStrongCount = bleBaseStrongCount * (1.0f - alpha) + bleStrongCount * alpha;
  for (int i = 0; i < 8; i++) {
    rfBaseZone[i] = rfBaseZone[i] * (1.0f - alpha) + zoneEnergy[i] * alpha;
  }

  if (quiet || learning) {
    rfNoiseFloor = rfNoiseFloor * 0.88f + rawMotion * 0.12f;
  } else {
    rfNoiseFloor = rfNoiseFloor * 0.98f + min(rawMotion, 60) * 0.02f;
  }
  rfNoiseFloor = constrain(rfNoiseFloor, 8.0f, 70.0f);
}

static void inferPresence() {
  int avg = networkCount ? rssiSum / networkCount : -100;
  int countDelta = abs(networkCount - lastNetworkCount);
  int strongestDelta = abs(strongest - lastStrongest);
  int avgDelta = abs(avg - lastRssiAvg);
  int rfVolatility = countDelta * 9 + strongestDelta * 3 + avgDelta * 4;
  int deviceFlux = newDeviceCount * 18 + vanishedDeviceCount * 12;
  int bleVolatility = abs(bleCount - lastBleCount) * 10 +
                      abs(bleStrongest - lastBleStrongest) * 3 +
                      newBleCount * 14 +
                      vanishedBleCount * 8;
  findTopZones();
  int zoneMotion = zoneEnergy[topZoneA] + zoneEnergy[topZoneB] / 2;
  int rawMotion = rfVolatility + deviceFlux + zoneMotion + bleVolatility;

  updateAdaptiveBaseline(avg, rawMotion);

  int sectorDelta = 0;
  for (int i = 0; i < 8; i++) {
    sectorDelta += abs(zoneEnergy[i] - (int)rfBaseZone[i]);
  }
  int baselineDrift = abs(networkCount - (int)rfBaseCount) * 6 +
                      abs(strongest - (int)rfBaseStrong) * 3 +
                      abs(avg - (int)rfBaseAvg) * 4 +
                      abs(strongCount - (int)rfBaseStrongCount) * 8 +
                      abs(bleCount - (int)bleBaseCount) * 9 +
                      abs(bleStrongest - (int)bleBaseStrong) * 3 +
                      abs(bleStrongCount - (int)bleBaseStrongCount) * 10 +
                      sectorDelta / 3;
  int localEvidence = max(0, strongest + 82) * 2 +
                      max(0, bleStrongest + 86) * 2 +
                      strongCount * 7 +
                      bleStrongCount * 9 +
                      max(0, zoneEnergy[topZoneA] - (int)rfBaseZone[topZoneA]);
  int motionAboveNoise = max(0, rawMotion - (int)(rfNoiseFloor * 1.15f));
  motionScore = constrain((motionAboveNoise * sensitivity + sectorDelta / 2 + deviceFlux + bleVolatility) / 2, 0, 100);
  roomScore = constrain((baselineDrift + localEvidence + motionScore) / 3, 0, 100);

  if (motionScore >= 45) {
    motionStreak = min(20, motionStreak + 1);
    quietStreak = 0;
  } else {
    quietStreak = min(20, quietStreak + 1);
    motionStreak = max(0, motionStreak - 1);
  }
  updateMotionField();

  int topAExcess = zoneEnergy[topZoneA] - (int)rfBaseZone[topZoneA];
  int topBExcess = zoneEnergy[topZoneB] - (int)rfBaseZone[topZoneB];
  int zoneGap = abs(topZoneA - topZoneB);
  zoneGap = min(zoneGap, 8 - zoneGap);
  bool separatedZones = zoneGap >= 2;
  bool secondMoverCandidate = topBExcess > 18 && topAExcess > 20 && separatedZones && motionScore >= 65;
  if (secondMoverCandidate) multiMoverStreak = min(10, multiMoverStreak + 1);
  else multiMoverStreak = max(0, multiMoverStreak - 1);

  int occupiedEvidence = roomScore +
                         motionStreak * 5 +
                         max(0, strongCount - (int)rfBaseStrongCount) * 10 +
                         max(0, bleStrongCount - (int)bleBaseStrongCount) * 12;
  if (occupiedEvidence >= 94 && multiMoverStreak >= 4 && topBExcess > 28) {
    possibleHumanCount = 3;
  } else if (occupiedEvidence >= 78 && multiMoverStreak >= 3) {
    possibleHumanCount = 2;
  } else if (occupiedEvidence >= 42 || topAExcess > 10) {
    possibleHumanCount = 1;
  } else {
    possibleHumanCount = 0;
  }
  humanConfidence = constrain((roomScore * 2 + motionScore * 2 + silhouetteScore * 3 + meshScore + motionStreak * 8 + possibleHumanCount * 18) / 9, 0, 100);
  classifyRfShape();
  bool rawHuman = scanSamples >= 4 &&
                  possibleHumanCount >= 1 &&
                  humanConfidence >= 55 &&
                  (silhouetteScore >= 28 || motionStreak >= 1 || bleStrongCount > bleBaseStrongCount + 1);
  bool rawEmpty = scanSamples >= 4 &&
                  humanConfidence <= 38 &&
                  motionScore <= 34 &&
                  silhouetteScore <= 30 &&
                  quietStreak >= 2;
  if (rawHuman) {
    presenceStreak = min(10, presenceStreak + 1);
    absenceStreak = 0;
  } else if (rawEmpty) {
    absenceStreak = min(10, absenceStreak + 1);
    presenceStreak = max(0, presenceStreak - 1);
  } else {
    presenceStreak = max(0, presenceStreak - 1);
    absenceStreak = max(0, absenceStreak - 1);
  }
  if (humanDetected) {
    if (absenceStreak >= 2) humanDetected = false;
  } else if (presenceStreak >= 1) {
    humanDetected = true;
  }

  if (humanDetected) {
    inferenceLine = "DETECTED: HUMAN";
  } else if (motionScore >= 75 && roomScore >= 55) {
    inferenceLine = "THIS ROOM: movement";
  } else if (newBleCount >= 2 && bleStrongest > -78) {
    inferenceLine = "THIS ROOM: BLE nearby";
  } else if (newDeviceCount >= 2 && strongest > -72) {
    inferenceLine = "THIS ROOM: new radios";
  } else if (strongCount >= 3 && strongest > -58) {
    inferenceLine = "THIS ROOM: devices";
  } else if (motionScore >= 45) {
    inferenceLine = "NEARBY: RF shift";
  } else if (roomScore >= 35) {
    inferenceLine = "THIS ROOM: occupied";
  } else if (networkCount <= 2) {
    inferenceLine = "Sparse RF room";
  } else if (scanSamples < 8) {
    inferenceLine = "Learning RF baseline";
  } else {
    inferenceLine = "THIS ROOM: quiet";
  }
  if (silhouetteScore >= 70) tacticalLine = "RF silhouette strong";
  else if (silhouetteScore >= 42) tacticalLine = "RF silhouette forming";
  else if (possibleHumanCount >= 3) tacticalLine = "3+ inferred movers";
  else if (possibleHumanCount >= 2) tacticalLine = "2 inferred movers";
  else if (possibleHumanCount == 1) tacticalLine = "1 local motion zone";
  else if (motionScore >= 35) tacticalLine = "Wall/near-room signal";
  else if (scanSamples < 8) tacticalLine = "Learning normal RF";
  else tacticalLine = "No active motion";
  lastNetworkCount = networkCount;
  lastStrongest = strongest;
  lastRssiAvg = avg;
  lastBleCount = bleCount;
  lastBleStrongest = bleStrongest;
  scanSamples++;
}

static void emitPresenceEventIfNeeded() {
  if (scanSamples < 4) return;
  Serial.print("PBL_RADAR_STATUS,present=");
  Serial.print(humanDetected ? 1 : 0);
  Serial.print(",confidence=");
  Serial.print(humanConfidence);
  Serial.print(",room=");
  Serial.print(roomScore);
  Serial.print(",motion=");
  Serial.print(motionScore);
  Serial.print(",silhouette=");
  Serial.print(silhouetteScore);
  Serial.print(",ble=");
  Serial.print(bleCount);
  Serial.print(",ap=");
  Serial.print(networkCount);
  Serial.print(",mesh=");
  Serial.print(meshScore);
  Serial.print(",anchor=");
  Serial.println(anchorQuality);

  if (humanDetected == lastHumanDetected) return;
  uint32_t now = millis();
  if (now - lastPresenceEventAt < 1500) return;

  lastPresenceEventAt = now;
  if (humanDetected) {
    humanPresentSince = now;
    Serial.print("PBL_RADAR_EVENT,HUMAN_PRESENT,confidence=");
    Serial.print(humanConfidence);
    Serial.print(",room=");
    Serial.print(roomScore);
    Serial.print(",motion=");
    Serial.print(motionScore);
    Serial.print(",silhouette=");
    Serial.println(silhouetteScore);
  } else {
    uint32_t duration = humanPresentSince ? (now - humanPresentSince) / 1000 : 0;
    lastPresenceDurationSec = duration;
    Serial.print("PBL_RADAR_EVENT,HUMAN_LEFT,duration=");
    Serial.print(duration);
    Serial.print(",confidence=");
    Serial.print(humanConfidence);
    Serial.print(",room=");
    Serial.print(roomScore);
    Serial.print(",motion=");
    Serial.print(motionScore);
    Serial.print(",silhouette=");
    Serial.println(silhouetteScore);
    humanPresentSince = 0;
  }
  lastHumanDetected = humanDetected;
}

static void computePresence() {
  if (baselineCount < 0) {
    baselineCount = networkCount;
    baselineStrong = strongest;
    baselineRssiSum = rssiSum;
    presenceScore = 10;
    statusLine = "Learning room";
    inferPresence();
    return;
  }

  int countDelta = abs(networkCount - baselineCount);
  int strongDelta = abs(strongest - baselineStrong);
  int sumDelta = abs(rssiSum - baselineRssiSum) / max(1, baselineCount);
  int activity = countDelta * 8 + strongCount * 5 + strongDelta * 2 + sumDelta;
  activity *= sensitivity;
  presenceScore = constrain(activity, 0, 100);

  if (presenceScore >= 70) statusLine = "Presence likely";
  else if (presenceScore >= 40) statusLine = "Movement nearby";
  else if (presenceScore >= 20) statusLine = "Room changed";
  else statusLine = "Quiet room";

  inferPresence();

  baselineCount = (baselineCount * 9 + networkCount) / 10;
  baselineStrong = (baselineStrong * 9 + strongest) / 10;
  baselineRssiSum = (baselineRssiSum * 9 + rssiSum) / 10;
}

static void scanBle() {
  if (!bleScan) return;
  for (int i = 0; i < bleCount; i++) {
    prevBleHashes[i] = bleHashes[i];
    prevBleRssi[i] = bleRssi[i];
  }
  prevBleCount = bleCount;
  bleCount = 0;
  newBleCount = 0;
  vanishedBleCount = 0;
  bleStrongCount = 0;
  bleStrongest = -100;
  bleRssiSum = 0;

  BLEScanResults *results = bleScan->start(1, false);
  int count = results ? min(results->getCount(), 48) : 0;
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice dev = results->getDevice(i);
    int rssi = dev.getRSSI();
    String address = String(dev.getAddress().toString().c_str());
    uint32_t hash = hashString(address);
    bleHashes[bleCount] = hash;
    bleRssi[bleCount] = rssi;
    if (!hashIn(hash, prevBleHashes, prevBleCount)) newBleCount++;
    int prior = prevBleRssiFor(hash);
    int delta = prior == -127 ? 8 : abs(rssi - prior);
    int zone = (int)(hash & 0x7);
    zoneEnergy[zone] += constrain(delta + max(0, rssi + 88) / 5, 0, 24);
    bleCount++;
    bleStrongest = max(bleStrongest, rssi);
    bleRssiSum += rssi;
    if (rssi > -78) bleStrongCount++;
  }
  for (int i = 0; i < prevBleCount; i++) {
    if (!hashIn(prevBleHashes[i], bleHashes, bleCount)) vanishedBleCount++;
  }
  bleScan->clearResults();
}

static void scanWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  int n = WiFi.scanNetworks(false, true);
  networkCount = max(0, n);
  strongest = -100;
  strongCount = 0;
  rssiSum = 0;
  for (int i = 0; i < seenCount; i++) {
    prevHashes[i] = seenHashes[i];
    prevRssi[i] = seenRssi[i];
  }
  prevCount = seenCount;
  seenCount = 0;
  newDeviceCount = 0;
  vanishedDeviceCount = 0;
  for (int i = 0; i < 8; i++) zoneEnergy[i] = 0;
  pointCount = min(networkCount, 24);
  uint32_t bestHash = 0;
  int bestZone = 0;
  for (int i = 0; i < pointCount; i++) {
    int rssi = WiFi.RSSI(i);
    int ch = WiFi.channel(i);
    uint8_t *bssid = WiFi.BSSID(i);
    uint32_t hash = bssid ? fnv1a(bssid, 6) : hashString(WiFi.SSID(i));
    int zone = ((int)(hash & 0x7) + ch) % 8;
    if (seenCount < 32) {
      seenHashes[seenCount] = hash;
      seenRssi[seenCount] = rssi;
      seenCount++;
      if (!hashIn(hash, prevHashes, prevCount)) newDeviceCount++;
    }
    int prior = prevRssiFor(hash);
    int delta = prior == -127 ? 12 : abs(rssi - prior);
    int localWeight = max(0, rssi + 86) / 4;
    zoneEnergy[zone] += constrain(delta + localWeight, 0, 32);
    points[i] = {(int16_t)rssi, (uint8_t)ch, 0};
    if (rssi > strongest) {
      strongest = rssi;
      bestHash = hash;
      bestZone = zone;
    }
    rssiSum += rssi;
    if (rssi > -62) strongCount++;
  }
  int previousAnchor = anchorRssi;
  anchorHash = bestHash;
  anchorRssi = strongest;
  anchorZone = bestZone;
  anchorDelta = previousAnchor <= -99 ? 0 : abs(anchorRssi - previousAnchor);
  anchorQuality = constrain((anchorRssi + 92) * 3 + strongCount * 8, 0, 100);
  for (int i = 0; i < prevCount; i++) {
    if (!hashIn(prevHashes[i], seenHashes, seenCount)) vanishedDeviceCount++;
  }
  WiFi.scanDelete();
  scanBle();
  updateSignalMesh();
  computePresence();
  recordAnalyticsSample();
  emitPresenceEventIfNeeded();
}

static void drawHumanShape(int zone, int energy, uint16_t color) {
  if (energy < 10) return;
  int cx = 120;
  int cy = 139;
  int r = map(constrain(energy, 10, 80), 10, 80, 44, 76);
  float a = (zone * 45 + 22) * DEG_TO_RAD;
  int x = cx + cos(a) * r;
  int y = cy + sin(a) * r;
  tft.drawCircle(x, y - 8, 6, color);
  tft.drawLine(x, y - 2, x, y + 16, color);
  tft.drawLine(x - 10, y + 5, x + 10, y + 5, color);
  tft.drawLine(x, y + 16, x - 8, y + 28, color);
  tft.drawLine(x, y + 16, x + 8, y + 28, color);
  tft.drawCircle(x, y + 8, 20, color);
}

static uint16_t heatColor(int heat) {
  if (heat > 74) return C_HOT;
  if (heat > 52) return C_WARN;
  if (heat > 30) return C_WARM;
  if (heat > 14) return C_ACCENT;
  if (heat > 5) return C_COOL;
  return C_GRID;
}

static void drawHeatSpot(int x, int y, int heat, int maxRadius) {
  heat = constrain(heat, 0, 100);
  if (heat < 4) return;
  int r = map(heat, 4, 100, 3, maxRadius);
  uint16_t outer = heatColor(max(6, heat / 3));
  uint16_t mid = heatColor(max(12, heat * 2 / 3));
  uint16_t core = heatColor(heat);
  if (r > 9) tft.fillCircle(x, y, r, outer);
  if (r > 5) tft.fillCircle(x, y, max(2, r * 2 / 3), mid);
  tft.fillCircle(x, y, max(1, r / 3), core);
}

static void drawMetricBar(int x, int y, int w, const String &label, int value, uint16_t color) {
  value = constrain(value, 0, 100);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString(label, x, y, 1);
  tft.drawRect(x + 24, y + 2, w, 5, C_GRID);
  tft.fillRect(x + 25, y + 3, max(1, (w - 2) * value / 100), 3, color);
}

static void drawAnalyticsBar(int x, int y, int w, const String &label, int value, uint16_t color) {
  value = constrain(value, 0, 100);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.drawString(label, x, y, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(String(value) + "%", x + w, y, 2);
  tft.drawRect(x, y + 18, w, 8, C_GRID);
  tft.fillRect(x + 1, y + 19, max(1, (w - 2) * value / 100), 6, color);
}

static void drawAnalyticsSpark(int x, int y, int w, int h, int *values, uint16_t color) {
  tft.drawRect(x, y, w, h, C_GRID);
  if (historyCount < 2) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString("learning", x + w / 2, y + h / 2, 1);
    return;
  }
  int previousX = x + 2;
  int oldest = (historyIndex - historyCount + 30) % 30;
  int previousValue = values[oldest];
  int previousY = y + h - 3 - previousValue * (h - 6) / 100;
  for (int i = 1; i < historyCount; i++) {
    int idx = (oldest + i) % 30;
    int px = x + 2 + i * (w - 4) / max(1, historyCount - 1);
    int py = y + h - 3 - constrain(values[idx], 0, 100) * (h - 6) / 100;
    tft.drawLine(previousX, previousY, px, py, color);
    previousX = px;
    previousY = py;
  }
}

static void drawAnalyticsCard(int x, int y, int w, int h, const String &label, const String &value, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, C_PANEL);
  tft.drawRoundRect(x, y, w, h, 5, color);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString(label, x + 7, y + 5, 1);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.drawString(value, x + 7, y + 20, 2);
}

static void drawRfSilhouette(int cx, int cy) {
  int total = 0;
  float sumX = 0;
  float sumY = 0;
  for (int i = 0; i < 8; i++) {
    int heat = constrain((int)zoneHeat[i], 0, 90);
    int trail = constrain((int)zoneTrail[i], 0, 90);
    if (trail > 8) {
      float a = (i * 45 + 22) * DEG_TO_RAD;
      int inner = 34;
      int outer = map(trail, 8, 90, 46, 86);
      int x1 = cx + cos(a - 0.18f) * inner;
      int y1 = cy + sin(a - 0.18f) * inner;
      int x2 = cx + cos(a + 0.18f) * inner;
      int y2 = cy + sin(a + 0.18f) * inner;
      int x3 = cx + cos(a) * outer;
      int y3 = cy + sin(a) * outer;
      uint16_t col = heatColor(trail);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, col);
      tft.drawTriangle(x1, y1, x2, y2, x3, y3, C_BG);
      if (heat > 32) drawHeatSpot(x3, y3, heat, 18);
    }
    if (heat > 5) {
      float a = (i * 45 + 22) * DEG_TO_RAD;
      sumX += cos(a) * heat;
      sumY += sin(a) * heat;
      total += heat;
    }
  }

  if (total < 24) return;
  int offsetX = constrain((int)(sumX / total * 34), -34, 34);
  int offsetY = constrain((int)(sumY / total * 34), -34, 34);
  int x = cx + offsetX;
  int y = cy + offsetY;
  int radius = map(constrain(silhouetteScore, 20, 100), 20, 100, 12, 30);
  uint16_t core = heatColor(silhouetteScore);

  tft.drawCircle(x, y, radius + 14, C_GRID);
  tft.fillEllipse(x, y, max(6, radius / 2), radius + 8, heatColor(max(20, silhouetteScore / 2)));
  tft.fillEllipse(x, y, max(4, radius / 3), radius, core);
  tft.fillCircle(x, y - radius - 11, max(4, radius / 4), core);
  tft.drawEllipse(x, y, max(8, radius / 2 + 5), radius + 12, C_TEXT);
  tft.drawFastHLine(x - radius - 16, y, radius * 2 + 32, C_MUTED);
  tft.drawFastVLine(x, y - radius - 18, radius * 2 + 38, C_MUTED);
}

static void drawThermalField(int cx, int cy, int r) {
  for (int i = 0; i < 8; i++) {
    int heat = constrain((int)zoneHeat[i], 0, 100);
    int trail = constrain((int)zoneTrail[i], 0, 100);
    int level = max(heat, trail * 3 / 4);
    if (level < 5) continue;
    float a = (i * 45 + 22) * DEG_TO_RAD;
    int rr = map(level, 5, 100, 34, r - 8);
    int x = cx + cos(a) * rr;
    int y = cy + sin(a) * rr;
    drawHeatSpot(x, y, level, 28);
  }
}

static void drawMeshField(int cx, int cy, int r) {
  float anchorAngle = (anchorZone * 45 + 22) * DEG_TO_RAD;
  int ax = cx + cos(anchorAngle) * (r - 8);
  int ay = cy + sin(anchorAngle) * (r - 8);
  tft.drawCircle(ax, ay, 6, C_WIFI);
  tft.drawFastHLine(ax - 9, ay, 19, C_WIFI);
  tft.drawFastVLine(ax, ay - 9, 19, C_WIFI);
  for (int i = 0; i < 8; i++) {
    int heat = constrain((int)zoneHeat[i] + zoneEnergy[i] / 2, 0, 100);
    if (heat < 12) continue;
    float a = (i * 45 + 22) * DEG_TO_RAD;
    int ex = cx + cos(a) * (r - 12);
    int ey = cy + sin(a) * (r - 12);
    uint16_t col = heatColor(heat);
    tft.drawLine(ax, ay, ex, ey, col);
    if (heat > 45) tft.drawLine(ax + 1, ay, ex + 1, ey, col);
  }
}

static void drawRadar() {
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.fillRect(0, 0, 240, 37, C_PANEL);
  tft.drawFastHLine(0, 37, 240, C_ACCENT);
  uint16_t verdictColor = humanDetected ? C_HOT : (humanConfidence >= 36 ? C_WARN : C_ACCENT);
  drawTopTabs();
  String verdict = humanDetected ? "DETECTED: HUMAN" : (humanConfidence >= 36 ? "SIGNAL: POSSIBLE" : "SCANNING");
  tft.setTextColor(verdictColor, C_BG);
  tft.drawString(verdict + "  C" + String(humanConfidence) + "%", 120, 49, 1);

  int cx = 120;
  int cy = 137;
  int r = 78;
  drawThermalField(cx, cy, r);
  drawMeshField(cx, cy, r);
  tft.drawCircle(cx, cy, r, C_GRID);
  tft.drawCircle(cx, cy, 55, C_GRID2);
  tft.drawCircle(cx, cy, 28, C_GRID2);
  tft.drawFastHLine(cx - r, cy, r * 2, C_GRID2);
  tft.drawFastVLine(cx, cy - r, r * 2, C_GRID2);
  for (int i = 0; i < 8; i++) {
    float ga = i * 45 * DEG_TO_RAD;
    int heat = constrain((int)zoneHeat[i], 0, 90);
    uint16_t col = heatColor(heat);
    int len = map(heat, 0, 90, 36, r);
    tft.drawLine(cx, cy, cx + cos(ga) * r, cy + sin(ga) * r, C_GRID2);
    if (heat > 10) tft.drawLine(cx, cy, cx + cos(ga) * len, cy + sin(ga) * len, col);
  }
  drawRfSilhouette(cx, cy);

  float a = sweepDeg * DEG_TO_RAD;
  int sx = cx + cos(a) * r;
  int sy = cy + sin(a) * r;
  tft.drawLine(cx, cy, sx, sy, C_ACCENT);
  tft.fillCircle(cx, cy, 3, C_ACCENT);

  for (int i = 0; i < pointCount; i++) {
    int pr = map(constrain(points[i].rssi, -95, -35), -95, -35, r, 14);
    int deg = (points[i].channel * 29 + i * 17) % 360;
    float pa = deg * DEG_TO_RAD;
    int px = cx + cos(pa) * pr;
    int py = cy + sin(pa) * pr;
    int heat = map(constrain(points[i].rssi, -90, -40), -90, -40, 8, 72);
    drawHeatSpot(px, py, heat, points[i].rssi > -62 ? 10 : 6);
    tft.drawCircle(px, py, points[i].rssi > -62 ? 4 : 3, C_WIFI);
  }

  int bleDots = min(bleCount, 10);
  for (int i = 0; i < bleDots; i++) {
    int pr = map(constrain(bleRssi[i], -96, -42), -96, -42, r - 5, 18);
    int deg = ((int)(bleHashes[i] & 0xff) * 7 + i * 31) % 360;
    float pa = deg * DEG_TO_RAD;
    int px = cx + cos(pa) * pr;
    int py = cy + sin(pa) * pr;
    int heat = map(constrain(bleRssi[i], -96, -42), -96, -42, 6, 78);
    drawHeatSpot(px, py, heat, bleRssi[i] > -78 ? 10 : 6);
    tft.drawFastHLine(px - 4, py, 9, bleRssi[i] > -78 ? C_BLE : C_MUTED);
    tft.drawFastVLine(px, py - 4, 9, bleRssi[i] > -78 ? C_BLE : C_MUTED);
  }

  uint16_t c = heatColor(max(roomScore, silhouetteScore));
  tft.fillRoundRect(6, 213, 228, 60, 5, C_PANEL);
  tft.drawRoundRect(6, 213, 228, 60, 5, C_GRID);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(c, C_PANEL);
  tft.drawString(inferenceLine, 13, 218, 2);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.drawString(shapeLabel, 13, 235, 1);
  drawMetricBar(13, 249, 48, "R", roomScore, C_ACCENT);
  drawMetricBar(82, 249, 48, "M", motionScore, C_WARN);
  drawMetricBar(151, 249, 48, "X", meshScore, verdictColor);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString("BLE " + String(bleCount), 198, 249, 1);
  tft.drawString("AP " + String(networkCount), 198, 260, 1);

  drawNavButton(4, 281, 54, "SENS", C_ACCENT, navSelection == 0);
  drawNavButton(62, 281, 54, "BASE", C_WARN, navSelection == 1);
  drawNavButton(120, 281, 54, "SCAN", C_WIFI, navSelection == 2);
  drawNavButton(178, 281, 58, viewButtonLabel(), C_BLE, navSelection == 3);
}

static void drawHumanStatus() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, 240, 37, C_PANEL);
  tft.drawFastHLine(0, 37, 240, C_ACCENT);
  tft.setTextDatum(MC_DATUM);
  drawTopTabs();

  uint16_t stateColor = humanDetected ? C_HOT : C_ACCENT;
  String state = humanDetected ? "HUMAN PRESENT" : "NO HUMAN";
  String movement = "Quiet";
  if (humanDetected && motionScore >= 55) movement = "Movement";
  else if (humanDetected && motionScore >= 28) movement = "Small movement";
  else if (humanDetected) movement = "Stable";

  uint32_t duration = humanDetected && humanPresentSince ? (millis() - humanPresentSince) / 1000 : lastPresenceDurationSec;
  String durationLabel = humanDetected ? "Inside now" : "Last visit";

  tft.fillRoundRect(8, 50, 224, 64, 6, C_PANEL);
  tft.drawRoundRect(8, 50, 224, 64, 6, stateColor);
  tft.setTextColor(stateColor, C_PANEL);
  tft.drawString(state, 120, 73, 4);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString("Confidence " + String(humanConfidence) + "%", 120, 101, 2);

  drawAnalyticsCard(8, 122, 106, 50, "DURATION", formatDuration(duration), C_WARN);
  drawAnalyticsCard(126, 122, 106, 50, "STATUS", movement, stateColor);
  drawAnalyticsCard(8, 180, 106, 48, "SHAPE", shapeLabel, C_WARN);
  drawAnalyticsCard(126, 180, 106, 48, "MESH", String(meshScore) + "%", C_WIFI);

  tft.fillRoundRect(8, 235, 224, 36, 5, C_PANEL);
  tft.drawRoundRect(8, 235, 224, 36, 5, C_GRID);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.drawString(durationLabel + " | " + movement, 120, 247, 2);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString(humanDetected ? "Privacy safe: no camera, no mic" : "Waiting for room activity", 120, 263, 1);

  drawNavButton(4, 281, 54, "SENS", C_ACCENT, navSelection == 0);
  drawNavButton(62, 281, 54, "BASE", C_WARN, navSelection == 1);
  drawNavButton(120, 281, 54, "SCAN", C_WIFI, navSelection == 2);
  drawNavButton(178, 281, 58, viewButtonLabel(), C_BLE, navSelection == 3);
}

static void drawAnalytics() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, 240, 37, C_PANEL);
  tft.drawFastHLine(0, 37, 240, C_ACCENT);
  tft.setTextDatum(MC_DATUM);
  drawTopTabs();
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("LIVE RF + BLE SIGNAL INTEL", 120, 49, 1);

  uint16_t verdictColor = humanDetected ? C_HOT : (humanConfidence >= 36 ? C_WARN : C_ACCENT);
  drawAnalyticsCard(6, 58, 110, 43, "VERDICT", humanDetected ? "HUMAN" : "WATCH", verdictColor);
  drawAnalyticsCard(124, 58, 110, 43, "SOURCES", "AP " + String(networkCount) + "  BLE " + String(bleCount), C_WIFI);
  drawAnalyticsCard(6, 106, 70, 39, "NEW", "W" + String(newDeviceCount) + " B" + String(newBleCount), C_WARN);
  drawAnalyticsCard(84, 106, 70, 39, "LEFT", "W" + String(vanishedDeviceCount) + " B" + String(vanishedBleCount), C_BLE);
  drawAnalyticsCard(162, 106, 72, 39, "MESH", String(meshScore) + "%", C_ACCENT);

  tft.fillRoundRect(6, 151, 228, 69, 5, C_PANEL);
  tft.drawRoundRect(6, 151, 228, 69, 5, C_GRID);
  drawAnalyticsBar(15, 157, 94, "CONF", humanConfidence, verdictColor);
  drawAnalyticsBar(129, 157, 94, "ROOM", roomScore, C_ACCENT);
  drawAnalyticsBar(15, 188, 94, "MOTION", motionScore, C_WARN);
  drawAnalyticsBar(129, 188, 94, "SHAPE", max(silhouetteScore, meshScore), C_HOT);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("CONF", 8, 225, 1);
  tft.drawString("MOT", 66, 225, 1);
  tft.drawString("ROOM", 124, 225, 1);
  tft.drawString("SHAPE", 182, 225, 1);
  drawAnalyticsSpark(6, 238, 52, 33, historyConfidence, verdictColor);
  drawAnalyticsSpark(64, 238, 52, 33, historyMotion, C_WARN);
  drawAnalyticsSpark(122, 238, 52, 33, historyRoom, C_ACCENT);
  drawAnalyticsSpark(180, 238, 54, 33, historySilhouette, C_HOT);

  drawNavButton(4, 281, 54, "SENS", C_ACCENT, navSelection == 0);
  drawNavButton(62, 281, 54, "BASE", C_WARN, navSelection == 1);
  drawNavButton(120, 281, 54, "SCAN", C_WIFI, navSelection == 2);
  drawNavButton(178, 281, 58, viewButtonLabel(), C_BLE, navSelection == 3);
}

static void drawCurrentView() {
  if (viewMode == VIEW_ANALYTICS) drawAnalytics();
  else if (viewMode == VIEW_HUMAN) drawHumanStatus();
  else drawRadar();
}

static void resetBaseline();

static void activateNavButton(int button) {
  button = constrain(button, 0, 3);
  if (button == 0) {
    sensitivity = sensitivity % 4 + 1;
    inferenceLine = "Sensitivity " + String(sensitivity);
    tacticalLine = "Button OK";
  } else if (button == 1) {
    resetBaseline();
    inferenceLine = "Baseline reset";
    tacticalLine = "Learning normal RF";
  } else if (button == 2) {
    inferenceLine = "Manual scan";
    tacticalLine = "Scanning now";
    scanWifi();
  } else {
    if (viewMode == VIEW_RADAR) viewMode = VIEW_HUMAN;
    else if (viewMode == VIEW_HUMAN) viewMode = VIEW_ANALYTICS;
    else viewMode = VIEW_RADAR;
    navSelection = 0;
    Serial.print("PBL_RADAR_UI,bottom_view=");
    if (viewMode == VIEW_RADAR) Serial.println("RADAR");
    else if (viewMode == VIEW_HUMAN) Serial.println("HUMAN");
    else Serial.println("ANALYTICS");
  }
}

static void resetBaseline() {
  baselineCount = networkCount;
  baselineStrong = strongest;
  baselineRssiSum = rssiSum;
  int avg = networkCount ? rssiSum / networkCount : -100;
  rfBaseCount = networkCount;
  rfBaseStrong = strongest;
  rfBaseAvg = avg;
  rfBaseStrongCount = strongCount;
  for (int i = 0; i < 8; i++) rfBaseZone[i] = zoneEnergy[i];
  for (int i = 0; i < 8; i++) {
    zoneHeat[i] = 0;
    zoneTrail[i] = 0;
  }
  bleBaseCount = bleCount;
  bleBaseStrong = bleStrongest;
  bleBaseStrongCount = bleStrongCount;
  rfNoiseFloor = 18;
  scanSamples = 0;
  motionStreak = 0;
  quietStreak = 0;
  multiMoverStreak = 0;
  presenceStreak = 0;
  absenceStreak = 0;
  statusLine = "Baseline set";
  inferenceLine = "Learning RF baseline";
  tacticalLine = "Adaptive baseline";
  presenceScore = 0;
  motionScore = 0;
  roomScore = 0;
  possibleHumanCount = 0;
  silhouetteScore = 0;
  humanConfidence = 0;
  meshScore = 0;
  anchorDelta = 0;
  anchorQuality = 0;
  shapeLabel = "No clear shape";
  humanDetected = false;
  lastHumanDetected = false;
  humanPresentSince = 0;
  lastPresenceDurationSec = 0;
  lastPresenceEventAt = 0;
}

static void handleControlTap(int x, int y) {
  if (y <= 90) {
    if (x < 80) viewMode = VIEW_RADAR;
    else if (x < 160) viewMode = VIEW_HUMAN;
    else viewMode = VIEW_ANALYTICS;
    navSelection = 0;
    Serial.print("PBL_RADAR_UI,tab=");
    if (viewMode == VIEW_RADAR) Serial.println("RADAR");
    else if (viewMode == VIEW_HUMAN) Serial.println("HUMAN");
    else Serial.println("ANALYTICS");
    drawCurrentView();
    return;
  }
  if (y < 260) return;

  int button = constrain(x / 60, 0, 3);
  navSelection = button;
  Serial.print("PBL_RADAR_UI,touch_button=");
  Serial.println(button);
  activateNavButton(button);
  drawCurrentView();
}

void setup() {
  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, HIGH);
  pinMode(PWR_ON_PIN, OUTPUT);
  digitalWrite(PWR_ON_PIN, HIGH);
  pinMode(BK_LIGHT_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(300);

  tft.begin();
  tft.setRotation(0);
  setBrightness(16);

  SPI.begin(TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_MOSI_PIN);
  touch.begin(240, 320);
  touch.setCal(285, 1788, 311, 1877, 240, 320);
  touch.setRotation(3);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();
  bleScan->setActiveScan(false);
  bleScan->setInterval(120);
  bleScan->setWindow(80);
  randomSeed(esp_random());
  drawCurrentView();
}

void loop() {
  uint32_t now = millis();
  if (now - lastScan > 1800) {
    lastScan = now;
    scanWifi();
  }
  if (now - lastFrame > 120) {
    lastFrame = now;
    sweepDeg = (sweepDeg + 10) % 360;
    drawCurrentView();
  }

  int x = 0, y = 0;
  if (touchTap(x, y)) {
    handleControlTap(x, y);
  }
  int buttonEvent = readButtonEvent();
  if (buttonEvent == BUTTON_SHORT) {
    navSelection = (navSelection + 1) % 4;
    tacticalLine = "Hold to enter";
    Serial.print("PBL_RADAR_UI,selected=");
    Serial.println(navSelection);
    drawCurrentView();
  } else if (buttonEvent == BUTTON_LONG) {
    Serial.print("PBL_RADAR_UI,enter=");
    Serial.println(navSelection);
    activateNavButton(navSelection);
    drawCurrentView();
  }
}
