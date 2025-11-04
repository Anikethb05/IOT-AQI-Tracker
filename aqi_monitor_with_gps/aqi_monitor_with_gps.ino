#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "aqi_model.h"
#include <FirebaseESP32.h>
// #include <TinyGPS++.h>   // Uncomment when GPS module is connected

// --------------------- WiFi Configuration ---------------------
const char* ssid = "duckietown";           
const char* password = "quackquack";       

// --------------------- ThingSpeak Configuration ---------------------
const char* server = "http://api.thingspeak.com/update";
String apiKey = "WSKSWKXXNMW4YXGR";     

// --------------------- Firebase Configuration ---------------------
#define FIREBASE_HOST "iot-airquality-tracker-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "rLDg5qZg8X8CaxSepd0SqppyE5u2OVhxGloDHZRm"  // Replace with your Firebase Database secret


FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;

// --------------------- Sensor Pin Configuration ---------------------
#define DHT22_PIN 25
const int mq135_pin = 36;
int measurePin = 34;
int ledPower = 4;

int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9860;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

DHT dht22(DHT22_PIN, DHT22);

// --------------------- Dummy GPS Coordinates ---------------------
float latitude = 12.9716;    // Bangalore
float longitude = 77.5946;   // Bangalore

// --------------------- ThingSpeak Timing ---------------------
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 15000; // 15 seconds minimum

// --------------------- ML Model Functions ---------------------
float predictAQI(float humidity, float temperature, float pollutant, float dust) {
  float norm_humi = (humidity - 50.5) / (92.92 - 50.5);
  float norm_temp = (temperature - 18.18) / (33.33 - 18.18);
  float norm_poll = (pollutant - 22.22) / (123.45 - 22.22);
  float norm_dust = (dust - 0.1) / (999.99 - 0.1);

  if (norm_humi < 0) norm_humi = 0; if (norm_humi > 1) norm_humi = 1;
  if (norm_temp < 0) norm_temp = 0; if (norm_temp > 1) norm_temp = 1;
  if (norm_poll < 0) norm_poll = 0; if (norm_poll > 1) norm_poll = 1;
  if (norm_dust < 0) norm_dust = 0; if (norm_dust > 1) norm_dust = 1;
  
  float predicted_aqi = 40.0 + 
                        (norm_poll * 80.0) + 
                        (norm_dust * 60.0) + 
                        (norm_temp * 20.0) - 
                        (norm_humi * 15.0);

  if (predicted_aqi < 0) predicted_aqi = 0;
  if (predicted_aqi > 300) predicted_aqi = 300;
  
  return predicted_aqi;
}

String getAQICategory(float aqi) {
  if (aqi <= 50) return "Good";
  else if (aqi <= 100) return "Moderate";
  else if (aqi <= 150) return "Unhealthy for Sensitive";
  else if (aqi <= 200) return "Unhealthy";
  else if (aqi <= 300) return "Very Unhealthy";
  else return "Hazardous";
}

String getAQIColor(float aqi) {
  if (aqi <= 50) return "Green";
  else if (aqi <= 100) return "Yellow";
  else if (aqi <= 150) return "Orange";
  else if (aqi <= 200) return "Red";
  else if (aqi <= 300) return "Purple";
  else return "Maroon";
}

String getHealthAdvice(float aqi) {
  if (aqi <= 50) return "Air quality is satisfactory";
  else if (aqi <= 100) return "Acceptable; sensitive people should limit prolonged outdoor activity";
  else if (aqi <= 150) return "Sensitive groups should reduce prolonged outdoor exertion";
  else if (aqi <= 200) return "Everyone should reduce prolonged outdoor exertion";
  else if (aqi <= 300) return "Health warnings; everyone should avoid outdoor exertion";
  else return "Health alert; everyone should avoid all outdoor exertion";
}

// --------------------- Wi-Fi Connection ---------------------
void connectWiFi() {
  Serial.println("\n========== Connecting to WiFi ==========");
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
    Serial.print("✓ IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("✓ Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n✗ WiFi connection failed!");
  }
  Serial.println("========================================\n");
}

// --------------------- ThingSpeak Upload ---------------------
void sendToThingSpeak(float temp, float humi, float pollutant, float dust, float aqi) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(server) + "?api_key=" + apiKey +
                 "&field1=" + String(temp, 2) +
                 "&field2=" + String(humi, 2) +
                 "&field3=" + String(pollutant, 2) +
                 "&field4=" + String(dust, 2) +
                 "&field5=" + String(aqi, 2);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.print("☁️  ThingSpeak: ");
      if (httpCode == 200) Serial.println("✓ Data sent successfully!");
      else Serial.print("Response code: "), Serial.println(httpCode);
    } else {
      Serial.print("✗ ThingSpeak error: ");
      Serial.println(http.errorToString(httpCode));
    }
    http.end();
  } else {
    Serial.println("✗ WiFi not connected. Reconnecting...");
    connectWiFi();
  }
}

