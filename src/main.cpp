/* --------------- snip: full working main.cpp as per your wiring and logic --------------- */
/*
 ******************************************************
 * ESP32 Wroom – Greenhouse Controller + Server Sync
 *
 * I2C split (как просил пользователь):
 *   • SHT31 on Wire0: SDA=16, SCL=17
 *   • OLED  on Wire1: SDA=22, SCL=23
 *
 * UI: rotary encoder (CLK=15, DT=2, SW=4)
 *
 * Relays (ACTIVE LOW) — ORDER EXACTLY AS REQUESTED:
 *   {13,14,27,26,25,33,32,35}
 *   ⚠ GPIO35 is input-only on ESP32; фактически реле не переключит.
 *
 * CD74HC4067:
 *   SIG=34(ADC), S0=21, S1=19, S2=18, S3=5
 *   EN=GND (always enabled), CH15=GND (bleed)
 *   Water sensor на CH14 через MUX (нет отдельного ADC-пина).
 *
 * Server:
 *   Base URL: https://vyroslo.replit.app
 *   Меню Server: OFF/ON/TEST. OFF по умолчанию (никакой серверной активности).
 *   TEST выполняет один POST /api/v1/device/auth, показывает код+JSON и
 *   автоматически возвращает в меню Server.
 *   ON включает фон: auth→telemetry→config→WS, с вежливой обработкой 401/429.
 ******************************************************
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_SHT31.h"
#include <Preferences.h>
#include <time.h>

// ---------------- Build / Debug ----------------
#define FW_VERSION      "v1.1.0"
#define HW_VERSION      "ESP32-Wroom-32"
#define DEBUG_LEVEL     1     // 0=none, 1=basic, 2=verbose
static void DBG(const String &s, int lvl=1){ if (DEBUG_LEVEL>=lvl) Serial.println(s); }

// ---------------- WiFi ----------------
const char* WIFI_SSID = "d3xr";
const char* WIFI_PASS = "d3xr-home";

// ---------------- Server ----------------
#define BASE_URL        "https://vyroslo.replit.app"
#define DEVICE_ID       "gh-02"
#define DEVICE_SECRET   "daad861d-e1df-4fb3-9668-c4382e437f71"

// ---------------- Display / UI ----------------
#define OLED_ADDR  0x3C
#define SCREEN_W   128
#define SCREEN_H   64
static const int HEADER_H = 10;
static const int LINE_H   = 10;
static const int CONTENT_TOP = HEADER_H + 2;
TwoWire WireDisplay(1);
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &WireDisplay, -1);

// ---------------- I2C (как указал пользователь) ----------------
#define I2C_SDA    16  // SHT31 bus
#define I2C_SCL    17
#define OLED_SDA   22  // OLED bus
#define OLED_SCL   23

// ---------------- Relays ----------------
const uint8_t RELAY_PINS[8] = {13,14,27,26,25,33,32,35};
const bool RELAY_ACTIVE_HIGH = false;  // active on LOW
inline int relayOnLevel()  { return RELAY_ACTIVE_HIGH ? HIGH : LOW;  }
inline int relayOffLevel() { return RELAY_ACTIVE_HIGH ? LOW  : HIGH; }
bool         relayState[8]      = {false,false,false,false,false,false,false,false};
unsigned long relayChangedAt[8] = {0,0,0,0,0,0,0,0};
const char*   RELAY_IDS[8] = {
  "light_top","light_mid","light_bot","humidifier",
  "exhaust_low","exhaust_high","reserve_1","reserve_2"
};
static inline bool isOutputCapable(int g){ return !(g>=34 && g<=39); }

// ---------------- Encoder ----------------
const uint8_t ENC_CLK = 15;
const uint8_t ENC_DT  = 2;
const uint8_t ENC_SW  = 4;
volatile int8_t encDelta = 0;
int lastCLK = HIGH;
unsigned long lastEncMicros = 0;
const unsigned long ENC_DEBOUNCE_US = 800;
bool lastBtn = HIGH;
unsigned long btnDownAt = 0;
const unsigned long LONG_PRESS_MS = 700;

// ---------------- MUX / Soil ----------------
const uint8_t MUX_SIG = 34;    // ADC
const uint8_t MUX_S0  = 21;
const uint8_t MUX_S1  = 19;
const uint8_t MUX_S2  = 18;
const uint8_t MUX_S3  = 5;
#define MUX_GND_CH 15

const int   SOIL_SAMPLES       = 14;
const int   SOIL_SETTLE_US     = 500;
const float SOIL_FLOAT_STD_MIN = 80.0;
const int   SOIL_FLOAT_MIN     = 900;
const int   SOIL_FLOAT_MAX     = 3000;
const uint8_t SOIL_OK_HYST     = 2;
const uint8_t SOIL_NA_HYST     = 2;
const int   SOIL_RING_DEV_MAX  = 60;

static int16_t soilSmooth[16];
static bool    soilInit[16];
static uint8_t soilOkCnt[16];
static uint8_t soilNaCnt[16];
static int16_t soilRing[16][3];
static uint8_t soilRingCount[16];
static uint8_t soilRingPos[16];
static int16_t soilAvg3s[16];
static bool    soilAvgOk[16];
static uint8_t soilScanIdx = 0;
unsigned long  soilLastFixMs = 0;
const uint32_t SOIL_REDRAW_MS = 1000;

// ---------------- SHT31 ----------------
Adafruit_SHT31 sht44 = Adafruit_SHT31(); // 0x44
Adafruit_SHT31 sht45 = Adafruit_SHT31(); // 0x45
bool have44=false, have45=false;

// ---------------- UI State ----------------
enum UiState { UI_HOME, UI_MENU, UI_RELAYS, UI_SENSORS, UI_SOIL, UI_SERVER, UI_SERVER_TEST_SENDING, UI_SERVER_TEST_RESULT };
UiState ui = UI_HOME;
int menuIndex = 0;
const char* mainMenuItems[] = {"Relays","Sensors","Soil","Server","Exit"};
const int mainMenuCount = 5;
int relSel = 0, relTop = 0;
int soilSel = 0, soilTop = 0;
static inline int visibleRows() { return (SCREEN_H - CONTENT_TOP) / LINE_H; }
int16_t homeScroll = 0, homeContentH = 0;
const int16_t HOME_SCROLL_STEP = LINE_H;
const uint32_t HOME_REDRAW_MS = 600;
const uint32_t SENS_REDRAW_MS = 2000;
unsigned long lastDrawMs = 0;

// ---------------- Server State ----------------
Preferences prefs;
String jwtToken;
String cfgETag;
bool   serverEnabled   = false;     // OFF по умолчанию
bool   serverAuthed    = false;
bool   wsConnected     = false;
int    TELEMETRY_SEC   = 10;
int    CONFIG_SEC      = 60;
int    HEARTBEAT_SEC   = 30;
unsigned long tLastTelemetry = 0;
unsigned long tLastConfig    = 0;
unsigned long tLastHeartbeat = 0;
WebSocketsClient webSocket;

// === AUTH / Backoff ===
bool          authBlocked = false;         // 401 → блок до перезагрузки
unsigned long lastAuthAttempt = 0;
unsigned long authRetryDelay  = 0;
int           authFailCount   = 0;
unsigned long retryAfterUntilMs = 0;
unsigned long lastRetryAfterSec = 0;

const unsigned long MIN_DELAY_MS = 30UL*1000UL;
const unsigned long MAX_DELAY_MS = 60UL*60UL*1000UL;

void scheduleAfterMs(unsigned long ms){ authRetryDelay = ms; lastAuthAttempt = millis(); }
void scheduleBackoffDouble(){ if (authRetryDelay == 0) authRetryDelay = MIN_DELAY_MS; else authRetryDelay = min(authRetryDelay * 2, MAX_DELAY_MS); lastAuthAttempt = millis(); }
void scheduleRetryAfterWindow(){ unsigned long waitMs = (lastRetryAfterSec ? lastRetryAfterSec : 15UL*60UL)*1000UL + 30000UL; retryAfterUntilMs = millis() + waitMs; }

// === LED (по умолчанию выключены; при желании назначь пины) ===
enum LedColor { LED_OFF, LED_RED, LED_YELLOW, LED_GREEN };
#ifndef LED_RED_PIN
#define LED_RED_PIN    -1
#endif
#ifndef LED_YLW_PIN
#define LED_YLW_PIN    -1
#endif
#ifndef LED_GRN_PIN
#define LED_GRN_PIN    -1
#endif
static inline bool safePin(int p){ return !(p>=6 && p<=11) && p!=12 && p!=-1; }
void ledSetup() { if (safePin(LED_RED_PIN)) pinMode(LED_RED_PIN, OUTPUT); if (safePin(LED_YLW_PIN)) pinMode(LED_YLW_PIN, OUTPUT); if (safePin(LED_GRN_PIN)) pinMode(LED_GRN_PIN, OUTPUT); }
void setLED(LedColor c){
  auto w=[&](int p, int v){ if (safePin(p)) digitalWrite(p, v); };
  w(LED_RED_PIN,   c==LED_RED   ? HIGH : LOW);
  w(LED_YLW_PIN,   c==LED_YELLOW? HIGH : LOW);
  w(LED_GRN_PIN,   c==LED_GREEN ? HIGH : LOW);
}

// ---------------- Helpers ----------------
static void muxSelect(uint8_t ch) {
  digitalWrite(MUX_S0, (ch & 0x01) ? HIGH : LOW);
  digitalWrite(MUX_S1, (ch & 0x02) ? HIGH : LOW);
  digitalWrite(MUX_S2, (ch & 0x04) ? HIGH : LOW);
  digitalWrite(MUX_S3, (ch & 0x08) ? HIGH : LOW);
  delayMicroseconds(SOIL_SETTLE_US);
}
static void sortSmall(int *a, int n){
  for (int i=1;i<n;i++){int key=a[i];int j=i-1;while(j>=0 && a[j]>key){a[j+1]=a[j];j--;}a[j+1]=key;}
}
static int readSoilStable(uint8_t ch, bool &ok) {
  #if (MUX_GND_CH >= 0)
    if (ch != MUX_GND_CH) {
      muxSelect(MUX_GND_CH);
      for (int i=0;i<3;i++) { (void)analogRead(MUX_SIG); delayMicroseconds(200); }
    }
  #endif
  muxSelect(ch);
  (void)analogRead(MUX_SIG); delayMicroseconds(200);
  (void)analogRead(MUX_SIG); delayMicroseconds(200);
  const int N = SOIL_SAMPLES; int v[N]; long sum = 0;
  for (int i=0;i<N;i++){ v[i]=analogRead(MUX_SIG); sum+=v[i]; delayMicroseconds(200); }
  sortSmall(v, N);
  int start = 2, end = N-2; long tsum=0; int cnt=0;
  for (int i=start;i<end;i++){ tsum+=v[i]; cnt++; }
  float avg = (cnt>0)? (float)tsum/cnt : (float)sum/N;
  float var=0; for(int i=start;i<end;i++){ float d=v[i]-avg; var+=d*d; }
  float stddev = (cnt>0)? sqrtf(var/cnt) : 0;
  bool floating = (stddev > SOIL_FLOAT_STD_MIN && avg > SOIL_FLOAT_MIN && avg < SOIL_FLOAT_MAX);
  ok = !floating;
  return (int)avg;
}
static void updateSoilChannel(uint8_t ch){
  bool ok=false; int val = readSoilStable(ch, ok);
  if (!soilInit[ch]) { soilSmooth[ch]=val; soilInit[ch]=true; }
  if (ok){ soilOkCnt[ch] = soilOkCnt[ch] < 250 ? soilOkCnt[ch]+1 : 250; soilNaCnt[ch]=0; }
  else   { soilNaCnt[ch] = soilNaCnt[ch] < 250 ? soilNaCnt[ch]+1 : 250; }
  if (ok){ soilSmooth[ch] = (soilSmooth[ch]*3 + val)/4; }
}
static bool soilIsShownOK(uint8_t ch){
  if (!soilInit[ch]) return false;
  if (soilNaCnt[ch] >= SOIL_NA_HYST) return false;
  if (soilOkCnt[ch] >= SOIL_OK_HYST) return true;
  return false;
}
static void commitSoilIfDue(){
  unsigned long now = millis();
  if (now - soilLastFixMs >= SOIL_REDRAW_MS) {
    for (int ch = 0; ch < 16; ch++) {
      bool ok  = soilIsShownOK(ch);
      int  val = soilSmooth[ch];
      if (ok) {
        soilRing[ch][ soilRingPos[ch] ] = val;
        if (soilRingCount[ch] < 3) soilRingCount[ch]++;
        soilRingPos[ch] = (soilRingPos[ch] + 1) % 3;
      }
      if (soilRingCount[ch] > 0) {
        long sum = 0; for (int i = 0; i < soilRingCount[ch]; i++) sum += soilRing[ch][i];
        long sum2 = 0; for (int i = 0; i < soilRingCount[ch]; i++){ long d = soilRing[ch][i] - sum/soilRingCount[ch]; sum2 += d*d; }
        int avg3 = (int)(sum / soilRingCount[ch]);
        int dev3 = (soilRingCount[ch] > 1) ? (int)sqrtf((float)sum2 / soilRingCount[ch]) : 0;
        if (dev3 <= SOIL_RING_DEV_MAX) { soilAvg3s[ch] = avg3; soilAvgOk[ch] = ok; }
        else                           { soilAvgOk[ch] = false; }
      } else {
        soilAvgOk[ch] = false; soilAvg3s[ch] = 0;
      }
    }
    soilLastFixMs = now;
  }
}
static void setRelay(int idx, bool on) {
  if (idx < 0 || idx > 7) return;
  relayState[idx] = on; relayChangedAt[idx] = millis();
  int pin = RELAY_PINS[idx];
  if (isOutputCapable(pin)) { pinMode(pin, OUTPUT); digitalWrite(pin, on ? relayOnLevel() : relayOffLevel()); }
}
static void toggleRelay(int idx) { setRelay(idx, !relayState[idx]); }

// --- Encoder polling ---
static void pollEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastCLK) {
    unsigned long now = micros();
    if (now - lastEncMicros > ENC_DEBOUNCE_US) {
      int dt = digitalRead(ENC_DT);
      if (dt != clk) encDelta++; else encDelta--;
      lastEncMicros = now;
    }
    lastCLK = clk;
  }
}
static int8_t takeEncoderDelta() { noInterrupts(); int8_t d = encDelta; encDelta = 0; interrupts(); return d; }
enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG };
static ButtonEvent pollButton() {
  ButtonEvent ev = BTN_NONE; int b = digitalRead(ENC_SW); unsigned long t = millis();
  if (lastBtn == HIGH && b == LOW) { btnDownAt = t; }
  if (lastBtn == LOW && b == HIGH) { unsigned long dur = t - btnDownAt; ev = (dur >= LONG_PRESS_MS) ? BTN_LONG : BTN_SHORT; }
  lastBtn = b; return ev;
}

// ---------------- Time / ISO8601 ----------------
static String isoTimestamp() {
  time_t now; time(&now);
  struct tm tm; gmtime_r(&now, &tm);
  char buf[30]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &tm);
  return String(buf);
}

// ---------------- Display helpers ----------------
static void drawHeader(const char* title) {
    display.fillRect(0,0,SCREEN_W,HEADER_H,SSD1306_BLACK);
    display.setCursor(0,0);
    display.print(title);
}
static void drawHome() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    drawHeader("HOME");
    int lines = 1; // WiFi
    int sensorLines = (have44 ? 1 : 0) + (have45 ? 1 : 0);
    if (sensorLines == 0) sensorLines = 1;
    lines += sensorLines;
    int onCount = 0;
    for (int i = 0; i < 8; i++) if (relayState[i]) onCount++;
    if (onCount > 0) lines += 1 + onCount;
    if (serverEnabled) lines += 1;
    homeContentH = CONTENT_TOP + lines * LINE_H;
    int16_t maxScroll = homeContentH > SCREEN_H ? (homeContentH - SCREEN_H) : 0;
    if (homeScroll < 0) homeScroll = 0;
    if (homeScroll > maxScroll) homeScroll = maxScroll;
    int y = CONTENT_TOP;
    auto line = [&](const String &s) {
        int ys = y - homeScroll;
        if (ys >= CONTENT_TOP && ys <= SCREEN_H - LINE_H) {
            display.setCursor(0, ys);
            display.print(s);
        }
        y += LINE_H;
    };
    char wifiBuf[48];
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(wifiBuf, sizeof(wifiBuf), "%s %d.%d.%d.%d", WIFI_SSID, ip[0], ip[1], ip[2], ip[3]);
    } else {
        snprintf(wifiBuf, sizeof(wifiBuf), "No WiFi");
    }
    line(String(wifiBuf));
    if (have44 || have45) {
        if (have44) {
            float t = sht44.readTemperature(), h = sht44.readHumidity();
            char b[32];
            if (isnan(t) || isnan(h)) snprintf(b, 32, "0x44: -");
            else snprintf(b, 32, "0x44: %.1fC %.0f%%", t, h);
            line(String(b));
        }
        if (have45) {
            float t = sht45.readTemperature(), h = sht45.readHumidity();
            char b[32];
            if (isnan(t) || isnan(h)) snprintf(b, 32, "0x45: -");
            else snprintf(b, 32, "0x45: %.1fC %.0f%%", t, h);
            line(String(b));
        }
    } else {
        line("SHT31: not found");
    }
    if (onCount > 0) {
        line("Relays ON:");
        for (int i = 0; i < 8; i++) {
            if (relayState[i]) {
                char rb[28];
                snprintf(rb, sizeof(rb), "R%d (%d): ON", i + 1, RELAY_PINS[i]);
                line(String(rb));
            }
        }
    }
    if (serverEnabled) {
        String s = String("Server: ") + (serverAuthed ? "AUTH" : "AUTH…") + ", WS:" + (wsConnected ? "ON" : "OFF");
        line(s);
    }
    display.display();
}
static void drawMenu() {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  drawHeader("MENU");
  for (int i=0;i<mainMenuCount;i++) {
    int y = CONTENT_TOP + i*LINE_H;
    if (i == menuIndex) { display.fillRect(0, y-1, SCREEN_W, LINE_H, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); display.setCursor(3, y); display.print(mainMenuItems[i]); display.setTextColor(SSD1306_WHITE); }
    else { display.setCursor(3, y); display.print(mainMenuItems[i]); }
  }
  display.display();
}
static void drawRelays() {
  display.clearDisplay(); drawHeader("RELAYS");
  int rows = visibleRows(); int y = CONTENT_TOP;
  for (int i=0;i<rows;i++) {
    int idx = relTop + i; if (idx >= 8) break;
    display.setCursor(0,y); y += LINE_H;
    if (idx == relSel) display.print("> "); else display.print("  ");
    display.printf("R%d (%d): %s", idx+1, RELAY_PINS[idx], relayState[idx] ? "ON " : "OFF");
  }
  display.display();
}
static void drawSensors() {
  display.clearDisplay(); drawHeader("SENSORS"); int y=CONTENT_TOP;
  if (have44 || have45) {
    if (have44) { float t=sht44.readTemperature(), h=sht44.readHumidity(); display.setCursor(0,y); y+=LINE_H; if(isnan(t)||isnan(h)) display.print("SHT31 0x44: -"); else display.printf("0x44: %.1fC  %.0f%%", t, h); }
    if (have45) { float t=sht45.readTemperature(), h=sht45.readHumidity(); display.setCursor(0,y); y+=LINE_H; if(isnan(t)||isnan(h)) display.print("SHT31 0x45: -"); else display.printf("0x45: %.1fC  %.0f%%", t, h); }
  } else { display.setCursor(0,y); y+=LINE_H; display.print("SHT31: not found"); }
  muxSelect(14); (void)analogRead(MUX_SIG); delayMicroseconds(200); int water = analogRead(MUX_SIG);
  display.setCursor(0,y); y+=LINE_H; display.printf("Water MUX14: %d", water);
  display.display();
}
static int serverMenuSel = 1; // 0=ON, 1=OFF, 2=TEST ; по умолчанию OFF
static void drawServer() {
  display.clearDisplay();
  drawHeader("SERVER");
  int y = CONTENT_TOP;
  for (int i=0;i<3;i++) {
    display.setCursor(0, y); y += LINE_H;
    bool selected = (serverMenuSel == i);
    if (selected) display.print("> "); else display.print("  ");
    if (i==0) display.print("ON");
    if (i==1) display.print("OFF");
    if (i==2) display.print("TEST");
    if ((i==0 && serverEnabled) || (i==1 && !serverEnabled)) display.print(" *");
  }
  display.display();
}
static String testResultStr;
static unsigned long testStartTime = 0;
static bool testBlockInput = false;
static void drawServerTestSending() {
  display.clearDisplay();
  drawHeader("TEST AUTH");
  int y = CONTENT_TOP + 10;
  display.setCursor(0, y); y+=LINE_H;
  display.print("Connecting to server...");
  display.setCursor(0, y); y+=LINE_H;
  display.print("Sending auth request");

  // Simple animation
  int dots = (millis() / 500) % 4;
  for(int i = 0; i < dots; i++) display.print(".");

  display.display();
}

static void drawServerTestResult() {
  display.clearDisplay();
  drawHeader("TEST RESULT");
  int y = CONTENT_TOP;
  display.setCursor(0, y); y+=LINE_H;
  display.print("POST /api/.../auth");
  display.setCursor(0, y); y+=LINE_H;
  display.print("Response:");
  display.setCursor(0, y); y+=LINE_H;
  const int maxLen = 100;
  String s = testResultStr.length()>maxLen ? testResultStr.substring(0,maxLen)+"..." : testResultStr;
  display.print(s);

  // Show instructions
  y = SCREEN_H - LINE_H - 2;
  display.setCursor(0, y);
  if (!testBlockInput) {
    display.print("Long press to exit");
  } else {
    display.print("Wait 3 sec...");
  }

  display.display();
}
static void drawSoil() {
    display.clearDisplay();
    drawHeader("SOIL");
    int rows = visibleRows();
    int y = CONTENT_TOP;
    for (int i = 0; i < rows; i++) {
        int idx = soilTop + i;
        if (idx >= 16) break;
        display.setCursor(0, y); y += LINE_H;
        if (idx == soilSel) display.print("> "); else display.print("  ");
        display.printf("S%d: %d%s", idx + 1, soilAvg3s[idx], soilAvgOk[idx] ? "" : " (NA)");
    }
    display.display();
}

// ---------------- Network helpers ----------------
static void wifiConnectAndReport() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) { delay(200); }
  if (WiFi.status() == WL_CONNECTED) { DBG("WiFi OK"); configTime(0,0,"pool.ntp.org","time.nist.gov"); }
  else { DBG("WiFi FAIL"); }
}
static String baseHostFromUrl(const String& url){
  int p = url.indexOf("//"); if (p<0) return String(); int s = p+2; int e = url.indexOf('/', s); if (e<0) e = url.length();
  String host = url.substring(s, e); int colon = host.indexOf(':'); if (colon>0) host = host.substring(0, colon); return host;
}
unsigned long parseRetryAfterSec(const String& v){
  if (v.length()==0) return 0;
  bool num=true; for(char c: v) if (!isDigit(c)) { num=false; break; }
  if (num) return v.toInt();
  return 0;
}

// ---------------- WS / Commands ----------------
static int relayIndexById(const String& id) {
    for (int i = 0; i < 8; i++) {
        if (id == RELAY_IDS[i]) return i;
    }
    return -1;
}

static void wsSendAck(const String& cmdId, const String& status) {
  DynamicJsonDocument doc(256);
  doc["cmd_id"] = cmdId;
  doc["status"] = status;
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(out);
}

static float soilPercentFromRaw(int raw) {
    float pct = (3500 - raw) * 100.0f / (3500 - 1400);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static void handleCommandJson(const char* json){
  DynamicJsonDocument doc(768); if (deserializeJson(doc, json)) return;
  String cmdId = doc["cmd_id"].as<String>(); String type = doc["type"].as<String>();
  if (type=="relay_control"){
    String relayId = doc["relay_id"].as<String>(); bool state = doc["state"] | false; int idx = relayIndexById(relayId);
    if (idx>=0){ setRelay(idx, state); wsSendAck(cmdId, "ok"); } else wsSendAck(cmdId, "unknown_relay");
  } else if (type=="config_update"){
    cfgETag = ""; wsSendAck(cmdId, "ok");
  } else {
    wsSendAck(cmdId, "unknown_type");
  }
}
static void wsEvent(WStype_t type, uint8_t * payload, size_t length){
  switch(type){
    case WStype_CONNECTED: wsConnected=true; DBG("WS connected"); break;
    case WStype_DISCONNECTED: wsConnected=false; DBG("WS disconnected"); break;
    case WStype_TEXT: if (length>0) handleCommandJson((const char*)payload); break;
    default: break;
  }
}
static void wsConnect(){
  if (!serverAuthed) return;
  String host = baseHostFromUrl(BASE_URL); String path = "/ws?token=" + jwtToken;
  webSocket.beginSSL(host.c_str(), 443, path.c_str());
  webSocket.onEvent(wsEvent);
  webSocket.setReconnectInterval(5000);
}

// ---------------- Auth / API ----------------
static String isoNow(){ return isoTimestamp(); }

static int tryAuthenticateOnce(String& outJwt) {
  WiFiClientSecure tls; tls.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);

  String url = String(BASE_URL) + "/api/v1/device/auth";
  if (!http.begin(tls, url)) { delay(1); return -1; }

  const char* hdrKeys[] = {"Retry-After"};
  http.collectHeaders(hdrKeys, 1);

  DynamicJsonDocument body(512);
  body["device_id"] = DEVICE_ID;
  body["secret"]    = DEVICE_SECRET;
  body["fw"]        = FW_VERSION;
  body["hw"]        = HW_VERSION;

  String payload; serializeJson(body, payload);
  int code = http.POST(payload);
  String rawResp = http.getString();

  // для TEST рисуем, что есть
  testResultStr = String("Code: ") + code + "\n" + rawResp;

  if (code == 200) {
    DynamicJsonDocument doc(768);
    if (deserializeJson(doc, rawResp)==DeserializationError::Ok) {
      outJwt = doc["jwt"].as<String>();
    } else {
      code = 500;
    }
  } else if (code == 429) {
    String ra = http.header("Retry-After");
    lastRetryAfterSec = (ra.length()>0 ? parseRetryAfterSec(ra) : 15UL*60UL);
  }
  http.end();
  delay(1);
  return code;
}

static void serverSendTelemetry(){
  if (!serverAuthed) return; if (WiFi.status()!=WL_CONNECTED) return;
  DynamicJsonDocument doc(3072);
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = isoNow();
  JsonArray air = doc.createNestedArray("air");
  if (have44){ float t=sht44.readTemperature(), h=sht44.readHumidity(); JsonObject a = air.createNestedObject(); a["id"]="air_top"; if(!isnan(t)) a["t_c"]=t; if(!isnan(h)) a["rh"]=h; }
  if (have45){ float t=sht45.readTemperature(), h=sht45.readHumidity(); JsonObject a = air.createNestedObject(); a["id"]="air_bot"; if(!isnan(t)) a["t_c"]=t; if(!isnan(h)) a["rh"]=h; }
  JsonArray soil = doc.createNestedArray("soil");
  for (int ch=0; ch<16; ch++){
    if (!soilAvgOk[ch]) continue; JsonObject s = soil.createNestedObject(); char id[16]; snprintf(id,sizeof(id),"soil_%02d", ch+1); s["id"] = id; s["raw"] = soilAvg3s[ch]; s["percentage"] = soilPercentFromRaw(soilAvg3s[ch]);
  }
  JsonArray rel = doc.createNestedArray("relays"); unsigned long now = millis();
  for (int i=0;i<8;i++){ JsonObject r = rel.createNestedObject(); r["id"] = RELAY_IDS[i]; r["state"]= relayState[i]; r["duration_ms"]= (unsigned long)(relayState[i]? (now - relayChangedAt[i]) : 0); }
  muxSelect(14); (void)analogRead(MUX_SIG); delayMicroseconds(200); int water = analogRead(MUX_SIG); JsonObject w = doc.createNestedObject("water"); w["low"] = (water < 1200);

  String payload; serializeJson(doc, payload);

  WiFiClientSecure tls; tls.setInsecure(); HTTPClient http; String url = String(BASE_URL) + "/api/v1/device/telemetry";
  http.begin(tls, url); http.addHeader("Content-Type","application/json"); http.addHeader("Authorization", String("Bearer ")+jwtToken);
  (void)http.POST(payload);
  http.end(); delay(1);
}

static void applyPollingFromConfig(const JsonObject &root){
  if (root.containsKey("polling")){
    JsonObject p=root["polling"].as<JsonObject>();
    if (p["telemetry_sec"]) TELEMETRY_SEC = p["telemetry_sec"].as<int>();
    if (p["config_sec"])    CONFIG_SEC    = p["config_sec"].as<int>();
  }
}

static void serverFetchConfig(){
  if (!serverAuthed) return; if (WiFi.status()!=WL_CONNECTED) return;
  WiFiClientSecure tls; tls.setInsecure(); HTTPClient http; String url=String(BASE_URL)+"/api/v1/device/config";
  http.begin(tls, url); http.addHeader("Authorization", String("Bearer ")+jwtToken); if (cfgETag.length()>0) http.addHeader("If-None-Match", cfgETag);
  int code=http.GET(); String resp=http.getString();
  if (code==200){ StaticJsonDocument<4096> doc; if (deserializeJson(doc, resp)==DeserializationError::Ok){ JsonObject root=doc.as<JsonObject>(); if (root["etag"]) cfgETag = root["etag"].as<String>(); applyPollingFromConfig(root); } }
  else if (code==401){ serverAuthed=false; authBlocked=true; setLED(LED_RED); }
  http.end(); delay(1);
}

static bool serverHeartbeat(){
  if (!serverAuthed) return false;
  WiFiClientSecure tls; tls.setInsecure(); HTTPClient http; String url = String(BASE_URL) + "/api/v1/device/heartbeat";
  http.begin(tls, url); http.addHeader("Content-Type","application/json"); http.addHeader("Authorization", String("Bearer ")+jwtToken);
  int code=http.POST("{}");
  if (code==401){ serverAuthed=false; authBlocked=true; setLED(LED_RED); }
  http.end(); delay(1);
  return code==200;
}

// ---------------- Server Control / Loop ----------------
static void serverEnable(bool en) {
    serverEnabled = en;
    if (!en) {
        webSocket.disconnect();
        wsConnected = false;
        serverAuthed = false;
        authBlocked = false;
        authFailCount = 0;
        lastAuthAttempt = 0;
        authRetryDelay = 0;
        retryAfterUntilMs = 0;
        setLED(LED_OFF);
        return;
    }
    serverAuthed = false;
    authBlocked = false;
    authFailCount = 0;
    lastAuthAttempt = 0;
    authRetryDelay = 0;
    retryAfterUntilMs = 0;
    setLED(LED_OFF);
    wifiConnectAndReport();
    tLastTelemetry = tLastConfig = tLastHeartbeat = 0;
}

static void authServiceLoop(){
  if (!serverEnabled) return;
  if (authBlocked) return;
  if (serverAuthed) return;
  if (retryAfterUntilMs && millis() < retryAfterUntilMs) { setLED(LED_YELLOW); return; }
  if (millis() - lastAuthAttempt < authRetryDelay) return;

  String jwt;
  int code = tryAuthenticateOnce(jwt);

  if (code == 200) {
    jwtToken = jwt; serverAuthed=true; authFailCount=0; authRetryDelay=MIN_DELAY_MS; retryAfterUntilMs=0; setLED(LED_GREEN);
    wsConnect();
    return;
  }
  authFailCount++;

  if (code == 401) { authBlocked = true; setLED(LED_RED); return; }
  if (code == 429) { scheduleRetryAfterWindow(); setLED(LED_YELLOW); authFailCount=0; return; }

  if (code >= 500 || code < 0) scheduleAfterMs(2UL*60UL*1000UL);
  else scheduleAfterMs(MIN_DELAY_MS);

  if (authFailCount >= 3) { scheduleBackoffDouble(); authFailCount = 0; }
}

static void serverLoop() {
    if (!serverEnabled) return;
    if (WiFi.status() != WL_CONNECTED) wifiConnectAndReport();

    authServiceLoop();
    if (!serverAuthed) return;

    webSocket.loop();
    if (!wsConnected) {
      static unsigned long lastTry=0;
      if (millis()-lastTry > 7000) { wsConnect(); lastTry = millis(); }
    }

    unsigned long now = millis();
    if (now - tLastTelemetry >= (unsigned long)TELEMETRY_SEC*1000UL){ serverSendTelemetry(); tLastTelemetry = now; }
    if (now - tLastConfig    >= (unsigned long)CONFIG_SEC   *1000UL){ serverFetchConfig();  tLastConfig    = now; }
    if (now - tLastHeartbeat >= (unsigned long)HEARTBEAT_SEC*1000UL){
      if (!serverHeartbeat()) { serverAuthed=false; authBlocked=true; setLED(LED_RED); }
      tLastHeartbeat = now;
    }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  // Relays safe OFF
  for (int i=0;i<8;i++) { int pin=RELAY_PINS[i]; if (isOutputCapable(pin)){ pinMode(pin, OUTPUT); digitalWrite(pin, relayOffLevel()); } relayState[i]=false; relayChangedAt[i]=millis(); }
  // ADC/MUX
  analogReadResolution(12); analogSetAttenuation(ADC_11db); analogSetPinAttenuation(MUX_SIG, ADC_11db);
  pinMode(MUX_S0, OUTPUT); pinMode(MUX_S1, OUTPUT); pinMode(MUX_S2, OUTPUT); pinMode(MUX_S3, OUTPUT); muxSelect(0);
  // Encoder
  pinMode(ENC_CLK, INPUT_PULLUP); pinMode(ENC_DT,  INPUT_PULLUP); pinMode(ENC_SW,  INPUT_PULLUP); lastCLK = digitalRead(ENC_CLK); lastBtn = digitalRead(ENC_SW);
  // I2C + Display / Sensors
  Wire.begin(I2C_SDA, I2C_SCL, 400000);                 // SHT31 на 16/17
  WireDisplay.begin(OLED_SDA, OLED_SCL, 400000);        // OLED  на 22/23
  ledSetup(); setLED(LED_OFF);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR); display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setCursor(0,0); display.println(F("Greenhouse Controller")); display.display();
  // SHT31
  have44 = sht44.begin(0x44); have45 = sht45.begin(0x45);
  // Soil init
  for (int i=0;i<16;i++){ soilInit[i]=false; soilOkCnt[i]=0; soilNaCnt[i]=0; soilSmooth[i]=0; soilRingCount[i]=0; soilRingPos[i]=0; soilAvg3s[i]=0; soilAvgOk[i]=false; for(int k=0;k<3;k++) soilRing[i][k]=0; }
  soilLastFixMs = millis();
  // Prefs
  prefs.begin("greenhouse", false);
  // WiFi (подключим, но сервер по умолчанию OFF)
  wifiConnectAndReport();
}

// ---------------- Loop ----------------
void loop() {
  updateSoilChannel(soilScanIdx); soilScanIdx = (soilScanIdx + 1) & 0x0F; commitSoilIfDue();

  pollEncoder(); ButtonEvent bev = pollButton(); int8_t d = takeEncoderDelta();

  switch (ui) {
    case UI_HOME:
      if (d != 0) {
        homeScroll += (d > 0 ? HOME_SCROLL_STEP : -HOME_SCROLL_STEP);
        int16_t maxScroll = homeContentH > SCREEN_H ? (homeContentH - SCREEN_H) : 0;
        if (homeScroll < 0) homeScroll = 0; if (homeScroll > maxScroll) homeScroll = maxScroll; drawHome();
      }
      if (bev == BTN_SHORT) { ui = UI_MENU; drawMenu(); }
      if (millis() - lastDrawMs > HOME_REDRAW_MS) { drawHome(); lastDrawMs = millis(); }
      break;

    case UI_MENU:
      if (d != 0) { menuIndex += (d > 0 ? 1 : -1); if (menuIndex < 0) menuIndex = mainMenuCount-1; if (menuIndex >= mainMenuCount) menuIndex = 0; drawMenu(); }
      if (bev == BTN_SHORT) {
        if      (menuIndex == 0) { ui = UI_RELAYS;  drawRelays(); }
        else if (menuIndex == 1) { ui = UI_SENSORS; drawSensors(); }
        else if (menuIndex == 2) { ui = UI_SOIL;    drawSoil(); }
        else if (menuIndex == 3) { ui = UI_SERVER;  drawServer(); }
        else { ui = UI_HOME;     drawHome(); }
      }
      break;

    case UI_RELAYS:
      if (d != 0) {
        relSel += (d > 0 ? 1 : -1); if (relSel < 0) relSel = 7; if (relSel > 7) relSel = 0;
        int rows = visibleRows(); if (relSel < relTop) relTop = relSel; if (relSel > relTop + (rows-1)) relTop = relSel - (rows-1);
        if (relTop < 0) relTop = 0; if (relTop > 8 - rows) relTop = max(0, 8 - rows);
        drawRelays();
      }
      if (bev == BTN_SHORT) { toggleRelay(relSel); drawRelays(); }
      else if (bev == BTN_LONG) { ui = UI_MENU; drawMenu(); }
      break;

    case UI_SENSORS:
      if (bev == BTN_SHORT) { drawSensors(); }
      else if (bev == BTN_LONG) { ui = UI_MENU; drawMenu(); }
      if (millis() - lastDrawMs > SENS_REDRAW_MS) { drawSensors(); lastDrawMs = millis(); }
      break;

    case UI_SOIL:
      if (d != 0) {
        soilSel += (d > 0 ? 1 : -1); if (soilSel < 0) soilSel = 15; if (soilSel > 15) soilSel = 0;
        int rows = visibleRows(); if (soilSel < soilTop) soilTop = soilSel; if (soilSel > soilTop + (rows-1)) soilTop = soilSel - (rows-1);
        if (soilTop < 0) soilTop = 0; if (soilTop > 16 - rows) soilTop = max(0, 16 - rows);
        drawSoil();
      }
      if (bev == BTN_SHORT) { drawSoil(); }
      else if (bev == BTN_LONG) { ui = UI_MENU; drawMenu(); }
      if (millis() - lastDrawMs > SOIL_REDRAW_MS) { drawSoil(); lastDrawMs = millis(); }
      break;

    case UI_SERVER: {
      static bool firstEnter = true;
      if (firstEnter) { serverMenuSel = serverEnabled ? 0 : 1; firstEnter = false; }
      if (d != 0) {
        serverMenuSel += (d > 0 ? 1 : -1);
        if (serverMenuSel < 0) serverMenuSel = 2;
        if (serverMenuSel > 2) serverMenuSel = 0;
        drawServer();
      }
      if (bev == BTN_SHORT) {
        if (serverMenuSel == 0) { serverEnable(true);  drawServer(); }   // ON
        else if (serverMenuSel == 1) { serverEnable(false); drawServer(); } // OFF
        else if (serverMenuSel == 2) {
          ui = UI_SERVER_TEST_SENDING;
          testStartTime = millis();
          testBlockInput = false;
          drawServerTestSending();
        }
      } else if (bev == BTN_LONG) {
        ui = UI_MENU; firstEnter = true; drawMenu();
      }
      if (millis() - lastDrawMs > 1000) { drawServer(); lastDrawMs = millis(); }
    } break;

    case UI_SERVER_TEST_SENDING:
      // Block input during sending
      if (millis() - testStartTime > 500) {
        // After 500ms animation, do the actual request
        wifiConnectAndReport();
        String jwt;
        (void)tryAuthenticateOnce(jwt);
        ui = UI_SERVER_TEST_RESULT;
        testStartTime = millis();
        testBlockInput = true;
        drawServerTestResult();
      } else {
        // Show animation while waiting
        drawServerTestSending();
      }
      break;

    case UI_SERVER_TEST_RESULT:
      // Check if 3 seconds have passed since result shown
      if (testBlockInput && millis() - testStartTime >= 3000) {
        testBlockInput = false;
      }

      // Only allow long press exit after 3 seconds
      if (!testBlockInput && bev == BTN_LONG) {
        ui = UI_SERVER;
        drawServer();
      } else if (testBlockInput || millis() % 1000 < 500) {
        // Redraw result periodically or when blocking input
        drawServerTestResult();
      }
      break;
  }

  serverLoop();
  delay(1);
}

/* Notes:
 * - Server OFF по умолчанию: никакой авторизации/телеметрии/WS пока не включишь ON.
 * - TEST делает один POST /api/v1/device/auth и показывает код+JSON, затем авто-возврат.
 * - 401 → authBlocked=true (красный LED), до перезагрузки попыток нет.
 * - 429 → ждём Retry-After (+30s), одна попытка; повторный 429 → удвоение ожидания (≤60 минут).
 * - До успешного AUTH не шлём ни телеметрию, ни конфиг, ни WS — нулевая серверная активность.
 * - GPIO35 вход-только: если оставлен в массиве — фактически не управляет реле (безопасная обёртка).
 */