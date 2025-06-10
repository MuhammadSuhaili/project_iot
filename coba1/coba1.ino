#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Ganti dengan data kamu
const char* ssid = "iotot";
const char* password = "mahalbang";

const String firebaseApiKey = "AIzaSyDMF3F0XwgSf9EbSF3GfTZV2QtcZWVwHQE";
const String projectId = "irrigation-38972";
const String collection = "Riwayat";

const int pompaPin = 2; // GPIO untuk pompa
const int sensorSuhuPin = 34;  // ADC input untuk sensor suhu
const int sensorSoilPin = 35;   // ADC input untuk kelembapan tanah

bool pompaStatus = false;

// Fungsi untuk membaca sensor suhu (contoh sederhana pakai sensor analog)
float bacaSuhu() {
  int adcValue = analogRead(sensorSuhuPin);
  // Konversi ADC ke suhu (ini contoh asumsi sensor TMP36 atau sejenis)
  float voltage = adcValue * (3.3 / 4095.0);
  float tempC = (voltage - 0.5) * 100.0;
  return tempC;
}

// Fungsi untuk baca kelembapan tanah
int bacaKelembapan() {
  int soilValue = analogRead(sensorSoilPin);
  // Bisa ditambahkan kalibrasi sesuai sensor
  return soilValue;
}

// Fungsi ambil timestamp ISO8601 untuk Firebase
String getTimestamp() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "";
  }
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

void setupWiFi() {
  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Terhubung, IP: ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Sinkronisasi waktu untuk timestamp
  Serial.println("Waktu sinkron...");
}

// Fungsi untuk update dokumen Riwayat/current di Firestore dengan PATCH
void sendToFirebase(const String& status, float suhu, int soilMoisture) {
  WiFiClientSecure client;
  client.setInsecure(); // Untuk development, bypass sertifikat SSL

  HTTPClient https;

  String url = "https://firestore.googleapis.com/v1/projects/" + projectId +
               "/databases/(default)/documents/" + collection + "/current?key=" + firebaseApiKey;

  String payload = "{"
                   "\"fields\": {"
                   "\"status_pompa\": {\"stringValue\": \"" + status + "\"},"
                   "\"suhu\": {\"doubleValue\": " + String(suhu, 2) + "},"
                   "\"kelembapan\": {\"integerValue\": " + String(soilMoisture) + "},"
                   "\"waktu\": {\"timestampValue\": \"" + getTimestamp() + "\"}"
                   "}"
                   "}";

  Serial.println("Mengirim data ke Firebase:");
  Serial.println(payload);

  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");

  int httpCode = https.sendRequest("PATCH", payload);
  String response = https.getString();

  if (httpCode == 200) {
    Serial.println("✅ Data berhasil diupdate di Firebase (Riwayat/current)!");
  } else {
    Serial.printf("❌ Gagal update data, HTTP code: %d\n", httpCode);
    Serial.println("Response: " + response);
  }

  https.end();
}

// Fungsi kontrol pompa
void setPompa(bool status) {
  pompaStatus = status;
  digitalWrite(pompaPin, pompaStatus ? HIGH : LOW);
  Serial.printf("Pompa %s\n", pompaStatus ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  pinMode(pompaPin, OUTPUT);
  setPompa(false); // Pompa mati awalnya
  setupWiFi();
}

unsigned long lastSend = 0;
const unsigned long interval = 10000; // Kirim data tiap 10 detik

void loop() {
  // Contoh simulasi: nyalakan pompa setiap 30 detik selama 10 detik
  static unsigned long lastToggle = 0;
  unsigned long now = millis();

  if (now - lastToggle > 30000) {
    setPompa(true);
    lastToggle = now;
  }
  if (now - lastToggle > 10000 && pompaStatus == true) {
    setPompa(false);
  }

  if (now - lastSend > interval) {
    lastSend = now;

    float suhu = bacaSuhu();
    int kelembapan = bacaKelembapan();

    Serial.printf("Suhu: %.2f °C, Kelembapan tanah: %d\n", suhu, kelembapan);

    sendToFirebase(pompaStatus ? "ON" : "OFF", suhu, kelembapan);
  }
}
