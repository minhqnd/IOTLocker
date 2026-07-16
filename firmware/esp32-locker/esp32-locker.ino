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

  ensureWifi();
  sendHeartbeat();
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
    // cua chua tung mo thi coi nhu gui that bai, xoa UID vua luu de hoc van
    // trong
    if (result == DOOR_NOT_OPENED) {
      lockerUid[locker] = "";
      saveLocker(locker);
    }
    reportDoorError(result, locker);
    printLockerState();
    return;
  }

  report(true, locker + 1, "DEPOSIT_OK", "GUI THANH CONG");
  postDeposit(uid, locker);
  printLockerState();
}

void handleReopen(const String &uid) {
  // B chi mo lai hoc dang co do, khong doi UID va cung khong xoa trang thai
  handleExistingLocker(uid, 'B', "CHE DO B: THEM", "Them do vao", "REOPEN_OK",
                       "THEM THANH CONG", false);
}

void handlePickup(const String &uid) {
  // C la lay do: mo dung hoc, dong cua xong moi xoa UID de hoc trong lai
  handleExistingLocker(uid, 'C', "CHE DO C: LAY", "Lay do ra", "PICKUP_OK",
                       "LAY THANH CONG", true);
}

void handleExistingLocker(const String &uid, char mode, const char *title,
                          const char *actionText, const char *okCode,
                          const char *okTitle, bool clearAfterOpen) {
  int locker = findLockerByUid(uid);
  if (locker < 0) {
    report(false, 0, "NOT_FOUND", "THE CHUA GUI DO");
    return;
  }

  AccessCheck access = checkServerAccess(uid, mode);
  if (!canContinueAfterAccess(access, locker, uid))
    return;

  DoorResult result = openLocker(locker, title, actionText);
  if (result != DOOR_OK) {
    reportDoorError(result, locker);
    return;
  }

  report(true, locker + 1, okCode, okTitle);

  if (clearAfterOpen) {
    lockerUid[locker] = "";
    saveLocker(locker);
    postPickup(uid);
  } else {
    logEvent("reopen", uid, locker);
  }

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

  // OLED chi noi user can lam gi tiep theo vi cua da mo
  showLockerGuide(locker, actionText, "Dong cua lai");
  if (!waitForDoorState(locker, DOOR_OPEN_STATE, WAIT_OPEN_TIMEOUT)) {
    lockDoor(locker);
    showLockerGuide(locker, "Cua khong mo", "Kiem tra lai");
    return DOOR_NOT_OPENED;
  }

  // cua dang mo, yeu cau user dong cua lai ket thuc luong
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
      if (digitalRead(DOOR_PINS[locker]) == wantedState)
        return true;
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
  report(false, locker + 1,
         result == DOOR_NOT_OPENED ? "NOT_OPENED" : "DOOR_OPEN",
         result == DOOR_NOT_OPENED ? "CUA KHONG MO" : "CUA CHUA DONG");
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

String lockerKey(int locker) { return "uid" + String(locker); }

String lockerLabel(int locker) {
  String label = "HOC ";
  if (locker + 1 < 10)
    label += "0";
  label += String(locker + 1);
  return label;
}

bool isValidUid(const String &uid) {
  if (uid.length() < UID_MIN_LENGTH || uid.length() > UID_MAX_LENGTH)
    return false;

  for (unsigned int i = 0; i < uid.length(); i++) {
    if (!isxdigit((unsigned char)uid.charAt(i)))
      return false;
  }

  return true;
}

int findLockerByUid(const String &uid) {
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i] == uid)
      return i;
  }
  return -1;
}

int findEmptyLocker() { return findLockerByUid(""); }

void unlockDoor(int locker) {
  lockerServo[locker].write(SERVO_UNLOCK_ANGLE);
  delay(700);
}

void lockDoor(int locker) {
  lockerServo[locker].write(SERVO_LOCK_ANGLE);
  delay(700);
}

void showIdle() {
  if (!oledReady)
    return;

  int used = 0;
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i].length() > 0)
      used++;
  }

  display.clearDisplay();
  drawLine(1, 0, "TU GUI DO");
  drawLine(1, 18,
           "Trong: " + String(LOCKER_COUNT - used) + "/" +
               String(LOCKER_COUNT));
  drawLine(1, 32, "Dang dung: " + String(used));
  drawLine(1, 50, serverOnline ? "Online" : "Offline mode");
  display.display();
  idleVisible = true;
}

void showText(const String &line1, const String &line2) {
  if (!oledReady)
    return;
  idleVisible = false;

  display.clearDisplay();
  drawLine(1, 8, line1);
  drawLine(2, 30, line2);
  display.display();
}

void showLockerGuide(int locker, const String &line2, const String &line3) {
  if (!oledReady)
    return;
  idleVisible = false;

  display.clearDisplay();
  drawLine(2, 0, lockerLabel(locker));
  drawLine(1, 30, line2);
  drawLine(1, 44, line3);
  display.display();
}

