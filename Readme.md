# esp32-passive-wifi-monitor

Passive 802.11 management frame sniffer on ESP32-C3. Detects nearby devices via probe requests and beacon frames using promiscuous mode. Alternates between a sniffing phase and an MQTT reporting phase to avoid TCP/promiscuous mode conflicts.

---

## Stack

- **Firmware** — ESP32-C3, Arduino framework
- **Transport** — MQTT over Wi-Fi
- **Broker** — Mosquitto (local PC)
- **Subscriber** — Python + paho-mqtt + SQLite

---

## Setup

### Firmware
1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Add board URL in **File → Preferences**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Install **esp32 by Espressif Systems** via Boards Manager
4. Install libraries: `PubSubClient` and `ArduinoJson`
5. Set **Tools → USB CDC On Boot → Enabled**
6. Fill in your credentials at the top of `wifi_sniffer.ino`:
   ```cpp
   const char* WIFI_SSID     = "YOUR_SSID";
   const char* WIFI_PASSWORD = "YOUR_PASSWORD";
   const char* MQTT_BROKER   = "YOUR_PC_LOCAL_IP";
   ```
7. Flash to board

### Broker
Install Mosquitto, then edit `mosquitto.conf` and add:
```
listener 1883
allow_anonymous true
```
Then restart: `net stop mosquitto && net start mosquitto`

### Subscriber
```
pip install paho-mqtt
python monitor.py
```

---

## Known Issues

- **AP Isolation** — Most home routers block device-to-device traffic. If the ESP32 can't reach the broker, use a mobile hotspot instead.
- **Firewall** — Allow port 1883 inbound on your PC: `netsh advfirewall firewall add rule name="Mosquitto" dir=in action=allow protocol=TCP localport=1883`

---
