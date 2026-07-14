#include <Preferences.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum DoorResult {
  DOOR_OK,
  DOOR_NOT_OPENED,
  DOOR_LEFT_OPEN
};

const int LOCKER_COUNT = 2;
const int SERVO_PINS[LOCKER_COUNT] = {18, 23};
const int DOOR_PINS[LOCKER_COUNT] = {19, 27};

const int SERVO_LOCK_ANGLE = 90;
const int SERVO_UNLOCK_ANGLE = 0;
const int DOOR_CLOSED_STATE = LOW;
const int DOOR_OPEN_STATE = HIGH;

const unsigned long WAIT_OPEN_TIMEOUT = 10000UL;
const unsigned long WAIT_CLOSE_TIMEOUT = 30000UL;
const unsigned long DOOR_ALARM_AFTER = 15000UL;
const unsigned long RESULT_HOLD = 3000UL;

const byte UID_MIN_LENGTH = 6;
const byte UID_MAX_LENGTH = 14;
const byte REQUEST_MAX_LENGTH = 32;

const int BUZZER_PIN = 4;
const bool BUZZER_ACTIVE_HIGH = true;

const int UNO_RX_PIN = 16;
const int UNO_TX_PIN = 17;
HardwareSerial UnoSerial(2);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledReady = false;

Servo lockerServo[LOCKER_COUNT];
String lockerUid[LOCKER_COUNT];
Preferences preferences;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  UnoSerial.begin(9600, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  Wire.begin(21, 22);
  Wire.setClock(100000);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledReady) {
    display.setTextColor(SSD1306_WHITE);
    showText("SMART LOCKER", "Khoi dong...");
  }

  preferences.begin("lockers", false);
  for (int i = 0; i < LOCKER_COUNT; i++) {
    lockerServo[i].setPeriodHertz(50);
    lockerServo[i].attach(SERVO_PINS[i], 500, 2400);
    pinMode(DOOR_PINS[i], INPUT_PULLUP);
    lockDoor(i);
    loadLocker(i);
  }

  printLockerState();
  showIdle();
  Serial.println(F("ESP32 V2 READY"));
}

void loop() {
  readUnoRequest();
}

void readUnoRequest() {
  static String line = "";

  while (UnoSerial.available()) {
    char c = UnoSerial.read();

    if (c == '\n') {
      line.trim();
      if (line.length() > 0) processRequest(line);
      line = "";
      continue;
    }

    if (c == '\r') continue;

    if (line.length() >= REQUEST_MAX_LENGTH) {
      line = "";
      report(false, 0, "BAD_REQUEST", "LOI YEU CAU");
      continue;
    }

    line += c;
  }
}

void processRequest(const String &request) {
  Serial.print(F("Nhan Uno: "));
  Serial.println(request);

  int separator1 = request.indexOf('|');
  int separator2 = request.indexOf('|', separator1 + 1);

  // chi nhan dung format tu Uno: REQ|MODE|UID, sai thi bo luon cho de debug
  if (separator1 < 0 || separator2 < 0 || !request.startsWith("REQ|")) {
    report(false, 0, "BAD_REQUEST", "LOI YEU CAU");
    returnToIdle();
    return;
  }

  String modeText = request.substring(separator1 + 1, separator2);
  String uid = request.substring(separator2 + 1);
  modeText.trim();
  uid.trim();
  uid.toUpperCase();

  if (modeText.length() != 1) {
    report(false, 0, "BAD_REQUEST", "LOI YEU CAU");
    returnToIdle();
    return;
  }

  char mode = modeText.charAt(0);
  Serial.print(F("UID="));
  Serial.print(uid);
  Serial.print(F(" mode="));
  Serial.println(mode);

  // hard reset - dang demo - cai nay dung se dung master key, hoac dung passcode tu server
  if (mode == 'D') {
    beepScanOk();
    handleReset();
    returnToIdle();
    return;
  }

  if (!isValidUid(uid)) {
    report(false, 0, "BAD_UID", "THE KHONG HOP LE");
    returnToIdle();
    return;
  }

  beepScanOk();
  showText("DA NHAN THE", "Dang xu ly...");

  if (mode == 'A') handleDeposit(uid);
  else if (mode == 'B') handleReopen(uid);
  else if (mode == 'C') handlePickup(uid);
  else report(false, 0, "BAD_MODE", "SAI CHE DO");

  returnToIdle();
}

