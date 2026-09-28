#include "gateway.h"
#include "modbus_rtu.h"
#include "config.h"
#include "board.h"
#include "net.h"
#include <WiFi.h>
#include <lwip/sockets.h>

GwStats gwStats;

// ---------------------------------------------------------------------------
// Clientes TCP
// ---------------------------------------------------------------------------
struct Slot {
  WiFiClient c;
  bool       used = false;
  uint32_t   ip = 0;
  uint16_t   port = 0;
  uint32_t   sinceMs = 0, lastActMs = 0, lastByteMs = 0;
  uint32_t   reqs = 0;
  size_t     n = 0;
  uint8_t    buf[6 + RTU_MAX_ADU + 8];
};

static WiFiServer s_server;
static Slot       s_slots[MAX_TCP_CLIENTS];
static volatile bool    s_listening = false;
static volatile uint8_t s_clientCount = 0;

// ---------------------------------------------------------------------------
// Estadistica por esclavo (Unit ID 1..247)
// ---------------------------------------------------------------------------
struct UidStat {
  uint32_t req, ok, exc, tout, err;
  uint16_t lastMs;
  uint32_t lastSeenMs;
};
static UidStat s_uid[248];

// ---------------------------------------------------------------------------
// Monitor de trafico (anillo)
// ---------------------------------------------------------------------------
#define TR_SLOTS 64
#define TR_BYTES 64
struct TraceEnt {
  uint32_t seq, ms;
  uint8_t  kind;
  uint16_t len;
  uint8_t  d[TR_BYTES];
  char     note[20];
};
static TraceEnt     s_tr[TR_SLOTS];
static uint32_t     s_trSeq = 0;
static volatile bool s_monEn = false;
static portMUX_TYPE s_trMux = portMUX_INITIALIZER_UNLOCKED;

void gwTrace(TraceKind k, const uint8_t* d, size_t n, const char* note) {
  if (!s_monEn) return;
  portENTER_CRITICAL(&s_trMux);
  TraceEnt& e = s_tr[s_trSeq % TR_SLOTS];
  e.seq  = ++s_trSeq;
  e.ms   = millis();
  e.kind = k;
  e.len  = (uint16_t)n;
  if (d && n) memcpy(e.d, d, min(n, (size_t)TR_BYTES));
  if (note) { strncpy(e.note, note, sizeof(e.note) - 1); e.note[sizeof(e.note) - 1] = 0; }
  else e.note[0] = 0;
  portEXIT_CRITICAL(&s_trMux);
}

void gwMonitorEnable(bool en) { s_monEn = en; }
bool gwMonitorEnabled() { return s_monEn; }
void gwMonitorClear() {
  portENTER_CRITICAL(&s_trMux);
  for (auto& e : s_tr) e.seq = 0;
  portEXIT_CRITICAL(&s_trMux);
}

uint32_t gwMonitorJson(JsonArray a, uint32_t since) {
  static const char* K[] = { "TCP>", ">RTU", "RTU>", ">TCP", "EVT" };
  // copia rapida bajo el candado, serializa fuera
  static TraceEnt snap[TR_SLOTS];
  uint32_t last;
  portENTER_CRITICAL(&s_trMux);
  memcpy(snap, s_tr, sizeof(snap));
  last = s_trSeq;
  portEXIT_CRITICAL(&s_trMux);

  uint32_t first = (last > TR_SLOTS) ? last - TR_SLOTS + 1 : 1;
  if (since + 1 > first) first = since + 1;
  char hex[TR_BYTES * 3 + 4];
  for (uint32_t s = first; s <= last; s++) {
    const TraceEnt& e = snap[(s - 1) % TR_SLOTS];
    if (e.seq != s) continue;
    JsonObject o = a.add<JsonObject>();
    o["s"] = e.seq;
    o["t"] = e.ms;
    o["k"] = K[e.kind];
    o["n"] = e.len;
    size_t m = min((size_t)e.len, (size_t)TR_BYTES), p = 0;
    for (size_t i = 0; i < m; i++) p += snprintf(hex + p, sizeof(hex) - p, i ? " %02X" : "%02X", e.d[i]);
    if (e.len > TR_BYTES) p += snprintf(hex + p, sizeof(hex) - p, " ..");
    hex[p] = 0;
    o["h"] = hex;
    if (e.note[0]) o["x"] = e.note;
  }
  return last;
}

void gwStatsReset() {
  gwStats = GwStats{};
  memset(s_uid, 0, sizeof(s_uid));
}

// ---------------------------------------------------------------------------
// Escaneo del bus
// ---------------------------------------------------------------------------
static volatile bool    s_scanOn = false;
static volatile uint8_t s_scanCur = 0, s_scanFrom = 1, s_scanTo = 247;
static volatile uint16_t s_scanTo_ms = 100;
static uint32_t s_scanFound[8];

