bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_TIMEOUT) {
    delay(200);
  }

  bool connected = WiFi.status() == WL_CONNECTED;
  Serial.print(F("[WIFI] "));
  Serial.println(connected ? WiFi.localIP().toString()
                           : "timeout, chay offline");
  return connected;
}

void sendHeartbeat() {
  lastHeartbeatAt = millis();
  if (!ensureWifi()) {
    serverOnline = false;
    if (idleVisible)
      showIdle();
    return;
  }

  StaticJsonDocument<128> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  serverOnline = postJsonOk("/api/locker/heartbeat", bodyDoc);
  Serial.print(F("[API] heartbeat "));
  Serial.println(serverOnline ? F("ok") : F("fail"));
  if (serverOnline)
    postLockerState();
  if (idleVisible)
    showIdle();
}

bool postLockerState() {
  if (!ensureWifi())
    return false;

  StaticJsonDocument<512> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  JsonArray lockers = bodyDoc.createNestedArray("lockers");
  for (int i = 0; i < LOCKER_COUNT; i++) {
    if (lockerUid[i].length() == 0)
      continue;

    JsonObject item = lockers.createNestedObject();
    item["locker"] = i + 1;
    item["uid"] = lockerUid[i];
  }

  bool ok = postJsonOk("/api/locker/state", bodyDoc);
  Serial.print(F("[SYNC] local -> server "));
  Serial.println(ok ? F("ok") : F("fail"));
  return ok;
}

void postDeposit(const String &uid, int locker) {
  if (!ensureWifi()) {
    logEvent("deposit_offline", uid, locker);
    return;
  }

  StaticJsonDocument<160> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  bodyDoc["uid"] = uid;
  bodyDoc["locker"] = locker + 1;
  if (postJsonOk("/api/locker/deposit", bodyDoc))
    logEvent("deposit_api", uid, locker);
  else
    logEvent("deposit_api_fail", uid, locker);
}

void postPickup(const String &uid) {
  if (!ensureWifi()) {
    Serial.println(F("[API] pickup offline"));
    return;
  }

  StaticJsonDocument<160> bodyDoc;
  bodyDoc["deviceId"] = DEVICE_ID;
  bodyDoc["uid"] = uid;
  Serial.print(F("[API] pickup "));
  Serial.println(postJsonOk("/api/locker/pickup", bodyDoc) ? F("ok")
                                                           : F("fail"));
}

AccessCheck checkServerAccess(const String &uid, char mode) {
  AccessCheck result = {ACCESS_BYPASS, "", "", 0};

  // Mat mang thi cho mo tam, vi tu van con UID local trong NVS de doi chieu.
  if (!ensureWifi())
    return result;

  String path = "/api/locker/access?deviceId=" + String(DEVICE_ID) +
                "&uid=" + uid + "&mode=" + String(mode);
  StaticJsonDocument<1536> doc;
  if (!getJson(path, doc)) {
    Serial.println(F("[API] access timeout, bypass"));
    return result;
  }

  if (doc["allowOpen"] | false) {
    result.decision = ACCESS_ALLOW;
    return result;
  }

  const char *paymentId = doc["paymentId"] | "";
  if (paymentId[0] != '\0') {
    result.decision = ACCESS_NEED_PAYMENT;
    result.paymentId = paymentId;
    result.fee = doc["fee"] | 0;

    const char *qrPayload = doc["qrPayload"] | "";
    if (qrPayload[0] != '\0') {
      result.qrPayload = qrPayload;
      Serial.print(F("PAYQR|"));
      Serial.println(qrPayload);
    }
    return result;
  }

  if (doc["found"].is<bool>() && !(doc["found"] | false)) {
    result.decision = ACCESS_DENY;
    return result;
  }

  return result;
}

bool canContinueAfterAccess(const AccessCheck &access, int locker,
                            const String &uid) {
  if (access.decision == ACCESS_ALLOW || access.decision == ACCESS_BYPASS)
    return true;

  if (access.decision == ACCESS_NEED_PAYMENT) {
    bool paid = waitForPayment(access.paymentId, access.qrPayload, access.fee);
    if (paid)
      return true;

    report(false, locker + 1, "PAY_TIMEOUT", "CHUA THANH TOAN");
    return false;
  }

  report(false, locker + 1, "SERVER_DENY", "KHONG MO DUOC");
  Serial.print(F("[ACCESS DENY] UID="));
  Serial.println(uid);
  return false;
}

bool isPaymentReady(const String &paymentId) {
  String path = "/api/payment/status?paymentId=" + paymentId;
  StaticJsonDocument<512> doc;
  if (!getJson(path, doc)) {
    Serial.println(F("[PAY] timeout, bypass"));
    return true;
  }

  bool paid = (doc["paid"] | false) || (doc["allowOpen"] | false);
  Serial.print(F("[PAY] status="));
  Serial.print((const char *)(doc["paymentStatus"] | "unknown"));
  Serial.print(F(" paid="));
  Serial.println(paid ? F("yes") : F("no"));
  return paid;
}

bool waitForPayment(const String &paymentId, const String &qrPayload, int fee) {
  if (paymentId.length() == 0)
    return false;

  unsigned long startedAt = millis();
  while (millis() - startedAt < PAYMENT_POLL_TIMEOUT) {
    showPayment(paymentId, qrPayload, fee);

    if (!ensureWifi()) {
      Serial.println(F("[PAY] mat mang luc cho, bypass"));
      return true;
    }

    if (isPaymentReady(paymentId)) {
      showText("DA THANH TOAN", "Dang mo...");
      return true;
    }

    delay(PAYMENT_POLL_EVERY);
  }

  // Co luc webhook ve sat nut timeout, check them lan cuoi truoc khi bao fail.
  if (isPaymentReady(paymentId)) {
    showText("DA THANH TOAN", "Dang mo...");
    return true;
  }

  Serial.println(F("[PAY] het gio cho thanh toan"));
  return false;
}

String httpGet(const String &path) { return httpRequest("GET", path, ""); }

String httpPost(const String &path, const String &body) {
  return httpRequest("POST", path, body);
}

bool postJsonOk(const String &path, JsonDocument &bodyDoc) {
  String response = httpPost(path, toJson(bodyDoc));
  StaticJsonDocument<512> doc;
  return response.length() > 0 && parseJson(response, doc) &&
         (doc["ok"] | false);
}

bool getJson(const String &path, JsonDocument &doc) {
  String response = httpGet(path);
  return response.length() > 0 && parseJson(response, doc);
}

String httpRequest(const char *method, const String &path, const String &body) {
  HTTPClient http;
  String url = String(API_BASE_URL) + path;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT);

  int status;
  if (method[0] == 'P') {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(body);
  } else {
    status = http.GET();
  }

  String response = "";
  if (status > 0)
    response = http.getString();
  http.end();

  Serial.print('[');
  Serial.print(method);
  Serial.print(F("] "));
  Serial.print(path);
  Serial.print(F(" -> "));
  Serial.println(status);
  return response;
}

bool parseJson(const String &response, JsonDocument &doc) {
  DeserializationError error = deserializeJson(doc, response);
  if (!error)
    return true;

  Serial.print(F("[JSON] "));
  Serial.println(error.c_str());
  return false;
}

String toJson(JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  return body;
}