void handleDeposit(const String &uid) {
  int existingLocker = findLockerByUid(uid);
  if (existingLocker >= 0) {
    report(false, existingLocker + 1, "ALREADY_HAS", "THE DA CO HOC");
    return;
  }

  // A = gui lan dau, the nao da co hoc roi thi khong cho tao hoc moi nua
  int locker = findEmptyLocker();
  if (locker < 0) {
    report(false, 0, "FULL", "TU DA DAY");
    return;
  }

  lockerUid[locker] = uid;
  saveLocker(locker);

  DoorResult result = openLocker(locker, "CHE DO A: GUI", "Cho do vao");
  if (result != DOOR_OK) {
    // cua chua tung mo thi coi nhu gui that bai, xoa UID vua luu de hoc van trong
    if (result == DOOR_NOT_OPENED) {
      lockerUid[locker] = "";
      saveLocker(locker);
    }
    reportDoorError(result, locker);
    printLockerState();
    return;
  }

  report(true, locker + 1, "DEPOSIT_OK", "GUI THANH CONG");
  logEvent("deposit", uid, locker);
  printLockerState();
}

void handleReopen(const String &uid) {
  int locker = findLockerByUid(uid);
  if (locker < 0) {
    report(false, 0, "NOT_FOUND", "THE CHUA GUI DO");
    return;
  }

  // B chi mo lai hoc dang co do, khong doi UID va cung khong xoa trang thai
  DoorResult result = openLocker(locker, "CHE DO B: THEM", "Them do vao");
  if (result == DOOR_OK) {
    report(true, locker + 1, "REOPEN_OK", "THEM THANH CONG");
    logEvent("reopen", uid, locker);
  } else {
    reportDoorError(result, locker);
  }
}

void handlePickup(const String &uid) {
  int locker = findLockerByUid(uid);
  if (locker < 0) {
    report(false, 0, "NOT_FOUND", "THE CHUA GUI DO");
    return;
  }

  // C la lay do: mo dung hoc, dong cua xong moi xoa UID de hoc trong lai
  DoorResult result = openLocker(locker, "CHE DO C: LAY", "Lay do ra");
  if (result != DOOR_OK) {
    reportDoorError(result, locker);
    return;
  }

  lockerUid[locker] = "";
  saveLocker(locker);
  report(true, locker + 1, "PICKUP_OK", "LAY THANH CONG");
  logEvent("pickup", uid, locker);
  printLockerState();
}

void handleReset() {
  for (int i = 0; i < LOCKER_COUNT; i++) {
    lockerUid[i] = "";
    saveLocker(i);
  }

  showText("RESET TU", "Da xoa het");
  report(true, 0, "RESET_OK", "DA RESET");
  printLockerState();
}

DoorResult openLocker(int locker, const char *title, const char *actionText) {
  showText(title, lockerLabel(locker));
  unlockDoor(locker);

  // servo da nha khoa roi, nen OLED chi noi user can lam gi tiep theo
  showLockerGuide(locker, actionText, "Dong cua lai");
  if (!waitForDoorState(locker, DOOR_OPEN_STATE, WAIT_OPEN_TIMEOUT)) {
    lockDoor(locker);
    showLockerGuide(locker, "Cua khong mo", "Kiem tra lai");
    return DOOR_NOT_OPENED;
  }

  // den day la cua da tung mo, gio chi cho user dong cua lai de ket thuc luong
  showLockerGuide(locker, actionText, "Dong cua lai");
  if (!waitForDoorClose(locker)) {
    showLockerGuide(locker, "Cua chua dong", "Dong cua lai");
    return DOOR_LEFT_OPEN;
  }

  lockDoor(locker);
  showLockerGuide(locker, "Da khoa cua", "Hoan tat");
  return DOOR_OK;
}

bool waitForDoorState(int locker, int wantedState, unsigned long timeout) {
  unsigned long startedAt = millis();

  while (millis() - startedAt < timeout) {
    if (digitalRead(DOOR_PINS[locker]) == wantedState) {
      delay(200);
      if (digitalRead(DOOR_PINS[locker]) == wantedState) return true;
    }
    delay(20);
  }

  return false;
}

