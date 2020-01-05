/*
 * edited by UT9UF
 * ut9uf@ukr.net Jan 5, 2020
 * https://github.com/andykv59/ESP8266-clock-72
 * based on Марсель Ахкамов clock
 * 
 */
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <EEPROM.h>
#include "Timer.h"

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "Fonts.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //newer MD_Parola lib version requires HARDWARE_TYPE
#define MAX_DEVICES 4 // 4 matrix 
#define CLK_PIN     D5 // or SCK
#define DATA_PIN    D7 // or MOSI
#define CS_PIN      D8 // or SS
// in order to reset EEPROM connect pin D3 (gpio0) to GRND

MD_Parola  P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES); //newer MD_Parola lib version requires HARDWARE_TYPE - 3 field
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))

// Global data
typedef struct
{
  textEffect_t  effect;   // text effect to display
  char *        psz;      // text string nul terminated
  uint16_t      speed;    // speed multiplier of library default
  uint16_t      pause;    // pause multiplier for library default
} sCatalog;

sCatalog  catalog[] =
{
  { PA_SLICE, "SLICE", 1, 1 },
  { PA_MESH, "MESH", 10, 1 },
  { PA_FADE, "FADE", 20, 1 },
  { PA_WIPE, "WIPE", 5, 1 },
  { PA_WIPE_CURSOR, "WPE_C", 4, 1 },
  { PA_OPENING, "OPEN", 3, 1 },
  { PA_OPENING_CURSOR, "OPN_C", 4, 1 },
  { PA_CLOSING, "CLOSE", 3, 1 },
  { PA_CLOSING_CURSOR, "CLS_C", 4, 1 },
  { PA_BLINDS, "BLIND", 7, 1 },
  { PA_DISSOLVE, "DSLVE", 7, 1 },
  { PA_SCROLL_UP, "SC_U", 5, 1 },
  { PA_SCROLL_DOWN, "SC_D", 5, 1 },
  { PA_SCROLL_LEFT, "SC_L", 5, 1 },
  { PA_SCROLL_RIGHT, "SC_R", 5, 1 },
  { PA_SCROLL_UP_LEFT, "SC_UL", 7, 1 },
  { PA_SCROLL_UP_RIGHT, "SC_UR", 7, 1 },
  { PA_SCROLL_DOWN_LEFT, "SC_DL", 7, 1 },
  { PA_SCROLL_DOWN_RIGHT, "SC_DR", 7, 1 },
  { PA_SCAN_HORIZ, "SCANH", 4, 1 },
  { PA_SCAN_VERT, "SCANV", 3, 1 },
  { PA_GROW_UP, "GRW_U", 7, 1 },
  { PA_GROW_DOWN, "GRW_D", 7, 1 },
};

Timer t;

#include "global.h"
#include "NTP.h"

// Include the HTML, STYLE and Script "Pages"

#include "Page_Admin.h"
#include "Page_Script.js.h"
#include "Page_Style.css.h"
#include "Page_NTPSettings.h"
#include "Page_Information.h"
#include "Page_Brightnes.h"
#include "Page_General.h"
#include "Page_NetworkConfiguration.h"

extern "C" {
#include "user_interface.h"
}

Ticker ticker;

os_timer_t myTimer;

#define LED_PIN 2
#define buttonPin 0 // reset EEPROM

String weatherKey;
String ipstring;
String Text;
char buf[256];

String y;     // год
String mon;   // месяц
String wd;    // день недели
String d;     // дни
String h;     // часы
String m;     // минуты
String s;     // секунды

int disp=0;
int rnd;
int lp=0;

unsigned long eventTime=0;
int buttonstate =1;

String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
float temp;
String tempz;

int clouds;
float windSpeed;
int windDeg;

String date;
String date1;
String currencyRates;
String weatherString;
String weatherString1;
String weatherStringz;
String weatherStringz1;
String weatherStringz2;

String cityID;
  
WiFiClient client;

String chipID;

