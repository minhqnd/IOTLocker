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

void unlockDoor(int locker) {
  lockerServo[locker].write(SERVO_UNLOCK_ANGLE);
  delay(700);
}

void lockDoor(int locker) {
  lockerServo[locker].write(SERVO_LOCK_ANGLE);
  delay(700);
}
