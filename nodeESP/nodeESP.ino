#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <GxEPD2_BW.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_BME280.h>
#include "figures.h"

#include <Fonts/FreeSansBold18pt7b.h> 
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define CS   5 
#define DC   17
#define RST  16
#define BUSY 4

#define BAT_ADC_PIN 32
#define ADC_MAX 4095.0
#define ADC_REF 3.3

int batteryLevel = 0;

char weatherCode[3];
String weatherString = "\n";
volatile bool newIconRecieved = false;

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(CS, DC, RST, BUSY));
uint8_t peerMac[] = { 0x88, 0x13, 0xBF, 0xC8, 0x6E, 0xAC };
SensirionI2cScd4x scd4x;
Adafruit_BME280 bme;


struct SensorData {
  uint16_t co2_ppm    = 0;
  float temperature_C = 0;
  float humidity_rh   = 0;
  float pressure      = 0;
  int battery         = 0;
};
SensorData currentData;


typedef struct __attribute__((packed)) {
  uint16_t co2;
  float temperature;
  float humidity;
  float pressure;
  int   battery;
} SensorPacket;

//------------------ SETUP/LOOP BLOCK ---------------------
void setup() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  initSensors();
  initDisplay();
  setupEspNow();

  drawFrame();
}

void loop() {

  takeMeasurements();
  
  refreshTemperature();
  refreshCO2();
  refreshHumidity();
  refreshWarnings();
  refreshBattery();

  if(newIconRecieved){
    newIconRecieved = false;
    refreshWeatherIcon(getWeatherIconByCode(String(weatherCode)));
  } else {
    refreshWeatherIcon(nullptr);
  }

  sendSensorData();

  delay(30000);
}

//------------------ INIT FUNCTIONS -----------------------
void initSensors(){
  Wire.begin();

  
  // bme.setSampling(Adafruit_BME280::MODE_FORCED,
  //                 Adafruit_BME280::SAMPLING_X1,
  //                 Adafruit_BME280::SAMPLING_X1,
  //                 Adafruit_BME280::SAMPLING_X1,
  //                 Adafruit_BME280::FILTER_OFF
  //                 );
  bme.begin(0x76);

  scd4x.begin(Wire, 0x62);
  scd4x.stopPeriodicMeasurement();
  scd4x.startLowPowerPeriodicMeasurement();
}

void initDisplay(){
  display.init(115200, false, 10, false);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_now_init();
  esp_now_register_recv_cb(onEspNowReceive);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = 0;

  esp_now_add_peer(&peer);
}
//------------------ HELPER FUNCTIONS ---------------------
void takeMeasurements(){
  scd4x.readMeasurement(currentData.co2_ppm, currentData.temperature_C, currentData.humidity_rh);

  currentData.temperature_C = bme.readTemperature();     
  currentData.humidity_rh   = bme.readHumidity();        
  currentData.pressure      = bme.readPressure() / 100.0;

  int raw = analogRead(BAT_ADC_PIN);
  determineBatteryLevel(raw);
}

void determineBatteryLevel(int raw) {
  if (raw > 2120) batteryLevel = 5;
  else if (raw > 2030) batteryLevel = 4;
  else if (raw > 1970) batteryLevel = 3;
  else if (raw > 1870) batteryLevel = 2;
  else if (raw > 1730) batteryLevel = 1;
  else batteryLevel = 0;
}

const unsigned char* getWeatherIconByCode(const String& iconCodeIn){
  String code = iconCodeIn;
  code.toLowerCase(); 

  // 01d / 01n : clear sky
  if (code == "01d") return sunny_bits;
  if (code == "01n") return clear_night_bits;

  // 02d / 02n : few clouds
  if (code == "02d") return partly_cloudy_bits;
  if (code == "02n") return partly_cloudy_night_bits;

  // 03d / 03n : scattered clouds
  // 04d / 04n : broken clouds
  if (code == "03d" || code == "03n" ||
      code == "04d" || code == "04n")
    return cloudy_bits;

  // 09d / 09n : shower rain  -> drizzle icon
  if (code == "09d") return drizzle_bits;
  if (code == "09n") return drizzle_bits; 

  // 10d / 10n : rain
  if (code == "10d") return rain_thunderstorm_bits; 
  if (code == "10n") return rain_night_bits;

  // 11d / 11n : thunderstorm
  if (code == "11d" || code == "11n")
    return sever_thunderstorm_bits;      

  // 13d / 13n : snow
  if (code == "13d" || code == "13n")
    return snow_bits;

  // 50d / 50n : mist/fog
  if (code == "50d" || code == "50n")
    return fog_bits;

  // 
  return NULL;
}

int getCenteredX(const String &text, int boxX, int boxWidth) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return boxX + (boxWidth - w) / 2;
}

//---------------------- FRAME TEMPLATE DRAWING ---------------
void drawFrame() {
    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        constexpr int BAT_W = 32;
        constexpr int BAT_H = 14;
        constexpr int MARGIN = 4;

        int xBat = display.width() - BAT_W - MARGIN;
        int yBat = MARGIN;

        display.drawBitmap(xBat, yBat, battery_bits, BAT_W, BAT_H, GxEPD_BLACK);

        constexpr int TOP_H = 24;
        int colW = display.width() / 3;

        int baseLine = TOP_H + 46;
        int labelLine = baseLine - 15;

        display.setFont(&FreeSans9pt7b);

        display.setCursor(30, labelLine);  display.print("TEMP");
        display.setCursor(105, labelLine); display.print("CO2");
        display.setCursor(175, labelLine); display.print("HUM");

    } while (display.nextPage());
}

//----------------------- REFRESH ELEMENTS --------------------

constexpr int BOX_Y = 60;   // Box top
constexpr int BOX_H = 60;   // Box height
constexpr int BOX_W = 70;   // Bod width

