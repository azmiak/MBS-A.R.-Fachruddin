#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MC_Keypad_I2C.h"

#include <SPI.h>
#include <MFRC522.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <ArduinoJson.h>
#include <WiFiManager.h>

// ======================================================
// API URL
// ======================================================
const char* URL_TRANSAKSI =
  "https://dock-bulldog-briskness.ngrok-free.dev/api/external-api/hardware/transaction";

const char* URL_CEK_SALDO =
  "https://dock-bulldog-briskness.ngrok-free.dev/api/external-api/hardware/checkwallet";

// ======================================================
// LCD I2C
// ======================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================================
// KEYPAD I2C
// ======================================================
KeypadI2C keypad(0x21);

// ======================================================
// RFID RC522
// ======================================================
#define SS_PIN 5
#define RST_PIN 27

MFRC522 mfrc522(SS_PIN, RST_PIN);

// ======================================================
// BUZZER
// ======================================================
#define BUZZER_PIN 4

// ======================================================
// WIFI MANAGER
// ======================================================
WiFiManager wm;

// ======================================================
// MODE
// ======================================================
enum Mode {

  MENU,

  TRANSAKSI_INPUT,
  TRANSAKSI_SCAN,
  TRANSAKSI_PIN,

  CEK_SALDO_SCAN,
  CEK_SALDO_PIN
};

Mode mode = MENU;

// ======================================================
// GLOBAL VARIABLE
// ======================================================
String inputTotal = "";
String currentUID = "";
String inputPIN = "";

// ======================================================
// BUZZER FUNCTION
// ======================================================
void beep(int duration) {

  digitalWrite(BUZZER_PIN, HIGH);

  delay(duration);

  digitalWrite(BUZZER_PIN, LOW);
}

void beepKey() {

  beep(30);
}

void beepCard() {

  beep(120);
}

void beepSuccess() {

  beep(100);

  delay(80);

  beep(100);
}

void beepFail() {

  beep(500);
}

// ======================================================
// FORMAT RUPIAH
// ======================================================
String formatRupiah(long value) {

  String s = String(value);
  String out = "";

  int count = 0;

  for (int i = s.length() - 1; i >= 0; i--) {

    out = s[i] + out;

    count++;

    if (count == 3 && i != 0) {

      out = "." + out;

      count = 0;
    }
  }

  return out;
}

// ======================================================
// SHOW MENU
// ======================================================
void showMenu() {

  lcd.clear();

  delay(5);

  lcd.setCursor(0, 0);
  lcd.print("A:Transaksi   ");

  lcd.setCursor(0, 1);
  lcd.print("B:Cek Saldo   ");

  mode = MENU;
}

// ======================================================
// RFID READ UID
// ======================================================
String readUID() {

  if (!(mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())) {
    return "";
  }

  String id = "";

  for (byte i = 0; i < mfrc522.uid.size; i++) {

    if (mfrc522.uid.uidByte[i] < 0x10)
      id += "0";

    id += String(
      mfrc522.uid.uidByte[i],
      HEX);
  }

  id.toUpperCase();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  return id;
}

// ======================================================
// WIFI CHECK
// ======================================================
bool ensureWiFi() {

  if (WiFi.status() == WL_CONNECTED)
    return true;

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Reconnect WiFi");

  WiFi.reconnect();

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {

    delay(300);
  }

  return WiFi.status() == WL_CONNECTED;
}

// ======================================================
// API TRANSAKSI
// ======================================================
bool apiTransaksi(
  const String& uid,
  const String& pin,
  const String& total,
  int& saldoAkhir,
  String& errMsg) {

  saldoAkhir = -1;
  errMsg = "";

  if (!ensureWiFi()) {

    errMsg = "WiFi";

    return false;
  }

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  String url =
    String(URL_TRANSAKSI) + "?card-id=" + uid + "&pin=" + pin + "&total=" + total;

  Serial.println(url);

  http.begin(client, url);

  int code = http.GET();

  Serial.print("HTTP CODE: ");
  Serial.println(code);

  if (code <= 0) {

    http.end();

    errMsg = "HTTP";

    return false;
  }

  String response = http.getString();

  Serial.println(response);

  http.end();

  StaticJsonDocument<512> doc;

  DeserializationError err =
    deserializeJson(doc, response);

  if (err) {

    errMsg = "JSON";

    return false;
  }

  bool ok = doc["status"];

  if (ok) {

    if (doc["data"].containsKey("balance")) {

      saldoAkhir =
        doc["data"]["balance"];
    }

    return true;
  }

  errMsg =
    (const char*)doc["message"];

  return false;
}

