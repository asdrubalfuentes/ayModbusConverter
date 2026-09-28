#include "gateway.h"
#include "config.h"
#include "board.h"
#include "net.h"
#include <WiFi.h>
#include <lwip/sockets.h>
#include <freertos/queue.h>
#include <time.h>

GwStats gwStats;

const char* txStateStr(TxState s) {
  switch (s) {
    case TXS_OK:    return "OK";
    case TXS_FAULT: return "FALLAS";
    case TXS_DOWN:  return "DESCONECTADO";
    case TXS_OFF:   return "APAGADO";
    default:        return "SIN TRAFICO";
  }
}

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;   // tablas compartidas

// ===========================================================================
// Monitor de trafico (anillo)
// ===========================================================================
#define TR_SLOTS 64
#define TR_BYTES 256
struct TraceEnt {
  uint32_t seq, ms;
  uint8_t  kind, port;
  uint16_t len;
  uint8_t  d[TR_BYTES];
  char     note[20];
};
static TraceEnt      s_tr[TR_SLOTS];
static uint32_t      s_trSeq = 0;
static volatile bool s_monEn = false;
static portMUX_TYPE  s_trMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t      s_epochAtMs0 = 0;   // epoca UNIX (s) cuando millis() == 0, para el pcap

void gwTrace(TraceKind k, uint8_t port, const uint8_t* d, size_t n, const char* note) {
  if (!s_monEn) return;
  portENTER_CRITICAL(&s_trMux);
  TraceEnt& e = s_tr[s_trSeq % TR_SLOTS];
  e.seq  = ++s_trSeq;
  e.ms   = millis();
  e.kind = k;
  e.port = port;
  e.len  = (uint16_t)n;
  if (d && n) memcpy(e.d, d, min(n, (size_t)TR_BYTES));
  if (note) { strncpy(e.note, note, sizeof(e.note) - 1); e.note[sizeof(e.note) - 1] = 0; }
  else e.note[0] = 0;
  portEXIT_CRITICAL(&s_trMux);
}

void gwMonitorEnable(bool en) {
  s_monEn = en;
  time_t t = time(nullptr);
  if (t > 1600000000) s_epochAtMs0 = (uint32_t)t - millis() / 1000;
}
bool gwMonitorEnabled() { return s_monEn; }
void gwMonitorClear() {
  portENTER_CRITICAL(&s_trMux);
  for (auto& e : s_tr) e.seq = 0;
  portEXIT_CRITICAL(&s_trMux);
}

static bool traceCopy(uint32_t seq, TraceEnt& out) {
  bool ok;
  portENTER_CRITICAL(&s_trMux);
  const TraceEnt& e = s_tr[(seq - 1) % TR_SLOTS];
  ok = (e.seq == seq);
  if (ok) out = e;
  portEXIT_CRITICAL(&s_trMux);
  return ok;
}

static uint32_t traceFirst(uint32_t since, uint32_t& last) {
  portENTER_CRITICAL(&s_trMux);
  last = s_trSeq;
  portEXIT_CRITICAL(&s_trMux);
  uint32_t first = (last > TR_SLOTS) ? last - TR_SLOTS + 1 : 1;
  if (since + 1 > first) first = since + 1;
  return first;
}

uint32_t gwMonitorJson(JsonArray a, uint32_t since) {
  static const char* K[] = { "TCP>", ">RTU", "RTU>", ">TCP", "EVT" };
  static TraceEnt e;                       // estatico: 300 B fuera de la pila de la tarea web
  uint32_t last, first = traceFirst(since, last);
  char hex[TR_BYTES * 3 + 4];
  for (uint32_t s = first; s <= last; s++) {
    if (!traceCopy(s, e)) continue;
    JsonObject o = a.add<JsonObject>();
    o["s"] = e.seq;
    o["t"] = e.ms;
    o["k"] = K[e.kind];
    if (e.port != 0xFF) o["p"] = e.port + 1;
    o["n"] = e.len;
    size_t m = min((size_t)e.len, (size_t)TR_BYTES), p = 0;
    for (size_t i = 0; i < m; i++) p += snprintf(hex + p, sizeof(hex) - p, i ? " %02X" : "%02X", e.d[i]);
    hex[p] = 0;
    o["h"] = hex;
    if (e.note[0]) o["x"] = e.note;
  }
  return last;
}

// pcapng con dos interfaces: 0 = tramas RTU (DLT_USER0=147), 1 = Modbus TCP/MBAP (DLT_USER1=148).
// En Wireshark: Preferencias > Protocols > DLT_USER: 147 -> mbrtu, 148 -> mbtcp.
static void w32(Print& o, uint32_t v) { o.write((const uint8_t*)&v, 4); }
static void w16(Print& o, uint16_t v) { o.write((const uint8_t*)&v, 2); }

void gwPcap(Print& out) {
  // Section Header Block
  w32(out, 0x0A0D0D0A); w32(out, 28); w32(out, 0x1A2B3C4D); w16(out, 1); w16(out, 0);
  w32(out, 0xFFFFFFFF); w32(out, 0xFFFFFFFF); w32(out, 28);
  // Interface Description Blocks
  for (uint16_t lt : { (uint16_t)147, (uint16_t)148 }) {
    w32(out, 1); w32(out, 20); w16(out, lt); w16(out, 0); w32(out, 65535); w32(out, 20);
  }
  static TraceEnt e;
  uint32_t last, first = traceFirst(0, last);
  bool rtuOverTcp = cfg.tcpProto == TCP_RTU_OVER;
  for (uint32_t s = first; s <= last; s++) {
    if (!traceCopy(s, e) || e.kind == TR_EVENT || !e.len) continue;
    uint32_t cap = min((uint32_t)e.len, (uint32_t)TR_BYTES);
    uint32_t pad = (4 - (cap & 3)) & 3;
    uint32_t blk = 32 + cap + pad;
    uint32_t ifc = ((e.kind == TR_TCP_RX || e.kind == TR_TCP_TX) && !rtuOverTcp) ? 1 : 0;
    uint64_t us = ((uint64_t)s_epochAtMs0 * 1000ULL + e.ms) * 1000ULL;
    w32(out, 6); w32(out, blk); w32(out, ifc);
    w32(out, (uint32_t)(us >> 32)); w32(out, (uint32_t)us);
    w32(out, cap); w32(out, e.len);
    out.write(e.d, cap);
    for (uint32_t i = 0; i < pad; i++) out.write((uint8_t)0);
    w32(out, blk);
  }
}

// ===========================================================================
// Salud de esclavos y puertos
// ===========================================================================
#define MAX_SLAVES 64
struct Slave {
  bool     used;
  uint8_t  port, uid;
  bool     offline;
  uint8_t  consec;            // timeouts seguidos
  uint8_t  winN;              // resultados en la ventana (max 16)
  uint16_t win;               // bit = 1 -> fallo
  uint16_t lastMs;
  uint32_t req, ok, exc, tout, err, fast;
  uint32_t lastSeenMs, lastReqMs, lastProbeMs;
  char     ident[40];
};
static Slave s_sl[MAX_SLAVES];

