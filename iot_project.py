import network
import urequests
import machine
import time
import ntptime
import dht
from machine import Pin, ADC
from umqtt.simple import MQTTClient

# === Konfigurasi WiFi & MQTT ===
WIFI_SSID = "pakein"
WIFI_PASS = "09090909"
MQTT_BROKER = "broker.emqx.io"
MQTT_TOPIC = b"suhel/iot"

# === Firebase ===
FIREBASE_API_KEY = "AIzaSyAudzddrF8l-VxvmAD-oQr8-RtkPrzHt80"
PROJECT_ID = "irrigation-3395c"
COLLECTION = "Riwayat"

# === Pin Sensor ===
DHT_PIN = 14
SOIL_PIN = 34
LED_PIN = 25

# === Inisialisasi ===
dht_sensor = dht.DHT22(Pin(DHT_PIN))
soil_sensor = ADC(Pin(SOIL_PIN))
soil_sensor.atten(ADC.ATTN_11DB)  # Rentang hingga 3.6V
led = Pin(LED_PIN, Pin.OUT)

# === Fungsi WiFi ===
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print("Menghubungkan ke WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASS)
        while not wlan.isconnected():
            time.sleep(0.5)
            print(".", end="")
    print("\nWiFi terhubung:", wlan.ifconfig())

# === Sinkronisasi waktu NTP ===
def sync_time():
    try:
        ntptime.settime()
        print("🕒 Waktu tersinkronisasi.")
    except:
        print("❌ Gagal sinkronisasi waktu.")

# === Format ISO Timestamp ===
def get_timestamp():
    tm = time.gmtime()
    return "%04d-%02d-%02dT%02d:%02d:%02dZ" % (tm[0], tm[1], tm[2], tm[3], tm[4], tm[5])

# === Kirim ke Firebase ===
def send_to_firebase(status, suhu, kelembapan):
    url = f"https://firestore.googleapis.com/v1/projects/{PROJECT_ID}/databases/(default)/documents/{COLLECTION}?key={FIREBASE_API_KEY}"

    data = {
        "fields": {
            "status": {"stringValue": status},
            "suhu": {"doubleValue": round(suhu, 2)},
            "kelembapan": {"integerValue": kelembapan},
            "waktu": {"timestampValue": get_timestamp()}
        }
    }

    try:
        headers = {"Content-Type": "application/json"}
        response = urequests.post(url, json=data, headers=headers)
        print("✅ Data dikirim ke Firebase.")
        print("Kode HTTP:", response.status_code)
        print("Respon:", response.text)
        response.close()
    except Exception as e:
        print("❌ Gagal kirim ke Firebase:", e)

# === Callback MQTT ===
def mqtt_callback(topic, msg):
    print("📩 Pesan MQTT:", msg)
    dht_sensor.measure()
    suhu = dht_sensor.temperature()
    kelembapan = soil_sensor.read()

    if msg == b'on':
        led.value(1)
        print("⚡ Pompa dinyalakan")
        send_to_firebase("ON", suhu, kelembapan)
    elif msg == b'off':
        led.value(0)
        print("💡 Pompa dimatikan")
        send_to_firebase("OFF", suhu, kelembapan)

# === Setup MQTT ===
def setup_mqtt():
    client_id = b"ESP32Client"
    client = MQTTClient(client_id, MQTT_BROKER)
    client.set_callback(mqtt_callback)
    client.connect()
    client.subscribe(MQTT_TOPIC)
    print("📡 Terhubung ke MQTT broker.")
    return client

# === MAIN ===
connect_wifi()
sync_time()
mqtt_client = setup_mqtt()

last_send = time.ticks_ms()
INTERVAL = 5000  # 5 detik

while True:
    mqtt_client.check_msg()

    if time.ticks_diff(time.ticks_ms(), last_send) > INTERVAL:
        last_send = time.ticks_ms()

        try:
            dht_sensor.measure()
            suhu = dht_sensor.temperature()
            kelembapan = soil_sensor.read()

            print("🌡 Suhu:", suhu, "°C")
            print("🌱 Kelembapan tanah:", kelembapan)

            mqtt_client.publish(b"suhu/iot", str(suhu))
            mqtt_client.publish(b"kelembapan/iot", str(kelembapan))

        except Exception as e:
            print("❗ Gagal membaca sensor:", e)

    time.sleep(0.1)