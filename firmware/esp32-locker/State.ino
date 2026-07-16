void loadLocker(int locker) {
  String key = lockerKey(locker);
  String stored = preferences.getString(key.c_str(), "");
  stored.toUpperCase();

  if (stored.length() > 0 && !isValidUid(stored)) {
    stored = "";
    preferences.putString(key.c_str(), "");
  }

  lockerUid[locker] = stored;
}

void saveLocker(int locker) {
  String key = lockerKey(locker);
  preferences.putString(key.c_str(), lockerUid[locker]);
}

String lockerKey(int locker) { return "uid" + String(locker); }

String lockerLabel(int locker) {
  String label = "HOC ";
  if (locker + 1 < 10)
    label += "0";
  label += String(locker + 1);
  return label;
}

bool isValidUid(const String &uid) {
  if (uid.length() < UID_MIN_LENGTH || uid.length() > UID_MAX_LENGTH)
    return false;

  for (unsigned int i = 0; i < uid.length(); i++) {
    if (!isxdigit((unsigned char)uid.charAt(i)))
      return false;
  }

  return true;
}

int findLockerByUid(const String &uid) {
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i] == uid)
      return i;
  }
  return -1;
}

int findEmptyLocker() { return findLockerByUid(""); }

void printLockerState() {
  Serial.println(F("===== LOCKER STATE ====="));
  for (int i = 0; i < LOCKER_COUNT; i++) {
    Serial.print(F("Hoc "));
    Serial.print(i + 1);
    Serial.print(F(": "));
    if (lockerUid[i].length() == 0)
      Serial.println(F("TRONG"));
    else
      Serial.println(lockerUid[i]);
  }
  Serial.println(F("========================"));
}

void logEvent(const char *eventName, const String &uid, int locker) {
  Serial.print(F("[EVENT] "));
  Serial.print(eventName);
  Serial.print(F(" | UID="));
  Serial.print(uid);
  Serial.print(F(" | locker="));
  Serial.println(locker + 1);
}