bool waitForDoorClose(int locker) {
  unsigned long startedAt = millis();
  bool alarmStarted = false;

  while (millis() - startedAt < WAIT_CLOSE_TIMEOUT) {
    if (digitalRead(DOOR_PINS[locker]) == DOOR_CLOSED_STATE) {
      delay(200);
      if (digitalRead(DOOR_PINS[locker]) == DOOR_CLOSED_STATE) {
        buzzerOff();
        return true;
      }
    }

    if (!alarmStarted && millis() - startedAt > DOOR_ALARM_AFTER) {
      alarmStarted = true;
      showLockerGuide(locker, "Qua lau", "Dong cua lai");
    }

    if (alarmStarted) {
      buzzerOn();
      delay(150);
      buzzerOff();
      delay(350);
    } else {
      delay(20);
    }
  }

  buzzerOff();
  return false;
}

void reportDoorError(DoorResult result, int locker) {
  if (result == DOOR_NOT_OPENED) {
    report(false, locker + 1, "NOT_OPENED", "CUA KHONG MO");
  } else {
    report(false, locker + 1, "DOOR_OPEN", "CUA CHUA DONG");
  }
}

void report(bool ok, int lockerNumber, const char *code, const char *title) {
  Serial.print(ok ? F("OK|") : F("ERR|"));
  Serial.print(lockerNumber);
  Serial.print('|');
  Serial.println(code);
  showResult(ok, title, lockerNumber);
}

void loadLocker(int locker) {
  String key = lockerKey(locker);
  String stored = preferences.getString(key.c_str(), "");
  stored.toUpperCase();

  if (stored.length() > 0 && !isValidUid(stored)) {
    stored = "";
    preferences.putString(key.c_str(), "");
  }

  lockerUid[locker] = stored;
}

void saveLocker(int locker) {
  String key = lockerKey(locker);
  preferences.putString(key.c_str(), lockerUid[locker]);
}

String lockerKey(int locker) {
  return "uid" + String(locker);
}

String lockerLabel(int locker) {
  String label = "HOC ";
  if (locker + 1 < 10) label += "0";
  label += String(locker + 1);
  return label;
}

bool isValidUid(const String &uid) {
  if (uid.length() < UID_MIN_LENGTH || uid.length() > UID_MAX_LENGTH) return false;

  for (unsigned int i = 0; i < uid.length(); i++) {
    char c = uid.charAt(i);
    bool hex = (c >= '0' && c <= '9') ||
               (c >= 'A' && c <= 'F') ||
               (c >= 'a' && c <= 'f');
    if (!hex) return false;
  }

  return true;
}

int findLockerByUid(const String &uid) {
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i] == uid) return i;
  }
  return -1;
}

int findEmptyLocker() {
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i].length() == 0) return i;
  }
  return -1;
}

void unlockDoor(int locker) {
  lockerServo[locker].write(SERVO_UNLOCK_ANGLE);
  delay(700);
}

void lockDoor(int locker) {
  lockerServo[locker].write(SERVO_LOCK_ANGLE);
  delay(700);
}

void showIdle() {
  showText("TU GUI DO", "San sang");
}

void showText(const String &line1, const String &line2) {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 8);
  display.println(line1);
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.println(line2);
  display.display();
}

void showLockerGuide(int locker, const String &line2, const String &line3) {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(lockerLabel(locker));
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println(line2);
  display.setCursor(0, 44);
  display.println(line3);
  display.display();
}

void showResult(bool ok, const String &title, int lockerNumber) {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(ok ? "[OK]" : "[LOI]");
  display.setCursor(0, 20);
  display.println(title);

  if (lockerNumber > 0) {
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.println("Hoc " + String(lockerNumber));
  }

  display.display();
}

void returnToIdle() {
  delay(RESULT_HOLD);
  showIdle();
}

void buzzerOn() {
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? HIGH : LOW);
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? LOW : HIGH);
}

void beepScanOk() {
  for (int i = 0; i < 2; i++) {
    buzzerOn();
    delay(80);
    buzzerOff();
    if (i == 0) delay(80);
  }
}

void printLockerState() {
  Serial.println(F("===== LOCKER STATE ====="));
  for (int i = 0; i < LOCKER_COUNT; i++) {
    Serial.print(F("Hoc "));
    Serial.print(i + 1);
    Serial.print(F(": "));
    if (lockerUid[i].length() == 0) Serial.println(F("TRONG"));
    else Serial.println(lockerUid[i]);
  }
  Serial.println(F("========================"));
}

void logEvent(const char *eventName, const String &uid, int locker) {
  Serial.print(F("[EVENT] "));
  Serial.print(eventName);
  Serial.print(F(" | UID="));
  Serial.print(uid);
  Serial.print(F(" | locker="));
  Serial.println(locker + 1);
}