struct PortHealth { uint32_t win; uint8_t winN; uint32_t lastActMs; };
static PortHealth s_ph[NUM_PORTS];

static Slave* slaveGet(uint8_t port, uint8_t uid, bool create) {
  for (auto& s : s_sl) if (s.used && s.port == port && s.uid == uid) return &s;
  if (!create) return nullptr;
  Slave* r = nullptr;
  portENTER_CRITICAL(&s_mux);
  for (auto& s : s_sl) if (!s.used) { s = Slave{}; s.port = port; s.uid = uid; s.used = true; r = &s; break; }
  portEXIT_CRITICAL(&s_mux);
  return r;
}

static TxState slaveState(const Slave& s) {
  if (s.offline) return TXS_DOWN;
  if (!s.winN || millis() - s.lastReqMs > 60000) return TXS_IDLE;
  uint16_t mask = (s.winN >= 16) ? 0xFFFF : ((1u << s.winN) - 1);
  uint16_t f = s.win & mask;
  if (f == mask) return TXS_DOWN;
  if (f) return TXS_FAULT;
  return TXS_OK;
}

TxState gwPortState(uint8_t p) {
  if (p >= NUM_PORTS || !cfg.port[p].en) return TXS_OFF;
  const PortHealth& h = s_ph[p];
  if (!h.winN || millis() - h.lastActMs > 30000) return TXS_IDLE;
  uint32_t mask = (h.winN >= 32) ? 0xFFFFFFFFUL : ((1UL << h.winN) - 1);
  uint32_t f = h.win & mask;
  if (f == mask) return TXS_DOWN;
  if (f) return TXS_FAULT;
  return TXS_OK;
}

TxState gwGlobalState() {
  static const uint8_t RANK[] = { 0, 1, 2, 3, 0 };   // IDLE OK FAULT DOWN OFF
  TxState worst = TXS_IDLE;
  bool any = false;
  for (uint8_t p = 0; p < NUM_PORTS; p++) {
    TxState s = gwPortState(p);
    if (s == TXS_OFF) continue;
    any = true;
    if (RANK[s] > RANK[worst]) worst = s;
  }
  return any ? worst : TXS_OFF;
}

int gwSlaveList(SlaveBrief* out, int max) {
  int n = 0;
  for (uint8_t p = 0; p < NUM_PORTS; p++)
    for (int u = 1; u <= 247 && n < max; u++)
      for (auto& s : s_sl)
        if (s.used && s.port == p && s.uid == u) { out[n++] = SlaveBrief{ p, (uint8_t)u, slaveState(s) }; break; }
  return n;
}

static void recordResult(uint8_t p, Slave* s, RtuResult r, uint32_t lat, bool exc) {
  bool fail = (r == RTU_TIMEOUT || r == RTU_CRC || r == RTU_BADFRAME);
  PortHealth& h = s_ph[p];
  h.win = (h.win << 1) | (fail ? 1 : 0);
  if (h.winN < 32) h.winN++;
  h.lastActMs = millis();
  gwStats.lastActMs = millis();

  switch (r) {
    case RTU_OK:      gwStats.rtuOk++; if (exc) gwStats.rtuExc++; gwStats.lastLatencyMs = lat; break;
    case RTU_TIMEOUT: gwStats.rtuTimeout++; break;
    case RTU_CRC:     gwStats.rtuCrc++; break;
    case RTU_BCAST:   gwStats.bcast++; break;
    default:          gwStats.rtuBad++; break;
  }
  if (!s) return;
  s->win = (s->win << 1) | (fail ? 1 : 0);
  if (s->winN < 16) s->winN++;
  if (r == RTU_OK) {
    s->ok++; if (exc) s->exc++;
    s->lastMs = lat; s->lastSeenMs = millis();
    s->consec = 0;
    if (s->offline) { s->offline = false; gwTrace(TR_EVENT, p, nullptr, 0, "esclavo recuperado"); }
  } else if (fail) {
    if (r == RTU_TIMEOUT) s->tout++; else s->err++;
    if (s->consec < 255) s->consec++;
    uint8_t lim = cfg.port[p].offlineAfter;
    if (lim && s->consec >= lim && !s->offline) {
      s->offline = true;
      s->lastProbeMs = millis();
      gwTrace(TR_EVENT, p, nullptr, 0, "esclavo caido");
    }
  }
}

// ===========================================================================
// Cache (tabla de sondeo) y aprendizaje
// ===========================================================================
struct CacheEnt {
  uint32_t tsMs;              // ultimo dato valido (0 = sin dato / invalidado)
  uint32_t lastPollMs;
  uint32_t okCount, failCount;
  uint8_t  lastRes;           // RtuResult del ultimo sondeo
  uint8_t  exc;               // excepcion del esclavo en el ultimo sondeo
  uint16_t nbytes;
  uint8_t  data[250];
};
static CacheEnt s_cache[MAX_POLLS];
static volatile uint32_t s_pollUpdated = 0;

uint32_t gwPollUpdatedMask(bool clear) {
  uint32_t m = s_pollUpdated;
  if (clear) s_pollUpdated = 0;
  return m;
}

static bool pollMatches(const PollEntry& e, uint8_t port, uint8_t uid, uint8_t fc, uint16_t addr, uint16_t qty) {
  return e.en && e.port == port && e.uid == uid && e.fc == fc &&
         addr >= e.addr && (uint32_t)addr + qty <= (uint32_t)e.addr + e.qty;
}

static void cacheStore(int i, const uint8_t* resp, size_t len) {
  // resp: [uid][fc][bytecount][datos]
  if (len < 3) return;
  uint8_t bc = resp[2];
  if (bc + 3u > len || bc > sizeof(s_cache[i].data)) return;
  portENTER_CRITICAL(&s_mux);
  memcpy(s_cache[i].data, resp + 3, bc);
  s_cache[i].nbytes = bc;
  s_cache[i].tsMs = millis();
  s_cache[i].exc = 0;
  portEXIT_CRITICAL(&s_mux);
  s_pollUpdated |= (1UL << i);
}

// Intenta responder una lectura desde la cache. pdu out: [fc][bc][datos]
static bool cacheLookup(uint8_t port, uint8_t uid, uint8_t fc, uint16_t addr, uint16_t qty,
                        uint8_t* pdu, size_t& pduLen, uint32_t& ageMs) {
  uint32_t now = millis();
  for (int i = 0; i < MAX_POLLS; i++) {
    const PollEntry& e = cfg.poll[i];
    if (!pollMatches(e, port, uid, fc, addr, qty)) continue;
    CacheEnt& c = s_cache[i];
    if (!c.tsMs || now - c.tsMs > cfg.port[port].cacheMaxAgeMs) continue;
    uint16_t off = addr - e.addr;
    portENTER_CRITICAL(&s_mux);
    pdu[0] = fc;
    if (fc >= 3) {
      pdu[1] = qty * 2;
      memcpy(pdu + 2, c.data + off * 2, qty * 2);
      pduLen = 2 + qty * 2;
    } else {
      uint8_t bc = (qty + 7) / 8;
      pdu[1] = bc;
      memset(pdu + 2, 0, bc);
      for (uint16_t k = 0; k < qty; k++) {
        uint16_t b = off + k;
        if ((c.data[b >> 3] >> (b & 7)) & 1) pdu[2 + (k >> 3)] |= 1 << (k & 7);
      }
      pduLen = 2 + bc;
    }
    ageMs = now - c.tsMs;
    portEXIT_CRITICAL(&s_mux);
    return true;
  }
  return false;
}

