#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <time.h>

const char* Wifi_Name = "Your_wifi_Name";

const char* Wifi_Password = "YOUR_Wifi_Password";

const float Latitude = 31.55;
const float Longitude = 74.35;

const long GMT_offset_sec = 5 * 3600;
const int Daylight_offset_sec = 0;

#define Screen_width 128

#define Screen_height 64
#define Oled_address 0x3C
#define Oled_reset -1

Adafruit_SSD1306 oled(Screen_width, Screen_height, &Wire, Oled_reset);

#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define SW_start 5
#define SW_pause 6
#define SW_stop 7
#define Mode_switch 10

float roomtemp = 0;
float roomHumidity = 0;
bool roomSensorValid = false;

float outsideTemp = 0;
int weatherCode = 0;
bool weatherValid = false;

int timerSec = 0;
bool timerRunning = false;
bool timerPaused = false;

unsigned long lastTimerUpdate = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastDhtUpdate = 0;
unsigned long lastScreenUpdate = 0;

const unsigned long weatherRefreshInterval = 10 * 60 * 1000;
const unsigned long dhtRefreshInterval = 5000;
const unsigned long screenRefreshInterval = 250;

bool wasPressed(int pin, bool &lastState) {
  bool pressedrn = digitalRead(pin) == LOW;
  bool justPressed = pressedrn && !lastState;
  lastState = pressedrn;
  return justPressed;
}

void drawCentered(const char* text, int y, int size) {
  oled.setTextSize(size);
  oled.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t width, height;
  oled.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);

  oled.setCursor((SCREEN_WIDTH - width) / 2, y);
  oled.print(text);
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, Wifi_password);

  while (WiFi.status() != WL_connected) {
    delay(500);
  }
}

void setupClock() {
  configTime(
    GMT_offset_sec,
    Daylight_offser_sec,
    "pool.ntp.org",
    "time.nist.gov"
  );
}

void updateWeather() {
  if (WiFi.status() != WL_connected) {
    weatherValid = false;
    return;
  }

  HTTPClient http;

  String url = "https://api.open-meteo.com/v1/forecast?";
  url += "latitude=" + String(Latitude, 4);
  url += "&longitude=" + String(Longitude, 4);
  url += "&current=temperature_2m,weather_code";
  url += "&timezone=auto";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      outsidetemp = doc["current"]["temp_2m"];
      weatherCode = doc["current"]["weather_code"];
      weatherValid = true;
    }
  } else {
    weatherValid = false;
  }

  http.end();
}

const char* weatherText(int code) {
  if (code == 0) return "Clear";
  if (code == 1 || code == 2) return "Partly cloudy";
  if (code == 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95) return "Storm";
  return "Weather";
}

void updateRoomSensor() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temp) && !isnan(humidity)) {
    roomTemp = temp;
    roomHumidity = humidity;
    roomSensorValid = true;
  }
}

void startOrResumeTimer() {
  if (timerSeconds > 0) {
    timerRunning = true;
    timerPaused = false;
    lastTimerUpdate = millis();
  }
}

void pauseTimer() {
  if (timerRunning) {
    timerRunning = false;
    timerPaused = true;
  }
}

void stopTimer() {
  timerRunning = false;
  timerPaused = false;
  timerSeconds = 0;
}

void updateTimer() {
  if (!timerRunning) return;

  unsigned long now = millis();

  if (now - lastTimerUpdate >= 1000) {
    lastTimerUpdate = now;

    if (timerSeconds > 0) {
      timerSeconds--;
    }

    if (timerSeconds == 0) {
      timerRunning = false;
      timerPaused = false;
    }
  }
}

void drawHomeScreen() {
  oled.clearDisplay();

  struct tm timeInfo;

  if (getLocalTime(&timeInfo)) {
    char timeText[9];
    strftime(timeText, sizeof(timeText), "%H:%M:%S", &timeInfo);
    drawCentered(timeText, 0, 2);
  } else {
    drawCentered("Getting time...", 0, 1);
  }

  oled.setTextSize(1);
  oled.setCursor(0, 24);

  if (weatherValid) {
    oled.print("Out: ");
    oled.print(outsideTemp, 1);
    oled.print("C ");
    oled.println(weatherText(weatherCode));
  } else {
    oled.println("Out: waiting...");
  }

  oled.setCursor(0, 38);

  if (roomSensorValid) {
    oled.print("Room: ");
    oled.print(roomTemp, 1);
    oled.print("C  ");
    oled.print(roomHumidity, 0);
    oled.println("%");
  } else {
    oled.println("Room: waiting...");
  }

  oled.setCursor(0, 54);
  oled.println("Slide right: Timer");

  oled.display();
}

void drawTimerScreen() {
  oled.clearDisplay();

  drawCentered("TIMER", 0, 1);

  int minutes = timerSec / 60;
  int seconds = timerSec % 60;

  char timerText[9];
  sprintf(timerText, "%02d:%02d", minutes, seconds);

  drawCentered(timerText, 18, 2);

  oled.setTextSize(1);
  oled.setCursor(0, 48);

  if (timerRunning) {
    oled.println("Running");
  } else if (timerPaused) {
    oled.println("Paused");
  } else if (timerSeconds == 0) {
    oled.println("Ready / stopped");
  } else {
    oled.println("Stopped");
  }

  oled.setCursor(0, 58);
  oled.println("B1 start B2 pause B3 reset");

  oled.display();
}

void setup() {
  Serial.begin(115200);

  pinMode(Sw_start, INPUT_PULLUP);
  pinMode(SW_pause, INPUT_PULLUP);
  pinMode(SW_stop, INPUT_PULLUP);
  pinMode(MODE_SWITCH, INPUT_PULLUP);

  dht.begin();

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_adress)) {
    while (true) {
      delay(1000);
    }
  }

  oled.clearDisplay();
  drawCentered("Desk Buddy", 20, 2);
  oled.display();

  connectWiFi();
  setupClock();

  updateWeather();
  lastWeatherUpdate = millis();

  delay(2000);
  updateRoomSensor();
  lastDhtUpdate = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastWeatherUpdate >= weatherRefreshInterval) {
    lastWeatherUpdate = now;
    updateWeather();
  }

  if (now - lastDhtUpdate >= dhtRefreshInterval) {
    lastDhtUpdate = now;
    updateRoomSensor();
  }

  updateTimer();

  bool timerMode = digitalRead(MODE_SWITCH) == LOW;

  static bool lastStart = HIGH;
  static bool lastPause = HIGH;
  static bool lastStop = HIGH;

  if (wasPressed(SW_start, lastStart)) {
    startOrResumeTimer();
  }

  if (wasPressed(SW_pause, lastPause)) {
    pauseTimer();
  }

  if (wasPressed(SW_stop, lastStop)) {
    stopTimer();
  }

  if (now - lastScreenUpdate >= screenRefreshInterval) {
    lastScreenUpdate = now;

    if (timerMode) {
      drawTimerScreen();
    } else {
      drawHomeScreen();
    }
  }
}
