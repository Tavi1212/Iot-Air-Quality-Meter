#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include "weather_icons.h"

#define CS   5 
#define DC   17
#define RST  16
#define BUSY 4

#include <Fonts/FreeSansBold18pt7b.h> 
#include <Fonts/FreeSansBold12pt7b.h> 
#include <Fonts/FreeSans9pt7b.h> 

// GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(CS, DC, RST, BUSY));
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(CS, DC, RST, BUSY));
int randNum;

String openWeatherMapApiKey = "";
String city = "Timisoara";
String countryCode = "RO";
String condition = "";
String jsonBuffer;
String iconCode;
const unsigned char* currentIcon = nullptr;

const char* ssid     = " ";
const char* password = " ";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String dayStamp;
String timeStamp;

String lastDayStamp = "";
String lastTimeStamp = "";

char timeBuffer[6]; //HH:MM
char dateBuffer[12]; //MMM:DD:YYYY

enum ScreenMode {
  MAIN_MODE,
  SENSORS_MODE,
};
ScreenMode currentMode = MAIN_MODE; //for multiple screens 

struct SensorData {
  float temperature_C = 19.0;
  float temperature_F = 66.2;
  int co2_ppm = 950;
  int humidity_rh = 70;
  float pressure = 1009.97;
};
SensorData currentData;

void readSensors() {
  currentData.temperature_C = 19.5;
  currentData.temperature_F = (currentData.temperature_C * 9.0 / 5.0) + 32.0;
  currentData.co2_ppm = random(850, 1150); 
  currentData.humidity_rh = random(65, 75); 
  currentData.pressure = 1009.97; 
}

String getC02Message() {
  String message = "Co2 message here.";

  return message;
}

String getHumidityMessage() {
  String message = "Humidity message.";

  return message;
}

String getWarningMessage() {
  String message = "Warning based on data here.";

  return message;
}

const unsigned char* getWeatherIconByCode(const String& iconCodeIn)
{
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

  // Fallback
  return cloudy_bits;
}


void setup() {
  Serial.begin(115200); //baud rate for esp
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;
  jsonBuffer = httpGETRequest(serverPath.c_str());
  Serial.println(jsonBuffer);
  JSONVar myObject = JSON.parse(jsonBuffer);
  
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }
  condition = String((const char*) myObject["weather"][0]["main"]);//extracting info like "Cloudy" "Sunny" etc
  iconCode = String((const char*) myObject["weather"][0]["icon"]);
  currentIcon = getWeatherIconByCode(iconCode); //setting for each type of weather condition an icon
  Serial.print(condition);
// Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(7200);//Romania winter time 
  timeClient.setUpdateInterval(60000);//update every 60 sec

  while (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(100);
  }//forcing first time update

  display.init(115200, false, 10, false); //display.init(1000000);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  formatTimeAndDate();
  // initial frame
  drawFrame(true); // full refresh once

  lastTimeStamp = timeStamp;
  lastDayStamp  = dayStamp;
}

void formatTimeAndDate(){
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = localtime(&epochTime); 
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", ptm);
  strftime(dateBuffer, sizeof(dateBuffer), "%b/%d/%Y", ptm);
  timeStamp = String(timeBuffer);
  dayStamp = String(dateBuffer);
}

String httpGETRequest(const char* serverName) { //helper function - requests the url for the weather conditions
   HTTPClient http;
  http.begin(serverName);
  int httpResponseCode = http.GET();

  String payload = "";

  if (httpResponseCode > 0) {
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

void loop() {
  static uint8_t minuteCounter = 0;

  timeClient.update();
  formatTimeAndDate();

  if (timeStamp != lastTimeStamp || dayStamp != lastDayStamp) {
    Serial.print("DATE: ");
    Serial.println(dayStamp);
    Serial.print("HOUR: ");
    Serial.println(timeStamp);

    minuteCounter++;
    bool doFullRefresh = false;

    // one full refresh every 60 minutes
    if (minuteCounter >= 60) {
      minuteCounter = 0;
      doFullRefresh = true;
    }

    drawFrame(doFullRefresh);   // mostly partial, sometimes full

    lastTimeStamp = timeStamp;
    lastDayStamp  = dayStamp;
  }

  delay(1000);
}

void showNotification(const String &msg, int boxX, int boxY, int boxW, int boxH, int radius){

    display.fillRect(boxX, boxY, boxW, boxH, GxEPD_WHITE);
    display.drawRoundRect(boxX, boxY, boxW, boxH, radius, GxEPD_BLACK);

    display.setTextColor(GxEPD_BLACK);

    display.setCursor(boxX + 8, boxY + 6);
    display.print(msg);
}


void drawFrame(bool fullRefresh) {
  if (fullRefresh) {
    display.setFullWindow();
  } else {
    display.setPartialWindow(0, 0, display.width(), 64);
  }
 // display.setPartialWindow(0, 0, display.width(), 60); // x, y, width, height
  display.firstPage();
  do {
    if (fullRefresh) {
      display.fillScreen(GxEPD_WHITE);
    } else {
      display.fillRect(0, 0, display.width(), 64, GxEPD_WHITE);
    }
    //display.fillScreen(GxEPD_WHITE);//display.fillRect(20, 30, 50, 10, GxEPD_WHITE);
    display.fillRect(0, 0, display.width(), 64, GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);
    display.setTextWrap(false);

    display.setCursor(0, 20);
    display.print(timeStamp);
    if (currentIcon != nullptr) {
    display.drawInvertedBitmap(50, 0, currentIcon, ICON_WIDTH, ICON_HEIGHT, GxEPD_BLACK);
    }
   // display.setFont();   
    display.setCursor(90, 20);
    display.print(dayStamp);
    display.setCursor(220, 20);
    display.print("Bat");

    display.drawLine(0, 35, 250, 35, GxEPD_BLACK);
    //display.setFont();
    display.setCursor(30,50);
    display.print("TEMP");
    display.setCursor(30,70);
    String t = String(currentData.temperature_C) + "°C";
    display.print(t);
    display.drawLine(95, 75, 95, 35, GxEPD_BLACK);
    display.drawInvertedBitmap(0, 40, temperature_bits, ICON_WIDTH, ICON_HEIGHT, GxEPD_BLACK);
    display.setCursor(100,50);
    display.print("CO2");
    display.setCursor(100,70);
    display.print(currentData.co2_ppm);
    display.drawLine(155, 75, 155, 35, GxEPD_BLACK); //second line between co2 and humidity
    display.setCursor(160,50);
    display.print("Humidity");
    display.setCursor(180,70);
    String h = String(currentData.humidity_rh) + " %";
    display.print(h);
    display.drawInvertedBitmap(230, 38, humidity_bits, 16, 16, GxEPD_BLACK);
    display.setCursor(125,70);
    display.setFont();
    display.print(" PPM");
    display.drawLine(0, 75, 250, 75, GxEPD_BLACK); // botoom line
    display.drawInvertedBitmap(0, 81, messages_bits, 17, 17, GxEPD_BLACK);

    // notifications
    String mixedMessage = getC02Message() + getHumidityMessage();
    showNotification(mixedMessage, 20, 80, 230, 20, 8);
    showNotification(getWarningMessage(), 20, 102, 230, 20, 8);

  } while (display.nextPage());
}