static void cacheInvalidate(uint8_t port, uint8_t uid, uint8_t wfc, uint16_t addr, uint16_t qty) {
  uint8_t rfcA = (wfc == 5 || wfc == 15) ? 1 : 3;          // coils -> FC01 ; registros -> FC03
  for (int i = 0; i < MAX_POLLS; i++) {
    const PollEntry& e = cfg.poll[i];
    if (!e.en || e.port != port || e.uid != uid || e.fc != rfcA) continue;
    if ((uint32_t)addr + qty <= e.addr || addr >= (uint32_t)e.addr + e.qty) continue;
    s_cache[i].tsMs = 0;
    s_cache[i].lastPollMs = 0;                                // re-sondeo inmediato
  }
}

// Aprendizaje: que lecturas hacen los clientes
#define MAX_LEARN 32
struct Learn { uint8_t port, uid, fc; uint16_t addr, qty; uint32_t count, firstMs, lastMs; };
static Learn s_learn[MAX_LEARN];
static volatile bool s_learnEn = false;

void gwLearnEnable(bool en) { s_learnEn = en; }
bool gwLearnEnabled() { return s_learnEn; }
void gwLearnClear() { portENTER_CRITICAL(&s_mux); memset(s_learn, 0, sizeof(s_learn)); portEXIT_CRITICAL(&s_mux); }

static void learnRecord(uint8_t port, uint8_t uid, uint8_t fc, uint16_t addr, uint16_t qty) {
  if (!s_learnEn) return;
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  Learn* free = nullptr;
  for (auto& l : s_learn) {
    if (l.count && l.port == port && l.uid == uid && l.fc == fc && l.addr == addr && l.qty == qty) {
      l.count++; l.lastMs = now; free = nullptr; portEXIT_CRITICAL(&s_mux); return;
    }
    if (!l.count && !free) free = &l;
  }
  if (free) *free = Learn{ port, uid, fc, addr, qty, 1, now, now };
  portEXIT_CRITICAL(&s_mux);
}

void gwLearnJson(JsonArray a) {
  for (auto& l : s_learn) {
    if (!l.count) continue;
    JsonObject o = a.add<JsonObject>();
    o["port"] = l.port; o["uid"] = l.uid; o["fc"] = l.fc; o["addr"] = l.addr; o["qty"] = l.qty;
    o["count"] = l.count;
    o["period"] = l.count > 1 ? (l.lastMs - l.firstMs) / (l.count - 1) : 0;
    o["ago"] = (millis() - l.lastMs) / 1000;
  }
}

bool gwTagValues(int i, JsonObject o) {
  if (i < 0 || i >= MAX_POLLS) return false;
  const PollEntry& e = cfg.poll[i];
  if (!e.uid) return false;
  const CacheEnt& c = s_cache[i];
  uint32_t now = millis();
  o["i"] = i; o["name"] = e.name; o["en"] = e.en;
  o["port"] = e.port; o["uid"] = e.uid; o["fc"] = e.fc; o["addr"] = e.addr; o["qty"] = e.qty;
  o["period"] = e.periodMs;
  o["ok"] = c.okCount; o["fail"] = c.failCount;
  o["q"] = !c.tsMs ? "sin dato" : (c.lastRes == RTU_OK && !c.exc ? "ok" : (c.exc ? "excepcion" : rtuResultStr((RtuResult)c.lastRes)));
  if (c.exc) o["exc"] = c.exc;
  if (!c.tsMs) return true;
  o["age"] = now - c.tsMs;
  JsonArray v = o["v"].to<JsonArray>();
  portENTER_CRITICAL(&s_mux);
  uint8_t tmp[250]; uint16_t nb = c.nbytes;
  memcpy(tmp, c.data, nb);
  portEXIT_CRITICAL(&s_mux);
  if (e.fc >= 3) for (uint16_t k = 0; k < e.qty && 2 * k + 1 < nb; k++) v.add((uint16_t)((tmp[2 * k] << 8) | tmp[2 * k + 1]));
  else for (uint16_t k = 0; k < e.qty && (k >> 3) < nb; k++) v.add((tmp[k >> 3] >> (k & 7)) & 1);
  return true;
}

void gwTagsJson(JsonArray a) {
  for (int i = 0; i < MAX_POLLS; i++) {
    if (!cfg.poll[i].uid) continue;
    gwTagValues(i, a.add<JsonObject>());
  }
}

// ===========================================================================
// Trabajos y colas
// ===========================================================================
enum : uint8_t { J_FREE = 0, J_BUSY, J_QUEUED, J_DONE };
struct Job {
  volatile uint8_t state;
  bool     orphan;            // el solicitante dejo de esperar: la tarea del puerto lo libera
  uint8_t  port;
  uint8_t  bcastMask;         // broadcast: puertos que faltan
  uint8_t  req[RTU_MAX_ADU];  // [uid destino][pdu]
  size_t   reqLen;
  uint8_t  resp[RTU_MAX_ADU];
  size_t   respLen;
  RtuResult res;
  uint8_t  gwExc;
  uint32_t lat;
  bool     cached;
  uint32_t ageMs;
  // contexto TCP
  uint16_t tid;
  uint8_t  cuid;
};
#define EXT_JOBS 4
static Job s_jobs[MAX_TCP_CLIENTS + EXT_JOBS];   // 0..7 = un trabajo por cliente TCP
static QueueHandle_t s_q[NUM_PORTS];

// Cierra un trabajo: si el solicitante ya no espera (orphan) se libera, si no queda J_DONE.
// Bajo candado: el solicitante puede estar marcandolo como huerfano al mismo tiempo.
static void jobFinish(Job& j) {
  portENTER_CRITICAL(&s_mux);
  if (j.orphan) { j.orphan = false; j.state = J_FREE; }
  else j.state = J_DONE;
  portEXIT_CRITICAL(&s_mux);
}

static void enqueue(uint8_t ji) {
  Job& j = s_jobs[ji];
  j.state = J_QUEUED;
  if (xQueueSend(s_q[j.port], &ji, 0) != pdTRUE) {           // cola llena
    j.res = RTU_BADFRAME; j.gwExc = 0x0A; j.state = J_DONE;
  }
}

