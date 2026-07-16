#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <ctype.h>
#include <qrcode.h>

enum DoorResult { DOOR_OK, DOOR_NOT_OPENED, DOOR_LEFT_OPEN };

enum AccessDecision {
  ACCESS_ALLOW,
  ACCESS_NEED_PAYMENT,
  ACCESS_DENY,
  ACCESS_BYPASS
};

struct AccessCheck {
  AccessDecision decision;
  String paymentId;
  String qrPayload;
  int fee;
};

const int LOCKER_COUNT = 2;
// 18 hoc 1, 23 hoc 2
const int SERVO_PINS[LOCKER_COUNT] = {18, 23};
// 19 hoc 1, 27 hoc 2
const int DOOR_PINS[LOCKER_COUNT] = {19, 27};

const int SERVO_LOCK_ANGLE = 90;
const int SERVO_UNLOCK_ANGLE = 0;

// KY-003 + INPUT_PULLUP: nam cham gan cam bien = cua dong = LOW
const int DOOR_CLOSED_STATE = LOW;
const int DOOR_OPEN_STATE = HIGH;

const unsigned long WAIT_OPEN_TIMEOUT = 10000;
const unsigned long WAIT_CLOSE_TIMEOUT = 30000;
const unsigned long DOOR_ALARM_AFTER = 15000;
const unsigned long RESULT_HOLD = 3000;

const byte UID_MIN_LENGTH = 6;
const byte UID_MAX_LENGTH = 14;
const byte REQUEST_MAX_LENGTH = 32;

const int BUZZER_PIN = 4;
const bool BUZZER_ACTIVE_HIGH = true;

const int UNO_RX_PIN = 16;
const int UNO_TX_PIN = 17;
HardwareSerial UnoSerial(2);

const char WIFI_SSID[] = "307";
const char WIFI_PASSWORD[] = "66668888";
const char API_BASE_URL[] = "https://iot-locker.vercel.app";
const char DEVICE_ID[] = "locker-01";

const unsigned long WIFI_TIMEOUT = 7000;
const unsigned long HTTP_TIMEOUT = 5000;
const unsigned long HEARTBEAT_EVERY = 10000;
const unsigned long PAYMENT_POLL_EVERY = 2000;
const unsigned long PAYMENT_POLL_TIMEOUT = 120000;
const int QR_MAX_VERSION = 6;
byte qrDrawX = 0;
byte qrDrawY = 0;

void drawQrModules(esp_qrcode_handle_t qrcode);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledReady = false;

Servo lockerServo[LOCKER_COUNT];
String lockerUid[LOCKER_COUNT];
Preferences preferences;
unsigned long lastHeartbeatAt = 0;
bool serverOnline = false;
bool idleVisible = false;

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
    showText("SMART LOCKER", "Khoi dong");
  }

  preferences.begin("lockers", false);
  for (int i = 0; i < LOCKER_COUNT; i++) {
    lockerServo[i].setPeriodHertz(50);
    lockerServo[i].attach(SERVO_PINS[i], 500, 2400);
    pinMode(DOOR_PINS[i], INPUT_PULLUP);
    lockDoor(i);
    loadLocker(i);
  }

  ensureWifi();
  sendHeartbeat();
  postLockerState();
  printLockerState();
  showIdle();
  Serial.println(F("ESP32 V2 READY"));
}

void loop() {
  readUnoRequest();
  if (millis() - lastHeartbeatAt > HEARTBEAT_EVERY) {
    sendHeartbeat();
  }
}

void readUnoRequest() {
  static String line = "";

  while (UnoSerial.available()) {
    char c = UnoSerial.read();

    if (c == '\n') {
      line.trim();
      if (line.length() > 0)
        processRequest(line);
      line = "";
      continue;
    }

    if (c == '\r')
      continue;

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

  // chi nhan dung format tu Uno: REQ|MODE|UID, sai thi bo luon cho de debug
  if (!request.startsWith("REQ|") || request.length() < 7 ||
      request.charAt(5) != '|') {
    report(false, 0, "BAD_REQUEST", "LOI YEU CAU");
    returnToIdle();
    return;
  }

  char mode = request.charAt(4);
  String uid = request.substring(6);
  uid.trim();
  uid.toUpperCase();

  Serial.print(F("UID="));
  Serial.print(uid);
  Serial.print(F(" mode="));
  Serial.println(mode);

  // hard reset - dang demo - cai nay dung se dung master key, hoac dung
  // passcode tu server
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
  postLockerState();

  if (mode == 'A')
    handleDeposit(uid);
  else if (mode == 'B')
    handleReopen(uid);
  else if (mode == 'C')
    handlePickup(uid);
  else
    report(false, 0, "BAD_MODE", "SAI CHE DO");

  returnToIdle();
}
