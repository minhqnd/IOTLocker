#include <Keypad.h>
#include <LiquidCrystal.h>

// LCD 4-bit Mode Pinout: RS -> D10, Enable -> D11, D4 -> D12, D5 -> D13, D6 -> A0, D7 -> A1
LiquidCrystal lcd(10, 11, 12, 13, A0, A1);

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

String inputBuffer = "";

void setup() {
  // Serial Monitor for PC debugging
  Serial.begin(9600);
  Serial.println("Arduino Uno Hardware Test Started");

  // Initialize LCD
  lcd.begin(16, 2);
  lcd.print("TEST PHAN CUNG");
  lcd.setCursor(0, 1);
  lcd.print("Bam phim ma tran");
}

void loop() {
  char key = keypad.getKey();
  
  if (key != NO_KEY) {
    Serial.print("Phim da bam: ");
    Serial.println(key);
    
    if (key == '*') {
      // Clear buffer on '*'
      inputBuffer = "";
      lcd.clear();
      lcd.print("Da xoa bo nho");
      delay(1000);
      lcd.clear();
      lcd.print("Nhap tiep:");
    } 
    else if (key == '#') {
      // Submit/Confirm on '#'
      lcd.clear();
      lcd.print("Xac nhan:");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      Serial.println("Chuoi da nhap: " + inputBuffer);
      inputBuffer = ""; // Reset after confirm
    } 
    else {
      // Accumulate inputs
      if (inputBuffer.length() < 16) {
        inputBuffer += key;
      }
      lcd.clear();
      lcd.print("Dang nhap:");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
    }
  }
}