static bool readOnlyHit(uint8_t port, uint8_t uid, uint8_t fc, uint16_t addr, uint16_t qty) {
  if (!(fc == 5 || fc == 6 || fc == 15 || fc == 16)) return false;
  for (auto& r : cfg.ro) {
    if (!r.en || r.port != port || uid < r.uidFrom || uid > r.uidTo) continue;
    if ((uint32_t)addr + qty <= r.addrFrom || addr > r.addrTo) continue;
    return true;
  }
  return false;
}

static bool route(uint8_t cuid, uint8_t& port, uint8_t& target, uint8_t& bcastMask) {
  bcastMask = 0;
  if (cuid == 0 && cfg.uid0Broadcast) {
    for (uint8_t p = 0; p < NUM_PORTS; p++) if (cfg.port[p].en) bcastMask |= 1 << p;
    if (!bcastMask) return false;
    port = __builtin_ctz(bcastMask);
    target = 0;
    return true;
  }
  if (cfg.uidMode == UID_FIXED || cuid == 0 || cuid == 255) {
    if (!cfg.port[0].en) return false;
    port = 0; target = cfg.fixedUid;
    return true;
  }
  for (uint8_t p = 0; p < NUM_PORTS; p++) {
    const PortCfg& c = cfg.port[p];
    if (!c.en || cuid < c.uidFrom || cuid > c.uidTo) continue;
    int t = (int)cuid + c.uidOffset;
    if (t < 1 || t > 247) return false;
    port = p; target = (uint8_t)t;
    return true;
  }
  return false;
}

// Prepara un trabajo. true = hay que encolarlo; false = ya quedo resuelto (J_DONE).
// directPort >= 0: sin enrutamiento (herramientas / API con puerto explicito).
static bool prepare(Job& j, int directPort, uint8_t cuid, const uint8_t* pdu, size_t pduLen, bool useCache) {
  j.res = RTU_OK; j.gwExc = 0; j.cached = false; j.respLen = 0; j.lat = 0; j.ageMs = 0;
  j.cuid = cuid;
  uint8_t fc = pdu[0];
  uint8_t port, target, bm = 0;
  if (directPort >= 0) {
    port = directPort; target = cuid;
    if (!cfg.port[port].en) { j.gwExc = 0x0A; j.state = J_DONE; return false; }
    if (cuid == 0) bm = 1 << port;
  } else if (!route(cuid, port, target, bm)) {
    gwTrace(TR_EVENT, 0xFF, nullptr, 0, "sin ruta p/ Unit ID");
    j.gwExc = 0x0A; j.state = J_DONE;
    return false;
  }
  j.port = port; j.bcastMask = bm;
  j.req[0] = target;
  memcpy(j.req + 1, pdu, pduLen);
  j.reqLen = 1 + pduLen;

  uint16_t addr = pduLen >= 5 ? (pdu[1] << 8) | pdu[2] : 0;
  uint16_t qty  = pduLen >= 5 ? (pdu[3] << 8) | pdu[4] : 0;
  if (fc == 5 || fc == 6) qty = 1;

  if (target && readOnlyHit(port, target, fc, addr, qty)) {
    gwTrace(TR_EVENT, port, nullptr, 0, "escritura bloqueada");
    j.gwExc = 0x01; j.state = J_DONE;
    return false;
  }
  if (target && fc >= 1 && fc <= 4 && pduLen == 5) {
    learnRecord(port, target, fc, addr, qty);
    if (useCache && cfg.port[port].cacheEn) {
      size_t pl; uint32_t age;
      if (cacheLookup(port, target, fc, addr, qty, j.resp + 1, pl, age)) {
        j.resp[0] = target; j.respLen = 1 + pl;
        j.cached = true; j.ageMs = age;
        gwStats.cacheHits++;
        j.state = J_DONE;
        return false;
      }
    }
  }
  return true;
}

// Ejecuta un trabajo en la tarea de su puerto
static void execJob(uint8_t p, Job& j) {
  RtuPort& rp = rtuPort[p];
  uint8_t target = j.req[0];
  Slave* s = target ? slaveGet(p, target, true) : nullptr;
  if (s) { s->req++; s->lastReqMs = millis(); }

  if (s && s->offline && millis() - s->lastProbeMs < cfg.port[p].offlineRetryS * 1000UL) {
    j.res = RTU_OFFLINE;                 // respuesta inmediata, sin ocupar el bus
    s->fast++;
    gwStats.fastFail++;
  } else {
    if (s && s->offline) s->lastProbeMs = millis();
    j.res = rp.transact(j.req, j.reqLen, j.resp, j.respLen, sizeof(j.resp), j.lat);
    bool exc = (j.res == RTU_OK && (j.resp[1] & 0x80));
    recordResult(p, s, j.res, j.lat, exc);
    if (j.res == RTU_OK && !exc) {
      uint8_t fc = j.req[1];
      uint16_t addr = (j.req[2] << 8) | j.req[3];
      uint16_t qty  = (j.req[4] << 8) | j.req[5];
      if (fc >= 1 && fc <= 4) {                             // refresco gratis de la cache
        for (int i = 0; i < MAX_POLLS; i++) {
          const PollEntry& e = cfg.poll[i];
          if (e.en && e.port == p && e.uid == target && e.fc == fc && e.addr == addr && e.qty == qty) {
            cacheStore(i, j.resp, j.respLen);
            s_cache[i].lastRes = RTU_OK;
          }
        }
      } else if (fc == 5 || fc == 6 || fc == 15 || fc == 16) {
        cacheInvalidate(p, target, fc, addr, (fc == 5 || fc == 6) ? 1 : qty);
      }
    }
  }

  // broadcast: pasa al siguiente puerto habilitado
  if (j.bcastMask) {
    j.bcastMask &= ~(1 << p);
    if (j.bcastMask) {
      j.port = __builtin_ctz(j.bcastMask);
      uint8_t ji = &j - s_jobs;
      if (xQueueSend(s_q[j.port], &ji, 0) == pdTRUE) return;
    }
  }
  jobFinish(j);
}

bool gwExecute(int port, uint8_t uid, const uint8_t* pdu, size_t pduLen, GwResult& r,
               uint32_t waitMs, bool useCache) {
  r = GwResult{};
  if (pduLen < 1 || pduLen > RTU_MAX_ADU - 3) return false;
  int ji = -1;
  portENTER_CRITICAL(&s_mux);
  for (int i = MAX_TCP_CLIENTS; i < MAX_TCP_CLIENTS + EXT_JOBS; i++)
    if (s_jobs[i].state == J_FREE) { s_jobs[i].state = J_BUSY; s_jobs[i].orphan = false; ji = i; break; }
  portEXIT_CRITICAL(&s_mux);
  if (ji < 0) { r.gwExc = 0x0A; return false; }
  Job& j = s_jobs[ji];
  gwStats.tcpReq++;
  if (prepare(j, port, uid, pdu, pduLen, useCache)) {
    enqueue(ji);
    uint32_t t0 = millis();
    while (j.state != J_DONE) {
      if (millis() - t0 > waitMs) {
        portENTER_CRITICAL(&s_mux);
        bool done = (j.state == J_DONE);
        if (!done) j.orphan = true;
        portEXIT_CRITICAL(&s_mux);
        if (!done) { r.res = RTU_TIMEOUT; return false; }
        break;
      }
      vTaskDelay(2);
    }
  }
  r.res = j.res; r.gwExc = j.gwExc; r.len = j.respLen; r.lat = j.lat;
  r.cached = j.cached; r.ageMs = j.ageMs; r.port = j.port;
  memcpy(r.resp, j.resp, j.respLen);
  j.state = J_FREE;
  return true;
}

