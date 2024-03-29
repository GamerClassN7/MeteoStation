//Includes
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

//#include <DNSServer.h>

//Constant
#define LIGHT_PIN A0
#define DHT_PIN 4
#define LED_BUILTIN 16
// #define BATT_PIN A0
#define DHT_TYPE DHT11

//Variables
String ssid = "";
String pasw = "";
String server = "";
String hwId = "";
String url = "";

//Settings
bool deepSleepOn = true;
long sleepTime = 4;  //in minutes
bool lightSensor = true;
const IPAddress apIP(192, 168, 1, 1);
const IPAddress mask(255, 255, 255, 0);
String apSSID = "Node Meter";

//Aliasses
#if defined(DHT_PIN)
DHT dht(DHT_PIN, DHT_TYPE);
#endif
ESP8266WebServer webServer(80);

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);
  delay(10);

  #if defined(LIGH_TPIN)
  pinMode(LIGH_TPIN, INPUT);
  #endif
  #if defined(BATT_PIN)
  pinMode(BATT_PIN, INPUT);
  #endif
  
  Serial.println(F("HW: ")); 
  Serial.print(String(hwId));
  sleepTime = sleepTime * 60000;
  
  #if defined(DHT_PIN)
  dht.begin();
  #endif
}

void loop() {
  // WI-FI CONECTING
  Serial.print(F("WIFI SSID: "));
  Serial.println(String(ssid));
  Serial.print(F("SLEEP INTERVAL: "));
  Serial.println(String(sleepTime));
  
  bool restorSuccesful = restorSetting();
  
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.forceSleepWake();
  WiFi.begin(ssid, pasw);
  
  if (!checkConnection() && restorSuccesful) {
    scanWifi();
    setupWebServer();
    webServer.begin();
    while (true){  
      Serial.println(F("CONECTION SETTING LOOP"));
      webServer.handleClient();
    }
  }
  
  Serial.println(F("CONECTED TO WIFI"));
  Serial.print("IP: ");
  Serial.println(WiFi.localIP().toString());
  
  WiFiClient client;
  HTTPClient http;
  
  
  
  StaticJsonDocument<1024> jsonBuffer;
  jsonBuffer["token"] = hwId;
  #if defined(DHT_PIN)
    jsonBuffer["values"]["temp"]["value"] = String(getTemperature());
    jsonBuffer["values"]["temp"]["unit"] = "C";
    jsonBuffer["values"]["humi"]["value"] = String(getHumidity());
    jsonBuffer["values"]["humi"]["unit"] = "%";
  #endif
  
  #if defined(LIGHT_PIN)
    jsonBuffer["values"]["light"]["value"] = String(getLight());
    jsonBuffer["values"]["light"]["unit"] = "";
  #endif
  
  #if defined(BATT_PIN)
    jsonBuffer["values"]["battery"]["value"] = String(getBattery());
    jsonBuffer["values"]["battery"]["unit"] = "";
  #endif
  
  String requestJson = "";
  serializeJson(jsonBuffer, requestJson);
  Serial.print(F("JSON: ")); 
  Serial.println(requestJson);
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Content-Length", "2000");
  
  int httpCode = http.POST((String) requestJson);
  String payload = http.getString();  //Get the response payload
  jsonBuffer.clear();
  
  deserializeJson(jsonBuffer, payload);
  
  String hostname = jsonBuffer["device"]["hostname"];
  sleepTime = jsonBuffer["device"]["sleepTime"];
  jsonBuffer.clear();
  WiFi.hostname(hostname);
  
  Serial.print(F("HTTP CODE: "));
  Serial.println(String(httpCode)); //Print HTTP return code
  Serial.print(F("HTTP BODY: "));
  Serial.println(String(payload));  //Print request response payload
  http.end();  //Close connection
  Serial.println(F("DISCONECTED FROM WIFI"));
  WiFi.disconnect();
  
  if (deepSleepOn) {
    Serial.print(F("GOING TO SLEEP FOR ")); 
    Serial.println(String(sleepTime));
    ESP.deepSleep((sleepTime * 60) * 1000000, RF_DEFAULT);  // 20e6 is 20 microseconds
    delay(1000);
  } else {
    delay(1000);
    delay(sleepTime);
  }
}

#if defined(DHT_PIN)
float getTemperature() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println(F("Failed to read temperature from sensor!")) ;
    return 999;
  }
  return t;
}

float getHumidity() {
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println(F("Failed to read humidity from sensor!"));
    return 999;
  }
  return h;
}
#endif

#if defined(LIGHT_PIN)
int getLight() {
  int l = digitalRead(LIGHT_PIN);
  return l;
  if (l > 1000) {
    return 1;
  } else {
    return 0;
  }
  Serial.println(F("Failed to read light from sensor!"));
  return 999;
}
#endif

#if defined(BATT_PIN)
float getBattery() {
  float l = analogRead(BATT_PIN);
  float volts = 0;
  
  int sampleNumper = 100;
  for (int x = 0; x <= sampleNumper; x++) {
    volts = volts + (((l * 3.22265625) *2) /1000);
  }
  
  return volts/100;
}
#endif

bool checkConnection() {
  int count = 0;
  Serial.println(F("Waiting for Wi-Fi connection"));
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    Serial.print(F("."));
    count++;
  }
  Serial.println(F("Timed out."));
  return false;
}

