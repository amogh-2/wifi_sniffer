#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID     = "YOUR SSID";
const char* WIFI_PASSWORD = "YOUR PASSWORD";
const char* MQTT_BROKER   = "BROKER'S IP ADDRESS";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "wifi/monitor/devices";
const char* MQTT_CLIENT   = "esp32-sniffer";

const int HOP_INTERVAL_MS = 500;
const unsigned long SNIFF_DURATION_MS = 15000; 


#define MAX_DEVICES 50
struct ScannedDevice {
  uint8_t mac[6];
  int8_t  rssi;
  uint8_t type;
  uint8_t channel;
};

ScannedDevice deviceBuffer[MAX_DEVICES];
int deviceCount = 0;

enum SystemState { STATE_SNIFFING, STATE_REPORTING };
SystemState currentState = STATE_SNIFFING;
unsigned long stateTimer = 0;


WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

volatile bool   frameReady = false;
volatile uint8_t frameMac[6];
volatile int8_t  frameRssi;
volatile uint8_t frameType;

uint8_t currentChannel = 1;
unsigned long lastHop  = 0;


typedef struct {
  unsigned frame_ctrl  : 16;
  unsigned duration    : 16;
  uint8_t  addr1[6];
  uint8_t  addr2[6];
  uint8_t  addr3[6];
  unsigned seq_ctrl    : 16;
  uint8_t  payload[];
} __attribute__((packed)) wifi_mgmt_hdr_t;

#define FC_SUBTYPE_MASK      0x00F0
#define FC_SUBTYPE_PROBE_REQ  0x0040
#define FC_SUBTYPE_BEACON     0x0080

-
void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  wifi_mgmt_hdr_t* hdr = (wifi_mgmt_hdr_t*)pkt->payload;
  
  uint16_t fc      = hdr->frame_ctrl;
  uint16_t subtype = fc & FC_SUBTYPE_MASK;
  
  bool isProbe  = (subtype == FC_SUBTYPE_PROBE_REQ);
  bool isBeacon = (subtype == FC_SUBTYPE_BEACON);
  
  if (!isProbe && !isBeacon) return;
  if (frameReady) return; 
  
  memcpy((void*)frameMac, hdr->addr2, 6);
  frameRssi  = pkt->rx_ctrl.rssi;
  frameType  = isProbe ? 0 : 1;
  frameReady = true;
}

String macToString(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool isRandomizedMac(const uint8_t mac[6]) {
  return (mac[0] & 0x02) != 0;
}

void hopChannel() {
  currentChannel++;
  if (currentChannel > 13) currentChannel = 1;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

void addDeviceToBuffer(const uint8_t* mac, int8_t rssi, uint8_t type, uint8_t channel) {

  for (int i = 0; i < deviceCount; i++) {
    if (memcmp(deviceBuffer[i].mac, mac, 6) == 0) {
      deviceBuffer[i].rssi = rssi; 
      deviceBuffer[i].channel = channel;
      return;
    }
  }
  
  
  if (deviceCount < MAX_DEVICES) {
    memcpy(deviceBuffer[deviceCount].mac, mac, 6);
    deviceBuffer[deviceCount].rssi = rssi;
    deviceBuffer[deviceCount].type = type;
    deviceBuffer[deviceCount].channel = channel;
    deviceCount++;
  }
}

void connectToWiFi() {
  Serial.print("[WiFi] Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startAttempt = millis();
  
 
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Connection Timeout!");
  }
}

bool connectMqtt() {
  if (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting to broker...");
    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println(" Connected.");
      return true;
    } else {
      Serial.printf(" FAILED (rc=%d)\n", mqtt.state());
      return false;
    }
  }
  return true;
}

void runNetworkTest() {
  Serial.println("[Test] Pinging Network Targets...");
  WiFiClient testClient;
  testClient.setTimeout(3000);
  
  if (testClient.connect("8.8.8.8", 80)) {
    Serial.println("[Test] Internet reachable");
    testClient.stop();
  } else {
    Serial.println("[Test] Internet UNREACHABLE (Expected if local hotspot lacks data)");
  }
  
  if (testClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.println("[Test] Broker TCP Port reachable! Network path is clear.");
    testClient.stop();
  } else {
    Serial.println("[Test] CRITICAL: Broker NOT reachable. Check your Broker Config/Firewall.");
  }
}


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Wi-Fi Monitor ===");


  WiFi.mode(WIFI_STA);
  connectToWiFi();
  
 
  if (WiFi.status() == WL_CONNECTED) {
    runNetworkTest();
  }
  
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setSocketTimeout(10);

 
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  e
  Serial.println("\n[System] Starting Sniffing Phase...");
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  currentState = STATE_SNIFFING;
  stateTimer = millis();
}

void loop() {
  
 
  if (currentState == STATE_SNIFFING) {
    
    
    if (millis() - lastHop > HOP_INTERVAL_MS) {
      hopChannel();
      lastHop = millis();
    }
    
    if (frameReady) {
      uint8_t mac[6];
      int8_t  rssi;
      uint8_t type;
      
      noInterrupts();
      memcpy(mac, (void*)frameMac, 6);
      rssi       = frameRssi;
      type       = frameType;
      frameReady = false;
      interrupts();
      
      addDeviceToBuffer(mac, rssi, type, currentChannel);
    }
    
   
    if (millis() - stateTimer >= SNIFF_DURATION_MS) {
      Serial.printf("\n[System] Sniffing complete. Captured %d unique devices. Switching to Report Mode.\n", deviceCount);
      esp_wifi_set_promiscuous(false); 
      currentState = STATE_REPORTING;
      stateTimer = millis();
    }
  }
  
 
  else if (currentState == STATE_REPORTING) {
    connectToWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
      if (connectMqtt()) {
        Serial.printf("[MQTT] Transmitting %d payloads...\n", deviceCount);
        
        
        for (int i = 0; i < deviceCount; i++) {
          StaticJsonDocument<200> doc;
          doc["mac"]        = macToString(deviceBuffer[i].mac);
          doc["rssi"]       = deviceBuffer[i].rssi;
          doc["type"]       = (deviceBuffer[i].type == 0) ? "probe" : "beacon";
          doc["randomized"] = isRandomizedMac(deviceBuffer[i].mac);
          doc["channel"]    = deviceBuffer[i].channel;
          doc["ts"]         = millis();
          
          char payload[200];
          serializeJson(doc, payload);
          mqtt.publish(MQTT_TOPIC, payload);
          Serial.printf(" [TX] %s\n", payload);
          delay(50);
        }
        
        mqtt.disconnect();
      }
    } else {
      Serial.println("[System] Transmission skipped because Wi-Fi could not connect.");
    }
    
  
    deviceCount = 0;
    WiFi.disconnect(); 
    
    Serial.println("[System] Re-entering Sniffing Phase...");
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    
    currentState = STATE_SNIFFING;
    stateTimer = millis();
  }
}