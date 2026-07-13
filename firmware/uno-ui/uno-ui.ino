#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// LCD I2C. If the screen stays blank, try 0x3F instead of 0x27.
LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// HW-898-A TX -> Uno D10. D11 is an unused SoftwareSerial TX pin.
SoftwareSerial rfidSerial(10, 11);

// Uno A2 (TX) -> voltage divider -> ESP32 GPIO16.
// ESP32 GPIO17 -> Uno A3 (RX).
SoftwareSerial espSerial(A3, A2);

const byte UID_MIN_LENGTH = 6;
const byte UID_MAX_LENGTH = 14;
const unsigned long RFID_FRAME_TIMEOUT = 30;
const unsigned long DUPLICATE_DELAY = 1500;
const unsigned long ESP_TIMEOUT = 45000UL;

char selectedMode = 0;
String rfidBuffer = "";
unsigned long lastRfidByteAt = 0;
String lastUid = "";
unsigned long lastUidAt = 0;

void showMainMenu();
void showScanPrompt(char mode);
void showProcessing();
void showDone();
void showError(const String &message);
void readKeypad();
bool readRfid(String &uid);
bool finishRfidFrame(String &uid);
void handleUid(const String &uid);
String sendRequestToEsp32(char mode, const String &uid);
void showEspResponse(const String &response);
String messageForEspCode(const String &code, const String &locker);

void setup() {
  Serial.begin(9600);
  rfidSerial.begin(9600);
  espSerial.begin(9600);

  lcd.init();
  lcd.backlight();
  showMainMenu();

  rfidSerial.listen();
  Serial.println(F("UNO V2 READY"));
}

void loop() {
  readKeypad();

  if (selectedMode != 0) {
    String uid;
    if (readRfid(uid)) {
      handleUid(uid);
    }
  }
}

void readKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  Serial.print(F("Key: "));
  Serial.println(key);

  if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
    selectedMode = key;
    rfidBuffer = "";
    rfidSerial.listen();
    showScanPrompt(key);
    return;
  }

  if (key == '*') {
    selectedMode = 0;
    rfidBuffer = "";
    showMainMenu();
    rfidSerial.listen();
  }
}

void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("A:Gui B:Them"));
  lcd.setCursor(0, 1);
  lcd.print(F("C:Lay  *:Huy"));
}

void showScanPrompt(char mode) {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (mode == 'A') lcd.print(F("Che do A: Gui"));
  else if (mode == 'B') lcd.print(F("Che do B: Them"));
  else if (mode == 'C') lcd.print(F("Che do C: Lay"));
  else lcd.print(F("MASTER: Reset"));

  lcd.setCursor(0, 1);
  lcd.print(F("Hay quet the..."));
}

void showProcessing() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Dang xu ly..."));
  lcd.setCursor(0, 1);
  lcd.print(F("Vui long cho"));
}

void showDone() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Da xong!"));
  lcd.setCursor(0, 1);
  lcd.print(F("Xem OLED"));
}

void showError(const String &message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Co loi"));
  lcd.setCursor(0, 1);
  lcd.print(message.substring(0, 16));
}

bool readRfid(String &uid) {
  while (rfidSerial.available()) {
    byte value = rfidSerial.read();
    lastRfidByteAt = millis();

    bool isUidChar =
      (value >= '0' && value <= '9') ||
      (value >= 'A' && value <= 'F') ||
      (value >= 'a' && value <= 'f');

    if (isUidChar) {
      rfidBuffer += (char)value;
      if (rfidBuffer.length() >= UID_MAX_LENGTH) {
        return finishRfidFrame(uid);
      }
      continue;
    }

    // Space, CR, LF, STX, ETX, or other delimiters end a UID frame.
    if (rfidBuffer.length() >= UID_MIN_LENGTH) {
      return finishRfidFrame(uid);
    }

    rfidBuffer = "";
  }

  if (rfidBuffer.length() >= UID_MIN_LENGTH && millis() - lastRfidByteAt > RFID_FRAME_TIMEOUT) {
    return finishRfidFrame(uid);
  }

  return false;
}

bool finishRfidFrame(String &uid) {
  rfidBuffer.trim();

  if (rfidBuffer.length() < UID_MIN_LENGTH) {
    rfidBuffer = "";
    return false;
  }

  uid = rfidBuffer;
  uid.toUpperCase();
  rfidBuffer = "";

  if (uid == lastUid && millis() - lastUidAt < DUPLICATE_DELAY) {
    return false;
  }

  lastUid = uid;
  lastUidAt = millis();

  while (rfidSerial.available()) {
    rfidSerial.read();
  }

  return true;
}

void handleUid(const String &uid) {
  Serial.print(F("UID=["));
  Serial.print(uid);
  Serial.print(F("] len="));
  Serial.println(uid.length());

  showProcessing();
  String response = sendRequestToEsp32(selectedMode, uid);

  selectedMode = 0;
  showEspResponse(response);
  delay(2500);
  showMainMenu();
  rfidSerial.listen();
}

String sendRequestToEsp32(char mode, const String &uid) {
  espSerial.listen();
  delay(30);

  espSerial.print(F("REQ|"));
  espSerial.print(mode);
  espSerial.print('|');
  espSerial.println(uid);

  Serial.print(F("Gui ESP32: REQ|"));
  Serial.print(mode);
  Serial.print('|');
  Serial.println(uid);

  String response = "";
  unsigned long startedAt = millis();

  while (millis() - startedAt < ESP_TIMEOUT) {
    while (espSerial.available()) {
      char c = espSerial.read();

      if (c == '\n') {
        response.trim();
        Serial.print(F("ESP32 tra ve: "));
        Serial.println(response);
        rfidSerial.listen();
        return response;
      }

      if (c != '\r') response += c;
    }
  }

  rfidSerial.listen();
  return "ERR|0|ESP_TIMEOUT";
}

void showEspResponse(const String &response) {
  int p1 = response.indexOf('|');
  int p2 = response.indexOf('|', p1 + 1);

  if (p1 < 0 || p2 < 0) {
    showError(response);
    return;
  }

  String status = response.substring(0, p1);
  String locker = response.substring(p1 + 1, p2);
  String code = response.substring(p2 + 1);
  String message = messageForEspCode(code, locker);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(status == "OK" ? F("Thanh cong") : F("Khong thanh cong"));
  lcd.setCursor(0, 1);
  lcd.print(message.substring(0, 16));
}

String messageForEspCode(const String &code, const String &locker) {
  if (code == "DEPOSIT_OK") return "Da gui hoc " + locker;
  if (code == "REOPEN_OK") return "Mo lai hoc " + locker;
  if (code == "PICKUP_OK") return "Da lay hoc " + locker;
  if (code == "RESET_OK") return "Da reset";
  if (code == "ALREADY_HAS") return "The da co hoc";
  if (code == "FULL") return "Tu da day";
  if (code == "NOT_FOUND") return "Chua gui do";
  if (code == "NOT_OPENED") return "Chua mo cua";
  if (code == "DOOR_OPEN") return "Cua chua dong";
  if (code == "BAD_UID") return "The loi";
  if (code == "BAD_MODE") return "Sai che do";
  if (code == "BAD_REQUEST") return "Loi yeu cau";
  if (code == "ESP_TIMEOUT") return "ESP timeout";
  return code;
}
