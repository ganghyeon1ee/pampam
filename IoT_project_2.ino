#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include "DHT.h"

// 모터 드라이버의 핀을 정의합니다.
const int motorPin1 = D5;  // 모터 드라이버의 IN1 핀
const int motorPin2 = D6;  // 모터 드라이버의 IN2 핀
const int enaPin = D7;     // 모터 드라이버의 ENA 핀 (PWM 핀)

// 펌프 제어를 위한 핀을 정의합니다.
const int sensorPin = A0;  // 토양 습도 센서 핀

// RGB LED 핀 정의
const int redPin = D4;  // RGB LED의 R 핀
const int greenPin = D3; // RGB LED의 G 핀
const int bluePin = D2; // RGB LED의 B 핀

// Wi-Fi 및 MQTT 설정
const char* ssid = "Ganghyeon"; // Wi-Fi SSID
const char* password = "12340000"; // Wi-Fi Password
const char* mqtt_server = "broker.mqtt-dashboard.com"; // MQTT broker 정보

// 카카오톡 설정
const char* kakaoToken = "S_aEluf1hUMTM_p87IsYll0PJKNY1ektAAAAAQoqJVAAAAGP4jwu6oh6dPOEuoNF"; // 카카오톡 액세스 토큰

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
unsigned long lastSensorRead = 0;
unsigned long lastWateringTime = 0;
const long sensorInterval = 3000;
const long wateringInterval = 5000;  
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

DHT dht(D8, DHT11); // DHT11 습도 센서를 D8 핀에 연결

void setup_wifi() {
  delay(1000); 

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void receive_msg(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == "AIS1517_soil") {
    if (message == "1") {
      stopMotor();
    } else if (message == "2") {
      startMotor();
      sendKakaoMessage("물 주는 중...");
    } else if (message == "3") {
      stopMotor();
    }
  }

  if (String(topic) == "AIS1517_temp") {
    if (message == "1") {
        digitalWrite(redPin, HIGH); 
        digitalWrite(bluePin, LOW);  
        digitalWrite(greenPin, LOW);  
    } else if (message == "2") {
        digitalWrite(redPin, LOW);  
        digitalWrite(bluePin, HIGH); 
        digitalWrite(greenPin, LOW);  
    } else if (message == "3") {
        digitalWrite(redPin, LOW); 
        digitalWrite(bluePin, LOW); 
        digitalWrite(greenPin, HIGH); 
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(WiFi.macAddress());

    Serial.print("Attempting MQTT connection...");
    Serial.print("The client: ");
    Serial.println(clientId);

    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connected");
      client.subscribe("AIS1517_temp");
      client.subscribe("AIS1517_soil");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void sendKakaoMessage(const char* message) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setInsecure(); 

    String url = "https://kapi.kakao.com/v2/api/talk/memo/default/send";

    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + String(kakaoToken));
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "template_object=" + String("{\"object_type\":\"text\",\"text\":\"") + message + String("\",\"link\":{\"web_url\":\"http://www.example.com\"},\"button_title\":\"Visit\"}");

    int httpCode = http.POST(payload);

    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Response:");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpCode);
      String response = http.getString();
      Serial.println(response);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}
void stopAllActions() {
  stopMotor();
}

void startMotor() {
  digitalWrite(motorPin1, HIGH);
  digitalWrite(motorPin2, LOW);
  analogWrite(enaPin, 200); 
  lastWateringTime = millis();
}

void stopMotor() {
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
  analogWrite(enaPin, 0); 
}

void setup() {
  dht.begin(); 
  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(receive_msg);

  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(enaPin, OUTPUT);
  pinMode(redPin, OUTPUT); 
  pinMode(greenPin, OUTPUT); 
  pinMode(bluePin, OUTPUT); 
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  if (now - lastSensorRead >= sensorInterval) {
    lastSensorRead = now;

    int sensorValue = analogRead(sensorPin);  
    int humidity = map(sensorValue, 1023, 0, 0, 100);  

    // DHT11 센서에서 습도 및 온도 읽기
    float humi = dht.readHumidity();
    float temp = dht.readTemperature();

    Serial.print("Sensor Value: ");
    Serial.print(sensorValue);
    Serial.print(" - Humidity: ");
    Serial.print(humidity);
    Serial.print("%, Temperature: ");
    Serial.print(temp);
    Serial.println("°C");

    snprintf(msg, MSG_BUFFER_SIZE, "{\"humidity\":%d}", humidity);
    client.publish("AIS1517_soil_humidity", msg);
  }

  if (now - lastWateringTime >= wateringInterval && digitalRead(motorPin1) == HIGH) {
    stopMotor();
  }

  if (now - lastMsg > 3000) {
    lastMsg = now;

    float humi = dht.readHumidity();
    float temp = dht.readTemperature();

    snprintf(msg, MSG_BUFFER_SIZE, "{\"temp\":%.2f,\"humi\":%.2f}", temp, humi);

    Serial.print("Publish message: ");
    Serial.println(msg);

    client.publish("AIS1517_dht", msg);
    Serial.println("measurement data transfer");
  }
}
