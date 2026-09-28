#include "config.h"
#include <Preferences.h>
#include <string.h>
#include <memory>

AppConfig cfg;

static const char* NVS_NS  = "aymc";
static const char* NVS_KEY = "cfg";
static const char* DEF_PASS = "admin";

static void cpstr(char* d, const char* s, size_t n) {
  strncpy(d, s ? s : "", n - 1);
  d[n - 1] = 0;
}

String chipId() {
  uint64_t m = ESP.getEfuseMac();          // bytes en orden inverso: byte0 = MAC[0]
  char b[7];
  snprintf(b, sizeof(b), "%02X%02X%02X",
           (uint8_t)(m >> 24), (uint8_t)(m >> 32), (uint8_t)(m >> 40));
  return String(b);
}

String ipStr(uint32_t v) {
  if (!v) return "";
  return IPAddress(v).toString();
}

uint32_t strIp(const char* s) {
  IPAddress ip;
  return (s && *s && ip.fromString(s)) ? (uint32_t)ip : 0;
}

uint32_t serialConfigOf(uint8_t dataBits, char parity, uint8_t stopBits) {
  static const uint32_t T[2][3][2] = {
    { { SERIAL_7N1, SERIAL_7N2 }, { SERIAL_7E1, SERIAL_7E2 }, { SERIAL_7O1, SERIAL_7O2 } },
    { { SERIAL_8N1, SERIAL_8N2 }, { SERIAL_8E1, SERIAL_8E2 }, { SERIAL_8O1, SERIAL_8O2 } },
  };
  int b = dataBits == 7 ? 0 : 1;
  int p = parity == 'E' ? 1 : (parity == 'O' ? 2 : 0);
  int s = stopBits == 2 ? 1 : 0;
  return T[b][p][s];
}

String mqttBaseTopic() {
  if (cfg.mqttBase[0]) return String(cfg.mqttBase);
  return String("aysafi/") + cfg.hostname;
}

static void portFactory(PortCfg& p, int i) {
  p = PortCfg{};
  p.en            = (i == 0);
  cpstr(p.name, i == 0 ? "RS-485" : "RS-232", sizeof(p.name));
  p.baud          = 9600;
  p.dataBits      = 8;
  p.parity        = 'N';
  p.stopBits      = 1;
  p.respTimeoutMs = 1000;
  p.charTimeoutMs = 0;
  p.turnaroundMs  = 20;
  p.bcastDelayMs  = 100;
  p.retries       = 0;
  p.uidFrom       = (i == 0) ? 1 : 101;
  p.uidTo         = (i == 0) ? 247 : 147;
  p.uidOffset     = (i == 0) ? 0 : -100;
  p.offlineAfter  = 3;
  p.offlineRetryS = 10;
  p.cacheEn       = false;
  p.cacheMaxAgeMs = 2000;
}

