#include "web.h"
#include "web_ui.h"
#include "config.h"
#include "net.h"
#include "gateway.h"
#include "modbus_rtu.h"
#include "display.h"
#include "fw_version.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <time.h>

volatile uint32_t webRebootAt = 0;
volatile uint8_t  webOtaReq = 0;
String            webOtaState = "";
String            webOtaLatest = "";

static WebServer web(80);
static bool   s_upOk = false;        // subida de firmware autorizada y sin error
static String s_upErr;

// ---------------------------------------------------------------------------
static void sendJson(int code, JsonDocument& doc) {
  String s;
  serializeJson(doc, s);
  web.sendHeader("Cache-Control", "no-store");
  web.send(code, "application/json", s);
}

static void sendOk(bool reboot = false) {
  JsonDocument d;
  d["ok"] = true;
  if (reboot) d["reboot"] = true;
  sendJson(200, d);
}

static void sendErr(int code, const char* msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  sendJson(code, d);
}

// Desde el AP de configuracion (protegido con WPA2) no se pide usuario/clave:
// el navegador del portal cautivo de los telefonos no maneja bien HTTP Basic.
static bool fromTrustedAp() {
  return netIsApClient((uint32_t)web.client().remoteIP()) && strlen(cfg.apPass) >= 8;
}

static bool auth() {
  if (fromTrustedAp()) return true;
  if (web.authenticate(cfg.adminUser, cfg.adminPass)) return true;
  web.requestAuthentication(BASIC_AUTH, PRODUCT_NAME);
  return false;
}

static bool parseBody(JsonDocument& doc) {
  if (!web.hasArg("plain")) { sendErr(400, "sin cuerpo"); return false; }
  if (deserializeJson(doc, web.arg("plain")) != DeserializationError::Ok) { sendErr(400, "JSON invalido"); return false; }
  return true;
}

static void scheduleReboot(uint32_t ms = 1500) { webRebootAt = millis() + ms; }

// ---------------------------------------------------------------------------
// Portal cautivo: cualquier host desconocido pedido desde el AP -> la UI
// ---------------------------------------------------------------------------
static bool captiveRedirect() {
  if (!netIsApClient((uint32_t)web.client().remoteIP())) return false;
  String host = web.hostHeader();
  String ap = WiFi.softAPIP().toString();
  if (host == ap || host == String(cfg.hostname) + ".local") return false;
  web.sendHeader("Location", "http://" + ap + "/", true);
  web.send(302, "text/plain", "");
  return true;
}

static void handleRoot() {
  if (captiveRedirect()) return;
  if (!auth()) return;
  web.sendHeader("Cache-Control", "no-cache");
  web.send_P(200, "text/html; charset=utf-8", UI_HTML);
}

static void handleNotFound() {
  if (captiveRedirect()) return;
  web.send(404, "text/plain", "404");
}

// ---------------------------------------------------------------------------
static void handleStatus() {
  if (!auth()) return;
  JsonDocument d;
  d["name"]   = cfg.devName;
  d["fw"]     = FW_SEMVER;
  d["product"]= PRODUCT_NAME;
  d["uptime"] = millis() / 1000;
  d["heap"]   = ESP.getFreeHeap();
  d["minHeap"]= ESP.getMinFreeHeap();
  d["chip"]   = chipId();
  d["defPass"]= cfgDefaultPassword();
  d["mon"]    = gwMonitorEnabled();
  if (netTimeOk()) d["time"] = (uint32_t)time(nullptr);

  JsonObject n = d["net"].to<JsonObject>();
  n["host"] = cfg.hostname;
  JsonObject e = n["eth"].to<JsonObject>();
  e["link"]  = netEthLink();
  e["up"]    = netEthUp();
  e["ip"]    = netEthIp();
  e["speed"] = netEthSpeed();
  e["fd"]    = netEthFullDuplex();
  e["mac"]   = netEthMac();
  e["dhcp"]  = cfg.eth.dhcp;
  e["mask"]  = netEthMask();
  e["gw"]    = netEthGw();
  e["dns"]   = netEthDns();
  JsonObject a = n["ap"].to<JsonObject>();
  a["up"]   = netApUp();
  a["ip"]   = netApIp();
  a["ssid"] = cfg.apSsid;
  a["sta"]  = netApStations();

  JsonObject o = d["ota"].to<JsonObject>();
  o["state"]  = webOtaState;
  o["latest"] = webOtaLatest;

  gwStatusJson(d["gw"].to<JsonObject>());
  sendJson(200, d);
}

static void handleGetConfig() {
  if (!auth()) return;
  JsonDocument d;
  cfgToJson(d, false);
  sendJson(200, d);
}

