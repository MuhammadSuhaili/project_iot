

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#define DHTPIN 14
#define DHTTYPE DHT22
#define SOIL_PIN 34
#define RELAY_PIN 32

const char* ssid = "suhel";
const char* password = "tes123456";
const char* mqtt_server = "broker.emqx.io";

// Ganti dengan API Key Firebase kamu
const char* firebaseApiKey = "AIzaSyAudzddrF8l-VxvmAD-oQr8-RtkPrzHt80";  

// Firebase config
const char* projectId = "irrigation-3395c";
const char* collection = "Riwayat";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

String clientId = "ESP32Client-" + String((uint32_t)ESP.getEfuseMac(), HEX);

// ==========================
// Setup waktu dari NTP
// ==========================
void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sinkronisasi waktu...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" selesai.");
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "";
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

// ==========================
// Koneksi WiFi
// ==========================
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

// ==========================
// Kirim data ke Firebase
// ==========================
void sendToFirebase(const String& status, float suhu, int kelembapan) {
  HTTPClient https;
  WiFiClientSecure clientSecure;
  clientSecure.setInsecure(); // Bypass TLS

  String url = "https://firestore.googleapis.com/v1/projects/" + String(projectId)
             + "/databases/(default)/documents/" + String(collection)
             + "?key=" + firebaseApiKey;

  String payload = "{"
    "\"fields\": {"
      "\"status\": {\"stringValue\": \"" + status + "\"},"
      "\"suhu\": {\"doubleValue\": " + String(suhu, 2) + "},"
      "\"kelembapan\": {\"integerValue\": " + String(kelembapan) + "},"
      "\"waktu\": {\"timestampValue\": \"" + getTimestamp() + "\"}"
    "}"
  "}";

  https.begin(clientSecure, url);
  https.addHeader("Content-Type", "application/json");

  int httpCode = https.POST(payload);
  String response = https.getString();

  if (httpCode == 200 || httpCode == 202) {
    Serial.println("✅ Data berhasil dikirim ke Firebase!");
  } else {
    Serial.println("❌ Gagal mengirim data ke Firebase.");
  }

  Serial.print("Kode respons HTTP: ");
  Serial.println(httpCode);
  Serial.print("Respons Firebase: ");
  Serial.println(response);

  https.end();
}

// ==========================
// Callback MQTT
// ==========================
void callback(char* topic, byte* message, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }

  Serial.println("📩 Pesan MQTT diterima: " + msg);

  float t = dht.readTemperature();
  int soilValue = analogRead(SOIL_PIN);

  if (msg == "on") {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("⚡ Pompa dinyalakan");
    sendToFirebase("ON", t, soilValue);
  } else if (msg == "off") {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("💡 Pompa dimatikan");
    sendToFirebase("OFF", t, soilValue);
  }
}

// ==========================
// Reconnect MQTT
// ==========================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT broker...");
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung");
      client.subscribe("suhel/iot");
    } else {
      Serial.print("Gagal, kode: ");
      Serial.print(client.state());
      Serial.println(" coba lagi 2 detik...");
      delay(2000);
    }
  }
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  dht.begin();
  setup_wifi();
  setupTime();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// ==========================
// LOOP
// ==========================
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

    client.publish("suhu/iot", String(t, 2).c_str());
    client.publish("kelembapan/iot", String(soilValue).c_str());
  } else {
    Serial.println("❗ Gagal membaca sensor DHT!");
  }

  delay(5000); // interval 5 detik
}