// ===========================================================================
// Herramientas: escaneo, autodeteccion, exploracion de registros
// ===========================================================================
enum ToolKind : uint8_t { TOOL_NONE = 0, TOOL_SCAN, TOOL_DETECT, TOOL_DISCOVER };
static const char* TOOL_NAMES[] = { "", "scan", "detect", "discover" };
static const uint32_t DET_BAUDS[] = { 9600, 19200, 38400, 115200, 4800, 57600, 2400, 1200 };
static const char     DET_PAR[]   = { 'N', 'E', 'O' };
#define DET_COMBOS (8 * 3)
#define DISC_BLOCKS 256

static volatile bool s_reopen[NUM_PORTS];

static struct Tool {
  volatile uint8_t kind;
  volatile bool    stopReq;
  uint8_t  port;
  bool     done;
  uint16_t progress, total;          // pasos
  char     status[48];
  // scan
  uint8_t  from, to, cur;
  uint16_t timeoutMs;
  uint32_t found[8];
  // detect
  bool     passive;
  uint8_t  ids[32], nIds;
  uint8_t  combo, idIdx;
  bool     detFound;
  uint32_t detBaud; char detParity; uint8_t detUid; uint8_t detFrames;
  // discover
  uint8_t  uid;
  uint16_t step, maxAddr;
  uint8_t  phase;                    // 0 ident, 1..4 = FC3,4,1,2
  uint16_t blk;
  uint8_t  blocks[4][DISC_BLOCKS / 8];
  char     vendor[24], product[24], rev[12];
} s_tool;

static void toolStart(ToolKind k, uint8_t port) {
  memset((void*)&s_tool, 0, sizeof(s_tool));
  s_tool.port = port;
  s_tool.kind = k;
}

bool gwScanStart(uint8_t port, uint8_t from, uint8_t to, uint16_t timeoutMs) {
  if (s_tool.kind && !s_tool.done) return false;
  if (port >= NUM_PORTS || !cfg.port[port].en) return false;
  if (from < 1) from = 1;
  if (to > 247) to = 247;
  if (from > to) return false;
  toolStart(TOOL_SCAN, port);
  s_tool.from = from; s_tool.to = to; s_tool.cur = from;
  s_tool.timeoutMs = constrain(timeoutMs, 20, 2000);
  s_tool.total = to - from + 1;
  return true;
}

static void parseIds(const char* s, uint8_t* out, uint8_t& n) {
  n = 0;
  while (s && *s && n < 32) {
    while (*s == ' ' || *s == ',') s++;
    if (!*s) break;
    int a = strtol(s, (char**)&s, 10), b = a;
    if (*s == '-') { s++; b = strtol(s, (char**)&s, 10); }
    for (int u = a; u <= b && n < 32; u++) if (u >= 1 && u <= 247) out[n++] = u;
    while (*s && *s != ',') s++;
  }
  if (!n) for (int u = 1; u <= 10; u++) out[n++] = u;
}

bool gwDetectStart(uint8_t port, bool passive, const char* ids) {
  if (s_tool.kind && !s_tool.done) return false;
  if (port >= NUM_PORTS) return false;
  toolStart(TOOL_DETECT, port);
  s_tool.passive = passive;
  parseIds(ids, s_tool.ids, s_tool.nIds);
  s_tool.total = DET_COMBOS;
  return true;
}

bool gwDiscoverStart(uint8_t port, uint8_t uid, uint16_t step, uint16_t maxAddr) {
  if (s_tool.kind && !s_tool.done) return false;
  if (port >= NUM_PORTS || !cfg.port[port].en || uid < 1 || uid > 247) return false;
  toolStart(TOOL_DISCOVER, port);
  s_tool.uid = uid;
  s_tool.step = constrain(step, 1, 10000);
  s_tool.maxAddr = maxAddr ? maxAddr : 10000;
  if ((uint32_t)s_tool.maxAddr / s_tool.step > DISC_BLOCKS) s_tool.maxAddr = s_tool.step * DISC_BLOCKS;
  s_tool.total = 1 + 4 * (s_tool.maxAddr / s_tool.step);
  return true;
}

void gwReopenPort(uint8_t p) { if (p < NUM_PORTS) s_reopen[p] = true; }

void gwToolStop() { if (s_tool.kind && !s_tool.done) s_tool.stopReq = true; }

bool gwDetectApply() {
  if (s_tool.kind != TOOL_DETECT || !s_tool.done || !s_tool.detFound) return false;
  PortCfg& c = cfg.port[s_tool.port];
  c.en = true;
  c.baud = s_tool.detBaud;
  c.parity = s_tool.detParity;
  c.dataBits = 8;
  c.stopBits = 1;          // 1 o 2 bits de parada no se distinguen al recibir; 1 es lo habitual
  bool ok = cfgSave();
  s_reopen[s_tool.port] = true;                          // lo reabre la tarea del puerto
  return ok;
}

static void toolFinish(const char* msg) {
  strncpy(s_tool.status, msg, sizeof(s_tool.status) - 1);
  if (s_tool.kind == TOOL_DETECT) rtuPort[s_tool.port].begin();   // vuelve a la configuracion guardada
  s_tool.done = true;
}

static bool probe(RtuPort& rp, uint8_t uid, uint16_t timeoutMs, uint8_t fc = 3, uint16_t addr = 0,
                  uint8_t* resp = nullptr, size_t* rlOut = nullptr) {
  uint8_t req[6] = { uid, fc, (uint8_t)(addr >> 8), (uint8_t)addr, 0, 1 };
  uint8_t r[RTU_MAX_ADU]; size_t rl; uint32_t lat;
  RtuResult res = rp.transact(req, 6, resp ? resp : r, rlOut ? *rlOut : rl, RTU_MAX_ADU, lat, timeoutMs, 0);
  return res == RTU_OK;
}

static void scanStep() {
  RtuPort& rp = rtuPort[s_tool.port];
  uint8_t u = s_tool.cur;
  snprintf(s_tool.status, sizeof(s_tool.status), "Unit ID %u", u);
  if (probe(rp, u, s_tool.timeoutMs)) s_tool.found[u >> 5] |= (1UL << (u & 31));
  s_tool.progress++;
  if (u >= s_tool.to) {
    int n = 0; for (auto f : s_tool.found) n += __builtin_popcount(f);
    char m[40]; snprintf(m, sizeof(m), "Terminado: %d esclavo(s)", n);
    toolFinish(m);
  } else s_tool.cur = u + 1;
}