void cfgFactory() {
  cfg = AppConfig{};
  String id = chipId();

  cpstr(cfg.devName, (String("ayMC-") + id).c_str(), sizeof(cfg.devName));
  cpstr(cfg.adminUser, "admin", sizeof(cfg.adminUser));
  cpstr(cfg.adminPass, DEF_PASS, sizeof(cfg.adminPass));
  cfg.apiKey[0]    = 0;
  cfg.wizardDone   = false;
  cfg.oledEn       = true;
  cfg.oledFlip     = false;
  cfg.oledSleepMin = 10;
  cfg.oledContrast = 180;
  cfg.otaAuto      = false;
  cpstr(cfg.ntpServer, "pool.ntp.org", sizeof(cfg.ntpServer));
  cpstr(cfg.tz, "<-04>4<-03>,M9.1.6/24,M4.1.6/24", sizeof(cfg.tz));   // Chile continental

  id.toLowerCase();
  cpstr(cfg.hostname, (String("aymc-") + id).c_str(), sizeof(cfg.hostname));
  cfg.mdns = true;
  cfg.eth  = IpCfg{ true, 0, 0, 0, 0, 0 };
  cfg.apMode = AP_ALWAYS;
  id.toUpperCase();
  cpstr(cfg.apSsid, (String("ayModbus-") + id).c_str(), sizeof(cfg.apSsid));
  cpstr(cfg.apPass, "aysafi1234", sizeof(cfg.apPass));
  cfg.apChannel = 6;

  for (int i = 0; i < NUM_PORTS; i++) portFactory(cfg.port[i], i);
  cfg.initDelayMs = 0;

  cfg.tcpProto       = TCP_MBTCP;
  cfg.tcpPort        = 502;
  cfg.maxClients     = 4;
  cfg.idleTimeoutS   = 300;
  cfg.sendExceptions = true;
  cfg.uidMode        = UID_TRANSPARENT;
  cfg.fixedUid       = 1;
  cfg.uid0Broadcast  = true;
  cfg.tcpAliveS      = 60;
  cfg.allowEn        = false;

  cfg.mqttEn = false;
  cpstr(cfg.mqttHost, "emqx.aysafi.com", sizeof(cfg.mqttHost));
  cfg.mqttPort    = 8883;
  cfg.mqttTls     = true;
  cfg.mqttStatusS = 30;
  cfg.mqttRetain  = true;
  cfg.mqttCmd     = false;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------
static void ipToJson(JsonObject o, const IpCfg& c) {
  o["dhcp"] = c.dhcp;
  o["ip"]   = ipStr(c.ip);
  o["mask"] = ipStr(c.mask);
  o["gw"]   = ipStr(c.gw);
  o["dns1"] = ipStr(c.dns1);
  o["dns2"] = ipStr(c.dns2);
}

template <typename T>
static void rd(JsonVariantConst o, const char* k, T& dst) {
  if (o[k].is<T>()) dst = o[k].as<T>();
}
static void rdU8(JsonVariantConst o, const char* k, uint8_t& dst, int lo, int hi) {
  if (o[k].is<int>()) dst = (uint8_t)constrain(o[k].as<int>(), lo, hi);
}
static void rdU16(JsonVariantConst o, const char* k, uint16_t& dst, long lo, long hi) {
  if (o[k].is<long>()) dst = (uint16_t)constrain(o[k].as<long>(), lo, hi);
}
static void rdStr(JsonVariantConst o, const char* k, char* dst, size_t n) {
  if (o[k].is<const char*>()) cpstr(dst, o[k].as<const char*>(), n);
}
// clave: solo si viene no vacia
static void rdSecret(JsonVariantConst o, const char* k, char* dst, size_t n) {
  if (o[k].is<const char*>() && strlen(o[k].as<const char*>()) > 0) cpstr(dst, o[k].as<const char*>(), n);
}
static void rdIpv(JsonVariantConst o, const char* k, uint32_t& dst) {
  if (o[k].is<const char*>()) dst = strIp(o[k].as<const char*>());
}
static void ipFromJson(JsonVariantConst o, IpCfg& c) {
  if (o.isNull()) return;
  rd(o, "dhcp", c.dhcp);
  rdIpv(o, "ip", c.ip);   rdIpv(o, "mask", c.mask); rdIpv(o, "gw", c.gw);
  rdIpv(o, "dns1", c.dns1); rdIpv(o, "dns2", c.dns2);
}

static void portToJson(JsonObject s, const PortCfg& p) {
  s["en"]       = p.en;
  s["name"]     = p.name;
  s["baud"]     = p.baud;
  s["data"]     = p.dataBits;
  s["parity"]   = String(p.parity);
  s["stop"]     = p.stopBits;
  s["timeout"]  = p.respTimeoutMs;
  s["charTo"]   = p.charTimeoutMs;
  s["turn"]     = p.turnaroundMs;
  s["bcast"]    = p.bcastDelayMs;
  s["retries"]  = p.retries;
  s["uidFrom"]  = p.uidFrom;
  s["uidTo"]    = p.uidTo;
  s["uidOff"]   = p.uidOffset;
  s["offAfter"] = p.offlineAfter;
  s["offRetry"] = p.offlineRetryS;
  s["cache"]    = p.cacheEn;
  s["maxAge"]   = p.cacheMaxAgeMs;
}

static void portFromJson(JsonVariantConst s, PortCfg& p) {
  if (s.isNull()) return;
  rd(s, "en", p.en);
  rdStr(s, "name", p.name, sizeof(p.name));
  if (s["baud"].is<long>()) p.baud = (uint32_t)constrain(s["baud"].as<long>(), 300L, 921600L);
  rdU8(s, "data", p.dataBits, 7, 8);
  if (s["parity"].is<const char*>()) {
    char c = toupper(s["parity"].as<const char*>()[0]);
    if (c == 'N' || c == 'E' || c == 'O') p.parity = c;
  }
  rdU8(s, "stop", p.stopBits, 1, 2);
  rdU16(s, "timeout", p.respTimeoutMs, 20, 10000);
  rdU16(s, "charTo", p.charTimeoutMs, 0, 500);
  rdU16(s, "turn", p.turnaroundMs, 0, 5000);
  rdU16(s, "bcast", p.bcastDelayMs, 0, 5000);
  rdU8(s, "retries", p.retries, 0, 5);
  rdU8(s, "uidFrom", p.uidFrom, 1, 247);
  rdU8(s, "uidTo", p.uidTo, 1, 247);
  if (s["uidOff"].is<int>()) p.uidOffset = (int16_t)constrain(s["uidOff"].as<int>(), -246, 246);
  rdU8(s, "offAfter", p.offlineAfter, 0, 50);
  rdU16(s, "offRetry", p.offlineRetryS, 1, 3600);
  rd(s, "cache", p.cacheEn);
  rdU16(s, "maxAge", p.cacheMaxAgeMs, 50, 60000);
}

void cfgToJson(JsonDocument& doc, bool secrets) {
  JsonObject sys = doc["sys"].to<JsonObject>();
  sys["name"]     = cfg.devName;
  sys["user"]     = cfg.adminUser;
  sys["pass"]     = secrets ? cfg.adminPass : "";
  sys["apiKey"]   = secrets ? cfg.apiKey : "";
  sys["apiKeySet"]= cfg.apiKey[0] != 0;
  sys["wizard"]   = cfg.wizardDone;
  sys["oledEn"]   = cfg.oledEn;
  sys["oledFlip"] = cfg.oledFlip;
  sys["oledSleep"]= cfg.oledSleepMin;
  sys["oledContrast"] = cfg.oledContrast;
  sys["otaAuto"]  = cfg.otaAuto;
  sys["ntp"]      = cfg.ntpServer;
  sys["tz"]       = cfg.tz;

  JsonObject net = doc["net"].to<JsonObject>();
  net["host"] = cfg.hostname;
  net["mdns"] = cfg.mdns;
  ipToJson(net["eth"].to<JsonObject>(), cfg.eth);
  JsonObject ap = net["ap"].to<JsonObject>();
  ap["mode"] = cfg.apMode;
  ap["ssid"] = cfg.apSsid;
  ap["pass"] = secrets ? cfg.apPass : "";
  ap["ch"]   = cfg.apChannel;

  JsonArray pa = doc["ports"].to<JsonArray>();
  for (int i = 0; i < NUM_PORTS; i++) portToJson(pa.add<JsonObject>(), cfg.port[i]);

  JsonObject m = doc["mb"].to<JsonObject>();
  m["proto"]   = cfg.tcpProto;
  m["port"]    = cfg.tcpPort;
  m["maxCli"]  = cfg.maxClients;
  m["idle"]    = cfg.idleTimeoutS;
  m["exc"]     = cfg.sendExceptions;
  m["uidMode"] = cfg.uidMode;
  m["fixUid"]  = cfg.fixedUid;
  m["uid0Bc"]  = cfg.uid0Broadcast;
  m["alive"]   = cfg.tcpAliveS;
  m["initDelay"] = cfg.initDelayMs;
  m["allowEn"] = cfg.allowEn;
  JsonArray al = m["allow"].to<JsonArray>();
  for (auto& a : cfg.allow) {
    JsonObject o = al.add<JsonObject>();
    o["ip"] = ipStr(a.ip);
    o["mask"] = ipStr(a.mask);
  }

  JsonArray po = doc["polls"].to<JsonArray>();
  for (auto& p : cfg.poll) {
    if (!p.uid) continue;
    JsonObject o = po.add<JsonObject>();
    o["en"] = p.en; o["port"] = p.port; o["uid"] = p.uid; o["fc"] = p.fc;
    o["addr"] = p.addr; o["qty"] = p.qty; o["period"] = p.periodMs;
    o["mqtt"] = p.mqtt; o["name"] = p.name;
  }
  JsonArray ro = doc["ro"].to<JsonArray>();
  for (auto& r : cfg.ro) {
    if (!r.uidFrom) continue;
    JsonObject o = ro.add<JsonObject>();
    o["en"] = r.en; o["port"] = r.port; o["uidFrom"] = r.uidFrom; o["uidTo"] = r.uidTo;
    o["addrFrom"] = r.addrFrom; o["addrTo"] = r.addrTo;
  }

  JsonObject q = doc["mqtt"].to<JsonObject>();
  q["en"]      = cfg.mqttEn;
  q["host"]    = cfg.mqttHost;
  q["port"]    = cfg.mqttPort;
  q["tls"]     = cfg.mqttTls;
  q["user"]    = cfg.mqttUser;
  q["pass"]    = secrets ? cfg.mqttPass : "";
  q["base"]    = cfg.mqttBase;
  q["baseEff"] = mqttBaseTopic();
  q["statusS"] = cfg.mqttStatusS;
  q["retain"]  = cfg.mqttRetain;
  q["cmd"]     = cfg.mqttCmd;
}

void cfgFromJson(JsonVariantConst root) {
  JsonVariantConst sys = root["sys"];
  if (!sys.isNull()) {
    rdStr(sys, "name", cfg.devName, sizeof(cfg.devName));
    rdStr(sys, "user", cfg.adminUser, sizeof(cfg.adminUser));
    rdSecret(sys, "pass", cfg.adminPass, sizeof(cfg.adminPass));
    rdSecret(sys, "apiKey", cfg.apiKey, sizeof(cfg.apiKey));
    if (sys["apiKeyClear"] | false) cfg.apiKey[0] = 0;
    rd(sys, "wizard", cfg.wizardDone);
    rd(sys, "oledEn", cfg.oledEn);
    rd(sys, "oledFlip", cfg.oledFlip);
    rdU8(sys, "oledSleep", cfg.oledSleepMin, 0, 240);
    rdU8(sys, "oledContrast", cfg.oledContrast, 0, 255);
    rd(sys, "otaAuto", cfg.otaAuto);
    rdStr(sys, "ntp", cfg.ntpServer, sizeof(cfg.ntpServer));
    rdStr(sys, "tz", cfg.tz, sizeof(cfg.tz));
    if (!cfg.adminUser[0]) cpstr(cfg.adminUser, "admin", sizeof(cfg.adminUser));
  }

  JsonVariantConst net = root["net"];
  if (!net.isNull()) {
    rdStr(net, "host", cfg.hostname, sizeof(cfg.hostname));
    rd(net, "mdns", cfg.mdns);
    ipFromJson(net["eth"], cfg.eth);
    JsonVariantConst ap = net["ap"];
    if (!ap.isNull()) {
      rdU8(ap, "mode", cfg.apMode, 0, 2);
      rdStr(ap, "ssid", cfg.apSsid, sizeof(cfg.apSsid));
      rdSecret(ap, "pass", cfg.apPass, sizeof(cfg.apPass));
      rdU8(ap, "ch", cfg.apChannel, 1, 13);
    }
  }

  JsonArrayConst pa = root["ports"];
  if (!pa.isNull())
    for (int i = 0; i < NUM_PORTS && i < (int)pa.size(); i++) portFromJson(pa[i], cfg.port[i]);

  JsonVariantConst m = root["mb"];
  if (!m.isNull()) {
    rdU8(m, "proto", cfg.tcpProto, 0, 1);
    rdU16(m, "port", cfg.tcpPort, 1, 65535);
    rdU8(m, "maxCli", cfg.maxClients, 1, MAX_TCP_CLIENTS);
    rdU16(m, "idle", cfg.idleTimeoutS, 0, 65535);
    rd(m, "exc", cfg.sendExceptions);
    rdU8(m, "uidMode", cfg.uidMode, 0, 1);
    rdU8(m, "fixUid", cfg.fixedUid, 1, 247);
    rd(m, "uid0Bc", cfg.uid0Broadcast);
    rdU16(m, "alive", cfg.tcpAliveS, 0, 7200);
    rdU16(m, "initDelay", cfg.initDelayMs, 0, 60000);
    rd(m, "allowEn", cfg.allowEn);
    JsonArrayConst al = m["allow"];
    if (!al.isNull()) {
      for (int i = 0; i < MAX_ALLOW; i++) {
        cfg.allow[i] = AllowRule{};
        if (i < (int)al.size()) {
          rdIpv(al[i], "ip", cfg.allow[i].ip);
          rdIpv(al[i], "mask", cfg.allow[i].mask);
        }
      }
    }
  }

  // tablas: se reemplazan completas cuando vienen
  JsonArrayConst po = root["polls"];
  if (!po.isNull()) {
    for (int i = 0; i < MAX_POLLS; i++) {
      PollEntry& p = cfg.poll[i];
      p = PollEntry{};
      if (i >= (int)po.size()) continue;
      JsonVariantConst o = po[i];
      rd(o, "en", p.en);
      rdU8(o, "port", p.port, 0, NUM_PORTS - 1);
      rdU8(o, "uid", p.uid, 1, 247);
      rdU8(o, "fc", p.fc, 1, 4);
      rdU16(o, "addr", p.addr, 0, 65535);
      rdU16(o, "qty", p.qty, 1, (p.fc <= 2) ? 2000 : 125);
      rdU16(o, "period", p.periodMs, 100, 60000);
      rd(o, "mqtt", p.mqtt);
      rdStr(o, "name", p.name, sizeof(p.name));
      if (!p.fc) p.fc = 3;
      if (!p.qty) p.qty = 1;
      if (!p.periodMs) p.periodMs = 1000;
    }
  }
  JsonArrayConst ro = root["ro"];
  if (!ro.isNull()) {
    for (int i = 0; i < MAX_RO; i++) {
      RoRule& r = cfg.ro[i];
      r = RoRule{};
      if (i >= (int)ro.size()) continue;
      JsonVariantConst o = ro[i];
      rd(o, "en", r.en);
      rdU8(o, "port", r.port, 0, NUM_PORTS - 1);
      rdU8(o, "uidFrom", r.uidFrom, 1, 247);
      rdU8(o, "uidTo", r.uidTo, 1, 247);
      r.addrTo = 65535;
      rdU16(o, "addrFrom", r.addrFrom, 0, 65535);
      rdU16(o, "addrTo", r.addrTo, 0, 65535);
      if (!r.uidTo) r.uidTo = r.uidFrom;
    }
  }

  JsonVariantConst q = root["mqtt"];
  if (!q.isNull()) {
    rd(q, "en", cfg.mqttEn);
    rdStr(q, "host", cfg.mqttHost, sizeof(cfg.mqttHost));
    rdU16(q, "port", cfg.mqttPort, 1, 65535);
    rd(q, "tls", cfg.mqttTls);
    rdStr(q, "user", cfg.mqttUser, sizeof(cfg.mqttUser));
    rdSecret(q, "pass", cfg.mqttPass, sizeof(cfg.mqttPass));
    rdStr(q, "base", cfg.mqttBase, sizeof(cfg.mqttBase));
    rdU16(q, "statusS", cfg.mqttStatusS, 5, 3600);
    rd(q, "retain", cfg.mqttRetain);
    rd(q, "cmd", cfg.mqttCmd);
  }
}

// ---------------------------------------------------------------------------
void cfgLoad() {
  cfgFactory();
  Preferences p;
  if (!p.begin(NVS_NS, true)) return;
  size_t n = p.getBytesLength(NVS_KEY);
  if (n > 0 && n < 32768) {
    std::unique_ptr<char[]> buf(new char[n + 1]);
    p.getBytes(NVS_KEY, buf.get(), n);
    buf[n] = 0;
    JsonDocument doc;
    if (deserializeJson(doc, buf.get()) == DeserializationError::Ok) cfgFromJson(doc.as<JsonVariantConst>());
  }
  p.end();
}

bool cfgSave() {
  JsonDocument doc;
  cfgToJson(doc, true);
  doc["mqtt"].remove("baseEff");
  doc["sys"].remove("apiKeySet");
  String s;
  serializeJson(doc, s);
  Preferences p;
  if (!p.begin(NVS_NS, false)) return false;
  size_t n = p.putBytes(NVS_KEY, s.c_str(), s.length());
  p.end();
  return n == s.length();
}

void cfgErase() {
  Preferences p;
  if (p.begin(NVS_NS, false)) { p.clear(); p.end(); }
}

bool cfgDefaultPassword() { return strcmp(cfg.adminPass, DEF_PASS) == 0; }
