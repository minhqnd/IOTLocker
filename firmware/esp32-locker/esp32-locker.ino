#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "qrcode.h"
#include <ArduinoJson.h>

// ===== CONFIGURATION (Modify these values) =====
const char* WIFI_SSID   = "TEN_WIFI";
const char* WIFI_PASS   = "MAT_KHAU";
const String SERVER_URL = "http://192.168.1.10:3000"; // Next.js server base URL (no trailing slash)
const String LOCKER_ID  = "L01";

// UART2 configuration for communication with Arduino Uno
#define RXD2 16
#define TXD2 17

// Compartments setup: 2 compartments (C1 and C2)
const int NUM_COMP = 2;
const char* COMP_ID[NUM_COMP]  = {"C1", "C2"};
Servo servos[NUM_COMP];
const int SERVO_PINS[NUM_COMP] = {18, 23};

// OLED 128x64 I2C configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NVS Storage
Preferences prefs;

// Local states cached in memory and NVS
bool occupied[NUM_COMP];
String pinStore[NUM_COMP];
String orderStore[NUM_COMP];
bool isCodStore[NUM_COMP];

// Safety PIN lock (brute force protection)
int wrongPinCount = 0;
unsigned long lockedUntil = 0;

// Periodic status timer
unsigned long lastBeat = 0;
unsigned long lastQueueFlush = 0;

// --- Function Declarations ---
void oledShowText(String line1, String line2);
void drawQrOLED(String text, String orderCode, int amount);
void loadCabinetState();
void saveCompartmentState(int idx);
String generateRandomPIN();
void sendMsgToUno(String msg);
void openLockerCompartment(int idx);
bool executePOST(String endpoint, String jsonPayload, String &responseStr);
bool executeGET(String endpoint, String &responseStr);
void enqueueOfflineEvent(String payload);
void flushOfflineQueue();
void sendHeartbeat();
void handleUartLine(String line);
void doDeposit(String orderCode);
void doPinPickup(String pin);
void doOrderCOD(String orderCode);

// --- Setup & Loop ---

void setup() {
  // Debug Monitor
  Serial.begin(115200);
  Serial.println("ESP32 Brain Starting...");

  // UART2 to Uno
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Initialize random seed
  randomSeed(esp_random());

  // Attach Servos and set to locked position (0 degrees)
  for (int i = 0; i < NUM_COMP; i++) {
    servos[i].attach(SERVO_PINS[i]);
    servos[i].write(0); 
  }

  // Initialize OLED
  Wire.begin(21, 22);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED allocation failed");
  }
  oledShowText("SMART LOCKER", "Dang khoi dong...");

  // Load state from NVS
  prefs.begin("locker", false);
  loadCabinetState();

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    oledShowText("SMART LOCKER", "Online - Sẵn sàng");
  } else {
    Serial.println("WiFi Connection Timeout. Running in Offline Mode.");
    oledShowText("SMART LOCKER", "Offline - Sẵn sàng");
  }

  // Send initial status
  sendHeartbeat();
}

void loop() {
  // 1. Listen for key commands from Arduino Uno
  if (Serial2.available() > 0) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      handleUartLine(line);
    }
  }

  // 2. Perform periodic events (every 30 seconds)
  if (millis() - lastBeat > 30000) {
    lastBeat = millis();
    
    // Auto reconnect WiFi if lost
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Retrying connection...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    } else {
      // If connected, sync logs and heartbeat
      flushOfflineQueue();
      sendHeartbeat();
    }
  }
}

// --- Helper Functions ---

/**
 * Displays simple 2 lines of text on the OLED screen.
 */
void oledShowText(String line1, String line2) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 8);
  oled.println(line1);
  oled.setCursor(0, 32);
  oled.println(line2);
  oled.display();
}

/**
 * Draws a VietQR Code on the right side of the OLED,
 * and displays transaction details on the left side.
 */
void drawQrOLED(String qrString, String orderCode, int amount) {
  QRCode qrcode;
  // Version 7 has 45x45 size, suitable for ~120-150 alphanumeric characters
  int qrVersion = 7;
  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  
  oled.clearDisplay();
  
  // 1. Render left side text details
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 4);
  oled.println("QUET VIETQR");
  oled.setCursor(0, 18);
  oled.println("MA:" + orderCode);
  oled.setCursor(0, 32);
  oled.println(String(amount) + "d");
  oled.setCursor(0, 48);
  oled.println("BAM * HUY");

  // 2. Generate QR code structure
  qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, qrString.c_str());

  // Positioning QR code on the right side (centered vertically)
  int xOffset = 76;
  int yOffset = (SCREEN_HEIGHT - qrcode.size) / 2;

  // Draw quiet zone (white square backing)
  oled.fillRect(xOffset - 2, yOffset - 2, qrcode.size + 4, qrcode.size + 4, SSD1306_WHITE);

  // Draw QR pixels
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        oled.drawPixel(xOffset + x, yOffset + y, SSD1306_BLACK);
      }
    }
  }
  
  oled.display();
}