static void handlePostConfig() {
  if (!auth()) return;
  JsonDocument d;
  if (!parseBody(d)) return;
  cfgFromJson(d.as<JsonVariantConst>());
  if (!cfgSave()) { sendErr(500, "no se pudo guardar"); return; }
  sendOk(true);
  scheduleReboot();
}

static void handleBackup() {
  if (!auth()) return;
  JsonDocument d;
  d["product"] = PRODUCT_NAME;
  d["fw"] = FW_SEMVER;
  cfgToJson(d, true);
  String s;
  serializeJsonPretty(d, s);
  web.sendHeader("Content-Disposition", String("attachment; filename=\"") + cfg.hostname + "-config.json\"");
  web.send(200, "application/json", s);
}

static void handleRestore() {
  if (!auth()) return;
  JsonDocument d;
  if (!parseBody(d)) return;
  if (d["product"].is<const char*>() && strcmp(d["product"], PRODUCT_NAME) != 0) { sendErr(400, "el archivo no es de " PRODUCT_NAME); return; }
  cfgFactory();
  cfgFromJson(d.as<JsonVariantConst>());
  if (!cfgSave()) { sendErr(500, "no se pudo guardar"); return; }
  sendOk(true);
  scheduleReboot();
}

static void handleReboot() {
  if (!auth()) return;
  sendOk(true);
  scheduleReboot(800);
}

static void handleFactory() {
  if (!auth()) return;
  cfgErase();
  sendOk(true);
  scheduleReboot(800);
}

static void handleMonitorGet() {
  if (!auth()) return;
  uint32_t since = web.hasArg("since") ? (uint32_t)web.arg("since").toInt() : 0;
  JsonDocument d;
  d["en"] = gwMonitorEnabled();
  d["last"] = gwMonitorJson(d["items"].to<JsonArray>(), since);
  sendJson(200, d);
}

static void handleMonitorPost() {
  if (!auth()) return;
  JsonDocument d;
  if (!parseBody(d)) return;
  if (d["en"].is<bool>()) gwMonitorEnable(d["en"].as<bool>());
  if (d["clear"] | false) gwMonitorClear();
  sendOk();
}

static void handleStatsReset() {
  if (!auth()) return;
  gwStatsReset();
  sendOk();
}

// Herramienta de prueba: lectura/escritura directa a un esclavo del bus
static void handleTest() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  uint8_t  uid  = q["uid"]  | 1;
  uint8_t  fc   = q["fc"]   | 3;
  uint16_t addr = q["addr"] | 0;
  uint16_t qty  = q["qty"]  | 1;
  uint16_t tmo  = q["timeout"] | 0;

  uint8_t req[RTU_MAX_ADU];
  size_t n = 0;
  req[n++] = uid; req[n++] = fc;
  req[n++] = addr >> 8; req[n++] = addr & 0xFF;
  switch (fc) {
    case 1: case 2: case 3: case 4:
      qty = constrain(qty, 1, (fc <= 2) ? 2000 : 125);
      req[n++] = qty >> 8; req[n++] = qty & 0xFF;
      break;
    case 5: {
      bool v = (q["value"] | 0) != 0;
      req[n++] = v ? 0xFF : 0x00; req[n++] = 0x00;
      break;
    }
    case 6: {
      uint16_t v = (uint16_t)(q["value"] | 0);
      req[n++] = v >> 8; req[n++] = v & 0xFF;
      break;
    }
    case 16: {
      JsonArrayConst vals = q["values"];
      uint16_t c = constrain((int)vals.size(), 1, 123);
      req[n++] = c >> 8; req[n++] = c & 0xFF; req[n++] = c * 2;
      for (uint16_t i = 0; i < c; i++) {
        uint16_t v = (uint16_t)(vals[i] | 0);
        req[n++] = v >> 8; req[n++] = v & 0xFF;
      }
      break;
    }
    default:
      sendErr(400, "funcion no soportada");
      return;
  }

  uint8_t resp[RTU_MAX_ADU];
  size_t rl = 0; uint32_t lat = 0;
  if (!rtuLock(5000)) { sendErr(503, "bus ocupado"); return; }
  RtuResult r = rtuTransact(req, n, resp, rl, sizeof(resp), lat, tmo);
  rtuUnlock();

  JsonDocument d;
  d["ok"] = (r == RTU_OK || r == RTU_BCAST);
  d["result"] = rtuResultStr(r);
  d["lat"] = lat;
  char hex[RTU_MAX_ADU * 3 + 1]; size_t p = 0;
  for (size_t i = 0; i < rl; i++) p += snprintf(hex + p, sizeof(hex) - p, i ? " %02X" : "%02X", resp[i]);
  hex[p] = 0;
  d["raw"] = hex;
  if (r == RTU_OK) {
    if (resp[1] & 0x80) {
      d["ok"] = false;
      d["exception"] = resp[2];
    } else if (fc <= 4 && rl >= 3) {
      JsonArray v = d["values"].to<JsonArray>();
      uint8_t bc = resp[2];
      if (fc <= 2) {
        for (uint16_t i = 0; i < qty && (i >> 3) < bc; i++) v.add((resp[3 + (i >> 3)] >> (i & 7)) & 1);
      } else {
        for (uint16_t i = 0; i + 1 < bc && 4 + i < rl; i += 2) v.add((uint16_t)((resp[3 + i] << 8) | resp[4 + i]));
      }
    }
  }
  sendJson(200, d);
}

