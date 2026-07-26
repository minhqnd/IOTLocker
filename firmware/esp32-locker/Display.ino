void showIdle() {
  if (!oledReady)
    return;

  int used = 0;
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i].length() > 0)
      used++;
  }

  display.clearDisplay();
  // Font mac dinh khong co bold, ve lech 1px de tieu de trong dam hon.
  drawLineAt(10, 2, 0, "TU GUI DO");
  drawLineAt(11, 2, 0, "TU GUI DO");
  drawLine(1, 25,
           "Kha dung: " + String(LOCKER_COUNT - used) + "/" +
               String(LOCKER_COUNT));
  // drawLine(1, 32, "Dang dung: " + String(used));
  drawLine(1, 50, serverOnline ? "San sang" : "Mat ket noi");
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

void showPayment(const String &qrPayload, int fee, int secondsLeft) {
  if (!oledReady)
    return;
  idleVisible = false;

  display.clearDisplay();
  if (qrPayload.length() > 0) {
    drawQr(qrPayload, 0, 0);
    byte textX = qrRenderedSize + 4;
    drawLineAt(textX, 1, 2, "QUET QR");
    drawLineAt(textX, 2, 18, String(secondsLeft) + "s");
    drawLineAt(textX, 1, 42, String(fee) + " VND");
    drawLineAt(textX, 1, 56, "*: Huy");
  } else {
    drawLine(1, 0, "CAN THANH TOAN");
    drawLine(1, 18, "Phi: " + String(fee) + " VND");
    drawLine(2, 32, String(secondsLeft) + "s");
    drawLine(1, 54, "*: Huy");
  }
  display.display();
}

void showPaymentMenu(int fee) {
  sendUnoUi("PAY_MENU");
  if (!oledReady) return;
  idleVisible = false;
  display.clearDisplay();
  drawLine(1, 0, "PHI: " + String(fee) + " VND");
  drawLine(1, 16, "1: QUET QR");
  drawLine(1, 32, "2: TRA KHI LAY XE");
  drawLine(1, 48, "*: HUY");
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
  const byte quietZone = 4;
  qrRenderedSize = size + quietZone * 2;
  display.fillRect(qrDrawX, qrDrawY, qrRenderedSize, qrRenderedSize,
                   SSD1306_WHITE);

  for (int qrY = 0; qrY < size; qrY++) {
    for (int qrX = 0; qrX < size; qrX++) {
      if (esp_qrcode_get_module(qrcode, qrX, qrY)) {
        display.drawPixel(qrDrawX + qrX + quietZone,
                          qrDrawY + qrY + quietZone, SSD1306_BLACK);
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
  sendUnoUi("IDLE");
  idleReturnedEarly = true;
}

void sendUnoUi(const char *state) {
  UnoSerial.print(F("UI|"));
  UnoSerial.println(state);
}