/**
 * Sends a standard print command back to the Arduino Uno.
 */
void sendMsgToUno(String msg) {
  Serial2.println("MSG:" + msg);
}

/**
 * Loads the cabinet compartment statuses and credentials from NVS.
 */
void loadCabinetState() {
  for (int i = 0; i < NUM_COMP; i++) {
    occupied[i] = prefs.getBool(("occ" + String(i)).c_str(), false);
    pinStore[i] = prefs.getString(("pin" + String(i)).c_str(), "");
    orderStore[i] = prefs.getString(("ord" + String(i)).c_str(), "");
    isCodStore[i] = prefs.getBool(("cod" + String(i)).c_str(), false);

    Serial.printf("Compartment %s: Occ=%d, PIN=%s, Order=%s, isCOD=%d\n",
                  COMP_ID[i], occupied[i], pinStore[i].c_str(), orderStore[i].c_str(), isCodStore[i]);
  }
}

/**
 * Saves a single compartment status and credentials to NVS.
 */
void saveCompartmentState(int idx) {
  prefs.putBool(("occ" + String(idx)).c_str(), occupied[idx]);
  prefs.putString(("pin" + String(idx)).c_str(), pinStore[idx]);
  prefs.putString(("ord" + String(idx)).c_str(), orderStore[idx]);
  prefs.putBool(("cod" + String(idx)).c_str(), isCodStore[idx]);
}

/**
 * Generates a secure random 6-digit numeric PIN code.
 */
String generateRandomPIN() {
  return String(random(100000, 1000000));
}

/**
 * Actuates the compartment servo to release the latch,
 * waiting for 5 seconds before locking it again.
 */
void openLockerCompartment(int idx) {
  Serial.printf("Opening compartment: %s\n", COMP_ID[idx]);
  servos[idx].write(90); // Turn to open position
  delay(5000);           // Keep open for 5 seconds (temporary door simulation)
  servos[idx].write(0);  // Return to locked position
}

// --- Network Functions ---

/**
 * Executes a POST request returning success status and populating response.
 */
bool executePOST(String endpoint, String jsonPayload, String &responseStr) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  WiFiClient client;
  bool success = false;

  String fullUrl = SERVER_URL + endpoint;
  if (http.begin(client, fullUrl)) {
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      responseStr = http.getString();
      success = true;
    } else {
      Serial.printf("POST to %s failed, code: %d\n", endpoint.c_str(), httpResponseCode);
    }
    http.end();
  }
  return success;
}

/**
 * Executes a GET request returning success status and populating response.
 */
bool executeGET(String endpoint, String &responseStr) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  WiFiClient client;
  bool success = false;

  String fullUrl = SERVER_URL + endpoint;
  if (http.begin(client, fullUrl)) {
    int httpResponseCode = http.GET();
    
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      responseStr = http.getString();
      success = true;
    } else {
      Serial.printf("GET to %s failed, code: %d\n", endpoint.c_str(), httpResponseCode);
    }
    http.end();
  }
  return success;
}

/**
 * Queues an offline event payload in NVS to prevent losing events during network outages.
 */
void enqueueOfflineEvent(String payload) {
  int n = prefs.getInt("qn", 0);
  if (n < 20) { // Limit queue to 20 events to avoid memory issues
    prefs.putString(("q" + String(n)).c_str(), payload);
    prefs.putInt("qn", n + 1);
    Serial.printf("Enqueued offline event at index %d\n", n);
  }
}

/**
 * Flushes the offline event queue when internet connectivity is back online.
 */
void flushOfflineQueue() {
  if (WiFi.status() != WL_CONNECTED) return;
  int n = prefs.getInt("qn", 0);
  if (n == 0) return;

  Serial.printf("Flushing %d offline event(s) to server...\n", n);
  int sent = 0;
  for (int i = 0; i < n; i++) {
    String payload = prefs.getString(("q" + String(i)).c_str(), "");
    String response;
    
    // Determine the type of request to flush by parsing the JSON payload roughly
    String endpoint = "/api/deposit";
    if (payload.indexOf("pickup") != -1) {
      endpoint = "/api/pickup";
    }

    if (payload.length() > 0 && executePOST(endpoint, payload, response)) {
      sent++;
    } else {
      break; // Stop flushing if the server goes down again
    }
  }

  if (sent > 0) {
    int remain = n - sent;
    // Shift queue left
    for (int i = 0; i < remain; i++) {
      String nextPayload = prefs.getString(("q" + String(i + sent)).c_str(), "");
      prefs.putString(("q" + String(i)).c_str(), nextPayload);
    }
    // Delete shifted slots
    for (int i = remain; i < n; i++) {
      prefs.remove(("q" + String(i)).c_str());
    }
    prefs.putInt("qn", remain);
    Serial.printf("Successfully flushed %d events, %d remaining\n", sent, remain);
  }
}

