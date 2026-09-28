#include "mqtt_bridge.h"
#include "config.h"
#include "net.h"
#include "gateway.h"
#include "mb_util.h"
#include "fw_version.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

static WiFiClient       s_plain;
static WiFiClientSecure s_tls;
static PubSubClient     s_mq;
static String           s_base;
static uint32_t         s_lastTry = 0, s_lastStatus = 0;
static uint8_t          s_lastStates = 0xFF;

bool mqttConnected() { return cfg.mqttEn && s_mq.connected(); }

String mqttStateStr() {
  if (!cfg.mqttEn) return "desactivado";
  if (s_mq.connected()) return "conectado";
  if (!netEthUp()) return "sin LAN";
  switch (s_mq.state()) {
    case -4: return "sin respuesta del broker";
    case -2: return "no se pudo conectar";
    case 4:  return "usuario/clave rechazados";
    case 5:  return "no autorizado";
    default: return "desconectado (" + String(s_mq.state()) + ")";
  }
}

// estado combinado para detectar cambios: LAN + estado de cada puerto (2 bits c/u)
static uint8_t statesKey() {
  uint8_t k = netEthLink() ? 1 : 0;
  for (uint8_t p = 0; p < NUM_PORTS; p++) k |= (gwPortState(p) & 7) << (1 + 3 * p);
  return k;
}

static void publishStatus() {
  JsonDocument d;
  d["online"] = true;
  d["name"]   = cfg.devName;
  d["fw"]     = FW_SEMVER;
  d["ip"]     = netEthIp();
  d["uptime"] = millis() / 1000;
  d["lan"]    = netEthLink();
  d["state"]  = netEthLink() ? txStateStr(gwGlobalState()) : "LAN DESCONECTADA";
  JsonArray pa = d["ports"].to<JsonArray>();
  for (uint8_t p = 0; p < NUM_PORTS; p++) {
    if (!cfg.port[p].en) continue;
    JsonObject o = pa.add<JsonObject>();
    o["port"] = p + 1; o["name"] = cfg.port[p].name; o["state"] = txStateStr(gwPortState(p));
  }
  JsonArray sv = d["slaves"].to<JsonArray>();
  SlaveBrief sb[32];
  int n = gwSlaveList(sb, 32);
  for (int i = 0; i < n; i++) {
    JsonObject o = sv.add<JsonObject>();
    o["port"] = sb[i].port + 1; o["uid"] = sb[i].uid; o["state"] = txStateStr(sb[i].st);
  }
  JsonObject st = d["stats"].to<JsonObject>();
  st["req"] = gwStats.tcpReq; st["ok"] = gwStats.rtuOk; st["timeout"] = gwStats.rtuTimeout;
  st["crc"] = gwStats.rtuCrc; st["cache"] = gwStats.cacheHits; st["clients"] = gwClientCount();
  String s;
  serializeJson(d, s);
  s_mq.publish((s_base + "/status").c_str(), s.c_str(), true);
  s_lastStatus = millis();
}

static void publishTags() {
  uint32_t m = gwPollUpdatedMask(true);
  for (int i = 0; m && i < MAX_POLLS; i++) {
    if (!(m & (1UL << i))) continue;
    m &= ~(1UL << i);
    const PollEntry& e = cfg.poll[i];
    if (!e.mqtt || !e.uid) continue;
    JsonDocument d;
    gwTagValues(i, d.to<JsonObject>());
    d["ts"] = netTimeOk() ? (uint32_t)time(nullptr) : 0;
    String name = e.name[0] ? String(e.name)
                            : String("p") + (e.port + 1) + "u" + e.uid + "f" + e.fc + "a" + e.addr;
    String s;
    serializeJson(d, s);
    s_mq.publish((s_base + "/data/" + name).c_str(), s.c_str(), cfg.mqttRetain);
  }
}

static void onMessage(char* topic, byte* payload, unsigned int len) {
  if (!cfg.mqttCmd) return;
  JsonDocument q, d;
  if (deserializeJson(q, payload, len) != DeserializationError::Ok) {
    d["ok"] = false; d["error"] = "JSON invalido";
  } else {
    if (!q["id"].isNull()) d["id"] = q["id"];
    uint8_t pdu[RTU_MAX_ADU];
    const char* err = nullptr;
    size_t pl = mbBuildPdu(q.as<JsonVariantConst>(), pdu, &err);
    if (!pl) {
      d["ok"] = false; d["error"] = err;
    } else {
      int port = q["port"].is<int>() ? (int)q["port"] - 1 : -1;
      uint8_t fc = pdu[0];
      GwResult r;
      bool done = gwExecute(port, q["uid"] | 1, pdu, pl, r, 8000, fc <= 4);
      mbResultJson(r, done, fc, q["qty"] | 1, d.to<JsonObject>());
      if (!q["id"].isNull()) d["id"] = q["id"];
    }
  }
  String s;
  serializeJson(d, s);
  s_mq.publish((s_base + "/cmd/resp").c_str(), s.c_str(), false);
}

void mqttBegin() {
  if (!cfg.mqttEn) return;
  s_base = mqttBaseTopic();
  if (cfg.mqttTls) {
    s_tls.setInsecure();
    s_tls.setHandshakeTimeout(10);   // corre en el loop de la web: no bloquear 120 s (defecto)
    s_mq.setClient(s_tls);
  }
  else s_mq.setClient(s_plain);
  s_mq.setServer(cfg.mqttHost, cfg.mqttPort);
  s_mq.setBufferSize(2048);
  s_mq.setKeepAlive(30);
  s_mq.setSocketTimeout(5);
  s_mq.setCallback(onMessage);
}

void mqttLoop() {
  if (!cfg.mqttEn) return;
  uint32_t now = millis();
  if (!s_mq.connected()) {
    if (!netEthUp() || now - s_lastTry < 10000) return;
    s_lastTry = now;
    String will = "{\"online\":false}";
    bool ok = s_mq.connect(cfg.hostname, cfg.mqttUser[0] ? cfg.mqttUser : nullptr,
                           cfg.mqttUser[0] ? cfg.mqttPass : nullptr,
                           (s_base + "/status").c_str(), 1, true, will.c_str());
    Serial.printf("[mqtt] %s:%u %s\n", cfg.mqttHost, cfg.mqttPort, ok ? "conectado" : mqttStateStr().c_str());
    if (!ok) return;
    if (cfg.mqttCmd) s_mq.subscribe((s_base + "/cmd").c_str());
    publishStatus();
    s_lastStates = statesKey();
  }
  s_mq.loop();
  uint8_t k = statesKey();
  if (k != s_lastStates || now - s_lastStatus > cfg.mqttStatusS * 1000UL) {
    s_lastStates = k;
    publishStatus();
  }
  publishTags();
}