void setup() {
  P.begin();
  P.setInvert(false);
  P.setFont(fontBG); //russian letters included
  
  bool CFG_saved = false;
  int WIFI_connected = false;
  Serial.begin(115200);

  pinMode(LED_PIN,OUTPUT);
  pinMode(buttonPin,INPUT);
  digitalWrite(buttonPin, HIGH);
  digitalWrite(LED_PIN, HIGH);
 
  //**** Network Config load 
  EEPROM.begin(512); // define an EEPROM size of 512Bytes to store data
  CFG_saved = ReadConfig();

  //  Connect to WiFi acess point or start as Acess point
  if (CFG_saved)  //if no configuration yet saved, load defaults
  {    
      // Connect the ESP8266 to local WIFI network in Station mode
      Serial.println("Booting");     
      WiFi.mode(WIFI_STA);

  if (!config.dhcp)
  {
    WiFi.config(IPAddress(config.IP[0], config.IP[1], config.IP[2], config.IP[3] ),  IPAddress(config.Gateway[0], config.Gateway[1], config.Gateway[2], config.Gateway[3] ) , IPAddress(config.Netmask[0], config.Netmask[1], config.Netmask[2], config.Netmask[3] ) , IPAddress(config.DNS[0], config.DNS[1], config.DNS[2], config.DNS[3] ));
  }
      WiFi.begin(config.ssid.c_str(), config.password.c_str());
      printConfig();
      WIFI_connected = WiFi.waitForConnectResult(); 
      if(WIFI_connected!= WL_CONNECTED ){
        Serial.println("Connection Failed! Entering into AP mode...");
        Serial.print("Wifi IP:");Serial.println(WiFi.localIP());
        Serial.print("Email:");Serial.println(config.email.c_str());        
      }
  }

  if ( (WIFI_connected!= WL_CONNECTED) or !CFG_saved){
    // DEFAULT CONFIG
    scrollConnect();
    Serial.println("Setting AP mode default parameters");
    config.ssid = "UT9UF-clkAP";       // SSID of access point
    config.password = "" ;   // default password of access point is none
    config.dhcp = true;
    config.IP[0] = 192; config.IP[1] = 168; config.IP[2] = 1; config.IP[3] = 100;
    config.Netmask[0] = 255; config.Netmask[1] = 255; config.Netmask[2] = 255; config.Netmask[3] = 0;
    config.Gateway[0] = 192; config.Gateway[1] = 168; config.Gateway[2] = 1; config.Gateway[3] = 1;
    config.DNS[0] = 192; config.DNS[1] = 168; config.DNS[2] = 1; config.DNS[3] = 1;
    config.ntpServerName = "ua.pool.ntp.org"; // to be adjusted to your PT 
    config.Update_Time_Via_NTP_Every =  10;
    config.timeZone = 3;
    config.isDayLightSaving = true;
    config.DeviceName = "API key";
    config.email = "cityID";
    config.textBrightnessD = 7;
    config.textBrightnessN = 0;
    config.TimeBrightnessD = 8;
    config.TimeBrightnessN = 21;    
    WiFi.mode(WIFI_AP);  
    WiFi.softAP(config.ssid.c_str());
    Serial.print("Wifi IP:");Serial.println(WiFi.softAPIP());
   }

   Brightnes(); // set brightes after the boot

    // Start HTTP Server for configuration
    server.on ( "/", []() {
      Serial.println("admin.html");
      server.send_P ( 200, "text/html", PAGE_AdminMainPage);  // const char top of page
    }  );
  
    server.on ( "/favicon.ico",   []() {
      Serial.println("favicon.ico");
      server.send( 200, "text/html", "" );
    }  );
  
    // Network config
    server.on ( "/config.html", send_network_configuration_html );
    // Info Page
    server.on ( "/info.html", []() {
      Serial.println("info.html");
      server.send_P ( 200, "text/html", PAGE_Information );
    }  );
    server.on ( "/ntp.html", send_NTP_configuration_html  );
  
    // brightnes config
    server.on ( "/brightnes.html", send_brightnes_configuration_html  );  // brightnes

    server.on ( "/general.html", send_general_html  );
    //  server.on ( "/example.html", []() { server.send_P ( 200, "text/html", PAGE_EXAMPLE );  } );
    server.on ( "/style.css", []() {
      Serial.println("style.css");
      server.send_P ( 200, "text/plain", PAGE_Style_css );
    } );
    server.on ( "/microajax.js", []() {
      Serial.println("microajax.js");
      server.send_P ( 200, "text/plain", PAGE_microajax_js );
    } );
    server.on ( "/admin/values", send_network_configuration_values_html );
    server.on ( "/admin/connectionstate", send_connection_state_values_html );
    server.on ( "/admin/infovalues", send_information_values_html );
    server.on ( "/admin/ntpvalues", send_NTP_configuration_values_html );
    server.on ( "/admin/brightnesvalues", send_brightnes_configuration_values_html ); // brightnes
    server.on ( "/admin/generalvalues", send_general_configuration_values_html);
    server.on ( "/admin/devicename",     send_devicename_value_html); 
    server.onNotFound ( []() {
      Serial.println("Page Not Found");
      server.send ( 400, "text/html", "Page not Found" );
    }  );
    server.begin();
    Serial.println( "HTTP server started" );

    // ***********  OTA SETUP 
    ArduinoOTA.setHostname(config.DeviceName.c_str());
    ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
        ESP.restart();
    }); 
    ArduinoOTA.onError([](ota_error_t error) { 
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
        ESP.restart(); 
    });
  
     /* setup the OTA server */
    ArduinoOTA.begin();
    Serial.println("Ready");
          for(int i=0; i<3; i++){ // Bling the LED to show the program started
           digitalWrite(LED_PIN, LOW);
           delay(200);
           digitalWrite(LED_PIN, HIGH);
           delay(200);
          }
    
