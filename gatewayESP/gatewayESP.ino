#include <WiFi.h>
#include <esp_now.h>

typedef struct __attribute__((packed)) { 
  uint16_t co2; 
  float temperature; 
  float humidity; 
  float pressure; 
  int battery; 
} SensorPacket;

typedef struct __attribute__((packed)) {
  char weatherCode[4];
} WeatherPacket;

char weatherIcon[4] = "";
uint8_t receiverMAC[6] = {0x14, 0x2B, 0x2F, 0xDA, 0x02, 0x70};

void setup() {
  Serial.begin(115200);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  
  esp_now_init();
  esp_now_register_recv_cb(onDataRecv); 

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

unsigned long lastSend = 0;
const unsigned long sendInterval = 30000;

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 3) {         
        line.toCharArray(weatherIcon, 4);  
    }
  }

  unsigned long now = millis();
    if (now - lastSend >= sendInterval) {
        lastSend = now;

        WeatherPacket wp;
        strncpy(wp.weatherCode, weatherIcon, 4);
        esp_now_send(receiverMAC, (uint8_t*)&wp, sizeof(wp));
    }
}

void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
  if (len != sizeof(SensorPacket)) {
    return;
  }

  SensorPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  String line = "";

  for (int i = 0; i < 6; i++) {
    if (i > 0) line += ":";
    if (info->src_addr[i] < 16) line += "0"; 
    line += String(info->src_addr[i], HEX);
  }

  line += ",";
  line += String(pkt.co2);
  line += ",";
  line += String(pkt.temperature, 2); 
  line += ",";
  line += String(pkt.humidity, 2);
  line += ",";
  line += String(pkt.pressure / 100.0f, 2);
  line += ",";
  line += String(pkt.battery);

  Serial.println(line);
}