// ======================================================
// API CEK SALDO
// ======================================================
bool apiCekSaldo(
  const String& uid,
  const String& pin,
  long& balance,
  String& errMsg) {

  balance = -1;
  errMsg = "";

  if (!ensureWiFi()) {

    errMsg = "WiFi";

    return false;
  }

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  String url =
    String(URL_CEK_SALDO) + "?card-id=" + uid + "&pin=" + pin;

  Serial.println(url);

  http.begin(client, url);

  int code = http.GET();

  Serial.print("HTTP CODE: ");
  Serial.println(code);

  if (code <= 0) {

    http.end();

    errMsg = "HTTP";

    return false;
  }

  String response = http.getString();

  Serial.println(response);

  http.end();

  StaticJsonDocument<512> doc;

  DeserializationError err =
    deserializeJson(doc, response);

  if (err) {

    errMsg = "JSON";

    return false;
  }

  bool ok = doc["status"];

  if (ok) {

    balance =
      doc["data"]["balance"];

    return true;
  }

  errMsg =
    (const char*)doc["message"];

  return false;
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(115200);

  // ====================================================
  // I2C
  // ====================================================
  Wire.begin();

  // ====================================================
  // LCD
  // ====================================================
  lcd.init();
  lcd.backlight();

  // ====================================================
  // BUZZER
  // ====================================================
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // ====================================================
  // KEYPAD
  // ====================================================
  keypad.begin();

  // ====================================================
  // WIFI MANAGER
  // ====================================================
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Setup WiFi");

  bool res = wm.autoConnect(
    "ESP32-Transaksi",
    "12345678");

  if (!res) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");

    beepFail();

    delay(3000);

    ESP.restart();
  }

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");

  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  beepSuccess();

  delay(2000);

  // ====================================================
  // RFID
  // ====================================================
  SPI.begin();

  mfrc522.PCD_Init();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("RFID Ready");

  beepSuccess();

  delay(1500);

  showMenu();
}