// start internal time update ISR  
ipstring = (
String(WiFi.localIP()[0]) +"." +
String(WiFi.localIP()[1]) + "." +
String(WiFi.localIP()[2]) + "." +
String(WiFi.localIP()[3])
);

{
  for (uint8_t i=0; i<ARRAY_SIZE(catalog); i++)
  {
    catalog[i].speed *= P.getSpeed();
    catalog[i].pause *= 500;
  }
}

t.every(1000, ISRsecondTick);

if  (WiFi.status() == WL_CONNECTED) {
getTime();
scrollIP();

}  
   getTime();
   getWeatherData();
   getWeatherDataz();
weatherKey = config.DeviceName.c_str();
cityID = config.email.c_str();
}

// the loop function runs over and over again forever
void loop() {
  
  // OTA request handling
  ArduinoOTA.handle();

  //  WebServer requests handling
  server.handleClient();

  //  feed de DOG :) 
  customWatchdog = millis();

  //**** Normal Skecth code here ... 
t.update();
  if (lp >= 10) lp=0;
   if (disp ==0){
    if (lp==0){
       getTime();
       getWeatherData();
       getWeatherDataz();
    }
    
   getTime();
   disp=1;
   lp++;
   }
   
   if (disp ==1){
   rnd = random(0, ARRAY_SIZE(catalog));
   Text = h + ":" + m;
   displayInfo();
   }
   
   if (disp ==2){
   Text = d + " " + mon + " " + y + " " + wd;
   scrollText();
   }

   if (disp ==3){
   rnd = random(0, ARRAY_SIZE(catalog));
   Text = h + ":" + m;
   displayInfo1();
   }

   if (disp ==4){
   Text = weatherString;
   scrollText1();
   }

   if (disp ==5){
   rnd = random(0, ARRAY_SIZE(catalog));
   Text = h + ":" + m;
   displayInfo2();
   }

   if (disp ==6){
   Text = weatherStringz + " " + weatherStringz1;
   scrollText2();
   }
   
// D3 pin to GRND erases an EEPROM
int buttonstate=digitalRead(buttonPin);
if(buttonstate==HIGH) eventTime=millis();
if(millis()-eventTime>5000){      
digitalWrite(16, LOW);  
 ResetAll();             
Serial.println("EEPROM formatted");
ESP.restart();
}
else 
{
digitalWrite(16, HIGH); 
}
}
// end of loop


void Brightnes(){
    // matrix brightness
    if (DateTime.hour >= config.TimeBrightnessD && DateTime.hour < config.TimeBrightnessN) P.setIntensity(config.textBrightnessD);
    else P.setIntensity(config.textBrightnessN);
    //Serial.print("Brightness EEPROM Day. = "); Serial.println(config.textBrightnessD);Serial.print(" Start. = "); Serial.println(config.TimeBrightnessD);
    //Serial.print("Brightness EEPROM Night. = "); Serial.println(config.textBrightnessN);Serial.print(" Start. = "); Serial.println(config.TimeBrightnessN);
    //Serial.print("HOUR. = ");Serial.println(DateTime.hour);  
}

//erase of the EEPROM
void ResetAll(){
  EEPROM.begin(512);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++){
  EEPROM.write(i, 0);
  }
  EEPROM.end();
  ESP.reset();
}

//some magic starts here

void getTime(){
    getNTPtime();
    h = String (DateTime.hour/10) + String (DateTime.hour%10);
    m = String (DateTime.minute/10) + String (DateTime.minute%10);
    s = String (DateTime.second/10 + String (DateTime.second%10));
    d = String (DateTime.day);
    y = String (DateTime.year);
         
    if (DateTime.month == 1) mon = "Января";
    if (DateTime.month == 2) mon = "Февраля";
    if (DateTime.month == 3) mon = "Марта";
    if (DateTime.month == 4) mon = "Апреля";
    if (DateTime.month == 5) mon = "Мая";
    if (DateTime.month == 6) mon = "Июня";
    if (DateTime.month == 7) mon = "Июля";
    if (DateTime.month == 8) mon = "Августа";
    if (DateTime.month == 9) mon = "Сентября";
    if (DateTime.month == 10) mon = "Октября";
    if (DateTime.month == 11) mon = "Ноября";
    if (DateTime.month == 12) mon = "Декабря";

    if (DateTime.wday == 2) wd = "Понедельник";
    if (DateTime.wday == 3) wd = "Вторник";
    if (DateTime.wday == 4) wd = "Среда";
    if (DateTime.wday == 5) wd = "Четверг";
    if (DateTime.wday == 6) wd = "Пятница";
    if (DateTime.wday == 7) wd = "Суббота";
    if (DateTime.wday == 1) wd = "Воскресенье";
    
    Brightnes();
}

