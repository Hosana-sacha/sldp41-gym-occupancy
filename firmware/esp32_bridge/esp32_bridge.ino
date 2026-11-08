/*
  SLDP 41 - Gym occupancy: ESP32 Wi-Fi bridge (Option B)

  The Arduino UNO counts people and sends one text line over a wire:
        <count>,<occupancyRate>\n        e.g.  12,48.0
  This sketch reads those lines, then uploads the two values to
  Firebase Realtime Database (path /gym) over HTTPS using the REST API.

  Wiring (one-way, UNO -> ESP32):
    UNO D11 (TX) --[1k]--+--> ESP32 GPIO16 (RX2)
                         |
                       [2k]
                         |
                        GND          (divider: 5 V -> 3.3 V)
    UNO GND ---------------- ESP32 GND
    UNO 5V  ---------------- ESP32 VIN (or power the ESP32 by USB instead)

  Libraries: only the ESP32 Arduino core (WiFi, WiFiClientSecure, HTTPClient).
  Copy secrets.example.h to secrets.h and fill it in. secrets.h is git-ignored.
  Not compiled/tested on hardware by the author - see docs "stress points".
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"

// ---- Settings --------------------------------------------------------------
constexpr int      RX_PIN          = 16;        // ESP32 Serial2 RX  (from UNO TX, through the divider)
constexpr int      TX_PIN          = 17;        // unused, kept free
constexpr uint32_t UNO_BAUD        = 9600;      // must match the UNO sketch
constexpr int      CAPACITY        = 25;        // occupancy rate = count / CAPACITY * 100
constexpr uint32_t HEARTBEAT_MS    = 30000;     // re-send the same value this often ("I'm alive")
constexpr uint32_t MIN_GAP_MS      = 2000;      // never upload faster than this
constexpr uint32_t UNO_TIMEOUT_MS  = 60000;     // no line from the UNO for this long => stop heartbeats
constexpr uint32_t RETRY_MS        = 5000;      // wait after a failed upload
constexpr uint32_t TOKEN_LIFE_MS   = 50UL * 60 * 1000;  // Firebase ID tokens last 60 min; renew at 50
constexpr int      STATUS_LED_PIN  = 2;         // on-board LED on most ESP32 dev boards

// ---- State -----------------------------------------------------------------
String   idToken;
uint32_t tokenAt       = 0;
String   lineBuf;
bool     haveData      = false;
bool     dirty         = false;   // value changed and not uploaded yet
int      lastCount     = -1;
float    lastRate      = 0;
uint32_t lastUnoMsgAt  = 0;
uint32_t lastPublishAt = 0;
uint32_t nextTryAt     = 0;
uint32_t lastWifiTry   = 0;

// ---- Small helpers ---------------------------------------------------------
// Pulls the string value of "key" out of a flat JSON reply (avoids needing a JSON library).
String jsonString(const String& json, const char* key) {
  String pat = String("\"") + key + "\"";
  int k = json.indexOf(pat);
  if (k < 0) return "";
  int colon = json.indexOf(':', k + pat.length());
  if (colon < 0) return "";
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return "";
  int q2 = json.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  return json.substring(q1 + 1, q2);
}

void blink(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH); delay(60);
    digitalWrite(STATUS_LED_PIN, LOW);  delay(60);
  }
}

// ---- Wi-Fi -----------------------------------------------------------------
void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi: connecting");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " FAILED (will keep retrying)");
}

// ---- Firebase auth ---------------------------------------------------------
// Signs in with the dedicated device account (email + password) and keeps the ID token.
bool signIn() {
  WiFiClientSecure client;
  client.setInsecure();   // TODO(security): pin the Google root CA instead (see docs, stress points)
  HTTPClient http;
  http.setTimeout(8000);
  String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + FIREBASE_API_KEY;
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"email\":\"") + DEVICE_EMAIL + "\",\"password\":\"" + DEVICE_PASSWORD +
                "\",\"returnSecureToken\":true}";
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("Auth failed: HTTP %d\n", code);
    return false;
  }
  idToken = jsonString(resp, "idToken");
  tokenAt = millis();
  Serial.println(idToken.length() > 0 ? "Auth OK" : "Auth reply had no idToken");
  return idToken.length() > 0;
}

bool tokenValid() {
  return idToken.length() > 0 && (millis() - tokenAt) < TOKEN_LIFE_MS;
}

// ---- Upload ----------------------------------------------------------------
// Replaces /gym with {count, occupancyRate, capacity, updatedAt}. updatedAt is filled in
// by the Firebase server clock ({".sv":"timestamp"}), so the ESP32 needs no real-time clock.
bool publish(int count, float rate) {
  if (!tokenValid() && !signIn()) return false;

  WiFiClientSecure client;
  client.setInsecure();   // TODO(security): see signIn()
  HTTPClient http;
  http.setTimeout(8000);
  String url = String("https://") + FIREBASE_DB_HOST + "/gym.json?auth=" + idToken;
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"count\":") + count +
                ",\"occupancyRate\":" + String(rate, 1) +
                ",\"capacity\":" + CAPACITY +
                ",\"updatedAt\":{\".sv\":\"timestamp\"}}";
  int code = http.PUT(body);
  http.end();

  if (code == 401 || code == 403) idToken = "";   // token rejected -> sign in again next time
  Serial.printf("Upload count=%d rate=%.1f -> HTTP %d\n", count, rate, code);
  return code == 200;
}

// ---- Reading the UNO -------------------------------------------------------
// Accepts "12,48.0" or just "12" (rate is then computed here).
void handleLine(const String& raw) {
  String s = raw;
  s.trim();
  if (s.length() == 0) return;

  int comma = s.indexOf(',');
  String a = (comma < 0) ? s : s.substring(0, comma);
  a.trim();
  if (a.length() == 0 || a.length() > 4) return;
  for (unsigned i = 0; i < a.length(); i++) {
    if (!isDigit(a[i])) return;                    // garbage line -> ignore
  }
  int count = a.toInt();
  if (count < 0 || count > 500) return;

  float rate = count * 100.0f / CAPACITY;          // default: computed here
  if (comma >= 0) {
    float r = s.substring(comma + 1).toFloat();
    if (r >= 0 && r <= 2000) rate = r;             // trust the UNO's value when it is sane
  }

  lastUnoMsgAt = millis();
  if (!haveData || count != lastCount) dirty = true;
  haveData = true;
  lastCount = count;
  lastRate = rate;
}

void readUno() {
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\n') {
      handleLine(lineBuf);
      lineBuf = "";
    } else if (c != '\r') {
      if (lineBuf.length() < 24) lineBuf += c; else lineBuf = "";   // overlong -> discard
    }
  }
}

// ---- Arduino entry points --------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial2.begin(UNO_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  pinMode(STATUS_LED_PIN, OUTPUT);
  Serial.println("\nSLDP 41 ESP32 bridge starting");
  connectWifi();
}

void loop() {
  readUno();

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiTry > 10000) {
      lastWifiTry = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return;
  }

  uint32_t now = millis();
  bool unoAlive  = haveData && (now - lastUnoMsgAt) < UNO_TIMEOUT_MS;
  bool changed   = dirty && (now - lastPublishAt) >= MIN_GAP_MS;
  bool heartbeat = (now - lastPublishAt) >= HEARTBEAT_MS;

  // Heartbeats only while the UNO is still talking, so a dead counter shows as "offline" on the site.
  if (unoAlive && (changed || heartbeat) && (int32_t)(now - nextTryAt) >= 0) {
    if (publish(lastCount, lastRate)) {
      dirty = false;
      lastPublishAt = now;
      blink(1);
    } else {
      nextTryAt = now + RETRY_MS;
    }
  }
}
