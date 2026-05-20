#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

// ================= PIN =================
#define SS_PIN 5
#define RST_PIN 27
#define BUZZER_PIN 4

// ================= API =================
String classApi = "https://dock-bulldog-briskness.ngrok-free.dev/api/external-api/hardware/class-attendance";
String classID = "7a";

// ================= OBJECT =================
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiManager wm;

// ================= UID 8 DIGIT =================
String getUID8Digit(MFRC522 &rfid) {
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  if (uid.length() > 8) {
    uid = uid.substring(0, 8);
  }

  return uid;
}

// ================= BUZZER =================
void beepSuccess() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepFail() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);
  Serial.println("=== PRESENSI KELAS START ===");

  pinMode(BUZZER_PIN, OUTPUT);

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  // ================= WIFI MANAGER =================
  WiFi.mode(WIFI_STA);

  // OPTIONAL:
  // wm.resetSettings();

  bool res;

  // Nama hotspot konfigurasi
  res = wm.autoConnect("ESP32-Presensi");

  if (!res) {

    Serial.println("WiFi Failed");

    lcd.clear();
    lcd.print("WiFi Failed");

    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.print("WiFi Connected");

  delay(2000);

  // ================= RFID =================
  SPI.begin();
  rfid.PCD_Init();

  Serial.println("RFID Ready");

  lcd.clear();
  lcd.print("Scan Kartu...");
}

// ================= LOOP =================
void loop() {

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String cardID = getUID8Digit(rfid);

  Serial.println("----------------------------");
  Serial.print("RFID UID (8) : ");
  Serial.println(cardID);

  lcd.clear();
  lcd.print("Processing...");

  // ================= CHECK WIFI =================
  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi Disconnected");

    lcd.clear();
    lcd.print("WiFi Putus");

    beepFail();

    delay(2000);

    lcd.clear();
    lcd.print("Scan Kartu...");

    return;
  }

  // ================= HTTP REQUEST =================
  HTTPClient http;

  String url = classApi + "?card-id=" + cardID + "&class-id=" + classID;

  Serial.print("Request URL : ");
  Serial.println(url);

  http.begin(url);

  int httpCode = http.GET();

  Serial.print("HTTP Code   : ");
  Serial.println(httpCode);

  // ================= RESPONSE =================
  if (httpCode > 0) {

    String payload = http.getString();

    Serial.println("Response    :");
    Serial.println(payload);

    StaticJsonDocument<512> doc;

    DeserializationError error =
      deserializeJson(doc, payload);

    if (error) {

      Serial.println("JSON Error");

      lcd.clear();
      lcd.print("JSON Error");

      beepFail();

    } else {

      bool status = doc["status"];
      String message = doc["message"];

      lcd.clear();

      if (status == true) {

        String name = doc["data"]["name"];

        Serial.println("========== BERHASIL ==========");
        Serial.print("Nama : ");
        Serial.println(name);

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("PRESENSI OK");

        lcd.setCursor(0, 1);
        lcd.print(name.substring(0, 16));

        beepSuccess();

      } else {

        Serial.println("=========== GAGAL ===========");
        Serial.print("Pesan : ");
        Serial.println(message);

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("PRESENSI GAGAL");

        lcd.setCursor(0, 1);
        lcd.print(message.substring(0, 16));

        beepFail();
      }
    }

  } else {

    Serial.println("HTTP ERROR");

    lcd.clear();
    lcd.print("HTTP ERROR");

    beepFail();
  }

  http.end();

  delay(3000);

  lcd.clear();
  lcd.print("Scan Kartu...");

  rfid.PICC_HaltA();
}