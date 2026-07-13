#include <Preferences.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum DoorResult {
  DOOR_CYCLE_OK,
  DOOR_NOT_OPENED,
  DOOR_LEFT_OPEN
};

const int LOCKER_COUNT = 2;
const int SERVO_PINS[LOCKER_COUNT] = {18, 23};
const int REED_PINS[LOCKER_COUNT] = {19, 27};

const int SERVO_LOCK_ANGLE = 90;
const int SERVO_UNLOCK_ANGLE = 0;

// KY-003 with INPUT_PULLUP: magnet near sensor means the door is closed.
const int DOOR_CLOSED_STATE = LOW;
const int DOOR_OPEN_STATE = HIGH;

// Set to false while bench-testing without KY-003 installed.
const bool USE_DOOR_SENSOR = true;

const unsigned long WAIT_OPEN_TIMEOUT = 10000UL;
const unsigned long WAIT_CLOSE_TIMEOUT = 30000UL;
const unsigned long DOOR_ALARM_AFTER = 15000UL;
const unsigned long SENSORLESS_OPEN_HOLD = 5000UL;

const int BUZZER_PIN = 4;
const bool BUZZER_ACTIVE_HIGH = true;
const unsigned long RESULT_HOLD = 3000UL;

const byte UID_MIN_LENGTH = 6;
const byte UID_MAX_LENGTH = 14;

HardwareSerial UnoSerial(2);
const int UNO_RX_PIN = 16;
const int UNO_TX_PIN = 17;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledReady = false;

Servo lockerServo[LOCKER_COUNT];
String lockerUid[LOCKER_COUNT];
Preferences preferences;

void readUnoRequest();
void processRequest(const String &request);
void handleDeposit(const String &uid);
void handleReopen(const String &uid);
void handlePickup(const String &uid);
void handleMasterReset();
DoorResult runDoorCycle(int locker);
bool waitForDoorState(int locker, int wantedState, unsigned long timeout);
bool waitForDoorClose(int locker);
void unlockDoor(int locker);
void lockDoor(int locker);
bool isValidUid(const String &uid);
int findLockerByUid(const String &uid);
int findEmptyLocker();
void saveLocker(int locker);
String getPreferenceKey(int locker);
void sendResponse(const String &status, int lockerNumber, const String &message);
void returnToIdle();
void showIdleOled();
void showOled(const String &line1, const String &line2);
void showBigOled(const String &title, const String &big);
void showResultOled(const String &status, int lockerNumber, const String &message);
void printLockerState();
void logEvent(const String &eventName, const String &uid, int locker);
void buzzerOn();
void buzzerOff();
void beep(int times, int onMs, int offMs);
void beepScanOk();

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  UnoSerial.begin(9600, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  Wire.begin(21, 22);
  Wire.setClock(100000);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    showOled("SMART LOCKER", "Khoi dong...");
  }

  preferences.begin("lockers", false);

  for (int i = 0; i < LOCKER_COUNT; i++) {
    lockerServo[i].setPeriodHertz(50);
    lockerServo[i].attach(SERVO_PINS[i], 500, 2400);
    pinMode(REED_PINS[i], INPUT_PULLUP);
    lockDoor(i);

    String key = getPreferenceKey(i);
    String stored = preferences.getString(key.c_str(), "");
    stored.toUpperCase();

    if (stored.length() > 0 && !isValidUid(stored)) {
      Serial.print(F("NVS rac o hoc "));
      Serial.print(i + 1);
      Serial.println(F(" -> tu dong xoa"));
      stored = "";
      preferences.putString(key.c_str(), "");
    }

    lockerUid[i] = stored;
  }

  printLockerState();
  showIdleOled();
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

    if (c != '\r') line += c;
  }
}

void processRequest(const String &request) {
  Serial.print(F("Nhan Uno: "));
  Serial.println(request);

  int separator1 = request.indexOf('|');
  int separator2 = request.indexOf('|', separator1 + 1);

  if (separator1 < 0 || separator2 < 0 || !request.startsWith("REQ|")) {
    sendResponse("ERR", 0, "BAD_REQUEST");
    returnToIdle();
    return;
  }

  String modeString = request.substring(separator1 + 1, separator2);
  String uid = request.substring(separator2 + 1);
  uid.trim();
  uid.toUpperCase();

  if (modeString.length() != 1) {
    sendResponse("ERR", 0, "BAD_REQUEST");
    returnToIdle();
    return;
  }

  char mode = modeString.charAt(0);

  if (mode == 'D') {
    beepScanOk();
    handleMasterReset();
    returnToIdle();
    return;
  }

  if (!isValidUid(uid)) {
    sendResponse("ERR", 0, "BAD_UID");
    returnToIdle();
    return;
  }

  beepScanOk();
  showOled("DA NHAN THE", "Dang xu ly...");

  if (mode == 'A') handleDeposit(uid);
  else if (mode == 'B') handleReopen(uid);
  else if (mode == 'C') handlePickup(uid);
  else sendResponse("ERR", 0, "BAD_MODE");

  returnToIdle();
}

