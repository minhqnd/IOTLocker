#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

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

// HW-898-A TX -> D10. D11 is unused.
SoftwareSerial rfidSerial(10, 11);

// Uno A2 (TX) -> voltage divider -> ESP32 GPIO16.
SoftwareSerial espSerial(A3, A2);

const byte UID_MIN_LENGTH = 6;
const byte UID_MAX_LENGTH = 14;
const unsigned long RFID_FRAME_TIMEOUT = 30;

char mode = 0;
String uidBuffer = "";
unsigned long lastRfidByteAt = 0;

void setup() {
  Serial.begin(9600);
  rfidSerial.begin(9600);
  espSerial.begin(9600);

  lcd.init();
  lcd.backlight();
  showMenu();
  rfidSerial.listen();
}

void loop() {
  readKeypad();

  if (mode != 0) {
    String uid = readUid();
    if (uid.length() > 0) {
      sendRequest(mode, uid);
      mode = 0;
      showWaiting();
      delay(1500);
      showMenu();
      rfidSerial.listen();
    }
  }
}

void readKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
    mode = key;
    uidBuffer = "";
    rfidSerial.listen();
    showScanPrompt(mode);
    return;
  }

  // If the user picked the wrong action, pressing A/B/C/D again replaces it.
}

String readUid() {
  while (rfidSerial.available()) {
    char c = (char)rfidSerial.read();
    lastRfidByteAt = millis();

    if (isHexChar(c)) {
      uidBuffer += c;
      uidBuffer.toUpperCase();

      if (uidBuffer.length() >= UID_MAX_LENGTH) {
        return takeUid();
      }
    } else if (uidBuffer.length() >= UID_MIN_LENGTH) {
      return takeUid();
    } else {
      uidBuffer = "";
    }
  }

  if (uidBuffer.length() >= UID_MIN_LENGTH && millis() - lastRfidByteAt > RFID_FRAME_TIMEOUT) {
    return takeUid();
  }

  return "";
}

String takeUid() {
  String uid = uidBuffer;
  uid.trim();
  uid.toUpperCase();
  uidBuffer = "";

  while (rfidSerial.available()) {
    rfidSerial.read();
  }

  return uid;
}

bool isHexChar(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

void sendRequest(char selectedMode, const String &uid) {
  espSerial.listen();
  delay(30);

  espSerial.print(F("REQ|"));
  espSerial.print(selectedMode);
  espSerial.print('|');
  espSerial.println(uid);

  Serial.print(F("REQ|"));
  Serial.print(selectedMode);
  Serial.print('|');
  Serial.println(uid);
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("A Gui  B Them"));
  lcd.setCursor(0, 1);
  lcd.print(F("C Lay do"));
}

void showScanPrompt(char selectedMode) {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (selectedMode == 'A') lcd.print(F("GUI DO"));
  else if (selectedMode == 'B') lcd.print(F("GUI THEM"));
  else if (selectedMode == 'C') lcd.print(F("LAY DO"));
  else lcd.print(F("RESET TU"));

  lcd.setCursor(0, 1);
  if (selectedMode == 'B') lcd.print(F("Quet the dang gui"));
  else if (selectedMode == 'D') lcd.print(F("Quet the bat ky"));
  else lcd.print(F("Quet the cua ban"));
}

void showWaiting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Da nhan the"));
  lcd.setCursor(0, 1);
  lcd.print(F("Vui long cho"));
}