bool gwScanStart(uint8_t from, uint8_t to, uint16_t timeoutMs) {
  if (from < 1) from = 1;
  if (to > 247) to = 247;
  if (from > to) return false;
  memset(s_scanFound, 0, sizeof(s_scanFound));
  s_scanFrom = from; s_scanTo = to; s_scanCur = from;
  s_scanTo_ms = constrain(timeoutMs, 20, 2000);
  s_scanOn = true;
  return true;
}
void gwScanStop() { s_scanOn = false; }

void gwScanJson(JsonObject o) {
  o["active"] = (bool)s_scanOn;
  o["from"] = s_scanFrom;
  o["to"] = s_scanTo;
  o["cur"] = s_scanCur;
  JsonArray f = o["found"].to<JsonArray>();
  for (int u = 1; u <= 247; u++)
    if (s_scanFound[u >> 5] & (1UL << (u & 31))) f.add(u);
}

static void scanStep() {
  uint8_t u = s_scanCur;
  uint8_t req[6] = { u, 0x03, 0x00, 0x00, 0x00, 0x01 };   // FC03, reg 0, 1 registro
  uint8_t resp[RTU_MAX_ADU];
  size_t rl; uint32_t lat;
  if (!rtuLock(2000)) return;
  RtuResult r = rtuTransact(req, sizeof(req), resp, rl, sizeof(resp), lat, s_scanTo_ms, 0);
  rtuUnlock();
  // cualquier respuesta valida (incluida una excepcion) = hay un esclavo
  if (r == RTU_OK || r == RTU_CRC) s_scanFound[u >> 5] |= (1UL << (u & 31));
  if (u >= s_scanTo) s_scanOn = false;
  else s_scanCur = u + 1;
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
uint8_t gwClientCount() { return s_clientCount; }
bool    gwListening()   { return s_listening; }

void gwStatusJson(JsonObject o) {
  JsonObject s = o["stats"].to<JsonObject>();
  s["tcpReq"]  = gwStats.tcpReq;
  s["ok"]      = gwStats.rtuOk;
  s["exc"]     = gwStats.rtuExc;
  s["timeout"] = gwStats.rtuTimeout;
  s["crc"]     = gwStats.rtuCrc;
  s["bad"]     = gwStats.rtuBad;
  s["bcast"]   = gwStats.bcast;
  s["gwExc"]   = gwStats.gwExc;
  s["rejected"]= gwStats.rejected;
  s["conn"]    = gwStats.connTotal;
  s["rx"]      = gwStats.bytesTcpRx;
  s["tx"]      = gwStats.bytesTcpTx;
  s["lat"]     = gwStats.lastLatencyMs;
  s["gapUs"]   = rtuGapUs();
  o["listening"] = (bool)s_listening;

  JsonArray cl = o["clients"].to<JsonArray>();
  uint32_t now = millis();
  for (auto& sl : s_slots) {
    if (!sl.used) continue;
    JsonObject c = cl.add<JsonObject>();
    c["ip"]   = IPAddress(sl.ip).toString();
    c["port"] = sl.port;
    c["up"]   = (now - sl.sinceMs) / 1000;
    c["idle"] = (now - sl.lastActMs) / 1000;
    c["reqs"] = sl.reqs;
  }

  JsonArray sv = o["slaves"].to<JsonArray>();
  for (int u = 1; u <= 247; u++) {
    const UidStat& x = s_uid[u];
    if (!x.req) continue;
    JsonObject e = sv.add<JsonObject>();
    e["uid"]  = u;
    e["req"]  = x.req;
    e["ok"]   = x.ok;
    e["exc"]  = x.exc;
    e["tout"] = x.tout;
    e["err"]  = x.err;
    e["lat"]  = x.lastMs;
    e["ago"]  = x.lastSeenMs ? (now - x.lastSeenMs) / 1000 : -1;
  }
}

// ---------------------------------------------------------------------------
// Nucleo
// ---------------------------------------------------------------------------
static bool allowed(uint32_t ip) {
  if (!cfg.allowEn) return true;
  for (auto& a : cfg.allow) {
    if (!a.ip) continue;
    uint32_t m = a.mask ? a.mask : 0xFFFFFFFFUL;
    if ((ip & m) == (a.ip & m)) return true;
  }
  return false;
}

static void closeSlot(Slot& s) {
  s.c.stop();
  s.used = false;
  s.n = 0;
  uint8_t cnt = 0;
  for (auto& x : s_slots) if (x.used) cnt++;
  s_clientCount = cnt;
}

static void sendReply(Slot& s, uint16_t tid, uint8_t uid, const uint8_t* pdu, size_t pduLen) {
  uint8_t out[6 + RTU_MAX_ADU + 2];
  size_t n = 0;
  if (cfg.tcpProto == TCP_MBTCP) {
    out[0] = tid >> 8; out[1] = tid & 0xFF;
    out[2] = 0; out[3] = 0;
    out[4] = (pduLen + 1) >> 8; out[5] = (pduLen + 1) & 0xFF;
    out[6] = uid;
    memcpy(out + 7, pdu, pduLen);
    n = 7 + pduLen;
  } else {
    out[0] = uid;
    memcpy(out + 1, pdu, pduLen);
    uint16_t c = rtuCrc(out, 1 + pduLen);
    out[1 + pduLen] = c & 0xFF;
    out[2 + pduLen] = c >> 8;
    n = 3 + pduLen;
  }
  s.c.write(out, n);
  gwStats.bytesTcpTx += n;
  gwTrace(TR_TCP_TX, out, n, nullptr);
}

static void sendException(Slot& s, uint16_t tid, uint8_t uid, uint8_t fc, uint8_t code) {
  if (!cfg.sendExceptions) return;
  uint8_t pdu[2] = { (uint8_t)(fc | 0x80), code };
  gwStats.gwExc++;
  sendReply(s, tid, uid, pdu, 2);
}

// Una peticion completa: uid + pdu (sin CRC)
static void handleRequest(Slot& s, uint16_t tid, uint8_t uid, const uint8_t* pdu, size_t pduLen) {
  gwStats.tcpReq++;
  s.reqs++;
  if (pduLen < 1 || pduLen > RTU_MAX_ADU - 3) return;

  uint8_t target = uid;
  if (uid == 0 && cfg.uid0Broadcast)                          target = 0;
  else if (cfg.uidMode == UID_FIXED || uid == 0 || uid == 255) target = cfg.fixedUid;
  else if (cfg.uidMode == UID_MAP) {
    int t = (int)uid + cfg.mapOffset;
    if (uid < cfg.mapFrom || uid > cfg.mapTo || t < 1 || t > 247) {
      gwTrace(TR_EVENT, nullptr, 0, "UID fuera de mapa");
      sendException(s, tid, uid, pdu[0], 0x0A);      // Gateway Path Unavailable
      return;
    }
    target = (uint8_t)t;
  }

  uint8_t req[RTU_MAX_ADU];
  req[0] = target;
  memcpy(req + 1, pdu, pduLen);

  uint8_t resp[RTU_MAX_ADU];
  size_t rl = 0; uint32_t lat = 0;
  if (!rtuLock(10000)) { sendException(s, tid, uid, pdu[0], 0x0A); return; }
  RtuResult r = rtuTransact(req, 1 + pduLen, resp, rl, sizeof(resp), lat);
  rtuUnlock();
  gwStats.lastActMs = millis();

  UidStat* us = (target >= 1 && target <= 247) ? &s_uid[target] : nullptr;
  if (us) us->req++;

  switch (r) {
    case RTU_OK:
      gwStats.rtuOk++;
      gwStats.lastLatencyMs = lat;
      if (resp[1] & 0x80) gwStats.rtuExc++;
      if (us) { us->ok++; if (resp[1] & 0x80) us->exc++; us->lastMs = lat; us->lastSeenMs = millis(); }
      sendReply(s, tid, uid, resp + 1, rl - 1);       // Unit ID del cliente, sin tocar la PDU
      break;
    case RTU_BCAST:
      gwStats.bcast++;
      break;
    case RTU_TIMEOUT:
      gwStats.rtuTimeout++;
      if (us) us->tout++;
      sendException(s, tid, uid, pdu[0], 0x0B);       // Gateway Target Device Failed to Respond
      break;
    default:
      if (r == RTU_CRC) gwStats.rtuCrc++; else gwStats.rtuBad++;
      if (us) us->err++;
      sendException(s, tid, uid, pdu[0], 0x0B);
      break;
  }
}

// Procesa como maximo una peticion del buffer del cliente. true si hizo algo.
static bool serviceSlot(Slot& s) {
  if (!s.c.connected() && !s.c.available()) { closeSlot(s); return false; }

  int a = s.c.available();
  if (a > 0 && s.n < sizeof(s.buf)) {
    int r = s.c.read(s.buf + s.n, min((size_t)a, sizeof(s.buf) - s.n));
    if (r > 0) {
      s.n += r;
      gwStats.bytesTcpRx += r;
      s.lastActMs = s.lastByteMs = millis();
    }
  }
  if (!s.n) {
    if (cfg.idleTimeoutS && millis() - s.lastActMs > cfg.idleTimeoutS * 1000UL) {
      gwTrace(TR_EVENT, nullptr, 0, "cliente inactivo");
      closeSlot(s);
    }
    return false;
  }

  size_t frame = 0;
  if (cfg.tcpProto == TCP_MBTCP) {
    if (s.n < 7) return false;
    uint16_t len = (s.buf[4] << 8) | s.buf[5];
    if (s.buf[2] || s.buf[3] || len < 2 || len > RTU_MAX_ADU - 2) {   // protocolo != 0 / largo ilegal
      gwTrace(TR_EVENT, s.buf, min(s.n, (size_t)12), "MBAP invalido");
      closeSlot(s);
      return false;
    }
    frame = 6 + len;
    if (s.n < frame) return false;
    gwTrace(TR_TCP_RX, s.buf, frame, nullptr);
    uint16_t tid = (s.buf[0] << 8) | s.buf[1];
    handleRequest(s, tid, s.buf[6], s.buf + 7, len - 1);
  } else {
    size_t exp = rtuExpectedLen(s.buf, s.n, true);
    if (exp && s.n >= exp) frame = exp;
    else if (millis() - s.lastByteMs > 50) frame = s.n;   // funcion desconocida: fin por silencio
    else return false;
    gwTrace(TR_TCP_RX, s.buf, frame, nullptr);
    uint16_t c = frame >= 4 ? rtuCrc(s.buf, frame - 2) : 0;
    if (frame < 4 || (c & 0xFF) != s.buf[frame - 2] || (c >> 8) != s.buf[frame - 1]) {
      gwStats.rtuBad++;
      gwTrace(TR_EVENT, nullptr, 0, "CRC TCP invalido");
      s.n = 0;
      return true;
    }
    handleRequest(s, 0, s.buf[0], s.buf + 1, frame - 3);
  }

  if (s.used) {
    memmove(s.buf, s.buf + frame, s.n - frame);
    s.n -= frame;
  }
  return true;
}

static void acceptClients() {
  if (!s_server.hasClient()) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  WiFiClient nc = s_server.accept();
#else
  WiFiClient nc = s_server.available();
#endif
  if (!nc) return;
  uint32_t ip = (uint32_t)nc.remoteIP();
  if (!allowed(ip)) {
    gwStats.rejected++;
    gwTrace(TR_EVENT, nullptr, 0, "IP no permitida");
    nc.stop();
    return;
  }
  Slot* free = nullptr;
  uint8_t used = 0;
  for (auto& s : s_slots) { if (s.used) used++; else if (!free) free = &s; }
  if (!free || used >= cfg.maxClients) {
    gwStats.rejected++;
    gwTrace(TR_EVENT, nullptr, 0, "sin cupo");
    nc.stop();
    return;
  }
  nc.setNoDelay(true);
#ifdef TCP_KEEPIDLE
  if (cfg.tcpAliveS) {      // "TCP alive check": cierra conexiones de clientes caidos sin FIN
    int fd = nc.fd(), on = 1, idle = cfg.tcpAliveS, intv = 5, cnt = 3;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intv, sizeof(intv));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
  }
#endif
  free->c = nc;
  free->used = true;
  free->ip = ip;
  free->port = nc.remotePort();
  free->sinceMs = free->lastActMs = free->lastByteMs = millis();
  free->reqs = 0;
  free->n = 0;
  gwStats.connTotal++;
  s_clientCount = used + 1;
  gwTrace(TR_EVENT, nullptr, 0, "cliente conectado");
}