void handleDeposit(const String &uid) {
  int existingLocker = findLockerByUid(uid);
  if (existingLocker >= 0) {
    sendResponse("ERR", existingLocker + 1, "ALREADY_HAS");
    return;
  }

  int emptyLocker = findEmptyLocker();
  if (emptyLocker < 0) {
    sendResponse("ERR", 0, "FULL");
    return;
  }

  lockerUid[emptyLocker] = uid;
  saveLocker(emptyLocker);

  showBigOled("CHE DO A: GUI", "Hoc " + String(emptyLocker + 1));
  DoorResult result = runDoorCycle(emptyLocker);

  if (result == DOOR_CYCLE_OK) {
    sendResponse("OK", emptyLocker + 1, "DEPOSIT_OK");
    logEvent("deposit", uid, emptyLocker);
  } else if (result == DOOR_NOT_OPENED) {
    lockerUid[emptyLocker] = "";
    saveLocker(emptyLocker);
    sendResponse("ERR", emptyLocker + 1, "NOT_OPENED");
  } else {
    sendResponse("ERR", emptyLocker + 1, "DOOR_OPEN");
  }

  printLockerState();
}

void handleReopen(const String &uid) {
  int locker = findLockerByUid(uid);
  if (locker < 0) {
    sendResponse("ERR", 0, "NOT_FOUND");
    return;
  }

  showBigOled("CHE DO B: THEM", "Hoc " + String(locker + 1));
  DoorResult result = runDoorCycle(locker);

  if (result == DOOR_CYCLE_OK) {
    sendResponse("OK", locker + 1, "REOPEN_OK");
    logEvent("reopen", uid, locker);
  } else if (result == DOOR_NOT_OPENED) {
    sendResponse("ERR", locker + 1, "NOT_OPENED");
  } else {
    sendResponse("ERR", locker + 1, "DOOR_OPEN");
  }
}

void handlePickup(const String &uid) {
  int locker = findLockerByUid(uid);
  if (locker < 0) {
    sendResponse("ERR", 0, "NOT_FOUND");
    return;
  }

  showBigOled("CHE DO C: LAY", "Hoc " + String(locker + 1));
  DoorResult result = runDoorCycle(locker);

  if (result == DOOR_CYCLE_OK) {
    logEvent("pickup", uid, locker);
    lockerUid[locker] = "";
    saveLocker(locker);
    sendResponse("OK", locker + 1, "PICKUP_OK");
  } else if (result == DOOR_NOT_OPENED) {
    sendResponse("ERR", locker + 1, "NOT_OPENED");
  } else {
    sendResponse("ERR", locker + 1, "DOOR_OPEN");
  }

  printLockerState();
}

void handleMasterReset() {
  for (int i = 0; i < LOCKER_COUNT; i++) {
    lockerUid[i] = "";
    saveLocker(i);
  }

  Serial.println(F("!! MASTER RESET - da xoa het mapping"));
  showBigOled("MASTER RESET", "Da xoa het");
  sendResponse("OK", 0, "RESET_OK");
  printLockerState();
}

DoorResult runDoorCycle(int locker) {
  unlockDoor(locker);

  if (!USE_DOOR_SENSOR) {
    showOled("HOC DA MO", "Dang test...");
    delay(SENSORLESS_OPEN_HOLD);
    lockDoor(locker);
    showOled("DA KHOA LAI", "Hoan tat");
    return DOOR_CYCLE_OK;
  }

  showOled("HOC DA MO", "Mo cua ra...");
  bool opened = waitForDoorState(locker, DOOR_OPEN_STATE, WAIT_OPEN_TIMEOUT);

  if (!opened) {
    lockDoor(locker);
    showOled("HET THOI GIAN", "Chua mo cua");
    return DOOR_NOT_OPENED;
  }

  showOled("CUA DANG MO", "Hay dong cua");
  bool closed = waitForDoorClose(locker);

  if (!closed) {
    showOled("CANH BAO", "Cua chua dong");
    return DOOR_LEFT_OPEN;
  }

  lockDoor(locker);
  showOled("DA DONG CUA", "Hoan tat");
  return DOOR_CYCLE_OK;
}

bool waitForDoorState(int locker, int wantedState, unsigned long timeout) {
  unsigned long startedAt = millis();

  while (millis() - startedAt < timeout) {
    if (digitalRead(REED_PINS[locker]) == wantedState) {
      delay(200);
      if (digitalRead(REED_PINS[locker]) == wantedState) return true;
    }
    delay(20);
  }

  return false;
}

