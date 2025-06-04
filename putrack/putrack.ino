#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"
#include <time.h>

// RFID-RC522
#include <SPI.h>
#include <MFRC522.h>

// 🔧 WiFi 정보 입력
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// 🔧 Firebase 설정
#define FIREBASE_HOST "pu-track-default-rtdb.firebaseio.com/"
#define API_KEY ""
#define USER_EMAIL "dev.shinena@gmail.com"
#define USER_PASSWORD ""

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// 🔧 MQTT 설정
const char* mqtt_server = "d11b42071a764217bc11ec0f2f3185b5.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "putrack";
const char* mqtt_pass = "";
const char* mqtt_topic = "putrack/sensor";

const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
...
"-----END CERTIFICATE-----\n";

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// 🧊 DS18B20
#define DS18B20_PIN 4
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

// 💧 DHT22
#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//  📡 RFID-RC522
#define SS_PIN 10 
#define RST_PIN 0
#define SCK_PIN 6
#define MOSI_PIN 7
#define MISO_PIN 2
MFRC522 rfid(SS_PIN, RST_PIN);

//  📡 Wi-fi
void connectToWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔌 WiFi 연결 중");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi 연결됨");
}
//  📡 MQTT
void connectToMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);

  Serial.print("📶 WiFi 상태: "); Serial.println(WiFi.status());
  Serial.print("📡 서버 주소: "); Serial.println(mqtt_server);
  Serial.print("🔐 MQTT 포트: "); Serial.println(mqtt_port);

  while (!mqttClient.connected()) {
    Serial.print("🔐 MQTT 연결 시도...");
    if (mqttClient.connect("ESP32Client_12", mqtt_user, mqtt_pass)) {
      Serial.println("✅ MQTT 연결 성공");
    } else {
      Serial.print("❌ 실패 코드: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

//  🔐 Firebase
void setupFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = FIREBASE_HOST;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// 🔧 NTP 시간 설정
void configTimeWithNTP() {
  // 한국 시간대
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("⏳ 시간 동기화 중");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\n✅ 시간 동기화 완료");
}

// 🔧 시간 포맷팅
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  int year = timeinfo->tm_year + 1900;
  int month = timeinfo->tm_mon + 1;
  int day = timeinfo->tm_mday;
  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;
  int second = timeinfo->tm_sec;

  char buffer[25];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          year, month, day, hour, minute, second);
  
  return String(buffer);
}

// 🔧 RC522 세팅
void setupRC522() {
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  Serial.println("📶 RFID-RC522 준비 완료");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  connectToWifi();
  configTimeWithNTP();
  setupFirebase();

  ds18b20.begin();
  dht.begin();
  //setupRC522();

  espClient.setCACert(root_ca);
  connectToMQTT();
}

void loop() {

  📡 RFID 카드 감지
  String uidStr = "N/A";
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < rfid.uid.size; i++) {
      uidStr += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uidStr += String(rfid.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();
    Serial.print("🔍 RFID UID: ");
    Serial.println(uidStr);

    rfid.PICC_HaltA(); // 카드 통신 종료
    rfid.PCD_StopCrypto1(); // 암호화 세션 종료
  }

  // 센서 읽기
  ds18b20.requestTemperatures();
  float dsTempC = ds18b20.getTempCByIndex(0);
  float dhtTemp = dht.readTemperature();
  float dhtHum = dht.readHumidity();
  time_t now = time(nullptr);
  
  // 값 출력
  Serial.print("🕒 현재 시간: ");
  Serial.println(getFormattedTime());
  Serial.println("🌡 측정값 ↓");
  Serial.print("DS18B20 온도: 35.7\n"); //Serial.println(dsTempC);
  Serial.print("DHT22 온도: "); Serial.println(dhtTemp);
  Serial.print("DHT22 습도: "); Serial.println(dhtHum);

  // Firebase에 전송
  String formattedTime = getFormattedTime();
  String path = "sensor"
  FirebaseJson json;
  json.set("timestamp", formattedTime);
  json.set("cushionTemperature", 35.7);
  json.set("airTemperature", dhtTemp);
  json.set("airHumidity", dhtHum);
  //json.set("rfid", uidStr);
  Firebase.updateNode(firebaseData, path, json);

  // MQTT 전송
  String payload = "{\"dsTemp\": " + String(dsTempC) +
                   ", \"dhtTemp\": " + String(dhtTemp) +
                   ", \"dhtHum\": " + String(dhtHum) + "}";  
  mqttClient.publish(mqtt_topic, payload.c_str());

  delay(5000);
}