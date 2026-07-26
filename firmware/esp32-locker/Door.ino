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
  waitForDoorClose(locker, false);

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

bool isDoorClosedStable(int locker) {
  unsigned long closedAt = millis();
  while (millis() - closedAt < DOOR_CLOSED_STABLE) {
    if (digitalRead(DOOR_PINS[locker]) != DOOR_CLOSED_STATE)
      return false;
    delay(20);
  }
  return true;
}

void waitForDoorClose(int locker, bool alarmNow) {
  unsigned long startedAt = millis();
  unsigned long closedAt = 0;
  unsigned long beepStartedAt = 0;
  unsigned long lastBeepAt = alarmNow ? millis() - 1000 : 0;
  bool alarmStarted = alarmNow;
  bool beeping = false;

  if (alarmStarted)
    showLockerGuide(locker, "CUA DANG MO", "Dong cua lai");

  // Da mo cua thi cho den luc dong that, khong het gio roi bo mac cua.
  while (true) {
    if (digitalRead(DOOR_PINS[locker]) == DOOR_CLOSED_STATE) {
      if (closedAt == 0)
        closedAt = millis();
      if (millis() - closedAt >= DOOR_CLOSED_STABLE) {
        buzzerOff();
        return;
      }
    } else
      closedAt = 0;

    if (!alarmStarted && millis() - startedAt > DOOR_ALARM_AFTER) {
      alarmStarted = true;
      showLockerGuide(locker, "Qua lau", "Dong cua lai");
    }

    if (alarmStarted) {
      if (!beeping && millis() - lastBeepAt >= 1000) {
        buzzerOn();
        beeping = true;
        beepStartedAt = millis();
        lastBeepAt = beepStartedAt;
      }
      if (beeping && millis() - beepStartedAt >= 150) {
        buzzerOff();
        beeping = false;
      }
    }
    delay(20);
  }
}

void reportDoorError(int locker) {
  report(false, locker + 1, "NOT_OPENED", "CUA KHONG MO");
}

void report(bool ok, int lockerNumber, const char *code, const char *title) {
  Serial.print(ok ? F("OK|") : F("ERR|"));
  Serial.print(lockerNumber);
  Serial.print('|');
  Serial.println(code);
  showResult(ok, title, lockerNumber);
}

void moveServo(int locker, int angle) {
  if (!lockerServo[locker].attached()) {
    lockerServo[locker].setPeriodHertz(50);
    lockerServo[locker].attach(SERVO_PINS[locker], 500, 2400);
  }

  lockerServo[locker].write(angle);
  delay(SERVO_MOVE_DELAY);
  lockerServo[locker].detach();
}

void unlockDoor(int locker) { moveServo(locker, SERVO_UNLOCK_ANGLE); }

void lockDoor(int locker) { moveServo(locker, SERVO_LOCK_ANGLE); }
