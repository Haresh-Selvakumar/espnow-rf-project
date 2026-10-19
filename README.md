# Wireless RF Control and Link Performance Analysis Using ESP-NOW

## Scope
Two ESP32 development boards form a 2.4 GHz wireless communication link. A button on the transmitter controls an LED on the receiver. A 300-packet trial measures application-level packet delivery ratio and received signal strength (RSSI). No router, internet, external RF module, sensor, or separate antenna is needed for ordinary boards with onboard antennas.

This is an RF communication and link-testing project using integrated radio modules. It does not design an RF oscillator, modulator, matching network, or antenna from discrete components. Confirm that integrated modules satisfy your assignment if discrete RF circuit design is required.

## Parts
* Two classic ESP32 DevKit / ESP32-WROOM-32 development boards, with onboard antennas.
* One normally open pushbutton.
* One LED.
* One 330 ohm resistor (220 ohm to 1 kohm also works for an indicator LED).
* Breadboards and jumper wires.
* Two matching USB DATA cables and USB power sources.
* Laptop with Arduino IDE and the Espressif ESP32 board package.

The code targets Arduino-ESP32 3.x. ESP32-S3 can also run ESP-NOW, but choose the correct board and verify that GPIO18/19 are free; GPIO19 may be used for USB on some variants. These wiring instructions are for a classic ESP32 DevKit, not every ESP32-family board.

Local stock and prices are not verified. If you already have two suitable boards, reuse them. Do not buy bare ESP32-WROOM modules for this breadboard build.

## Wiring

Transmitter:
| Board pin | Connect to |
|---|---|
| GPIO18 | One side of pushbutton |
| GND | Other side of pushbutton |
| USB | Laptop or USB supply |

The code uses INPUT_PULLUP: released reads HIGH and pressed reads LOW. A four-leg tactile switch has two internally connected pairs; connect across the switching contacts, not across an already connected pair. Check continuity with a multimeter if unsure.

Receiver:
| Connection | Destination |
|---|---|
| GPIO19 | 330 ohm resistor |
| Resistor other end | LED anode, normally longer leg |
| LED cathode, normally shorter leg/flat side | GND |
| USB | Laptop or USB supply |

No wire or common ground connection is required BETWEEN the boards. Each board's circuit uses its own ground. GPIOs use 3.3 V logic: never connect a GPIO to 5 V. Power the development boards through USB for this build. Place onboard antennas clear of metal, loose wiring, and breadboard contacts where practical.

## Installation and uploading
1. Install/open Arduino IDE. In Boards Manager install `esp32 by Espressif Systems`, version 3.x. If it does not appear, use Espressif's installation guide linked below to add the official board-manager URL.
2. Select the correct board (normally ESP32 Dev Module for a generic classic ESP32 DevKit) and serial port.
3. Open `receiver/receiver.ino`, upload to the receiver, and open Serial Monitor at 115200 baud. It should say Ready.
4. Open `transmitter/transmitter.ino` and upload to the other board. Mark the physical boards TX and RX to avoid confusing ports.
5. Keep each sketch in the supplied folder with the same name. No third-party ESP-NOW library is required; the headers ship with the ESP32 package.
6. Select the transmitter's Serial Monitor and enter `s`, then Send. A newline setting is fine. It sends 300 numbered packets at approximately 100 ms intervals, finishing in approximately 30 seconds.
7. During the trial, hold the physical button: the receiver LED should light. Release it: the LED should go off. State is sampled in packets; a very quick tap may be missed. Start another trial with `s` for more demonstration time.
8. On completion the transmitter prints DONE, API_accepted and API_rejected. Wait two seconds and enter `p` in the receiver Serial Monitor for final statistics.

One Arduino IDE instance may offer only one active Serial Monitor. Use a second IDE window/instance or switch to the receiver port after starting/completing a trial. Opening a serial port can reset some boards: if switching ports resets your receiver and erases its counters, use a second serial terminal/window kept open throughout the test. Two laptops are also fine. Keep the transmitter connected to a laptop when issuing `s`; this version does not auto-start from a power bank.

## How it works
The ESP32 samples the button, puts the state and a sequence number into a 13-byte application payload, and sends it over its built-in radio. ESP-NOW uses vendor-specific Wi-Fi action frames; it is not a 433 MHz ASK transmitter. Both boards use channel 1. The receiver checks the packet identifier and fields, accepts only the first transmitter it sees until reset, ignores duplicate sequence numbers, and switches the LED.

The transmitter uses broadcast addressing for easy setup, so no MAC address needs to be pasted into the sketch. This demonstration is unencrypted. Source locking is convenience filtering, not authentication. It has no application acknowledgement or retry. Do not interpret API_accepted as delivery confirmation: it means the local sending API accepted the request.

