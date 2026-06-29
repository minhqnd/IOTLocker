#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Servo servo1;
Servo servo2;
const int SERVO1_PIN = 18;
const int SERVO2_PIN = 23;

void drawStaticQR() {
  QRCode qrcode;
  // Version 3 is 29x29 modules, easily fits on OLED height
  int qrVersion = 3;
  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  
  // Initialize QR code with a test URL (Richard Moore QRCode API)
  qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, "https://sepay.vn");
  
  oled.clearDisplay();
  
  // Left side panel text info
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 4);
  oled.println("ESP32 TEST");
  oled.setCursor(0, 18);
  oled.println("OLED OK");
  oled.setCursor(0, 32);
  oled.println("QR CODE v3");
  oled.setCursor(0, 48);
  oled.println("SERVOS LOOP");

  // Center QR code on the right side of the screen
  int xOffset = 85;
  int yOffset = (SCREEN_HEIGHT - qrcode.size) / 2;

  // Draw white background backing quiet zone
  oled.fillRect(xOffset - 2, yOffset - 2, qrcode.size + 4, qrcode.size + 4, SSD1306_WHITE);

  // Draw black modules
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        oled.drawPixel(xOffset + x, yOffset + y, SSD1306_BLACK);
      }
    }
  }
  
  oled.display();
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Hardware Test Starting (using libraries)...");

  // Attach servos (using ESP32Servo library)
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  
  // Set initial position to locked (0 degrees)
  servo1.write(0);
  servo2.write(0);

  // Initialize OLED
  Wire.begin(21, 22);
  if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED successfully initialized");
    drawStaticQR();
  } else {
    Serial.println("OLED initialization failed! Check I2C lines (SDA=21, SCL=22)");
  }
}

void loop() {
  Serial.println("\n--- Starting Servo Actuation Sequence ---");
  
  // 1. Actuate Servo 1 (Compartment 1)
  Serial.println("Activating Servo 1 (Pin 18) -> 90 degrees");
  servo1.write(90);
  delay(2000);
  Serial.println("Returning Servo 1 -> 0 degrees");
  servo1.write(0);
  
  delay(1000); // 1s pause between servos
  
  // 2. Actuate Servo 2 (Compartment 2)
  Serial.println("Activating Servo 2 (Pin 23) -> 90 degrees");
  servo2.write(90);
  delay(2000);
  Serial.println("Returning Servo 2 -> 0 degrees");
  servo2.write(0);
  
  Serial.println("Waiting 5 seconds before repeating...");
  delay(5000);
}