void displayInfo(){
    if (P.displayAnimate()){
    utf8rus(Text).toCharArray(buf, 256);
    P.displayText(buf, PA_CENTER, catalog[rnd].speed, 5000, catalog[rnd].effect, catalog[rnd].effect);   
    if (!P.displayAnimate()) disp = 2;
    }
}

void displayInfo1(){
    if (P.displayAnimate()){
    utf8rus(Text).toCharArray(buf, 256);
    P.displayText(buf, PA_CENTER, catalog[rnd].speed, 5000, catalog[rnd].effect, catalog[rnd].effect);   
    if (!P.displayAnimate()) disp = 4;
    }
}

void displayInfo2(){
    if (P.displayAnimate()){
    utf8rus(Text).toCharArray(buf, 256);
    P.displayText(buf, PA_CENTER, catalog[rnd].speed, 5000, catalog[rnd].effect, catalog[rnd].effect);   
    if (!P.displayAnimate()) disp = 6;
    }
}

void displayInfo3(){
    if (P.displayAnimate()){
    utf8rus(Text).toCharArray(buf, 256);
    P.displayText(buf, PA_CENTER, catalog[rnd].speed, 5000, catalog[rnd].effect, catalog[rnd].effect);   
    if (!P.displayAnimate()) disp = 0;
    }
}

void scrollText(){
  if  (P.displayAnimate()){
  utf8rus(Text).toCharArray(buf, 256);
  P.displayScroll(buf, PA_LEFT, PA_SCROLL_LEFT, 40);
  if (!P.displayAnimate()) disp = 3;
  }
}

void scrollText1(){
  if  (P.displayAnimate()){
  utf8rus(Text).toCharArray(buf, 256);
  P.displayScroll(buf, PA_LEFT, PA_SCROLL_LEFT, 40);
  if (!P.displayAnimate()) disp = 5;
  }
}

void scrollText2(){
  if  (P.displayAnimate()){
  utf8rus(Text).toCharArray(buf, 256);
  P.displayScroll(buf, PA_LEFT, PA_SCROLL_LEFT, 40);
  if (!P.displayAnimate()) disp = 0;
  }
}

void scrollIP(){  
  Text = "Ваш IP: "+ipstring;
  if  (P.displayAnimate()){
  utf8rus(Text).toCharArray(buf, 256);
  P.displayScroll(buf, PA_LEFT, PA_SCROLL_LEFT, 60);
  }
}

void scrollConnect(){
  Text = "Отсутствует подключение к WIFI. Подключитесь к точке доступа 'UT9UF-clkAP' и войдите в веб интерфейс 192.168.4.1" ;
  if  (P.displayAnimate()){
  utf8rus(Text).toCharArray(buf, 256);
  P.displayScroll(buf, PA_LEFT, PA_SCROLL_LEFT, 40);
  if (!P.displayAnimate()) disp = 3;
  }
}

// actual weather and forecast is taken from openweathermap.org

const char *weatherHost = "api.openweathermap.org";

void getWeatherData()
{
  Serial.print("connecting to getweatherdata..."); Serial.println(weatherHost);
  if (client.connect(weatherHost, 80)) {
    client.println(String("GET /data/2.5/weather?id=") + cityID + "&units=metric&appid=" + weatherKey + "&lang=ru" + "\r\n" +
                "Host: " + weatherHost + "\r\nUser-Agent: ArduinoWiFi/1.1\r\n" +
                "Connection: close\r\n\r\n");
  } else {
    Serial.println("connection to weather host failed");
    return;
  }
  String line;
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    Serial.println("at getWeatherData waiting for client.available ...");
    repeatCounter++;
  }
  while (client.connected() && client.available()) {
    char c = client.read(); 
    if (c == '[' || c == ']') c = ' ';
    line += c;
  }

  client.stop();
  
  //Serial.println("got client.stop and contenet is as follows ...");
  //Serial.println(line + "\n");
  
  DynamicJsonBuffer jsonBuf;
  JsonObject &root = jsonBuf.parseObject(line);
  if (!root.success())
  {
    Serial.println("at getWeatherDataparseObject() failed \n");
    return;
  }
 
  weatherDescription = root["weather"]["description"].as<String>();
  weatherDescription.toLowerCase();
  temp = root["main"]["temp"];
  humidity = root["main"]["humidity"];
  pressure = root["main"]["pressure"];
  windSpeed = root["wind"]["speed"];
  windDeg = root["wind"]["deg"];
  clouds = root["clouds"]["all"];
  String deg = String(char('~'+25));
  weatherString = "Темп: " + String(temp,0)+"*С ";
  weatherString += " " + weatherDescription + " ";
  weatherString += "Влажн: " + String(humidity) + "% ";
  weatherString += "Давл: " + String(pressure) + " hPa ";
  weatherString += "Небо: " + String(clouds) + "% ";

