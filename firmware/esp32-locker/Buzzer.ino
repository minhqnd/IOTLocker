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