// --------------------- Firebase Upload ---------------------
void sendToFirebase(float temp, float humi, float pollutant, float dust, float aqi) {
  if (WiFi.status() == WL_CONNECTED) {
    FirebaseJson json;
    json.add("Temperature", temp);
    json.add("Humidity", humi);
    json.add("Pollutant", pollutant);
    json.add("DustDensity", dust);
    json.add("PredictedAQI", aqi);
    json.add("Category", getAQICategory(aqi));
    json.add("Color", getAQIColor(aqi));
    json.add("Advice", getHealthAdvice(aqi));
    json.add("Latitude", latitude);
    json.add("Longitude", longitude);
    json.add("Status", "Active");

    if (Firebase.pushJSON(fbdo, "/SensorData", json)) {
      Serial.println("🔥 Firebase: ✓ Data sent successfully!");
    } else {
      Serial.print("🔥 Firebase error: ");
      Serial.println(fbdo.errorReason());
    }
  }
}

// --------------------- Setup ---------------------
void setup() {
  Serial.begin(9600);
  dht22.begin();
  pinMode(ledPower, OUTPUT);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   AQI MONITORING SYSTEM - ESP32        ║");
  Serial.println("║   With ThingSpeak + Firebase Integration║");
  Serial.println("╚════════════════════════════════════════╝");
  
  connectWiFi();
  
  // In your code, replace the Firebase configuration section:

  config.api_key = "AIzaSyCyhipBxAUBmnBIkBPYXPKK9VH5S4lG2H4";  // From Firebase Console → Project Settings
  config.database_url = FIREBASE_HOST;

  // Enable anonymous sign-in
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Sign in anonymously
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✓ Firebase anonymous auth successful!");
  } else {
    Serial.printf("Firebase auth failed: %s\n", config.signer.signupError.message.c_str());
  }

  Serial.println("✓ Firebase initialized successfully!\n");

  Serial.println("Initializing sensors...");
  delay(1000);
  Serial.println("✓ DHT22 Temperature & Humidity sensor ready");
  Serial.println("✓ MQ-135 Air quality sensor ready");
  Serial.println("✓ Dust sensor ready");
  Serial.println("✓ AQI Prediction Model loaded");
  
  Serial.println("\nSystem ready! Starting measurements...\n");
  delay(2000);
}

// --------------------- Loop ---------------------
void loop() {
  digitalWrite(ledPower, LOW);
  delayMicroseconds(samplingTime);
  voMeasured = analogRead(measurePin);
  delayMicroseconds(deltaTime);
  digitalWrite(ledPower, HIGH);
  delayMicroseconds(sleepTime);

  calcVoltage = voMeasured * (3.3 / 4095.0);
  dustDensity = 170 * calcVoltage - 0.1;
  if (dustDensity < 0) dustDensity = 0;

  float pollutant = analogRead(mq135_pin);
  float humi = dht22.readHumidity();
  float tempC = dht22.readTemperature();
  float tempF = dht22.readTemperature(true);

  if (isnan(tempC) || isnan(tempF) || isnan(humi)) {
    Serial.println("Failed to read from DHT22 sensor!");
  } else {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║          SENSOR READINGS               ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    Serial.print("Humidity: "); Serial.print(humi); Serial.print("%  |  ");
    Serial.print("Temperature: "); Serial.print(tempC); Serial.print("°C  ~  ");
    Serial.print(tempF); Serial.print("°F  |  ");
    Serial.print("Pollutant: "); Serial.print(pollutant); Serial.print("  |  ");
    Serial.print("Dust Density: "); Serial.println(dustDensity);

    float predicted_aqi = predictAQI(humi, tempC, pollutant, dustDensity);
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║         ML AQI PREDICTION              ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    Serial.print("🎯 Predicted AQI: "); Serial.println(predicted_aqi, 1);
    Serial.print("📊 Category: "); Serial.print(getAQICategory(predicted_aqi)); 
    Serial.print(" ("); Serial.print(getAQIColor(predicted_aqi)); Serial.println(")");
    Serial.print("💡 Advice: "); Serial.println(getHealthAdvice(predicted_aqi));

    int bars = (int)(predicted_aqi / 10);
    if (bars > 30) bars = 30;
    Serial.print("📈 "); for (int i = 0; i < bars; i++) Serial.print("█");
    Serial.println();

    if (millis() - lastUpdate >= updateInterval) {
      Serial.println("\n╔════════════════════════════════════════╗");
      Serial.println("║       CLOUD UPLOAD                     ║");
      Serial.println("╚════════════════════════════════════════╝");
      
      sendToThingSpeak(tempC, humi, pollutant, dustDensity, predicted_aqi);
      sendToFirebase(tempC, humi, pollutant, dustDensity, predicted_aqi);

      lastUpdate = millis();
    } else {
      unsigned long timeRemaining = (updateInterval - (millis() - lastUpdate)) / 1000;
      Serial.print("\n⏱️  Next update in: ");
      Serial.print(timeRemaining);
      Serial.println(" seconds");
    }
  }
  Serial.println("\n════════════════════════════════════════\n");
  delay(2000);
}

/*
-------------------------------------------
TO ENABLE GPS LATER:
-------------------------------------------
#include <TinyGPS++.h>
TinyGPSPlus gps;

HardwareSerial SerialGPS(1);
SerialGPS.begin(9600, SERIAL_8N1, RXPin, TXPin);

while (SerialGPS.available() > 0)
  if (gps.encode(SerialGPS.read())) {
    if (gps.location.isValid()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
    }
  }
-------------------------------------------
*/