String windDegString;

if (windDeg>=345 || windDeg<=22) windDegString = "СС ";
if (windDeg>=23 && windDeg<=68) windDegString = "СВ ";
if (windDeg>=69 && windDeg<=114) windDegString = "ВВ ";
if (windDeg>=115 && windDeg<=160) windDegString = "ЮВ ";
if (windDeg>=161 && windDeg<=206) windDegString = "ЮЮ ";
if (windDeg>=207 && windDeg<=252) windDegString = "ЮЗ ";
if (windDeg>=253 && windDeg<=298) windDegString = "ЗЗ ";
if (windDeg>=299 && windDeg<=344) windDegString = "СЗ ";

  weatherString += " Ветер: " + windDegString + " " + String(windSpeed,1) + " м/с";  
  //Serial.println("Temp: " + String(temp,0) + "*C\n");
}

// forecast from openweathermap.org

const char *weatherHostz = "api.openweathermap.org";
void getWeatherDataz()
{
  Serial.print("at getWeatherDataz connecting to "); Serial.println(weatherHostz);
  if (client.connect(weatherHostz, 80)) {
    client.println(String("GET /data/2.5/forecast/daily?id=") + cityID + "&units=metric&appid=" + weatherKey + "&lang=ru" + "&cnt=2" + "\r\n" +
                "Host: " + weatherHostz + "\r\nUser-Agent: ArduinoWiFi/1.1\r\n" +
                "Connection: close\r\n\r\n");
                
  } else {
    Serial.println("at getWeatherDataz connection failed");
    return;
  }
  String line;
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    Serial.println("at getWeatherDataz waiting for client.available .....");
    repeatCounter++;
  }
  while (client.connected() && client.available()) {
    char c = client.read(); 
    line += c;
  }
  
  client.stop();

  Serial.println("at getWeatherDataz line is" + line + "\n");

  DynamicJsonBuffer jsonBuf;
  
  JsonObject &root = jsonBuf.parseObject(line);
  
  if (!root.success()) {
      Serial.println("at getWeatherDataz parseObject() failed");
      return;
  }

  float tempMin = root ["list"][1]["temp"]["night"]; // day, max, night, eve, min, morn
  float tempMax = root ["list"][1]["temp"]["day"]; // day, max, night, eve, min, morn
  
  float hourSunrise = root ["list"][1]["sunrise"];
  float hourSunset = root ["list"][1]["sunset"];  
  float wSpeed = root ["list"][1]["speed"];
  int wDeg = root ["list"][1]["deg"];
  pressure = root ["list"][1]["pressure"];
  weatherDescription = root ["list"][1]["weather"][0]["description"].as<String>();
  
  weatherStringz = "Завтра днем: " + String(tempMax,1) + " ночью: " + String(tempMin,1) + " " + weatherDescription;
  String windDegString;

if (wDeg>=345 || wDeg<=22) windDegString = "СС";
if (wDeg>=23 && wDeg<=68) windDegString = "СВ";
if (wDeg>=69 && wDeg<=114) windDegString = "ВВ";
if (wDeg>=115 && wDeg<=160) windDegString = "ЮВ";
if (wDeg>=161 && wDeg<=206) windDegString = "ЮЮ";
if (wDeg>=207 && wDeg<=252) windDegString = "ЮЗ";
if (wDeg>=253 && wDeg<=298) windDegString = "ЗЗ";
if (wDeg>=299 && wDeg<=344) windDegString = "СЗ";

  weatherStringz1 = "Ветер: " + windDegString + " " + String(wSpeed,1) + " м/с" + " Давл: " + String(pressure) + " hPa ";
}

String utf8rus(String source)
{
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x30;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB8; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x70;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
return target;
}
