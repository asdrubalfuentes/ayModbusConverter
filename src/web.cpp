#include "web.h"
#include "web_ui.h"
#include "config.h"
#include "net.h"
#include "gateway.h"
#include "modbus_rtu.h"
#include "display.h"
#include "mb_util.h"
#include "mqtt_bridge.h"
#include <vector>
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

// API para maquinas (/api/read, /api/write, /api/tags): clave X-API-Key o ?key=, o HTTP Basic
static bool authApi() {
  if (cfg.apiKey[0]) {
    String k = web.hasHeader("X-API-Key") ? web.header("X-API-Key") : web.arg("key");
    if (k.length() && k == cfg.apiKey) return true;
  }
  return auth();
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
  d["wizard"] = cfg.wizardDone;
  d["state"]  = netEthLink() ? txStateStr(gwGlobalState()) : "LAN DESCONECTADA";
  d["mqtt"]   = mqttStateStr();
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

// Herramienta de prueba: lectura/escritura directa a un esclavo (puerto explicito, sin cache)
static void handleTest() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  uint8_t pdu[RTU_MAX_ADU];
  const char* err = nullptr;
  size_t pl = mbBuildPdu(q.as<JsonVariantConst>(), pdu, &err);
  if (!pl) { sendErr(400, err); return; }
  int port = (q["port"] | 1) - 1;
  GwResult r;
  bool done = gwExecute(port, q["uid"] | 1, pdu, pl, r, 15000, false);
  JsonDocument d;
  mbResultJson(r, done, pdu[0], q["qty"] | 1, d.to<JsonObject>());
  sendJson(200, d);
}

// API REST: GET /api/read?uid=1&fc=3&addr=0&qty=10[&port=1][&cache=0]
static void handleRead() {
  if (!authApi()) return;
  JsonDocument q;
  q["uid"] = web.hasArg("uid") ? web.arg("uid").toInt() : 1;
  q["fc"] = web.hasArg("fc") ? web.arg("fc").toInt() : 3;
  q["addr"] = web.arg("addr").toInt();
  q["qty"] = web.hasArg("qty") ? web.arg("qty").toInt() : 1;
  uint8_t fc = q["fc"];
  if (fc < 1 || fc > 4) { sendErr(400, "fc debe ser 1..4 (use /api/write para escribir)"); return; }
  uint8_t pdu[8];
  size_t pl = mbBuildPdu(q.as<JsonVariantConst>(), pdu, nullptr);
  int port = web.hasArg("port") ? web.arg("port").toInt() - 1 : -1;
  bool useCache = !web.hasArg("cache") || web.arg("cache") != "0";
  GwResult r;
  bool done = gwExecute(port, q["uid"], pdu, pl, r, 15000, useCache);
  JsonDocument d;
  d["uid"] = q["uid"]; d["fc"] = fc; d["addr"] = q["addr"];
  mbResultJson(r, done, fc, q["qty"], d.to<JsonObject>());
  sendJson(d["ok"] ? 200 : 502, d);
}

// API REST: POST /api/write {uid, fc(5|6|15|16), addr, value | values[, port]}
static void handleWrite() {
  if (!authApi()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  uint8_t fc = q["fc"] | 6;
  if (!(fc == 5 || fc == 6 || fc == 15 || fc == 16)) { sendErr(400, "fc debe ser 5, 6, 15 o 16"); return; }
  uint8_t pdu[RTU_MAX_ADU];
  const char* err = nullptr;
  size_t pl = mbBuildPdu(q.as<JsonVariantConst>(), pdu, &err);
  if (!pl) { sendErr(400, err); return; }
  int port = q["port"].is<int>() ? (int)q["port"] - 1 : -1;
  GwResult r;
  bool done = gwExecute(port, q["uid"] | 1, pdu, pl, r, 15000, false);
  JsonDocument d;
  mbResultJson(r, done, fc, 0, d.to<JsonObject>());
  sendJson(d["ok"] ? 200 : 502, d);
}

static void handleTags() {
  if (!authApi()) return;
  JsonDocument d;
  gwTagsJson(d["tags"].to<JsonArray>());
  sendJson(200, d);
}

static void handleLearnGet() {
  if (!auth()) return;
  JsonDocument d;
  d["en"] = gwLearnEnabled();
  gwLearnJson(d["items"].to<JsonArray>());
  sendJson(200, d);
}

static void handleLearnPost() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  if (q["en"].is<bool>()) gwLearnEnable(q["en"]);
  if (q["clear"] | false) gwLearnClear();
  sendOk();
}

