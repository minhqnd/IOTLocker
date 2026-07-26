#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <ctype.h>

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

// Uno A2 (TX) -> chia ap -> ESP32 GPIO16; ESP32 GPIO17 -> Uno A3 (RX).
SoftwareSerial espSerial(A3, A2);

const byte UID_MIN_LENGTH = 6;
const byte UID_MAX_LENGTH = 14;
const unsigned long RFID_FRAME_TIMEOUT = 30;

char mode = 0;
String uidBuffer = "";
String espBuffer = "";
unsigned long lastRfidByteAt = 0;

enum UiState { UI_IDLE, UI_BUSY, UI_PAY_MENU, UI_PAY_WAIT };
UiState uiState = UI_IDLE;

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
  if (uiState != UI_IDLE)
    readEspUi();

  readKeypad();

  if (uiState == UI_IDLE && mode != 0) {
    String uid = readUid();
    if (uid.length() > 0) {
      sendRequest(mode, uid);
      mode = 0;
      uiState = UI_BUSY;
      showWaiting();
      espSerial.listen();
    }
  }
}

void readKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  if (uiState != UI_IDLE) {
    bool canChoosePayment = uiState == UI_PAY_MENU &&
                            (key == '1' || key == '2' || key == '*');
    bool canCancelPayment = uiState == UI_PAY_WAIT && key == '*';
    if (canChoosePayment || canCancelPayment) {
      sendKey(key);
      uiState = UI_BUSY;
      showSendingKey(key);
    }
    return;
  }

  if (key == '*' && mode != 0) {
    mode = 0;
    uidBuffer = "";
    showMenu();
    return;
  }

  if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
    mode = key;
    uidBuffer = "";
    rfidSerial.listen();
    showScanPrompt(mode);
    return;
  }

  // If the user picked the wrong action, pressing A/B/C/D again replaces it.
}

void sendKey(char key) {
  espSerial.listen();
  delay(30);
  espSerial.print(F("KEY|"));
  espSerial.println(key);

  Serial.print(F("KEY|"));
  Serial.println(key);
  espSerial.listen();
}

void readEspUi() {
  while (espSerial.available()) {
    char c = espSerial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      espBuffer.trim();
      handleEspUi(espBuffer);
      espBuffer = "";
      continue;
    }

    if (espBuffer.length() < 24)
      espBuffer += c;
  }
}

void handleEspUi(const String &message) {
  if (message == "UI|PAY_MENU") {
    uiState = UI_PAY_MENU;
    showPaymentMenu();
  } else if (message == "UI|PAY_WAIT") {
    uiState = UI_PAY_WAIT;
    showPaymentWait();
  } else if (message == "UI|BUSY") {
    uiState = UI_BUSY;
    showWaiting();
  } else if (message == "UI|IDLE") {
    uiState = UI_IDLE;
    mode = 0;
    showMenu();
    rfidSerial.listen();
  }
}

String readUid() {
  while (rfidSerial.available()) {
    char c = (char)rfidSerial.read();
    lastRfidByteAt = millis();

    if (isxdigit((unsigned char)c)) {
      uidBuffer += (char)toupper((unsigned char)c);

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
  if (selectedMode == 'B') lcd.print(F("Quet the"));
  else if (selectedMode == 'D') lcd.print(F("Quet the bat ky"));
  else lcd.print(F("Quet the cua ban"));
}

void showWaiting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Dang xu ly"));
  lcd.setCursor(0, 1);
  lcd.print(F("Vui long cho"));
}

void showPaymentMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Vui long chon"));
  lcd.setCursor(0, 1);
  lcd.print(F("1:QR 2:The *:Huy"));
}

void showPaymentWait() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Quet QR de TT"));
  lcd.setCursor(0, 1);
  lcd.print(F("Bam * de huy"));
}

void showSendingKey(char key) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(key == '*' ? F("Dang huy") : F("Dang xu ly"));
  lcd.setCursor(0, 1);
  lcd.print(F("Vui long cho"));
}