void showResult(bool ok, const String &title, int lockerNumber) {
  if (!oledReady)
    return;
  idleVisible = false;

  display.clearDisplay();
  drawLine(1, 0, ok ? "[OK]" : "[LOI]");
  drawLine(1, 20, title);
  if (lockerNumber > 0)
    drawLine(2, 40, "Hoc " + String(lockerNumber));
  display.display();
}

void showPayment(const String &paymentId, const String &qrPayload, int fee) {
  if (!oledReady)
    return;
  idleVisible = false;

  display.clearDisplay();
  if (qrPayload.length() > 0) {
    drawQr(qrPayload, 0, 0);
    drawLineAt(56, 1, 0, "Quet QR");
    drawLineAt(56, 1, 14, String(fee) + " VND");
    drawLineAt(56, 1, 28, paymentId);
    drawLineAt(56, 1, 50, "Dang cho...");
  } else {
    drawLine(1, 0, "CAN THANH TOAN");
    drawLine(1, 18, "Phi: " + String(fee) + " VND");
    drawLine(1, 34, "Ma: " + paymentId);
    drawLine(1, 50, "Cho xac nhan...");
  }
  display.display();
}

void drawQr(const String &payload, byte x, byte y) {
  qrDrawX = x;
  qrDrawY = y;

  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = drawQrModules;
  cfg.max_qrcode_version = QR_MAX_VERSION;
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  esp_qrcode_generate(&cfg, payload.c_str());
}

void drawQrModules(esp_qrcode_handle_t qrcode) {
  int size = esp_qrcode_get_size(qrcode);
  byte border = 2;
  display.fillRect(qrDrawX, qrDrawY, size + border * 2, size + border * 2,
                   SSD1306_WHITE);

  for (int qrY = 0; qrY < size; qrY++) {
    for (int qrX = 0; qrX < size; qrX++) {
      if (esp_qrcode_get_module(qrcode, qrX, qrY)) {
        display.drawPixel(qrDrawX + qrX + border, qrDrawY + qrY + border,
                          SSD1306_BLACK);
      }
    }
  }
}

void drawLine(byte size, byte y, const String &text) {
  drawLineAt(0, size, y, text);
}

void drawLineAt(byte x, byte size, byte y, const String &text) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.println(text);
}

void returnToIdle() {
  delay(RESULT_HOLD);
  showIdle();
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_TIMEOUT) {
    delay(200);
  }

  bool connected = WiFi.status() == WL_CONNECTED;
  Serial.print(F("[WIFI] "));
  Serial.println(connected ? WiFi.localIP().toString()
                           : "timeout, chay offline");
  return connected;
}

void sendHeartbeat() {
  lastHeartbeatAt = millis();
  if (!ensureWifi()) {
    serverOnline = false;
    if (idleVisible)
      showIdle();
    return;
  }

  StaticJsonDocument<128> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  serverOnline = postJsonOk("/api/locker/heartbeat", bodyDoc);
  Serial.print(F("[API] heartbeat "));
  Serial.println(serverOnline ? F("ok") : F("fail"));
  if (idleVisible)
    showIdle();
}

void postDeposit(const String &uid, int locker) {
  if (!ensureWifi()) {
    logEvent("deposit_offline", uid, locker);
    return;
  }

  StaticJsonDocument<160> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  bodyDoc["uid"] = uid;
  bodyDoc["locker"] = locker + 1;
  if (postJsonOk("/api/locker/deposit", bodyDoc))
    logEvent("deposit_api", uid, locker);
  else
    logEvent("deposit_api_fail", uid, locker);
}

void postPickup(const String &uid) {
  if (!ensureWifi()) {
    Serial.println(F("[API] pickup offline"));
    return;
  }

  StaticJsonDocument<160> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  bodyDoc["uid"] = uid;
  Serial.print(F("[API] pickup "));
  Serial.println(postJsonOk("/api/locker/pickup", bodyDoc) ? F("ok")
                                                           : F("fail"));
}

AccessCheck checkServerAccess(const String &uid, char mode) {
  AccessCheck result = {ACCESS_BYPASS, "", "", 0};

  // Mat mang thi cho mo tam, vi tu van con UID local trong NVS de doi chieu.
  if (!ensureWifi())
    return result;

  String path = "/api/locker/access?deviceId=" + String(DEVICE_ID) +
                "&uid=" + uid + "&mode=" + String(mode);
  StaticJsonDocument<1536> doc;
  if (!getJson(path, doc)) {
    Serial.println(F("[API] access timeout, bypass"));
    return result;
  }

  if (doc["allowOpen"] | false) {
    result.decision = ACCESS_ALLOW;
    return result;
  }

  const char *paymentId = doc["paymentId"] | "";
  if (paymentId[0] != '\0') {
    result.decision = ACCESS_NEED_PAYMENT;
    result.paymentId = paymentId;
    result.fee = doc["fee"] | 0;

    const char *qrPayload = doc["qrPayload"] | "";
    if (qrPayload[0] != '\0') {
      result.qrPayload = qrPayload;
      Serial.print(F("PAYQR|"));
      Serial.println(qrPayload);
    }
    return result;
  }

  if (doc["found"].is<bool>() && !(doc["found"] | false)) {
    result.decision = ACCESS_DENY;
    return result;
  }

  return result;
}

