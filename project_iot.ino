#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN 14
#define DHTTYPE DHT22

#define SOIL_PIN 34
#define LED_PIN 26
#define SOIL_THRESHOLD 2000

const char* ssid = ".";
const char* password = "biawakahk";
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// Client ID unik
String clientId = "ESP32Client-" + String((uint32_t)ESP.getEfuseMac(), HEX);

void setup_wifi() {
  delay(100);
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung, IP: " + WiFi.localIP().toString());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Pesan MQTT diterima\nTopik: ");
  Serial.println(topic);
  Serial.print("Pesan: ");

  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  Serial.println(msg);

  if (msg == "on") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED dinyalakan");
  } else if (msg == "off") {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED dimatikan");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT broker...");
    if (client.connect(clientId.c_str())) {
      Serial.println("terhubung");
      client.subscribe("suhel/iot");
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 2 detik");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int soilValue = analogRead(SOIL_PIN);
  Serial.print("Soil Moisture: ");
  Serial.println(soilValue);

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print("°C  Humidity: ");
    Serial.print(h);
    Serial.println("%");

    // Kirim data suhu ke MQTT topik suhu/iot
    String suhuStr = String(t, 2);
    client.publish("suhu/iot", suhuStr.c_str());

    // Kirim data kelembapan tanah ke MQTT topik kelembapan/iot
    String kelembapanStr = String(soilValue);
    client.publish("kelembapan/iot", kelembapanStr.c_str());

  } else {
    Serial.println("Gagal membaca dari sensor DHT!");
  }

  delay(5000); // Kirim data setiap 5 detik
}