void refreshTemperature() {
    int boxX = 20;
    int boxY = BOX_Y;
    display.setPartialWindow(boxX, boxY, BOX_W, BOX_H);
    display.firstPage();

    String tempStr = String(currentData.temperature_C, 1);

    do {
        display.fillRect(boxX, boxY, BOX_W, BOX_H, GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);

        int x = getCenteredX(tempStr, boxX, BOX_W);
        int y = boxY + 20;

        display.setCursor(x, y);
        display.print(tempStr);

        display.setFont(&FreeSans9pt7b);
        display.setCursor(50, y + 20);
        display.print("°C");

    } while (display.nextPage());
}

void refreshCO2() {
    int boxX = 90;
    int boxY = BOX_Y;
    display.setPartialWindow(boxX, boxY, BOX_W, BOX_H);
    display.firstPage();

    String co2Str = String(currentData.co2_ppm);

    do {
        display.fillRect(boxX, boxY, BOX_W, BOX_H, GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);

        int x = getCenteredX(co2Str, boxX, BOX_W);
        int y = boxY + 20;

        display.setCursor(x, y);
        display.print(co2Str);

        display.drawLine(boxX, boxY + 2, boxX, boxY + 42, GxEPD_BLACK);         //Vertical Left

        display.setFont(&FreeSans9pt7b);
        display.setCursor(110, y + 20);
        display.print("ppm");

    } while (display.nextPage());
}

void refreshHumidity() {
    int boxX = 160;
    int boxY = BOX_Y;
    display.setPartialWindow(boxX, boxY, BOX_W, BOX_H);
    display.firstPage();

    String humStr = String(currentData.humidity_rh, 1);

    do {
        display.fillRect(boxX, boxY, BOX_W, BOX_H, GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);

        int x = getCenteredX(humStr, boxX, BOX_W);
        int y = boxY + 20;

        display.setCursor(x, y);
        display.print(humStr);

        display.drawLine(boxX, boxY, boxX, boxY + 40, GxEPD_BLACK);         

        display.setFont(&FreeSans9pt7b);
        display.setCursor(190, y + 20);
        display.print("%");

    } while (display.nextPage());
}

bool co2Warning = false;
bool humWarning = false;

void refreshWarnings() {
  if(currentData.co2_ppm > 999){
    if(co2Warning == false){
      co2Warning = true;
      display.setPartialWindow(145, 42, 16, 16);
      display.firstPage();
      do{
        display.drawBitmap(145, 42, warning_bits, 16, 16, GxEPD_BLACK);
      } while(display.nextPage());
    }
  } else{
    if(co2Warning == true){
      co2Warning = false;
      display.setPartialWindow(145, 42, 16, 16);
      display.firstPage();
      do{
        display.fillRect(145, 42, 16, 16, GxEPD_WHITE);
      } while(display.nextPage());
    }
  }
    
  if(currentData.humidity_rh < 40 || currentData.humidity_rh > 50){
    if(humWarning == false){
      humWarning = true;
      display.setPartialWindow(217, 42, 16, 16);
      display.firstPage();
      do{
        display.drawBitmap(217, 42, warning_bits, 16, 16, GxEPD_BLACK);
      } while(display.nextPage());

    }
  } else{
    if(humWarning == true){
      humWarning = false;
      display.setPartialWindow(217, 42, 16, 16);
      display.firstPage();
      do{
        display.fillRect(217, 42, 16, 16, GxEPD_WHITE);
      } while(display.nextPage());
    }
  }
}

void refreshBattery() {
  constexpr int BAT_W = 32;
  constexpr int BAT_H = 14;
  constexpr int MARGIN = 4;

  int xBat = display.width() - BAT_W - MARGIN;
  int yBat = MARGIN;

  display.setPartialWindow(xBat, yBat, BAT_W, BAT_H);
  display.firstPage();

  int levelCopy = batteryLevel;

  do {
    display.fillRect(xBat, yBat, BAT_W, BAT_H, GxEPD_WHITE);
    display.drawBitmap(xBat, yBat, battery_bits, BAT_W, BAT_H, GxEPD_BLACK);

    int x = xBat + 5;
    int y = yBat + 2;

    while (levelCopy > 0) {
      display.fillRect(x, y, 3, 10, GxEPD_BLACK);
      x += 4;
      levelCopy--;
    }

  } while (display.nextPage());
}

void refreshWeatherIcon(const unsigned char* icon) {

  int iconX = 180;
  int iconY = 0;

  display.setPartialWindow(iconX, iconY, ICON_WIDTH, ICON_HEIGHT);
  display.firstPage();

  do{
      display.fillRect(iconX, iconY, ICON_WIDTH, ICON_HEIGHT, GxEPD_WHITE);

      if (icon != nullptr) {
          display.drawInvertedBitmap(iconX, iconY, icon, ICON_WIDTH, ICON_HEIGHT, GxEPD_BLACK);
      }
  } while (display.nextPage());
}

//---------------------- ESP-NOW COMMUNICATION ---------------
void sendSensorData() {
    SensorPacket pkt;
    pkt.co2         = currentData.co2_ppm;    
    pkt.temperature = currentData.temperature_C;
    pkt.humidity    = currentData.humidity_rh;
    pkt.pressure    = currentData.pressure;
    pkt.battery     = batteryLevel;

    esp_err_t result = esp_now_send(peerMac, (uint8_t*)&pkt, sizeof(pkt));
}

void onEspNowReceive(const esp_now_recv_info_t*, const uint8_t* data, int len) {
  if (len < 3) return;

  weatherCode[0] = (char)data[0];
  weatherCode[1] = (char)data[1];
  weatherCode[2] = (char)data[2];

  newIconRecieved = true;
}