static void detectStep() {
  RtuPort& rp = rtuPort[s_tool.port];
  uint8_t ci = s_tool.combo;
  uint32_t baud = DET_BAUDS[ci / 3];
  char par = DET_PAR[ci % 3];
  if (s_tool.idIdx == 0 || s_tool.passive) {
    rp.open(baud, 8, par, 1);
    snprintf(s_tool.status, sizeof(s_tool.status), "%lu 8%c1", (unsigned long)baud, par);
  }
  uint32_t charMs = (11000UL + baud - 1) / baud;             // ms por caracter (redondeo arriba)

  if (s_tool.passive) {
    // escucha 1,5 s: bastan 2 tramas con CRC valido
    uint8_t buf[RTU_MAX_ADU];
    uint8_t good = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < 1500 && good < 2 && !s_tool.stopReq) {
      size_t n = rp.sniff(buf, sizeof(buf), 300);
      if (n && rtuCrcOk(buf, n)) { good++; s_tool.detUid = buf[0]; }
    }
    if (good >= 2) {
      s_tool.detFound = true; s_tool.detBaud = baud; s_tool.detParity = par; s_tool.detFrames = good;
    }
    s_tool.progress = ci + 1;
    s_tool.combo++;
  } else {
    uint16_t tmo = 60 + 25 * charMs;
    uint8_t uid = s_tool.ids[s_tool.idIdx];
    if (probe(rp, uid, tmo)) {
      // confirma con 2 lecturas mas: descarta coincidencias casuales con paridad erronea
      if (probe(rp, uid, tmo) && probe(rp, uid, tmo)) {
        s_tool.detFound = true; s_tool.detBaud = baud; s_tool.detParity = par; s_tool.detUid = uid;
      }
    }
    if (!s_tool.detFound && ++s_tool.idIdx >= s_tool.nIds) {
      s_tool.idIdx = 0;
      s_tool.progress = ci + 1;
      s_tool.combo++;
    }
  }
  if (s_tool.detFound) {
    char m[48];
    snprintf(m, sizeof(m), "Encontrado: %lu 8%c1, ID %u", (unsigned long)s_tool.detBaud, s_tool.detParity, s_tool.detUid);
    s_tool.progress = s_tool.total;
    toolFinish(m);
  } else if (s_tool.combo >= DET_COMBOS) {
    toolFinish(s_tool.passive ? "Sin trafico reconocible en el bus" : "Ningun equipo respondio");
  }
}

static void parseIdent(const uint8_t* r, size_t n) {
  // [uid][2B][0E][code][conform][more][next][num] {id len val}...
  if (n < 8 || r[1] != 0x2B) return;
  size_t i = 8;
  for (uint8_t k = 0; k < r[7] && i + 2 <= n; k++) {
    uint8_t id = r[i], len = r[i + 1];
    if (i + 2 + len > n) break;
    char* dst = id == 0 ? s_tool.vendor : id == 1 ? s_tool.product : id == 2 ? s_tool.rev : nullptr;
    size_t cap = id == 0 ? sizeof(s_tool.vendor) : id == 1 ? sizeof(s_tool.product) : sizeof(s_tool.rev);
    if (dst) { size_t m = min((size_t)len, cap - 1); memcpy(dst, r + i + 2, m); dst[m] = 0; }
    i += 2 + len;
  }
}

static void discoverStep() {
  RtuPort& rp = rtuPort[s_tool.port];
  const PortCfg& c = cfg.port[s_tool.port];
  uint16_t tmo = min<uint16_t>(c.respTimeoutMs, 500);
  if (s_tool.phase == 0) {
    strcpy(s_tool.status, "Identificacion (FC43)");
    uint8_t req[5] = { s_tool.uid, 0x2B, 0x0E, 0x01, 0x00 };
    uint8_t r[RTU_MAX_ADU]; size_t rl; uint32_t lat;
    if (rp.transact(req, 5, r, rl, sizeof(r), lat, tmo, 0) == RTU_OK && !(r[1] & 0x80)) parseIdent(r, rl);
    if (s_tool.vendor[0] || s_tool.product[0]) {
      Slave* s = slaveGet(s_tool.port, s_tool.uid, true);
      if (s) snprintf(s->ident, sizeof(s->ident), "%s %s", s_tool.vendor, s_tool.product);
    }
    s_tool.phase = 1; s_tool.blk = 0; s_tool.progress = 1;
    return;
  }
  static const uint8_t FCS[] = { 3, 4, 1, 2 };
  uint8_t fc = FCS[s_tool.phase - 1];
  uint16_t addr = s_tool.blk * s_tool.step;
  snprintf(s_tool.status, sizeof(s_tool.status), "FC%02u direccion %u", fc, addr);
  uint8_t r[RTU_MAX_ADU]; size_t rl = 0;
  // respuesta normal = existe; excepcion 02 = direccion ilegal (no existe)
  if (probe(rp, s_tool.uid, tmo, fc, addr, r, &rl) && !(r[1] & 0x80))
    s_tool.blocks[s_tool.phase - 1][s_tool.blk >> 3] |= 1 << (s_tool.blk & 7);
  s_tool.progress++;
  if ((uint32_t)(s_tool.blk + 1) * s_tool.step >= s_tool.maxAddr) {
    s_tool.blk = 0;
    if (++s_tool.phase > 4) toolFinish("Exploracion terminada");
  } else s_tool.blk++;
}

static void toolStep() {
  if (s_tool.stopReq) { toolFinish("Detenido"); return; }
  switch (s_tool.kind) {
    case TOOL_SCAN:     scanStep(); break;
    case TOOL_DETECT:   detectStep(); break;
    case TOOL_DISCOVER: discoverStep(); break;
    default: break;
  }
}

void gwToolJson(JsonObject o) {
  o["kind"] = TOOL_NAMES[s_tool.kind];
  if (!s_tool.kind) return;
  o["port"] = s_tool.port;
  o["active"] = !s_tool.done;
  o["progress"] = s_tool.progress;
  o["total"] = s_tool.total;
  o["status"] = s_tool.status;
  if (s_tool.kind == TOOL_SCAN) {
    JsonArray f = o["found"].to<JsonArray>();
    for (int u = 1; u <= 247; u++) if (s_tool.found[u >> 5] & (1UL << (u & 31))) f.add(u);
  } else if (s_tool.kind == TOOL_DETECT) {
    o["passive"] = s_tool.passive;
    o["found"] = s_tool.detFound;
    if (s_tool.detFound) {
      o["baud"] = s_tool.detBaud;
      o["parity"] = String(s_tool.detParity);
      o["uid"] = s_tool.detUid;
    }
  } else if (s_tool.kind == TOOL_DISCOVER) {
    o["uid"] = s_tool.uid;
    o["vendor"] = s_tool.vendor;
    o["product"] = s_tool.product;
    o["rev"] = s_tool.rev;
    o["step"] = s_tool.step;
    static const uint8_t FCS[] = { 3, 4, 1, 2 };
    JsonArray ranges = o["ranges"].to<JsonArray>();
    uint16_t nb = s_tool.maxAddr / s_tool.step;
    for (int t = 0; t < 4; t++) {
      int start = -1;
      for (int b = 0; b <= nb; b++) {
        bool on = b < nb && (s_tool.blocks[t][b >> 3] >> (b & 7)) & 1;
        if (on && start < 0) start = b;
        if (!on && start >= 0) {
          JsonObject r = ranges.add<JsonObject>();
          r["fc"] = FCS[t];
          r["from"] = start * s_tool.step;
          r["to"] = b * s_tool.step - 1;
          start = -1;
        }
      }
    }
  }
}

