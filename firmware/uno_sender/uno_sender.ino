/*
  SLDP 41 - Gym occupancy: UNO side of the link to the ESP32

  Integration for the counting code: call  publishIfNeeded(peopleCount);  once per loop().
  It sends "<count>,<rate>\n" when the count changes and also every 10 s as a heartbeat.

  Wiring: UNO D11 (TX) -> 1k -> ESP32 GPIO16, with 2k from that junction to GND, and a common GND.
  D10 is declared as RX only because SoftwareSerial needs a pin; nothing is connected to it.
  If D10/D11 are already used by the sensors/LCD, pick any two free pins and change below.

  DEMO_MODE = 1 fakes a count that walks 0..28 so the whole chain can be tested before the
  real counting code exists. Set it to 0 when integrating the real counting code.
*/

#include <SoftwareSerial.h>

#define DEMO_MODE 1

const byte ESP_RX_PIN = 10;               // unused
const byte ESP_TX_PIN = 11;               // -> divider -> ESP32 GPIO16
SoftwareSerial esp(ESP_RX_PIN, ESP_TX_PIN);

const int           CAPACITY     = 25;     // must match the ESP32 and the website
const unsigned long HEARTBEAT_MS = 10000;  // ESP32 treats 60 s of silence as "counter dead"

unsigned long lastSentAt    = 0;
int           lastSentCount = -1;

void sendOccupancy(int count) {
  float rate = count * 100.0 / CAPACITY;
  esp.print(count);
  esp.print(',');
  esp.println(rate, 1);                    // e.g. "12,48.0"
}

void publishIfNeeded(int count) {
  if (count != lastSentCount || millis() - lastSentAt >= HEARTBEAT_MS) {
    sendOccupancy(count);
    lastSentCount = count;
    lastSentAt = millis();
  }
}

// ---------------------------------------------------------------------------
int demoCount = 0;
int demoStep  = 1;
unsigned long demoAt = 0;

void setup() {
  Serial.begin(9600);       // USB debugging
  esp.begin(9600);          // must equal UNO_BAUD in the ESP32 sketch
}

void loop() {
#if DEMO_MODE
  if (millis() - demoAt >= 4000) {          // change the fake count every 4 s
    demoAt = millis();
    demoCount += demoStep;
    if (demoCount >= 28 || demoCount <= 0) demoStep = -demoStep;
  }
  publishIfNeeded(demoCount);
#else
  // int peopleCount = ...;   // <- the real counting code sets this
  // publishIfNeeded(peopleCount);
#endif
}
