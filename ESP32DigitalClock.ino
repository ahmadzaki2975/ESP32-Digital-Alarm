#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET -1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define LED 2
#define HourPushButton 17
#define MinutePushButton 18

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "Istana Mocha";
const char* password = "33547800";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;  // GMT+7
const int daylightOffset_sec = 0;
volatile bool hourInterruptFlag = false;
volatile bool minuteInterruptFlag = false;

unsigned long lastUpdate = 0;
const int updateInterval = 1000;

WiFiClient client;
HTTPClient http;
String serverPath = "http://192.168.50.30:5000";

int alarmHour = 0;
int alarmMinute = 0;

void displayTime(struct tm* timeinfo) {
  char buffer[40];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);

  char dateBuffer[40];
  strftime(dateBuffer, sizeof(dateBuffer), "%A, %d %B %Y", timeinfo);

  // Display to OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("===========");
  display.setCursor(0, 10);
  display.print(buffer);
  display.setCursor(0, 20);
  display.print(dateBuffer);
  display.setCursor(0, 40);
  display.print("===========");
  display.display();

  // Send to server
  String data = "{\"time\":\"" + String(buffer) + "\",\"date\":\"" + String(dateBuffer) + "\"}";

  Serial.println(data);
  char pressedKey = NULL;

  http.begin(client, serverPath + "/data");
  http.addHeader("Content-Type", "application/json");
  // int httpResponseCode = http.POST(data);
  http.end();
}

void IRAM_ATTR hourInterruptHandler() {
  static unsigned long lastDebounceTime = 0;  
  unsigned long debounceDelay = 200;          
  unsigned long currentTime = millis();

  if (currentTime - lastDebounceTime > debounceDelay) {
    hourInterruptFlag = true;
    if (alarmHour == 23) {
      alarmHour = 0;
    } else {
      alarmHour++;
    }
    Serial.println("Alarm hour : " + String(alarmHour));
    lastDebounceTime = currentTime; 
  }
}

void IRAM_ATTR minuteInterruptHandler() {
  static unsigned long lastDebounceTime = 0;  
  unsigned long debounceDelay = 200;          
  unsigned long currentTime = millis();

  if (currentTime - lastDebounceTime > debounceDelay) {
    minuteInterruptFlag = true;
    if (alarmMinute == 59) {
      alarmMinute = 0;
    } else {
      alarmMinute++;
    }
    Serial.println("Alarm minute : " + String(alarmMinute));
    lastDebounceTime = currentTime;
  }
}

void setAlarmOff() {
  // Blink LED
  digitalWrite(LED, HIGH);
  delay(50);
  digitalWrite(LED, LOW);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED, OUTPUT);
  pinMode(HourPushButton, INPUT_PULLUP);
  pinMode(MinutePushButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HourPushButton), hourInterruptHandler, FALLING);
  attachInterrupt(digitalPinToInterrupt(MinutePushButton), minuteInterruptHandler, FALLING);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
}

void loop() {
  unsigned long currentMillis = millis();
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (alarmHour == timeinfo->tm_hour && alarmMinute == timeinfo->tm_min) {
    setAlarmOff();
  }

  if (currentMillis - lastUpdate >= updateInterval) {
    lastUpdate = currentMillis;
    displayTime(timeinfo);

    if (hourInterruptFlag || minuteInterruptFlag) {
      minuteInterruptFlag = false;
      hourInterruptFlag = false;
      
      String data = "{\"alarm\": {\"hour\": " + String(alarmHour) + ", \"minute\": " + String(alarmMinute) + "}}";
      http.begin(client, serverPath + "/alarm");
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST(data);

      http.end();
    }
  }
}
