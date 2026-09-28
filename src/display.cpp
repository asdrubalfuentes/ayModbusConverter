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
static const uint8_t PAGES = 4;

static char     s_msg[4][26];
static bool     s_msgOn = false;
static uint32_t s_msgUntil = 0;

static const char* PAR[] = { "N", "E", "O" };

static String serialStr() {
  const char* p = cfg.parity == 'E' ? PAR[1] : (cfg.parity == 'O' ? PAR[2] : PAR[0]);
  return String(cfg.baud) + " " + cfg.dataBits + p + cfg.stopBits;
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
    if (i == s_page) oled.fillRect(52 + i * 6, 62, 4, 2);
    else oled.drawHorizontalLine(52 + i * 6, 63, 4);
  }
}

static void pageNet() {
  header(cfg.devName);
  if (netEthLink()) {
    oled.drawString(0, 15, "ETH " + netEthIp());
    oled.drawString(0, 26, String(netEthSpeed()) + "M " + (netEthFullDuplex() ? "Full" : "Half") + (netEthUp() ? "" : " (sin IP)"));
  } else {
    oled.drawString(0, 15, "ETH sin cable");
  }
  if (netApUp()) {
    oled.drawString(0, 37, String("AP ") + cfg.apSsid);
    oled.drawString(0, 48, "http://" + netApIp());
  } else {
    oled.drawString(0, 37, String(cfg.hostname) + ".local");
  }
}

static void pageModbus() {
  header("Pasarela Modbus");
  oled.drawString(0, 15, String(cfg.tcpProto == TCP_MBTCP ? "Modbus TCP" : "RTU/TCP") + " :" + cfg.tcpPort);
  oled.drawString(0, 26, "RS485 " + serialStr());
  oled.drawString(0, 37, "Clientes " + String(gwClientCount()) + "/" + cfg.maxClients);
  oled.drawString(0, 48, cfg.uidMode == UID_FIXED ? "UID fijo " + String(cfg.fixedUid)
                       : cfg.uidMode == UID_MAP ? "UID mapa " + String(cfg.mapFrom) + "-" + cfg.mapTo + " " + (cfg.mapOffset >= 0 ? "+" : "") + cfg.mapOffset
                       : String("UID transparente"));
}

static void pageTraffic() {
  header("Trafico");
  oled.drawString(0, 15, "Pet " + String(gwStats.tcpReq));
  oled.drawString(64, 15, "OK " + String(gwStats.rtuOk));
  oled.drawString(0, 26, "T.out " + String(gwStats.rtuTimeout));
  oled.drawString(64, 26, "CRC " + String(gwStats.rtuCrc));
  oled.drawString(0, 37, "Exc " + String(gwStats.rtuExc));
  oled.drawString(64, 37, "Bc " + String(gwStats.bcast));
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
      case 0: pageNet(); break;
      case 1: pageModbus(); break;
      case 2: pageTraffic(); break;
      default: pageSystem(); break;
    }
  }
  oled.display();
}
