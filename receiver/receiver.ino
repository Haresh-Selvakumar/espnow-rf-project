#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// For a classic ESP32 DevKit: GPIO19 -> 330 ohm -> LED anode; cathode -> GND.
constexpr int LED_PIN = 19;
constexpr uint8_t CHANNEL = 1;
constexpr uint16_t TOTAL = 300;
constexpr uint32_t MAGIC = 0x52464C31;
struct __attribute__((packed)) Packet {
  uint32_t magic;
  uint32_t run;
  uint16_t sequence;
  uint16_t total;
  uint8_t pressed;
};
static_assert(sizeof(Packet) == 13, "Unexpected packet size");
struct Event {
  Packet packet;
  int8_t rssi;
  uint8_t source[6];
};
QueueHandle_t events;
bool haveRun = false, lockedSource = false;
uint8_t sourceMac[6] = {};
bool seen[TOTAL] = {};
uint32_t runId = 0, lastRx = 0;
uint16_t received = 0, latestSequence = 0;
int32_t rssiSum = 0;
int8_t lastRssi = 0;
// Only the Wi-Fi callback writes this counter.
volatile uint32_t queueDrops = 0;

void requireOK(esp_err_t result, const char *operation) {
  if (result == ESP_OK) return;
  Serial.printf("ERROR: %s: %s\n", operation, esp_err_to_name(result));
  while (true) delay(1000);
}

// Arduino-ESP32 3.x callback. Keep it short: copy data to a queue.
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
  if (!info || !info->rx_ctrl || length != sizeof(Packet)) return;
  Event e = {};
  memcpy(&e.packet, data, sizeof(Packet));
  if (e.packet.magic != MAGIC || e.packet.total != TOTAL ||
      e.packet.sequence >= TOTAL || e.packet.pressed > 1) return;
  e.rssi = info->rx_ctrl->rssi;
  memcpy(e.source, info->src_addr, 6);
  if (xQueueSend(events, &e, 0) != pdTRUE) queueDrops++;
}

void printStatistics() {
  if (!haveRun) {
    Serial.println("No packets received. If transmitter completed 300 attempts, record 0/300; RSSI unavailable.");
    return;
  }
  Serial.printf("run=%lu received=%u/300 missing=%u PDR=%.2f%% loss=%.2f%% avgRSSI=%.1f dBm queueDrops=%lu\n",
    (unsigned long)runId, received, TOTAL - received,
    100.0f * received / TOTAL, 100.0f * (TOTAL - received) / TOTAL,
    (float)rssiSum / received, (unsigned long)queueDrops);
  Serial.println("Missing/loss values are FINAL only after the transmitter finishes the trial.");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  events = xQueueCreate(32, sizeof(Event));
  if (!events) {
    Serial.println("ERROR: cannot create receive queue");
    while (true) delay(1000);
  }
  WiFi.mode(WIFI_STA);
  requireOK(esp_wifi_set_ps(WIFI_PS_NONE), "disable Wi-Fi power saving");
  requireOK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE), "set channel");
  requireOK(esp_now_init(), "initialize ESP-NOW");
  requireOK(esp_now_register_recv_cb(onReceive), "register receive callback");
  Serial.println("Ready. First valid transmitter is locked until receiver reset.");
  Serial.println("Enter p for statistics; reset receiver before each measured trial.");
}

void loop() {
  Event e;
  while (xQueueReceive(events, &e, 0) == pdTRUE) {
    if (!lockedSource) {
      memcpy(sourceMac, e.source, 6);
      lockedSource = true;
    }
    if (memcmp(sourceMac, e.source, 6) != 0) continue;
    if (!haveRun || e.packet.run != runId) {
      haveRun = true;
      runId = e.packet.run;
      memset(seen, 0, sizeof(seen));
      received = 0;
      rssiSum = 0;
      latestSequence = 0;
      Serial.printf("NEW run=%lu\n", (unsigned long)runId);
    }
    uint16_t sequence = e.packet.sequence;
    if (seen[sequence]) continue;  // Count only distinct packet numbers.
    seen[sequence] = true;
    received++;
    rssiSum += e.rssi;
    lastRssi = e.rssi;
    if (received == 1 || sequence >= latestSequence) {
      latestSequence = sequence;
      digitalWrite(LED_PIN, e.packet.pressed ? HIGH : LOW);
      lastRx = millis();
    }
    if (received == 1 || received % 20 == 0 || sequence == TOTAL - 1) {
      Serial.printf("seq=%u button=%u RSSI=%d dBm unique=%u/300\n",
                    sequence, e.packet.pressed, lastRssi, received);
    }
  }
  // Demonstration failsafe: an interrupted link turns the LED off.
  if (haveRun && millis() - lastRx > 1000) digitalWrite(LED_PIN, LOW);
  while (Serial.available()) {
    char command = Serial.read();
    if (command == 'p' || command == 'P') printStatistics();
  }
  delay(1);
}
