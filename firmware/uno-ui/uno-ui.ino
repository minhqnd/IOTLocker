#include <Keypad.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

// --- Configuration & Pin Definitions ---

// LCD1602 Pinout (4-bit mode)
// RS -> D10, Enable -> D11, D4 -> D12, D5 -> D13, D6 -> A0, D7 -> A1
const int pin_RS = 10;
const int pin_EN = 11;
const int pin_D4 = 12;
const int pin_D5 = 13;
const int pin_D6 = A0;
const int pin_D7 = A1;
LiquidCrystal lcd(pin_RS, pin_EN, pin_D4, pin_D5, pin_D6, pin_D7);

// Keypad 4x4 Pinout
// Rows: D2, D3, D4, D5
// Columns: D6, D7, D8, D9
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// SoftwareSerial for UART communication with ESP32
// RX pin = A3 (connect to ESP32 TX2 GPIO17)
// TX pin = A2 (connect to ESP32 RX2 GPIO16 via 1k/2k voltage divider)
const int pin_RX = A3;
const int pin_TX = A2;
SoftwareSerial espSerial(pin_RX, pin_TX); // RX, TX

// --- State Management ---
enum SystemState {
  STATE_IDLE,       // Waiting for PIN, Order (A+code), or Shipper code (9999)
  STATE_DEPOSIT     // Waiting for shipper to enter order code
};

SystemState currentState = STATE_IDLE;
String inputBuffer = "";
const unsigned int MAX_INPUT_LENGTH = 16;

// --- Function Declarations ---
void updateLCD(String line1, String line2);
void displayDefaultScreen();
void handleKeypress(char key);
void readSerialFromESP();

// --- Setup & Loop ---

void setup() {
  // Initialize standard Serial for debugging/monitoring
  Serial.begin(9600);
  Serial.println("Uno Terminal Initialized");

  // Initialize SoftwareSerial for communication with ESP32
  espSerial.begin(9600);

  // Initialize LCD
  lcd.begin(16, 2);
  displayDefaultScreen();
}

void loop() {
  // 1. Read keypad input
  char key = keypad.getKey();
  if (key != NO_KEY) {
    handleKeypress(key);
  }

  // 2. Read messages from ESP32 via UART
  readSerialFromESP();
}

// --- Logic Implementations ---

/**
 * Updates the LCD 1602 screen with two lines of text.
 * Truncates lines to 16 characters if necessary.
 */
void updateLCD(String line1, String line2) {
  lcd.clear();
  
  lcd.setCursor(0, 0);
  if (line1.length() > 16) {
    lcd.print(line1.substring(0, 16));
  } else {
    lcd.print(line1);
  }
  
  lcd.setCursor(0, 1);
  if (line2.length() > 16) {
    lcd.print(line2.substring(0, 16));
  } else {
    lcd.print(line2);
  }
}

/**
 * Shows the default waiting screen on the LCD.
 */
void displayDefaultScreen() {
  updateLCD("Smart Locker L01", "Nhap PIN / Phim A");
}

/**
 * Handles keypad character inputs, building the buffers,
 * transitioning states, and sending UART payloads.
 */
void handleKeypress(char key) {
  Serial.print("Key pressed: ");
  Serial.println(key);

  // '*' acts as Clear or Cancel
  if (key == '*') {
    inputBuffer = "";
    if (currentState == STATE_DEPOSIT) {
      currentState = STATE_IDLE;
      Serial.println("Cancelled deposit mode");
    }
    // Inform ESP32 that the current operation is cancelled
    espSerial.println("CANCEL");
    displayDefaultScreen();
    return;
  }

  // '#' acts as Enter/Confirm
  if (key == '#') {
    if (inputBuffer.length() == 0) {
      return; // Nothing to submit
    }

    if (currentState == STATE_IDLE) {
      // Check for shipper entry code
      if (inputBuffer == "9999") {
        currentState = STATE_DEPOSIT;
        inputBuffer = "";
        updateLCD("NHAP MA DON:", "(Shipper)");
        Serial.println("Switched to DEPOSIT state");
      }
      // Check for COD order entry (starts with A)
      else if (inputBuffer.startsWith("A")) {
        String orderCode = inputBuffer.substring(1); // Strip the 'A'
        espSerial.println("ORDER:" + orderCode);
        Serial.println("Sent ORDER:" + orderCode);
        inputBuffer = "";
        updateLCD("Gui ma don...", "Vui long cho...");
      }
      // Otherwise, treat as regular prepaid PIN or COD-offline PIN entry
      else {
        espSerial.println("PIN:" + inputBuffer);
        Serial.println("Sent PIN:" + inputBuffer);
        inputBuffer = "";
        updateLCD("Gui ma PIN...", "Vui long cho...");
      }
    } 
    else if (currentState == STATE_DEPOSIT) {
      // Shipper enters the order code to deposit the package
      espSerial.println("DEPOSIT:" + inputBuffer);
      Serial.println("Sent DEPOSIT:" + inputBuffer);
      inputBuffer = "";
      currentState = STATE_IDLE;
      updateLCD("Gui deposit...", "Vui long cho...");
    }
    return;
  }

  // Handle character accumulation (0-9, A, B, C, D)
  if (inputBuffer.length() < MAX_INPUT_LENGTH) {
    inputBuffer += key;
    
    // Refresh LCD display with masked/unmasked buffer
    if (currentState == STATE_DEPOSIT) {
      updateLCD("NHAP MA DON:", inputBuffer);
    } 
    else { // STATE_IDLE
      if (inputBuffer.startsWith("A")) {
        // Display COD code entry clearly
        updateLCD("MA DON COD:", inputBuffer);
      } else {
        // Mask PIN entries with '*' for security
        String masked = "";
        for (unsigned int i = 0; i < inputBuffer.length(); i++) {
          masked += "*";
        }
        updateLCD("NHAP MA PIN:", masked);
      }
    }
  }
}

/**
 * Reads messages from the ESP32 via SoftwareSerial.
 * Protocol: expects "MSG:<text>" where <text> is printed on LCD.
 * Supporting line breaks using '|' separator. Example: "MSG:Line 1|Line 2"
 */
void readSerialFromESP() {
  if (espSerial.available() > 0) {
    String line = espSerial.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) return;

    Serial.print("UART received: ");
    Serial.println(line);

    if (line.startsWith("MSG:")) {
      String msg = line.substring(4);
      int pipeIdx = msg.indexOf('|');
      
      if (pipeIdx != -1) {
        String line1 = msg.substring(0, pipeIdx);
        String line2 = msg.substring(pipeIdx + 1);
        updateLCD(line1, line2);
      } else {
        updateLCD(msg, "");
      }
    }
  }
}