bool waitForDoorClose(int locker) {
  unsigned long startedAt = millis();
  bool alarmStarted = false;

  while (millis() - startedAt < WAIT_CLOSE_TIMEOUT) {
    if (digitalRead(REED_PINS[locker]) == DOOR_CLOSED_STATE) {
      delay(200);
      if (digitalRead(REED_PINS[locker]) == DOOR_CLOSED_STATE) {
        buzzerOff();
        return true;
      }
    }

    if (!alarmStarted && millis() - startedAt > DOOR_ALARM_AFTER) {
      alarmStarted = true;
      showOled("CANH BAO", "Dong cua!");
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

void unlockDoor(int locker) {
  lockerServo[locker].write(SERVO_UNLOCK_ANGLE);
  Serial.print(F("Mo khoa hoc "));
  Serial.println(locker + 1);
  delay(700);
}

void lockDoor(int locker) {
  lockerServo[locker].write(SERVO_LOCK_ANGLE);
  Serial.print(F("Khoa hoc "));
  Serial.println(locker + 1);
  delay(700);
}

bool isValidUid(const String &uid) {
  if (uid.length() < UID_MIN_LENGTH || uid.length() > UID_MAX_LENGTH) return false;

  for (unsigned int i = 0; i < uid.length(); i++) {
    char c = uid.charAt(i);
    bool ok =
      (c >= '0' && c <= '9') ||
      (c >= 'A' && c <= 'F') ||
      (c >= 'a' && c <= 'f');
    if (!ok) return false;
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

void saveLocker(int locker) {
  String key = getPreferenceKey(locker);
  preferences.putString(key.c_str(), lockerUid[locker]);
}

String getPreferenceKey(int locker) {
  return "uid" + String(locker);
}

void sendResponse(const String &status, int lockerNumber, const String &message) {
  UnoSerial.print(status);
  UnoSerial.print('|');
  UnoSerial.print(lockerNumber);
  UnoSerial.print('|');
  UnoSerial.println(message);

  Serial.print(F("Tra Uno: "));
  Serial.print(status);
  Serial.print('|');
  Serial.print(lockerNumber);
  Serial.print('|');
  Serial.println(message);

  showResultOled(status, lockerNumber, message);
}

void returnToIdle() {
  delay(RESULT_HOLD);
  showIdleOled();
}

void showIdleOled() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("SMART LOCKER"));
  display.setCursor(0, 24);
  display.println(F("Bam A/B/C tren"));
  display.setCursor(0, 36);
  display.println(F("keypad roi quet the"));
  display.display();
}

void showOled(const String &line1, const String &line2) {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(line1);
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.println(line2);
  display.display();
}

void showBigOled(const String &title, const String &big) {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.println(big);
  display.display();
}

void showResultOled(const String &status, int lockerNumber, const String &message) {
  if (!oledReady) return;

  String title;

  if (message.indexOf("DEPOSIT_OK") >= 0) title = "GUI THANH CONG";
  else if (message.indexOf("REOPEN_OK") >= 0) title = "DA MO LAI HOC";
  else if (message.indexOf("PICKUP_OK") >= 0) title = "LAY THANH CONG";
  else if (message.indexOf("RESET_OK") >= 0) title = "DA RESET";
  else if (message.indexOf("ALREADY_HAS") >= 0) title = "THE DA CO HOC";
  else if (message.indexOf("FULL") >= 0) title = "TU DA DAY";
  else if (message.indexOf("NOT_FOUND") >= 0) title = "THE CHUA GUI DO";
  else if (message.indexOf("NOT_OPENED") >= 0) title = "CHUA MO CUA";
  else if (message.indexOf("DOOR_OPEN") >= 0) title = "CUA CHUA DONG";
  else if (message.indexOf("BAD_UID") >= 0) title = "THE KHONG HOP LE";
  else if (message.indexOf("BAD_MODE") >= 0) title = "SAI CHE DO";
  else if (message.indexOf("BAD_REQUEST") >= 0) title = "LOI YEU CAU";
  else title = (status == "OK") ? "THANH CONG" : "CO LOI";

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(status == "OK" ? "[OK]" : "[LOI]");
  display.setCursor(0, 20);
  display.println(title);

  if (lockerNumber > 0) {
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.println("Hoc " + String(lockerNumber));
  }

  display.display();
}

void buzzerOn() {
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? HIGH : LOW);
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? LOW : HIGH);
}

void beep(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    buzzerOn();
    delay(onMs);
    buzzerOff();
    if (i < times - 1) delay(offMs);
  }
}

void beepScanOk() {
  beep(2, 80, 80);
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

void logEvent(const String &eventName, const String &uid, int locker) {
  Serial.print(F("[EVENT] "));
  Serial.print(eventName);
  Serial.print(F(" | UID="));
  Serial.print(uid);
  Serial.print(F(" | locker="));
  Serial.println(locker + 1);
}