static void handleScanPost() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  if (q["stop"] | false) { gwScanStop(); sendOk(); return; }
  if (!gwScanStart(q["from"] | 1, q["to"] | 247, q["timeout"] | 100)) { sendErr(400, "rango invalido"); return; }
  sendOk();
}

static void handleScanGet() {
  if (!auth()) return;
  JsonDocument d;
  gwScanJson(d.to<JsonObject>());
  sendJson(200, d);
}

static void handleOta() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  webOtaReq = (q["apply"] | false) ? 2 : 1;
  webOtaState = "consultando...";
  sendOk();
}

// ---- subida de firmware (.bin) desde el navegador ----
static void handleFwUpload() {
  HTTPUpload& up = web.upload();
  if (up.status == UPLOAD_FILE_START) {
    s_upErr = "";
    s_upOk = fromTrustedAp() || web.authenticate(cfg.adminUser, cfg.adminPass);
    if (!s_upOk) { s_upErr = "no autorizado"; return; }
    Serial.printf("[fw] subiendo %s\n", up.filename.c_str());
    dispProgress("Actualizando FW", up.filename.c_str(), 0);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { s_upOk = false; s_upErr = Update.errorString(); }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_upOk && Update.write(up.buf, up.currentSize) != up.currentSize) { s_upOk = false; s_upErr = Update.errorString(); }
    if (s_upOk) dispProgress("Actualizando FW", "recibiendo...", -1);
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_upOk && !Update.end(true)) { s_upOk = false; s_upErr = Update.errorString(); }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    s_upOk = false;
    s_upErr = "subida abortada";
  }
}

static void handleFwDone() {
  if (!s_upOk) {
    if (Update.isRunning()) Update.abort();
    sendErr(s_upErr == "no autorizado" ? 401 : 400, s_upErr.length() ? s_upErr.c_str() : "error");
    dispMessage("Actualizacion", "FALLO", s_upErr.c_str());
    return;
  }
  sendOk(true);
  dispMessage("Actualizacion", "OK", "reiniciando...", nullptr, 0);
  scheduleReboot(1200);
}

// ---------------------------------------------------------------------------
void webBegin() {
  web.on("/", HTTP_GET, handleRoot);
  web.on("/api/status", HTTP_GET, handleStatus);
  web.on("/api/config", HTTP_GET, handleGetConfig);
  web.on("/api/config", HTTP_POST, handlePostConfig);
  web.on("/api/backup", HTTP_GET, handleBackup);
  web.on("/api/restore", HTTP_POST, handleRestore);
  web.on("/api/reboot", HTTP_POST, handleReboot);
  web.on("/api/factory", HTTP_POST, handleFactory);
  web.on("/api/monitor", HTTP_GET, handleMonitorGet);
  web.on("/api/monitor", HTTP_POST, handleMonitorPost);
  web.on("/api/stats/reset", HTTP_POST, handleStatsReset);
  web.on("/api/test", HTTP_POST, handleTest);
  web.on("/api/scan", HTTP_GET, handleScanGet);
  web.on("/api/scan", HTTP_POST, handleScanPost);
  web.on("/api/ota", HTTP_POST, handleOta);
  web.on("/api/fw", HTTP_POST, handleFwDone, handleFwUpload);
  // deteccion de portal cautivo de Android / iOS / Windows
  web.on("/generate_204", handleNotFound);
  web.on("/gen_204", handleNotFound);
  web.on("/hotspot-detect.html", handleNotFound);
  web.on("/ncsi.txt", handleNotFound);
  web.on("/connecttest.txt", handleNotFound);
  web.onNotFound(handleNotFound);
  web.begin();
}

void webLoop() { web.handleClient(); }