// ===========================================================================
// Tarea de cada puerto
// ===========================================================================
static bool pollStep(uint8_t p) {
  if (millis() < cfg.initDelayMs) return false;
  uint32_t now = millis();
  static uint8_t rr[NUM_PORTS];                  // reparto round-robin entre entradas
  for (int k = 0; k < MAX_POLLS; k++) {
    int i = (rr[p] + k) % MAX_POLLS;
    const PollEntry& e = cfg.poll[i];
    if (!e.en || e.port != p || !e.uid) continue;
    CacheEnt& c = s_cache[i];
    if (c.lastPollMs && now - c.lastPollMs < e.periodMs) continue;
    rr[p] = i + 1;
    c.lastPollMs = now;

    static Job pj[NUM_PORTS];                     // trabajo interno de sondeo
    Job& j = pj[p];
    j = Job{};
    j.port = p;
    j.req[0] = e.uid; j.req[1] = e.fc;
    j.req[2] = e.addr >> 8; j.req[3] = e.addr & 0xFF;
    j.req[4] = e.qty >> 8;  j.req[5] = e.qty & 0xFF;
    j.reqLen = 6;
    gwStats.polls++;
    execJob(p, j);
    c.lastRes = j.res;
    if (j.res == RTU_OK && !(j.resp[1] & 0x80)) { c.okCount++; }    // execJob ya guardo el dato
    else {
      c.failCount++;
      c.exc = (j.res == RTU_OK) ? j.resp[2] : 0;
      s_pollUpdated |= (1UL << i);
    }
    return true;
  }
  return false;
}

// sondeo de fondo de esclavos caidos (sin esperar a que un cliente pregunte)
static void probeOfflineStep(uint8_t p) {
  uint32_t now = millis();
  for (auto& s : s_sl) {
    if (!s.used || s.port != p || !s.offline) continue;
    if (now - s.lastProbeMs < cfg.port[p].offlineRetryS * 1000UL) continue;
    s.lastProbeMs = now;
    uint8_t req[6] = { s.uid, 0x03, 0, 0, 0, 1 };
    uint8_t r[RTU_MAX_ADU]; size_t rl; uint32_t lat;
    RtuResult res = rtuPort[p].transact(req, 6, r, rl, sizeof(r), lat, 0, 0);
    recordResult(p, &s, res, lat, res == RTU_OK && (r[1] & 0x80));
    return;
  }
}

static void portTask(void* arg) {
  uint8_t p = (uint8_t)(uintptr_t)arg;
  for (;;) {
    if (s_reopen[p]) { s_reopen[p] = false; rtuPort[p].begin(); }
    bool toolHere = s_tool.kind && !s_tool.done && s_tool.port == p;
    uint8_t ji;
    if (xQueueReceive(s_q[p], &ji, toolHere ? 0 : pdMS_TO_TICKS(5)) == pdTRUE) {
      Job& j = s_jobs[ji];
      if (toolHere && s_tool.kind == TOOL_DETECT) {          // el puerto esta con otra velocidad
        j.res = RTU_OFFLINE; j.gwExc = 0x0A;
        jobFinish(j);
      } else {
        execJob(p, j);
      }
      continue;
    }
    if (toolHere) { toolStep(); continue; }
    if (!cfg.port[p].en || !rtuPort[p].isOpen()) continue;
    if (pollStep(p)) continue;
    probeOfflineStep(p);
  }
}

// ===========================================================================
// Clientes TCP
// ===========================================================================
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

uint8_t gwClientCount() { return s_clientCount; }
bool    gwListening()   { return s_listening; }

static bool allowed(uint32_t ip) {
  if (!cfg.allowEn) return true;
  for (auto& a : cfg.allow) {
    if (!a.ip) continue;
    uint32_t m = a.mask ? a.mask : 0xFFFFFFFFUL;
    if ((ip & m) == (a.ip & m)) return true;
  }
  return false;
}

static void closeSlot(int i) {
  Slot& s = s_slots[i];
  s.c.stop();
  s.used = false;
  s.n = 0;
  Job& j = s_jobs[i];
  portENTER_CRITICAL(&s_mux);
  if (j.state == J_QUEUED) j.orphan = true;       // la tarea del puerto lo liberara
  else j.state = J_FREE;
  portEXIT_CRITICAL(&s_mux);
  uint8_t cnt = 0;
  for (auto& x : s_slots) if (x.used) cnt++;
  s_clientCount = cnt;
}

static void sendAdu(Slot& s, uint16_t tid, uint8_t uid, const uint8_t* pdu, size_t pduLen) {
  uint8_t out[6 + RTU_MAX_ADU + 2];
  size_t n;
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
  gwTrace(TR_TCP_TX, 0xFF, out, n, nullptr);
}

// Envia la respuesta de un trabajo terminado
static void replyJob(Slot& s, Job& j) {
  uint8_t fc = j.req[1];
  uint8_t code = j.gwExc;
  if (!code) {
    switch (j.res) {
      case RTU_OK:    sendAdu(s, j.tid, j.cuid, j.resp + 1, j.respLen - 1); return;  // Unit ID del cliente
      case RTU_BCAST: return;                                                       // sin respuesta
      default:        code = 0x0B; break;       // Gateway Target Device Failed to Respond
    }
  }
  if (!cfg.sendExceptions) return;
  gwStats.gwExc++;
  uint8_t pdu[2] = { (uint8_t)(fc | 0x80), code };
  sendAdu(s, j.tid, j.cuid, pdu, 2);
}