The receive callback copies packets into a queue; the main loop handles LED updates and serial output. This avoids slow printing inside the Wi-Fi callback. A one-second interruption turns the LED off; it also goes off after a trial ends. This is a demonstration failsafe, not a validated safety system.

## Measurement procedure
1. Fix board height and antenna orientation. Keep receiver attached to the laptop and move the transmitter/laptop to the desired position.
2. Reset the receiver BEFORE each measured trial. Keep its serial terminal open and wait for Ready. This makes a completely failed trial distinguishable from an earlier successful one.
3. Enter `s` on the transmitter. Do not reset either board during the run.
4. After DONE, wait two seconds. Enter `p` on the receiver.
5. Record distance, obstacles, unique received packets, average RSSI, API_rejected, and queueDrops. Repeat three times at each position.
6. If zero packets arrive, record received=0, PDR=0%, loss=100%, RSSI=N/A. A receiver cannot calculate RSSI for absent packets.
7. For clean over-the-air comparisons, require API_rejected=0 and queueDrops=0. If either is nonzero, fix/repeat the run rather than attributing every missing packet to the RF channel. queueDrops counts receive-queue overflows since the receiver was reset, so reset it before each measured trial.

Try 1 m, 3 m, 5 m and 10 m in clear line of sight; then repeat a practical distance with one wall. These are experimental positions, not promised working distances. Range depends on board, antenna orientation, obstacles, interference and power supply.

## Calculations
N = 300 planned packet attempts; R = distinct valid packet sequence numbers received.

PDR (%) = 100 R / N.

Application packet loss (%) = 100 (N - R) / N.

Example ONLY: R=285 gives PDR=95% and loss=5%. Never submit the example as measured data.

The code knows the full planned total, so packets lost at the beginning and end are included, provided the transmitter finishes all 300 attempts. While the transmitter is still running, missing packets include those not sent yet; the loss result is not final.

RSSI is received signal strength in dBm. For example, -45 dBm is stronger than -75 dBm. The output's avgRSSI is the arithmetic mean of reported dBm values for received packets; it is not mean linear power, calibrated antenna gain, or a measurement of all attempted packets. Do not call packet loss bit-error rate. No one-way latency is measured because clocks are not synchronized and this build has no acknowledgement-based round-trip measurement.

Suggested observation table:
| Trial | Distance (m) | Obstacle | Received/300 | PDR (%) | Loss (%) | Avg RSSI (dBm) | API rejected | Queue drops |
|---|---|---|---|---|---|---|---|---|
| 1 | 1 | None | | | | | | |
| 2 | 3 | None | | | | | | |
| 3 | 5 | None | | | | | | |
| 4 | 10 | None | | | | | | |
| 5 | 5 | One wall | | | | | | |

## Troubleshooting
| Symptom | Check |
|---|---|
| ESP32 not listed in IDE | Install official Espressif board package |
| No serial port | USB data cable, correct connector, board-specific USB driver |
| Upload will not start | Correct board/port; on boards that need it, hold BOOT while connecting/upload starts |
| esp_now_recv_info_t or callback compile error | Use Arduino-ESP32 3.x; older 2.x has a different callback API |
| Ready but no radio packets | TX trial started with s; identical channel; correct sketches; inspect error output |
| LED never lights | Hold button during active trial; correct GPIOs, switch contacts, LED polarity and resistor |
| LED goes off after 30 seconds | Expected: trial ended and one-second link timeout expired |
| Receiver results disappear | Serial terminal reopened/reset board; keep receiver terminal open |
| ESP32 resets or brownout | Stable USB supply and cable; disconnect other loads |
| Loss rises at distance | Repeat with fixed orientation, clear antennas, same channel and reliable power |

## What to present
* Objective: implement a short-range RF control link and evaluate delivery under distance and obstacle changes.
* Hardware block description and wiring tables.
* Live remote button/LED demonstration.
* Serial screenshots showing actual run IDs, counts and RSSI.
* Observation table and your own measured conclusion.
* Concepts: RF propagation, integrated antennas, packetization, channel selection, sequence numbers, RSSI, packet delivery, interference and timeout behavior.

The control link is simple; the measurements provide the communication-engineering content. Keep today's scope here. Acknowledgements, retry and sensor telemetry are later extensions.

## Verification status
The packet structure, sequence accounting, callback API and wiring were reviewed against the official references. These sketches have NOT been compiled with an ESP32 toolchain or tested on physical boards in this environment. Run the bench demonstration before claiming successful hardware results.

## Official references
* Arduino-ESP32 installation: https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
* Arduino ESP-NOW: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/espnow.html
* ESP-IDF ESP-NOW frame format, channels and callbacks: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html
