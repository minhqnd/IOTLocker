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
  postLockerState();
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
    postLockerState();
  } else {
    logEvent("reopen", uid, locker);
    postLockerState();
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