bool canContinueAfterAccess(const AccessCheck &access, int locker,
                            const String &uid) {
  if (access.decision == ACCESS_ALLOW || access.decision == ACCESS_BYPASS)
    return true;

  if (access.decision == ACCESS_NEED_PAYMENT) {
    bool paid = waitForPayment(access.paymentId, access.qrPayload, access.fee);
    if (paid)
      return true;

    report(false, locker + 1, "PAY_TIMEOUT", "CHUA THANH TOAN");
    return false;
  }

  report(false, locker + 1, "SERVER_DENY", "KHONG MO DUOC");
  Serial.print(F("[ACCESS DENY] UID="));
  Serial.println(uid);
  return false;
}

bool isPaymentReady(const String &paymentId) {
  String path = "/api/payment/status?paymentId=" + paymentId;
  StaticJsonDocument<512> doc;
  if (!getJson(path, doc)) {
    Serial.println(F("[PAY] timeout, bypass"));
    return true;
  }

  bool paid = (doc["paid"] | false) || (doc["allowOpen"] | false);
  Serial.print(F("[PAY] status="));
  Serial.print((const char *)(doc["paymentStatus"] | "unknown"));
  Serial.print(F(" paid="));
  Serial.println(paid ? F("yes") : F("no"));
  return paid;
}

bool waitForPayment(const String &paymentId, const String &qrPayload, int fee) {
  if (paymentId.length() == 0)
    return false;

  unsigned long startedAt = millis();
  while (millis() - startedAt < PAYMENT_POLL_TIMEOUT) {
    showPayment(paymentId, qrPayload, fee);

    if (!ensureWifi()) {
      Serial.println(F("[PAY] mat mang luc cho, bypass"));
      return true;
    }

    if (isPaymentReady(paymentId)) {
      showText("DA THANH TOAN", "Dang mo...");
      return true;
    }

    delay(PAYMENT_POLL_EVERY);
  }

  // Co luc webhook ve sat nut timeout, check them lan cuoi truoc khi bao fail.
  if (isPaymentReady(paymentId)) {
    showText("DA THANH TOAN", "Dang mo...");
    return true;
  }

  Serial.println(F("[PAY] het gio cho thanh toan"));
  return false;
}

String httpGet(const String &path) { return httpRequest("GET", path, ""); }

String httpPost(const String &path, const String &body) {
  return httpRequest("POST", path, body);
}

bool postJsonOk(const String &path, JsonDocument &bodyDoc) {
  String response = httpPost(path, toJson(bodyDoc));
  StaticJsonDocument<512> doc;
  return response.length() > 0 && parseJson(response, doc) &&
         (doc["ok"] | false);
}

bool getJson(const String &path, JsonDocument &doc) {
  String response = httpGet(path);
  return response.length() > 0 && parseJson(response, doc);
}

String httpRequest(const char *method, const String &path, const String &body) {
  HTTPClient http;
  String url = String(API_BASE_URL) + path;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT);

  int status;
  if (method[0] == 'P') {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(body);
  } else {
    status = http.GET();
  }

  String response = "";
  if (status > 0)
    response = http.getString();
  http.end();

  Serial.print('[');
  Serial.print(method);
  Serial.print(F("] "));
  Serial.print(path);
  Serial.print(F(" -> "));
  Serial.println(status);
  return response;
}

bool parseJson(const String &response, JsonDocument &doc) {
  DeserializationError error = deserializeJson(doc, response);
  if (!error)
    return true;

  Serial.print(F("[JSON] "));
  Serial.println(error.c_str());
  return false;
}

String toJson(JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  return body;
}

void buzzerOn() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? HIGH : LOW); }

void buzzerOff() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? LOW : HIGH); }

void beepScanOk() {
  for (int i = 0; i < 2; i++) {
    buzzerOn();
    delay(80);
    buzzerOff();
    if (i == 0)
      delay(80);
  }
}

void printLockerState() {
  Serial.println(F("===== LOCKER STATE ====="));
  for (int i = 0; i < LOCKER_COUNT; i++) {
    Serial.print(F("Hoc "));
    Serial.print(i + 1);
    Serial.print(F(": "));
    if (lockerUid[i].length() == 0)
      Serial.println(F("TRONG"));
    else
      Serial.println(lockerUid[i]);
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
