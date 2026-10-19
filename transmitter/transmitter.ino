#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>

// For a classic ESP32 DevKit: button between GPIO18 and GND.
constexpr int BUTTON_PIN = 18;
constexpr uint8_t CHANNEL = 1;
constexpr uint16_t TOTAL = 300;
constexpr uint32_t MAGIC = 0x52464C31;
constexpr uint32_t INTERVAL_MS = 100;
uint8_t broadcastMac[6] = {255, 255, 255, 255, 255, 255};

// Identical packet layout is required on both boards.
struct __attribute__((packed)) Packet {
  uint32_t magic;
  uint32_t run;
  uint16_t sequence;
  uint16_t total;
  uint8_t pressed;
};
static_assert(sizeof(Packet) == 13, "Unexpected packet size");

bool active = false;
uint32_t runId = 0, lastSend = 0;
uint16_t nextSequence = 0, accepted = 0, rejected = 0;
bool lastRaw = false, stableButton = false;
uint32_t changedAt = 0;

void requireOK(esp_err_t result, const char *operation) {
  if (result == ESP_OK) return;
  Serial.printf("ERROR: %s: %s\n", operation, esp_err_to_name(result));
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  requireOK(esp_wifi_set_ps(WIFI_PS_NONE), "disable Wi-Fi power saving");
  requireOK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE), "set channel");
  requireOK(esp_now_init(), "initialize ESP-NOW");
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastMac, 6);
  peer.channel = CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  requireOK(esp_now_add_peer(&peer), "add broadcast peer");
  Serial.println("Ready. Enter s in Serial Monitor to start a 300-packet trial.");
}

void loop() {
  // Debounce the physical switch without stopping packet transmission.
  bool raw = digitalRead(BUTTON_PIN) == LOW;
  if (raw != lastRaw) {
    lastRaw = raw;
    changedAt = millis();
  }
  if (millis() - changedAt >= 25) stableButton = raw;

  while (Serial.available()) {
    char command = Serial.read();
    if ((command == 's' || command == 'S') && !active) {
      runId = esp_random();
      nextSequence = accepted = rejected = 0;
      lastSend = millis() - INTERVAL_MS;
      active = true;
      Serial.printf("START run=%lu, planned packets=%u\n", (unsigned long)runId, TOTAL);
    }
  }

  if (active && millis() - lastSend >= INTERVAL_MS) {
    lastSend = millis();
    Packet p = {MAGIC, runId, nextSequence, TOTAL, (uint8_t)stableButton};
    esp_err_t result = esp_now_send(broadcastMac, (const uint8_t *)&p, sizeof(p));
    // ESP_OK means accepted by the local API, not delivered to the receiver.
    if (result == ESP_OK) accepted++;
    else rejected++;
    nextSequence++;
    if (nextSequence == TOTAL) {
      active = false;
      Serial.printf("DONE run=%lu planned=%u API_accepted=%u API_rejected=%u\n",
                    (unsigned long)runId, TOTAL, accepted, rejected);
      Serial.println("Wait 2 seconds, then enter p on receiver. Enter s here for another trial.");
    }
  }
  delay(1);
}
