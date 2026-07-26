void buzzerOn() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? HIGH : LOW); }

void buzzerOff() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? LOW : HIGH); }

void beepScanOk() {
  buzzerOn();
  delay(120);
  buzzerOff();
}