// Lee datos del cliente y, si hay una peticion completa y su trabajo esta libre, la despacha
static bool serviceSlot(int i) {
  Slot& s = s_slots[i];
  Job& j = s_jobs[i];
  bool did = false;

  if (j.state == J_DONE) {
    replyJob(s, j);
    j.state = J_FREE;
    did = true;
  }
  if (!s.c.connected() && !s.c.available()) { closeSlot(i); return did; }

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
    if (cfg.idleTimeoutS && j.state == J_FREE && millis() - s.lastActMs > cfg.idleTimeoutS * 1000UL) {
      gwTrace(TR_EVENT, 0xFF, nullptr, 0, "cliente inactivo");
      closeSlot(i);
    }
    return did;
  }
  if (j.state != J_FREE) return did;            // una peticion en curso por cliente

  size_t frame = 0;
  uint16_t tid = 0;
  uint8_t  cuid;
  const uint8_t* pdu;
  size_t   pduLen;
  if (cfg.tcpProto == TCP_MBTCP) {
    if (s.n < 7) return did;
    uint16_t len = (s.buf[4] << 8) | s.buf[5];
    if (s.buf[2] || s.buf[3] || len < 2 || len > RTU_MAX_ADU - 2) {   // protocolo != 0 / largo ilegal
      gwTrace(TR_EVENT, 0xFF, s.buf, min(s.n, (size_t)12), "MBAP invalido");
      closeSlot(i);
      return did;
    }
    frame = 6 + len;
    if (s.n < frame) return did;
    tid = (s.buf[0] << 8) | s.buf[1];
    cuid = s.buf[6]; pdu = s.buf + 7; pduLen = len - 1;
  } else {
    size_t exp = rtuExpectedLen(s.buf, s.n, true);
    if (exp && s.n >= exp) frame = exp;
    else if (millis() - s.lastByteMs > 50) frame = s.n;   // funcion desconocida: fin por silencio
    else return did;
    if (!rtuCrcOk(s.buf, frame)) {
      gwStats.rtuBad++;
      gwTrace(TR_EVENT, 0xFF, s.buf, frame, "CRC TCP invalido");
      s.n = 0;
      return true;
    }
    cuid = s.buf[0]; pdu = s.buf + 1; pduLen = frame - 3;
  }
  gwTrace(TR_TCP_RX, 0xFF, s.buf, frame, nullptr);
  gwStats.tcpReq++;
  s.reqs++;

  if (pduLen >= 1 && pduLen <= RTU_MAX_ADU - 3) {
    j.state = J_BUSY; j.orphan = false;
    j.tid = tid;
    if (prepare(j, -1, cuid, pdu, pduLen, true)) enqueue(i);
  }
  memmove(s.buf, s.buf + frame, s.n - frame);
  s.n -= frame;
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
    gwTrace(TR_EVENT, 0xFF, nullptr, 0, "IP no permitida");
    nc.stop();
    return;
  }
  int fi = -1;
  uint8_t used = 0;
  for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
    if (s_slots[i].used) { used++; continue; }
    if (s_jobs[i].state == J_DONE) s_jobs[i].state = J_FREE;   // respuesta de un cliente que ya se fue
    if (fi < 0 && s_jobs[i].state == J_FREE) fi = i;
  }
  if (fi < 0 || used >= cfg.maxClients) {
    gwStats.rejected++;
    gwTrace(TR_EVENT, 0xFF, nullptr, 0, "sin cupo");
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
  Slot& f = s_slots[fi];
  f.c = nc;
  f.used = true;
  f.ip = ip;
  f.port = nc.remotePort();
  f.sinceMs = f.lastActMs = f.lastByteMs = millis();
  f.reqs = 0;
  f.n = 0;
  gwStats.connTotal++;
  s_clientCount = used + 1;
  gwTrace(TR_EVENT, 0xFF, nullptr, 0, "cliente conectado");
}

static void tcpTask(void*) {
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
      for (int i = 0; i < MAX_TCP_CLIENTS; i++)
        if (s_slots[i].used && serviceSlot(i)) busy = true;
    }
#if PIN_LED_ACT >= 0
    if (millis() - gwStats.lastActMs < 30) { digitalWrite(PIN_LED_ACT, HIGH); ledOffAt = millis() + 30; }
    else if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) { digitalWrite(PIN_LED_ACT, LOW); ledOffAt = 0; }
#endif
    if (!busy) vTaskDelay(1);
  }
}

// ===========================================================================
// Estado JSON
// ===========================================================================
void gwStatsReset() {
  gwStats = GwStats{};
  portENTER_CRITICAL(&s_mux);
  for (auto& s : s_sl) {
    if (!s.used) continue;
    s.req = s.ok = s.exc = s.tout = s.err = s.fast = 0;
  }
  portEXIT_CRITICAL(&s_mux);
}

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
  s["fast"]    = gwStats.fastFail;
  s["cache"]   = gwStats.cacheHits;
  s["polls"]   = gwStats.polls;
  s["rejected"]= gwStats.rejected;
  s["conn"]    = gwStats.connTotal;
  s["rx"]      = gwStats.bytesTcpRx;
  s["tx"]      = gwStats.bytesTcpTx;
  s["lat"]     = gwStats.lastLatencyMs;
  o["listening"] = (bool)s_listening;
  o["state"] = txStateStr(gwGlobalState());
  o["learn"] = (bool)s_learnEn;

  JsonArray pa = o["ports"].to<JsonArray>();
  for (uint8_t p = 0; p < NUM_PORTS; p++) {
    const PortCfg& c = cfg.port[p];
    JsonObject x = pa.add<JsonObject>();
    x["name"] = c.name;
    x["en"] = c.en;
    x["state"] = txStateStr(gwPortState(p));
    char fmt[24];
    snprintf(fmt, sizeof(fmt), "%lu %u%c%u", (unsigned long)c.baud, c.dataBits, c.parity, c.stopBits);
    x["fmt"] = fmt;
    x["gapUs"] = rtuPort[p].gapUs();
    x["queue"] = s_q[p] ? uxQueueMessagesWaiting(s_q[p]) : 0;
    x["cache"] = c.cacheEn;
  }

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
  for (uint8_t p = 0; p < NUM_PORTS; p++)
    for (int u = 1; u <= 247; u++)
      for (auto& x : s_sl) {
        if (!x.used || x.port != p || x.uid != u) continue;
        JsonObject e = sv.add<JsonObject>();
        e["port"] = p;
        e["uid"]  = u;
        e["state"]= txStateStr(slaveState(x));
        e["req"]  = x.req;
        e["ok"]   = x.ok;
        e["exc"]  = x.exc;
        e["tout"] = x.tout;
        e["err"]  = x.err;
        e["fast"] = x.fast;
        e["lat"]  = x.lastMs;
        e["ago"]  = x.lastSeenMs ? (long)((now - x.lastSeenMs) / 1000) : -1;
        if (x.ident[0]) e["ident"] = x.ident;
        break;
      }
}

void gwBegin() {
#if PIN_LED_ACT >= 0
  pinMode(PIN_LED_ACT, OUTPUT);
  digitalWrite(PIN_LED_ACT, LOW);
#endif
  gwStatsReset();
  for (uint8_t p = 0; p < NUM_PORTS; p++) {
    s_q[p] = xQueueCreate(MAX_TCP_CLIENTS + EXT_JOBS + 2, sizeof(uint8_t));
    char name[8]; snprintf(name, sizeof(name), "port%u", p + 1);
    xTaskCreatePinnedToCore(portTask, name, 6144, (void*)(uintptr_t)p, 4, nullptr, 1);
  }
  xTaskCreatePinnedToCore(tcpTask, "gwtcp", 6144, nullptr, 3, nullptr, 1);
}