static void gwTask(void*) {
  uint32_t ledOffAt = 0;
  for (;;) {
    if (!s_listening && netAnyUp() && millis() >= cfg.initDelayMs) {
      s_server.begin(cfg.tcpPort);
      s_server.setNoDelay(true);
      s_listening = true;
      Serial.printf("[gw] servidor %s en :%u\n",
                    cfg.tcpProto == TCP_MBTCP ? "Modbus TCP" : "RTU/TCP", cfg.tcpPort);
    }

    bool busy = false;
    if (s_listening) {
      acceptClients();
      for (auto& s : s_slots)
        if (s.used && serviceSlot(s)) busy = true;
    }
    if (s_scanOn) { scanStep(); busy = true; }

#if PIN_LED_ACT >= 0
    if (busy) { digitalWrite(PIN_LED_ACT, HIGH); ledOffAt = millis() + 30; }
    else if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) { digitalWrite(PIN_LED_ACT, LOW); ledOffAt = 0; }
#endif
    if (!busy) vTaskDelay(1);
  }
}

void gwBegin() {
#if PIN_LED_ACT >= 0
  pinMode(PIN_LED_ACT, OUTPUT);
  digitalWrite(PIN_LED_ACT, LOW);
#endif
  gwStatsReset();
  xTaskCreatePinnedToCore(gwTask, "gw", 8192, nullptr, 3, nullptr, 1);
}