// Herramientas: {kind:"scan",port,from,to,timeout} {kind:"detect",port,passive,ids}
// {kind:"discover",port,uid,step,max} {kind:"stop"}
static void handleToolPost() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  String k = q["kind"] | "";
  uint8_t port = (q["port"] | 1) - 1;
  bool ok;
  if (k == "stop")          { gwToolStop(); ok = true; }
  else if (k == "scan")     ok = gwScanStart(port, q["from"] | 1, q["to"] | 247, q["timeout"] | 100);
  else if (k == "detect")   ok = gwDetectStart(port, q["passive"] | false, q["ids"] | "1-10");
  else if (k == "discover") ok = gwDiscoverStart(port, q["uid"] | 1, q["step"] | 100, q["max"] | 10000);
  else if (k == "apply")    ok = gwDetectApply();
  else { sendErr(400, "herramienta desconocida"); return; }
  if (!ok) { sendErr(409, "no se pudo iniciar (otra herramienta en curso o puerto apagado)"); return; }
  sendOk();
}

static void handleToolGet() {
  if (!auth()) return;
  JsonDocument d;
  gwToolJson(d.to<JsonObject>());
  sendJson(200, d);
}

// Monitor como captura para Wireshark
struct VecPrint : public Print {
  std::vector<uint8_t> v;
  size_t write(uint8_t c) override { v.push_back(c); return 1; }
  size_t write(const uint8_t* b, size_t n) override { v.insert(v.end(), b, b + n); return n; }
};
static void handlePcap() {
  if (!auth()) return;
  VecPrint vp;
  vp.v.reserve(8192);
  gwPcap(vp);
  web.sendHeader("Content-Disposition", String("attachment; filename=\"") + cfg.hostname + "-modbus.pcapng\"");
  web.setContentLength(vp.v.size());
  web.send(200, "application/octet-stream", "");
  web.sendContent((const char*)vp.v.data(), vp.v.size());
}

// Aplica la configuracion de UN puerto sin reiniciar: {port: 1|2, cfg: {baud, parity, ...}}
static void handlePortApply() {
  if (!auth()) return;
  JsonDocument q;
  if (!parseBody(q)) return;
  int p = (q["port"] | 1) - 1;
  if (p < 0 || p >= NUM_PORTS) { sendErr(400, "puerto invalido"); return; }
  JsonDocument w;
  JsonArray pa = w["ports"].to<JsonArray>();
  for (int i = 0; i < NUM_PORTS; i++) {
    if (i == p) pa.add(q["cfg"]);
    else pa.add<JsonObject>();
  }
  cfgFromJson(w.as<JsonVariantConst>());
  bool ok = cfgSave();
  gwReopenPort(p);
  if (!ok) { sendErr(500, "no se pudo guardar"); return; }
  sendOk();
}

// Asistente: marcar como hecho sin reiniciar (boton "omitir")
static void handleWizardDone() {
  if (!auth()) return;
  cfg.wizardDone = true;
  cfgSave();
  sendOk();
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
  web.on("/api/tool", HTTP_GET, handleToolGet);
  web.on("/api/tool", HTTP_POST, handleToolPost);
  web.on("/api/read", HTTP_GET, handleRead);
  web.on("/api/write", HTTP_POST, handleWrite);
  web.on("/api/tags", HTTP_GET, handleTags);
  web.on("/api/learn", HTTP_GET, handleLearnGet);
  web.on("/api/learn", HTTP_POST, handleLearnPost);
  web.on("/api/pcap", HTTP_GET, handlePcap);
  web.on("/api/wizard/done", HTTP_POST, handleWizardDone);
  web.on("/api/port/apply", HTTP_POST, handlePortApply);
  web.on("/api/ota", HTTP_POST, handleOta);
  web.on("/api/fw", HTTP_POST, handleFwDone, handleFwUpload);
  // deteccion de portal cautivo de Android / iOS / Windows
  web.on("/generate_204", handleNotFound);
  web.on("/gen_204", handleNotFound);
  web.on("/hotspot-detect.html", handleNotFound);
  web.on("/ncsi.txt", handleNotFound);
  web.on("/connecttest.txt", handleNotFound);
  web.onNotFound(handleNotFound);
  static const char* HDRS[] = { "X-API-Key" };
  web.collectHeaders(HDRS, 1);
  web.begin();
}

void webLoop() { web.handleClient(); }