/**
 * Sends locker heartbeat status.
 */
void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;
  String payload = "{\"locker_id\":\"" + LOCKER_ID + "\"}";
  String response;
  executePOST("/api/heartbeat", payload, response);
}

// --- Handler Functions ---

/**
 * Routes Uart lines received from Arduino Uno.
 */
void handleUartLine(String line) {
  Serial.println("UART In: " + line);

  // Keypad wrong PIN locking check
  if (lockedUntil > 0 && millis() < lockedUntil) {
    unsigned long remaining = (lockedUntil - millis()) / 1000;
    sendMsgToUno("Keypad dang khoa|" + String(remaining) + "s de thu lai");
    return;
  }

  if (line.startsWith("DEPOSIT:")) {
    doDeposit(line.substring(8));
  } else if (line.startsWith("PIN:")) {
    doPinPickup(line.substring(4));
  } else if (line.startsWith("ORDER:")) {
    doOrderCOD(line.substring(6));
  } else if (line == "CANCEL") {
    // Handled locally inside polling loops
  }
}

/**
 * Handles Shipper package deposits.
 */
void doDeposit(String orderCode) {
  // Find a free compartment
  int idx = -1;
  for (int i = 0; i < NUM_COMP; i++) {
    if (!occupied[i]) {
      idx = i;
      break;
    }
  }

  if (idx < 0) {
    sendMsgToUno("Het hoc trong!");
    oledShowText("TU KHOA", "Het hoc trong!");
    return;
  }

  String newPin = generateRandomPIN();
  String compStr = COMP_ID[idx];

  // Prepare payload
  StaticJsonDocument<200> reqDoc;
  reqDoc["locker_id"] = LOCKER_ID;
  reqDoc["order_code"] = orderCode;
  reqDoc["compartment"] = compStr;
  reqDoc["pin"] = newPin;

  String reqPayload;
  serializeJson(reqDoc, reqPayload);

  // Set default fallback variables (Prepaid state)
  bool serverCOD = false;
  int serverAmount = 0;
  bool apiSuccess = false;

  String response;
  oledShowText("DEPOSIT", "Dang gui server...");
  
  if (executePOST("/api/deposit", reqPayload, response)) {
    StaticJsonDocument<200> resDoc;
    DeserializationError error = deserializeJson(resDoc, response);
    if (!error && resDoc["ok"] == true) {
      serverCOD = resDoc["is_cod"] | false;
      serverAmount = resDoc["amount"] | 0;
      apiSuccess = true;
    }
  }

  // Update local states in NVS and RAM
  occupied[idx] = true;
  pinStore[idx] = newPin;
  orderStore[idx] = orderCode;
  isCodStore[idx] = serverCOD;
  saveCompartmentState(idx);

  if (!apiSuccess) {
    // Offline mode backup
    enqueueOfflineEvent(reqPayload);
    Serial.println("Deposit queued offline.");
    sendMsgToUno("Cat vao hoc " + compStr + "|PIN: " + newPin + " (OFF)");
    oledShowText("GUI HANG LOCAL", "PIN: " + newPin + " | " + compStr);
  } else {
    // Online mode
    Serial.printf("Deposit success. isCOD=%d, Amount=%d\n", serverCOD, serverAmount);
    sendMsgToUno("Da cat hoc " + compStr + "|PIN: (Xem tren web)");
    oledShowText("DA LUU HANG", "Hoc " + compStr);
  }

  // Open compartment to let shipper drop the package
  openLockerCompartment(idx);
}

/**
 * Handles PIN matching pickups (Prepaid or COD offline).
 */