bool restorSetting() {
  Serial.println("Reading EEPROM");
  ssid = "";
  pasw = "";
  hwId = "";
  url = "";
  if (EEPROM.read(0) != 0) {
    Serial.println(F("Reading EEPROM"));
    for (int i = 0; i < 64; ++i) {
      ssid += char(EEPROM.read(i));
    }
    if (ssid == "") return false;
    Serial.print(F("SSID: "));
    Serial.println(String(ssid));
    for (int i = 64; i < 128; ++i) {
      pasw += char(EEPROM.read(i));
    }
    if (pasw == "") return false;
    Serial.print(F("PASS: ")); 
    Serial.println(String(pasw));
    for (int i = 128; i < 192; ++i) {
      hwId += char(EEPROM.read(i)); 
    }
    if (hwId == "") return false;
    Serial.print(F("TOKEN: "));
    Serial.println(String(hwId));
    for (int i = 192; i < 256; ++i) {
      url += char(EEPROM.read(i));
    }
    if (url == "") return false;
    Serial.print(F("URL: "));
    Serial.println(String(url));
    return true;
  } else {
    return false;
  }
}


void setupMode(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

String scanWifi(){
  String wifiList = "";
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println(F("scan done"));
  if (n == 0) {
    Serial.println(F("no networks found"));
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      //Serial.println("SSID: " + WiFi.SSID(i));
      wifiList = wifiList + F("<option value=\"");
      wifiList = wifiList + WiFi.SSID(i);
      wifiList = wifiList + F("\">");
      wifiList = wifiList + WiFi.SSID(i);
      wifiList = wifiList + F("</option>");
      delay(10);
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, mask);
    WiFi.softAP(apSSID);
    //dnsServer.start(53, "*", apIP);
    Serial.print(F("Starting Access Point at "));
    Serial.println(apSSID);
  }
  return wifiList;
}

void setupWebServer(){
  webServer.on("/setap", []() {
    ssid = webServer.arg("ssid");
    pasw = webServer.arg("pasw");
    hwId = webServer.arg("token");
    url = webServer.arg("url");
    
    for (int i = 0; i < 256; ++i) {
      EEPROM.write(i, 0);
    }
    
    Serial.println(F("Writing EEPROM..."));
    Serial.print(F("SSID:"));
    Serial.println(ssid);
    
    for (int i = 0; i < ssid.length(); ++i) {
      EEPROM.write(i,ssid[i]);
    }
    
    Serial.print(F("PASW:"));
    Serial.println(pasw);
    
    for (int i = 0; i < pasw.length(); ++i) {
      EEPROM.write(64 + i, pasw[i]);
    }
    
    Serial.print(F("TOKEN:"));
    Serial.println(hwId);
    
    for (int i = 0; i < hwId.length(); ++i) {
      EEPROM.write(128 + i, hwId[i]);
    }
    
    Serial.print(F("URL:")); 
    Serial.println(String(url));
    
    for (int i = 0; i < url.length(); ++i) {
      EEPROM.write(192 + i, url[i]);
    }
    EEPROM.commit();
    
    Serial.println(F("Write EEPROM done!"));
    String s = F("<h1>Setup complete.</h1><p>device will be connected to \"");
    s += ssid;
    s += F("\" after the restart.");
    webServer.send(200, "text/html", s);
    delay(1000);
    ESP.restart();
  });
  webServer.on("/restart", []() {
    ESP.restart();
  });
  webServer.onNotFound([]() {
    String s = F("<h1>"); 
    s += hwId; 
    s += F("</h1>");
    s += F("<a href=\"/restart\"><p>Restartovat</p><a/>");
    s += F("<a href=\"/setting\"><WIFI Network Setting</p><a/>");
    
    #if defined(DHT_PIN)
    s += F("<p>Temperature: ");
    s += String(getTemperature()) + F(" C</p>");
    s += F("<p>Humidity: ");
    s += String(getHumidity())+ F(" %</p>");
    #endif 
    
    #if defined(LIGHT_PIN)
    s += F("<p>Light: "); 
    s += String(getLight()); 
    s += F(" </p>");
    #endif
    
    #if defined(BATT_PIN)
    s += F("<p>Battery: ");
    s += String(getBattery());
    s += F("V </p>");
    #endif  
    
    webServer.send(200, "text/html", makePage("AP mode", s));
  });
  webServer.on("/setting", []() {
    String s = F("<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>");
    s += F("<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">");
    s += scanWifi();
    s += F("</select><br>Password: <input name=\"pasw\" length=64 type=\"password\">");
    s += F("<br>Password: <input name=\"pasw\" length=64 type=\"password\">");
    s += F("<br>Token: <input name=\"token\" length=64 type=\"text\">");
    s += F("<br>Api Url: <input name=\"url\" length=64 type=\"url\">");
    s += F("<input type=\"submit\"></form>");
    webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
  });
  webServer.on("/setting", []() {
    String s = F("<h1>Wi-Fi Settings</h1>");
    s += F("<a href=\"/restart\"><p>Restartovat</p><a/>");
    s += F("<a href=\"/setting\"><WIFI Network Setting</p><a/>");
    webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
  });
}

String makePage(String title, String contents) {
  String s = F("<!DOCTYPE html><html><head>");
  s += F("<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">");
  s += F("<title>");
  s += title;
  s += F("</title></head><body>");
  s += contents;
  s += F("</body></html>");
  return s;
}
