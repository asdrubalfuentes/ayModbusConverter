#include "display.h"
#include "board.h"
#include "config.h"
#include "net.h"
#include "gateway.h"
#include "fw_version.h"
#include "images.h"
#include <Wire.h>
#include <SSD1306Wire.h>

static SSD1306Wire oled(OLED_I2C_ADDR, PIN_OLED_SDA, PIN_OLED_SCL, GEOMETRY_128_64);
static bool     s_ok = false;
static bool     s_on = true;
static uint8_t  s_page = 0;
static uint32_t s_lastDraw = 0, s_lastWake = 0;
static const uint8_t PAGES = 6;
static uint8_t  s_lastKey = 0xFF;          // estados vistos (despierta la pantalla al cambiar)

static char     s_msg[4][26];
static bool     s_msgOn = false;
static uint32_t s_msgUntil = 0;

static String serialStr(uint8_t p) {
  const PortCfg& c = cfg.port[p];
  return String(c.baud) + " " + c.dataBits + c.parity + c.stopBits;
}

static const char* shortState(TxState s) {
  switch (s) {
    case TXS_OK:    return "OK";
    case TXS_FAULT: return "FALLAS";
    case TXS_DOWN:  return "DESCONECT.";
    case TXS_OFF:   return "apagado";
    default:        return "sin trafico";
  }
}

static String uptimeStr() {
  uint32_t s = millis() / 1000;
  char b[20];
  if (s >= 86400) snprintf(b, sizeof(b), "%lud %02luh", (unsigned long)(s / 86400), (unsigned long)((s % 86400) / 3600));
  else snprintf(b, sizeof(b), "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60), (unsigned long)(s % 60));
  return b;
}

void dispBegin() {
  if (!cfg.oledEn) return;
  s_ok = oled.init();
  if (!s_ok) { Serial.println("[oled] no detectado"); return; }
  if (cfg.oledFlip) oled.flipScreenVertically();
  oled.setContrast(cfg.oledContrast);
  oled.clear();
  oled.drawXbm(0, 4, aysafi_Logo_width, aysafi_Logo_height, aysafi_Logo_bits);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(56, 8, "ayModbus");
  oled.drawString(56, 20, "Converter");
  oled.drawString(56, 36, "RTU <> TCP");
  oled.drawString(56, 50, "v" FW_SEMVER);
  oled.display();
  s_lastWake = millis();
}

void dispWake() {
  s_lastWake = millis();
  if (s_ok && !s_on) { oled.displayOn(); s_on = true; }
}

void dispNextPage() {
  if (!s_on) { dispWake(); return; }
  dispWake();
  s_page = (s_page + 1) % PAGES;
  s_lastDraw = 0;
}

void dispMessage(const char* title, const char* l1, const char* l2, const char* l3, uint32_t holdMs) {
  const char* L[4] = { title, l1, l2, l3 };
  for (int i = 0; i < 4; i++) { strncpy(s_msg[i], L[i] ? L[i] : "", sizeof(s_msg[i]) - 1); s_msg[i][sizeof(s_msg[i]) - 1] = 0; }
  s_msgOn = true;
  s_msgUntil = holdMs ? millis() + holdMs : 0;
  dispWake();
  s_lastDraw = 0;
}

void dispClearMessage() { s_msgOn = false; s_lastDraw = 0; }

void dispProgress(const char* title, const char* detail, int pct) {
  if (!s_ok) return;
  dispWake();
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0, 0, title);
  oled.drawHorizontalLine(0, 12, 128);
  if (detail) oled.drawString(0, 18, detail);
  if (pct >= 0) oled.drawProgressBar(0, 40, 127, 10, (uint8_t)constrain(pct, 0, 100));
  oled.display();
}

// cabecera comun: nombre + indicadores de enlace y actividad
static void header(const char* title) {
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0, 0, title);
  String ind;
  if (netEthLink()) ind += "E";
  if (netApUp())    ind += "A";
  oled.setTextAlignment(TEXT_ALIGN_RIGHT);
  oled.drawString(118, 0, ind);
  // punto de actividad Modbus
  if (millis() - gwStats.lastActMs < 250) oled.fillCircle(124, 6, 3);
  else oled.drawCircle(124, 6, 3);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawHorizontalLine(0, 13, 128);
  // indicador de pagina
  for (uint8_t i = 0; i < PAGES; i++) {
    if (i == s_page) oled.fillRect(46 + i * 6, 62, 4, 2);
    else oled.drawHorizontalLine(46 + i * 6, 63, 4);
  }
}

// Estado grande: lo primero que se ve en el tablero
static void pageState() {
  header("Estado");
  const char* big;
  bool alarm;
  if (!netEthLink()) { big = "SIN LAN"; alarm = true; }
  else {
    TxState g = gwGlobalState();
    big = g == TXS_OK ? "OK" : g == TXS_FAULT ? "FALLAS" : g == TXS_DOWN ? "DESCONECTADO"
        : g == TXS_OFF ? "SIN PUERTOS" : "SIN TRAFICO";
    alarm = (g == TXS_FAULT || g == TXS_DOWN);
  }
  oled.setFont(ArialMT_Plain_16);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  if (alarm && (millis() / 500) % 2) {            // parpadeo invertido ante fallas
    oled.fillRect(0, 15, 128, 20);
    oled.setColor(BLACK);
    oled.drawString(64, 16, big);
    oled.setColor(WHITE);
  } else {
    oled.drawString(64, 16, big);
  }
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  int y = 37;
  for (uint8_t p = 0; p < NUM_PORTS; p++) {
    if (!cfg.port[p].en) continue;
    oled.drawString(0, y, String("P") + (p + 1) + " " + cfg.port[p].name);
    oled.setTextAlignment(TEXT_ALIGN_RIGHT);
    oled.drawString(127, y, shortState(gwPortState(p)));
    oled.setTextAlignment(TEXT_ALIGN_LEFT);
    y += 11;
  }
}