void doPinPickup(String pin) {
  for (int i = 0; i < NUM_COMP; i++) {
    if (occupied[i] && pinStore[i] == pin) {
      // PIN MATCHED - OPEN LOCKER
      wrongPinCount = 0; // Reset brute force counter
      
      String compStr = COMP_ID[i];
      String orderCode = orderStore[i];

      // Reset compartment memory and NVS cache
      occupied[i] = false;
      pinStore[i] = "";
      orderStore[i] = "";
      isCodStore[i] = false;
      saveCompartmentState(i);

      // Send update payload to server
      StaticJsonDocument<200> reqDoc;
      reqDoc["locker_id"] = LOCKER_ID;
      reqDoc["order_code"] = orderCode;
      reqDoc["compartment"] = compStr;
      
      String reqPayload;
      serializeJson(reqDoc, reqPayload);

      String response;
      if (!executePOST("/api/pickup", reqPayload, response)) {
        enqueueOfflineEvent(reqPayload);
        Serial.println("Pickup event queued offline.");
      }

      sendMsgToUno("PIN dung|Mo hoc " + compStr);
      oledShowText("LAY HANG", "Hoc " + compStr + " mo");
      
      // Actuate Servo
      openLockerCompartment(i);
      
      // Reset OLED
      oledShowText("SMART LOCKER", "San sang");
      return;
    }
  }

  // WRONG PIN LOGIC
  wrongPinCount++;
  if (wrongPinCount >= 5) {
    lockedUntil = millis() + 60000; // Lock for 60 seconds
    sendMsgToUno("Sai qua 5 lan|Keypad khoa 60s");
    oledShowText("KHOA KEYPAD", "Sai PIN qua 5 lan");
  } else {
    sendMsgToUno("PIN sai! (" + String(5 - wrongPinCount) + " lan con lai)");
    oledShowText("SAI PIN", "Moi nhap lai");
  }
}

/**
 * Handles COD Online QR display and polling logic.
 */
void doOrderCOD(String orderCode) {
  // Find matching order in local storage
  int idx = -1;
  for (int i = 0; i < NUM_COMP; i++) {
    if (occupied[i] && orderStore[i] == orderCode && isCodStore[i] == true) {
      idx = i;
      break;
    }
  }

  if (idx < 0) {
    sendMsgToUno("Don hang khong|dung hoac da giao");
    oledShowText("LOI", "Ma don khong hop le");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Fallback if cabinet is offline
    sendMsgToUno("Tu offline|Dung web lay PIN");
    oledShowText("TU OFFLINE", "Dung web nhan PIN");
    return;
  }

  // 1. Fetch VietQR string from Next.js server
  String qrResponse;
  oledShowText("VIETQR", "Dang lay QR code...");
  
  if (!executeGET("/api/qr?order_code=" + orderCode, qrResponse)) {
    sendMsgToUno("Loi he thong|Khong the lay QR");
    oledShowText("LOI HE THONG", "Khong lay duoc QR");
    return;
  }

  StaticJsonDocument<300> qrDoc;
  DeserializationError error = deserializeJson(qrDoc, qrResponse);
  if (error) {
    sendMsgToUno("Loi parse QR|Moi thu lai");
    return;
  }

  String qrString = qrDoc["qr_string"];
  int amount = qrDoc["amount"];

  // 2. Draw QR code on OLED
  drawQrOLED(qrString, orderCode, amount);
  sendMsgToUno("Quet QR de mua|Bam * de huy");

  // 3. Polling loop check for payment (maximum 180 seconds)
  unsigned long startPoll = millis();
  bool paidSuccess = false;
  
  while (millis() - startPoll < 180000) {
    // Check if customer cancelled from keyboard
    if (Serial2.available() > 0) {
      String line = Serial2.readStringUntil('\n');
      line.trim();
      if (line == "CANCEL") {
        Serial.println("Polling cancelled by user.");
        break;
      }
    }

    // Call status endpoint
    String statusResponse;
    if (executeGET("/api/payment-status?order_code=" + orderCode, statusResponse)) {
      StaticJsonDocument<200> statusDoc;
      DeserializationError err = deserializeJson(statusDoc, statusResponse);
      if (!err && statusDoc["paid"] == true) {
        paidSuccess = true;
        break;
      }
    }

    delay(2000); // Poll status every 2 seconds
  }

  // 4. Handle polling results
  oled.clearDisplay();
  oled.display();

  if (paidSuccess) {
    // Payment verified - open cabinet compartment
    String compStr = COMP_ID[idx];
    
    // Clear state
    occupied[idx] = false;
    pinStore[idx] = "";
    orderStore[idx] = "";
    isCodStore[idx] = false;
    saveCompartmentState(idx);

    // Notify pickup event
    StaticJsonDocument<200> reqDoc;
    reqDoc["locker_id"] = LOCKER_ID;
    reqDoc["order_code"] = orderCode;
    reqDoc["compartment"] = compStr;
    
    String reqPayload;
    serializeJson(reqDoc, reqPayload);
    String pickupResponse;
    executePOST("/api/pickup", reqPayload, pickupResponse);

    sendMsgToUno("Da nhan tien!|Mo hoc " + compStr);
    oledShowText("THANH TOAN OK", "Mo hoc " + compStr);
    
    openLockerCompartment(idx);
    
    oledShowText("SMART LOCKER", "San sang");
  } else {
    // Timeout or Cancelled
    sendMsgToUno("Thanh toan bi huy|Moi thu lai");
    oledShowText("HUY GIAO DICH", "Chua thanh toan");
    delay(2500);
    oledShowText("SMART LOCKER", "San sang");
  }
}