// ======================================================
// LOOP
// ======================================================
void loop() {

  char key = keypad.getKey();

  switch (mode) {

    // ==================================================
    // MENU
    // ==================================================
    case MENU:
      {

        if (key == 'A') {

          beepKey();

          inputTotal = "";

          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("Input Total");

          lcd.setCursor(0, 1);
          lcd.print("Rp:");

          mode = TRANSAKSI_INPUT;
        }

        else if (key == 'B') {

          beepKey();

          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("Cek Saldo");

          lcd.setCursor(0, 1);
          lcd.print("Tap Kartu");

          mode = CEK_SALDO_SCAN;
        }

        break;
      }

    // ==================================================
    // INPUT TOTAL
    // ==================================================
    case TRANSAKSI_INPUT:
      {

        if (key) {

          beepKey();

          if (key >= '0' && key <= '9') {

            if (inputTotal.length() < 7) {
              inputTotal += key;
            }
          }

          else if (key == '*') {

            if (inputTotal.length() > 0) {

              inputTotal.remove(
                inputTotal.length() - 1);
            }
          }

          else if (key == 'D') {

            inputTotal = "";
          }

          else if (key == '#') {

            if (inputTotal.length() > 0) {

              lcd.clear();

              lcd.setCursor(0, 0);
              lcd.print("Tap Kartu");

              mode = TRANSAKSI_SCAN;
            }
          }

          lcd.setCursor(0, 1);

          lcd.print("Rp:");
          lcd.print(inputTotal);
          lcd.print("       ");
        }

        if (key == 'C') {

          beepKey();

          showMenu();
        }

        break;
      }

    // ==================================================
    // SCAN KARTU TRANSAKSI
    // ==================================================
    case TRANSAKSI_SCAN:
      {

        String id = readUID();

        if (id != "") {

          beepCard();

          currentUID = id;

          inputPIN = "";

          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("Masukkan PIN");

          lcd.setCursor(0, 1);
          lcd.print("PIN:");

          mode = TRANSAKSI_PIN;
        }

        if (key == 'C') {

          beepKey();

          showMenu();
        }

        break;
      }

    // ==================================================
    // INPUT PIN TRANSAKSI
    // ==================================================
    case TRANSAKSI_PIN:
      {

        if (key) {

          beepKey();

          if (key >= '0' && key <= '9') {

            if (inputPIN.length() < 6) {
              inputPIN += key;
            }
          }

          else if (key == '*') {

            if (inputPIN.length() > 0) {

              inputPIN.remove(
                inputPIN.length() - 1);
            }
          }

          else if (key == '#') {

            if (inputPIN.length() >= 4) {

              lcd.clear();

              lcd.setCursor(0, 0);
              lcd.print("Processing...");

              int saldoAkhir = -1;

              String err;

              bool ok =
                apiTransaksi(
                  currentUID,
                  inputPIN,
                  inputTotal,
                  saldoAkhir,
                  err);

              lcd.clear();

              if (ok) {

                beepSuccess();

                lcd.setCursor(0, 0);
                lcd.print("Transaksi OK");

                lcd.setCursor(0, 1);

                lcd.print("Rp");
                lcd.print(
                  formatRupiah(saldoAkhir));

              } else {

                beepFail();

                lcd.setCursor(0, 0);
                lcd.print("Transaksi Ggl");

                lcd.setCursor(0, 1);

                lcd.print(err);
              }

              delay(3000);

              showMenu();
            }
          }

          lcd.setCursor(4, 1);

          // bersihkan area PIN
          lcd.print("            ");

          lcd.setCursor(4, 1);

          for (int i = 0; i < inputPIN.length(); i++) {
            lcd.print("*");
          }

          lcd.print("      ");
        }

        if (key == 'C') {

          beepKey();

          showMenu();
        }

        break;
      }

    // ==================================================
    // SCAN KARTU CEK SALDO
    // ==================================================
    case CEK_SALDO_SCAN:
      {

        String id = readUID();

        if (id != "") {

          beepCard();

          currentUID = id;

          inputPIN = "";

          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("Masukkan PIN");

          lcd.setCursor(0, 1);
          lcd.print("PIN:");

          mode = CEK_SALDO_PIN;
        }

        if (key == 'C') {

          beepKey();

          showMenu();
        }

        break;
      }

    // ==================================================
    // INPUT PIN CEK SALDO
    // ==================================================
    case CEK_SALDO_PIN:
      {

        if (key) {

          beepKey();

          if (key >= '0' && key <= '9') {

            if (inputPIN.length() < 6) {
              inputPIN += key;
            }
          }

          else if (key == '*') {

            if (inputPIN.length() > 0) {

              inputPIN.remove(
                inputPIN.length() - 1);
            }
          }

          else if (key == '#') {

            if (inputPIN.length() >= 4) {

              lcd.clear();

              lcd.setCursor(0, 0);
              lcd.print("Checking...");

              long balance;

              String err;

              bool ok =
                apiCekSaldo(
                  currentUID,
                  inputPIN,
                  balance,
                  err);

              lcd.clear();

              if (ok) {

                beepSuccess();

                lcd.setCursor(0, 0);
                lcd.print("Saldo:");

                lcd.setCursor(0, 1);

                lcd.print("Rp");
                lcd.print(
                  formatRupiah(balance));

              } else {

                beepFail();

                lcd.setCursor(0, 0);
                lcd.print("Check Failed");

                lcd.setCursor(0, 1);

                lcd.print(err);
              }

              delay(3000);

              showMenu();
            }
          }

          lcd.setCursor(4, 1);

          // bersihkan area PIN
          lcd.print("            ");

          lcd.setCursor(4, 1);

          for (int i = 0; i < inputPIN.length(); i++) {
            lcd.print("*");
          }

          lcd.print("      ");
        }

        if (key == 'C') {

          beepKey();

          showMenu();
        }

        break;
      }
  }
}