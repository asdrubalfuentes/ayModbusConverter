/**
 * ayModbusConverter  -  Aysafi
 *
 * Conversor transparente Modbus TCP <-> Modbus RTU (RS-485) sobre WT32-ETH01.
 *   - Servidor Modbus TCP (o RTU sobre TCP) en :502, multi-cliente.
 *   - Cada peticion se reenvia al bus RTU; el mapa de memoria es el del
 *     esclavo conectado (la pasarela no guarda registros propios).
 *   - Servicio por LAN (Ethernet, DHCP o IP fija); WiFi solo como AP con
 *     portal cautivo de configuracion.
 *   - Web de configuracion/diagnostico, OLED 128x64, OTA web y GitHub Releases.
 */
#include <Arduino.h>
#include "board.h"
#include "config.h"
#include "net.h"
#include "modbus_rtu.h"
#include "gateway.h"
#include "web.h"
#include "display.h"
#include "ota_update.h"
#include "fw_version.h"

// ---------------------------------------------------------------------------
// Boton: corto = pagina, 3 s = AP de configuracion, 10 s = fabrica
// ---------------------------------------------------------------------------
static void serviceButton() {
  static bool     down = false;
  static uint32_t t0 = 0;
  static uint8_t  stage = 0;
  bool pressed = digitalRead(PIN_BUTTON) == LOW;
  uint32_t now = millis();

  if (pressed && !down) { down = true; t0 = now; stage = 0; return; }
  if (pressed && down) {
    uint32_t held = now - t0;
    if (held > 3000 && stage == 0) { stage = 1; dispMessage("Boton", "Soltar: AP config", "Mantener: fabrica", nullptr, 0); }
    if (held > 10000 && stage == 1) { stage = 2; dispMessage("FABRICA", "Soltar para", "restaurar valores", nullptr, 0); }
    return;
  }
  if (!pressed && down) {
    down = false;
    uint32_t held = now - t0;
    if (held < 50) return;                              // rebote
    if (stage == 0) { dispNextPage(); return; }
    if (stage == 1) {
      netStartAp();
      dispMessage("AP activo", cfg.apSsid, "http://192.168.4.1", nullptr, 8000);
      return;
    }
    dispMessage("FABRICA", "Borrando config", "reiniciando...", nullptr, 0);
    dispLoop();
    cfgErase();
    delay(800);
    ESP.restart();
  }
}

// ---------------------------------------------------------------------------
// OTA GitHub
// ---------------------------------------------------------------------------
static void otaProgress(ota::Phase ph, int pct, const char* detail) {
  switch (ph) {
    case ota::Phase::Check:    webOtaState = "consultando"; break;
    case ota::Phase::UpToDate: webOtaState = "al dia"; break;
    case ota::Phase::Download: webOtaState = "descargando " + String(pct) + "%";
                               dispProgress("OTA GitHub", detail ? detail : "descargando", pct); break;
    case ota::Phase::Verify:   webOtaState = "verificando"; break;
    case ota::Phase::Flash:    webOtaState = "grabando"; break;
    case ota::Phase::Done:     webOtaState = "listo, reiniciando";
                               dispProgress("OTA GitHub", "reiniciando", 100); break;
    case ota::Phase::Error:    webOtaState = String("error: ") + (detail ? detail : ""); break;
  }
}

static void serviceOta() {
  static bool bootChecked = false;
  if (!bootChecked && cfg.otaAuto && netEthUp() && millis() > 15000) {
    bootChecked = true;
    webOtaReq = 2;
  }
  if (!webOtaReq) return;
  uint8_t req = webOtaReq;
  webOtaReq = 0;
  ota::Config oc;
  oc.owner = GITHUB_OWNER;
  oc.repo = GITHUB_REPO;
  oc.currentVersion = FW_SEMVER;
  ota::Result r = (req == 2) ? ota::run(oc, otaProgress) : ota::check(oc, otaProgress);
  if (r.latest[0]) webOtaLatest = r.latest;
  if (r.ok && r.hasUpdate && req == 1) webOtaState = String("disponible v") + r.latest;
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("\n[%s] v%s\n", PRODUCT_NAME, FW_SEMVER);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  cfgLoad();
  dispBegin();

  netBegin();
  rtuBegin();
  gwBegin();
  webBegin();

  Serial.printf("[cfg] %s  RS485 %lu %u%c%u  TCP:%u\n", cfg.devName, (unsigned long)cfg.baud,
                cfg.dataBits, cfg.parity, cfg.stopBits, cfg.tcpPort);
}

void loop() {
  netLoop();
  webLoop();
  serviceButton();
  dispLoop();
  serviceOta();

  if (webRebootAt && (int32_t)(millis() - webRebootAt) >= 0) {
    dispMessage("Reiniciando", "", "", nullptr, 0);
    dispLoop();
    delay(100);
    ESP.restart();
  }
  delay(2);
}
