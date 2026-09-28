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

void cfgFactory() {
  cfg = AppConfig{};
  String id = chipId();

  cpstr(cfg.devName, (String("ayMC-") + id).c_str(), sizeof(cfg.devName));
  cpstr(cfg.adminUser, "admin", sizeof(cfg.adminUser));
  cpstr(cfg.adminPass, DEF_PASS, sizeof(cfg.adminPass));
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

  cfg.baud          = 9600;
  cfg.dataBits      = 8;
  cfg.parity        = 'N';
  cfg.stopBits      = 1;
  cfg.respTimeoutMs = 1000;
  cfg.charTimeoutMs = 0;
  cfg.turnaroundMs  = 20;
  cfg.bcastDelayMs  = 100;
  cfg.retries       = 0;
  cfg.initDelayMs   = 0;

  cfg.tcpProto       = TCP_MBTCP;
  cfg.tcpPort        = 502;
  cfg.maxClients     = 4;
  cfg.idleTimeoutS   = 300;
  cfg.sendExceptions = true;
  cfg.uidMode        = UID_TRANSPARENT;
  cfg.fixedUid       = 1;
  cfg.uid0Broadcast  = true;
  cfg.mapFrom        = 1;
  cfg.mapTo          = 247;
  cfg.mapOffset      = 0;
  cfg.tcpAliveS      = 60;
  cfg.allowEn        = false;
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

void cfgToJson(JsonDocument& doc, bool secrets) {
  JsonObject sys = doc["sys"].to<JsonObject>();
  sys["name"]     = cfg.devName;
  sys["user"]     = cfg.adminUser;
  sys["pass"]     = secrets ? cfg.adminPass : "";
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

  JsonObject s = doc["ser"].to<JsonObject>();
  s["baud"]    = cfg.baud;
  s["data"]    = cfg.dataBits;
  s["parity"]  = String(cfg.parity);
  s["stop"]    = cfg.stopBits;
  s["timeout"] = cfg.respTimeoutMs;
  s["charTo"]  = cfg.charTimeoutMs;
  s["turn"]    = cfg.turnaroundMs;
  s["bcast"]   = cfg.bcastDelayMs;
  s["retries"] = cfg.retries;
  s["initDelay"] = cfg.initDelayMs;

  JsonObject m = doc["mb"].to<JsonObject>();
  m["proto"]   = cfg.tcpProto;
  m["port"]    = cfg.tcpPort;
  m["maxCli"]  = cfg.maxClients;
  m["idle"]    = cfg.idleTimeoutS;
  m["exc"]     = cfg.sendExceptions;
  m["uidMode"] = cfg.uidMode;
  m["fixUid"]  = cfg.fixedUid;
  m["uid0Bc"]  = cfg.uid0Broadcast;
  m["mapFrom"] = cfg.mapFrom;
  m["mapTo"]   = cfg.mapTo;
  m["mapOff"]  = cfg.mapOffset;
  m["alive"]   = cfg.tcpAliveS;
  m["allowEn"] = cfg.allowEn;
  JsonArray al = m["allow"].to<JsonArray>();
  for (auto& a : cfg.allow) {
    JsonObject o = al.add<JsonObject>();
    o["ip"] = ipStr(a.ip);
    o["mask"] = ipStr(a.mask);
  }
}

void cfgFromJson(JsonVariantConst root) {
  JsonVariantConst sys = root["sys"];
  if (!sys.isNull()) {
    rdStr(sys, "name", cfg.devName, sizeof(cfg.devName));
    rdStr(sys, "user", cfg.adminUser, sizeof(cfg.adminUser));
    rdSecret(sys, "pass", cfg.adminPass, sizeof(cfg.adminPass));
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

  JsonVariantConst s = root["ser"];
  if (!s.isNull()) {
    if (s["baud"].is<long>()) cfg.baud = (uint32_t)constrain(s["baud"].as<long>(), 300L, 921600L);
    rdU8(s, "data", cfg.dataBits, 7, 8);
    if (s["parity"].is<const char*>()) {
      char p = toupper(s["parity"].as<const char*>()[0]);
      if (p == 'N' || p == 'E' || p == 'O') cfg.parity = p;
    }
    rdU8(s, "stop", cfg.stopBits, 1, 2);
    rdU16(s, "timeout", cfg.respTimeoutMs, 20, 10000);
    rdU16(s, "charTo", cfg.charTimeoutMs, 0, 500);
    rdU16(s, "turn", cfg.turnaroundMs, 0, 5000);
    rdU16(s, "bcast", cfg.bcastDelayMs, 0, 5000);
    rdU8(s, "retries", cfg.retries, 0, 5);
    rdU16(s, "initDelay", cfg.initDelayMs, 0, 60000);
  }

  JsonVariantConst m = root["mb"];
  if (!m.isNull()) {
    rdU8(m, "proto", cfg.tcpProto, 0, 1);
    rdU16(m, "port", cfg.tcpPort, 1, 65535);
    rdU8(m, "maxCli", cfg.maxClients, 1, MAX_TCP_CLIENTS);
    rdU16(m, "idle", cfg.idleTimeoutS, 0, 65535);
    rd(m, "exc", cfg.sendExceptions);
    rdU8(m, "uidMode", cfg.uidMode, 0, 2);
    rdU8(m, "fixUid", cfg.fixedUid, 1, 247);
    rd(m, "uid0Bc", cfg.uid0Broadcast);
    rdU8(m, "mapFrom", cfg.mapFrom, 1, 247);
    rdU8(m, "mapTo", cfg.mapTo, 1, 247);
    if (m["mapOff"].is<int>()) cfg.mapOffset = (int16_t)constrain(m["mapOff"].as<int>(), -246, 246);
    rdU16(m, "alive", cfg.tcpAliveS, 0, 7200);
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
}

// ---------------------------------------------------------------------------
void cfgLoad() {
  cfgFactory();
  Preferences p;
  if (!p.begin(NVS_NS, true)) return;
  size_t n = p.getBytesLength(NVS_KEY);
  if (n > 0 && n < 8192) {
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
