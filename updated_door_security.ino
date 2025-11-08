#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // For parsing Telegram JSON

// ==================== WiFi Credentials ====================
const char* ssid     = "Mr. & Mrs.";
const char* password = "goldshark";

// ==================== Telegram Settings ====================
const char* botToken = "8524764913:AAFsR1rKimN5HXLDvAqv8A7Mg61RzaxDT7c";
const char* chatID   = "7563685213";

// ==================== Pin Definitions ====================
const int doorSensorPin = 4;   // GPIO for door sensor (LOW = closed, HIGH = open)
const int redLedPin     = 13;  // Red LED (Gate open)

// ==================== Timing ====================
const unsigned long debounceDelay = 50;
const unsigned long telegramInterval = 500;
const unsigned long wifiCheckInterval = 10000; // every 10 seconds check WiFi
const unsigned long reconnectTimeout = 7000;   // try reconnect max 7 seconds

// ==================== Internal State ====================
int lastDoorReading = HIGH;
int doorState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long lastTelegramCheck = 0;
unsigned long lastWiFiCheck = 0;
bool firstRun = true;
bool reconnecting = false;
long lastUpdateId = 0;

// ==================== Function: Send Telegram Message ====================
void sendTelegramMessage(String message) {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + 
                 "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;
    http.begin(url);
    http.GET();
    http.end();
  }
}

// ==================== WiFi Reconnect Function ====================
void maintainWiFiConnection() {
  if (millis() - lastWiFiCheck < wifiCheckInterval) return;
  lastWiFiCheck = millis();

  if (WiFi.status() != WL_CONNECTED && !reconnecting) {
    reconnecting = true;
    Serial.println("🔌 WiFi lost! Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < reconnectTimeout) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ WiFi reconnected!");
      sendTelegramMessage("🔁 WiFi reconnected!");
    } else {
      Serial.println("\n❌ WiFi reconnect failed, will retry later.");
    }

    reconnecting = false;
  }
}

// ==================== Function: Check Telegram Commands ====================
void checkTelegramCommands() {
  if(WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(botToken) + "/getUpdates?offset=" + String(lastUpdateId + 1);
  http.begin(url);
  int httpResponseCode = http.GET();
  if(httpResponseCode == 200){
    String payload = http.getString();
    StaticJsonDocument<2000> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if(!error){
      JsonArray result = doc["result"];
      for(JsonObject update : result){
        long update_id = update["update_id"];
        lastUpdateId = update_id;

        String text = update["message"]["text"].as<String>();

        if(text == "Door Condition" || text == "Door" || text == "door"){
          String status = (digitalRead(doorSensorPin) == LOW) ? "✅ Door is CLOSED" : "⚠️ Door is OPEN";
          sendTelegramMessage(status);
        }
      }
    }
  }
  http.end();
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);

  pinMode(doorSensorPin, INPUT_PULLUP);
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, LOW);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  sendTelegramMessage("🌐 WiFi Connected! Gate Monitor Started.");

  doorState = digitalRead(doorSensorPin);
  lastDoorReading = doorState;
  firstRun = true;
}

// ==================== Loop ====================
void loop() {
  maintainWiFiConnection();

  int reading = digitalRead(doorSensorPin);
  if (reading != lastDoorReading) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != doorState) {
      doorState = reading;

      if(doorState == HIGH) digitalWrite(redLedPin, HIGH);
      else digitalWrite(redLedPin, LOW);

      if(!firstRun){
        if (doorState == LOW) sendTelegramMessage("✅ Gate is CLOSED!");
        else sendTelegramMessage("⚠️ Gate is OPEN!");
      }
    }
  }
  lastDoorReading = reading;
  firstRun = false;

  if(millis() - lastTelegramCheck >= telegramInterval){
    checkTelegramCommands();
    lastTelegramCheck = millis();
  }
}