static void pageNet() {
  header(cfg.devName);
  if (netEthLink()) {
    oled.drawString(0, 15, "LAN " + netEthIp());
    oled.drawString(0, 26, String(netEthSpeed()) + "M " + (netEthFullDuplex() ? "Full" : "Half") + (netEthUp() ? "" : " (sin IP)"));
  } else {
    oled.drawString(0, 15, "LAN DESCONECTADA");
  }
  if (netApUp()) {
    oled.drawString(0, 37, String("AP ") + cfg.apSsid);
    oled.drawString(0, 48, "http://" + netApIp());
  } else {
    oled.drawString(0, 37, String(cfg.hostname) + ".local");
  }
}

static void pagePorts() {
  header("Puertos");
  oled.drawString(0, 15, String(cfg.tcpProto == TCP_MBTCP ? "Modbus TCP" : "RTU/TCP") + " :" + cfg.tcpPort +
                         "  cli " + gwClientCount() + "/" + cfg.maxClients);
  int y = 26;
  for (uint8_t p = 0; p < NUM_PORTS && y <= 48; p++) {
    const PortCfg& c = cfg.port[p];
    if (!c.en) { oled.drawString(0, y, String("P") + (p + 1) + " apagado"); y += 11; continue; }
    String ids = String(" ID") + c.uidFrom + "-" + c.uidTo;
    if (c.uidOffset) ids += String(c.uidOffset > 0 ? "+" : "") + c.uidOffset;
    oled.drawString(0, y, String("P") + (p + 1) + " " + serialStr(p) + ids + (c.cacheEn ? " C" : ""));
    y += 11;
  }
}

static void pageSlaves() {
  header("Esclavos");
  SlaveBrief sb[16];
  int n = gwSlaveList(sb, 16);
  if (!n) { oled.drawString(0, 26, "Sin trafico todavia"); return; }
  // de a 4 por pantalla, rota cada 3 s si hay mas
  int pages = (n + 3) / 4;
  int off = ((millis() / 3000) % pages) * 4;
  for (int i = 0; i < 4 && off + i < n; i++) {
    const SlaveBrief& e = sb[off + i];
    int y = 15 + i * 11;
    oled.drawString(0, y, String("P") + (e.port + 1) + " ID " + e.uid);
    oled.setTextAlignment(TEXT_ALIGN_RIGHT);
    oled.drawString(127, y, shortState(e.st));
    oled.setTextAlignment(TEXT_ALIGN_LEFT);
  }
}

static void pageTraffic() {
  header("Trafico");
  oled.drawString(0, 15, "Pet " + String(gwStats.tcpReq));
  oled.drawString(64, 15, "OK " + String(gwStats.rtuOk));
  oled.drawString(0, 26, "T.out " + String(gwStats.rtuTimeout));
  oled.drawString(64, 26, "CRC " + String(gwStats.rtuCrc));
  oled.drawString(0, 37, "Exc " + String(gwStats.rtuExc));
  oled.drawString(64, 37, "Cache " + String(gwStats.cacheHits));
  oled.drawString(0, 48, "Lat " + String(gwStats.lastLatencyMs) + " ms");
}

static void pageSystem() {
  header("Sistema");
  oled.drawString(0, 15, "FW v" FW_SEMVER);
  oled.drawString(0, 26, "Uptime " + uptimeStr());
  oled.drawString(0, 37, "Heap " + String(ESP.getFreeHeap() / 1024) + " kB");
  oled.drawString(0, 48, netEthMac());
}

void dispLoop() {
  if (!s_ok) return;
  uint32_t now = millis();
  if (s_msgOn && s_msgUntil && (int32_t)(now - s_msgUntil) >= 0) { s_msgOn = false; s_lastDraw = 0; }

  // un cambio de estado (LAN, puertos) despierta la pantalla y salta a la pagina de estado
  uint8_t key = netEthLink() ? 1 : 0;
  for (uint8_t p = 0; p < NUM_PORTS; p++) key |= (gwPortState(p) & 7) << (1 + 3 * p);
  if (key != s_lastKey) {
    bool changed = s_lastKey != 0xFF;
    s_lastKey = key;
    if (changed) { s_page = 0; dispWake(); }
  }

  // proteccion contra quemado del OLED
  if (s_on && cfg.oledSleepMin && !s_msgOn && now - s_lastWake > cfg.oledSleepMin * 60000UL) {
    oled.displayOff();
    s_on = false;
  }
  if (!s_on || now - s_lastDraw < 250) return;
  s_lastDraw = now;

  oled.clear();
  if (s_msgOn) {
    oled.setFont(ArialMT_Plain_10);
    oled.setTextAlignment(TEXT_ALIGN_CENTER);
    oled.drawString(64, 0, s_msg[0]);
    oled.drawHorizontalLine(0, 13, 128);
    for (int i = 1; i < 4; i++) oled.drawString(64, 4 + i * 14, s_msg[i]);
  } else {
    switch (s_page) {
      case 0: pageState(); break;
      case 1: pageNet(); break;
      case 2: pagePorts(); break;
      case 3: pageSlaves(); break;
      case 4: pageTraffic(); break;
      default: pageSystem(); break;
    }
  }
  oled.display();
}
