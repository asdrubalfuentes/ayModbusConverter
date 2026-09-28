#include "modbus_rtu.h"
#include "config.h"
#include "board.h"
#include "gateway.h"
#include <freertos/semphr.h>

static HardwareSerial& port = Serial2;
static SemaphoreHandle_t s_mtx = nullptr;
static uint32_t s_gapUs = 2000;
static uint32_t s_lastEndMs = 0;     // fin de la ultima transaccion (turnaround)

static uint32_t serialConfig() {
  // SERIAL_<bits><parity><stop>
  static const uint32_t T[2][3][2] = {
    // 7 bits: N1 N2 / E1 E2 / O1 O2
    { { SERIAL_7N1, SERIAL_7N2 }, { SERIAL_7E1, SERIAL_7E2 }, { SERIAL_7O1, SERIAL_7O2 } },
    { { SERIAL_8N1, SERIAL_8N2 }, { SERIAL_8E1, SERIAL_8E2 }, { SERIAL_8O1, SERIAL_8O2 } },
  };
  int b = cfg.dataBits == 7 ? 0 : 1;
  int p = cfg.parity == 'E' ? 1 : (cfg.parity == 'O' ? 2 : 0);
  int s = cfg.stopBits == 2 ? 1 : 0;
  return T[b][p][s];
}

void rtuBegin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();

  // bits por caracter: start + datos + paridad + stop
  uint32_t bits = 1 + cfg.dataBits + (cfg.parity == 'N' ? 0 : 1) + cfg.stopBits;
  uint32_t charUs = (bits * 1000000UL) / (cfg.baud ? cfg.baud : 9600);
  if (cfg.charTimeoutMs) {
    s_gapUs = (uint32_t)cfg.charTimeoutMs * 1000UL;
  } else {
    // t3.5 del estandar (1750 us fijo por encima de 19200) con margen para la
    // latencia del driver de la UART y equipos que no respetan el silencio.
    uint32_t t35 = (cfg.baud > 19200) ? 1750 : (charUs * 35) / 10;
    s_gapUs = max<uint32_t>(t35 + charUs, 2000);
  }

  port.end();
  port.setRxBufferSize(1024);
  port.begin(cfg.baud, serialConfig(), PIN_RS485_RX, PIN_RS485_TX);
  if (PIN_RS485_DE >= 0) {
    port.setPins(PIN_RS485_RX, PIN_RS485_TX, -1, PIN_RS485_DE);
    port.setMode(UART_MODE_RS485_HALF_DUPLEX);   // DE = RTS, activo durante TX
  }
  port.setRxTimeout(1);   // entrega los bytes al driver tras 1 caracter de silencio
  while (port.available()) port.read();
}

uint32_t rtuGapUs() { return s_gapUs; }

bool rtuLock(uint32_t waitMs) {
  return s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
void rtuUnlock() { if (s_mtx) xSemaphoreGive(s_mtx); }

uint16_t rtuCrc(const uint8_t* d, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= *d++;
    for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

size_t rtuExpectedLen(const uint8_t* b, size_t n, bool request) {
  if (n < 2) return 0;
  uint8_t fc = b[1];
  if (request) {
    switch (fc) {
      case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x08:
        return 8;
      case 0x07: case 0x0B: case 0x0C: case 0x11:
        return 4;
      case 0x0F: case 0x10:
        return n >= 7 ? 9 + b[6] : 0;
      case 0x14: case 0x15:
        return n >= 3 ? 5 + b[2] : 0;
      case 0x16:
        return 10;
      case 0x17:
        return n >= 11 ? 13 + b[10] : 0;
      default:
        return 0;
    }
  }
  if (fc & 0x80) return 5;
  switch (fc) {
    case 0x01: case 0x02: case 0x03: case 0x04:
    case 0x0C: case 0x11: case 0x14: case 0x15: case 0x17:
      return n >= 3 ? 5 + b[2] : 0;
    case 0x05: case 0x06: case 0x08: case 0x0B: case 0x0F: case 0x10:
      return 8;
    case 0x07:
      return 5;
    case 0x16:
      return 10;
    default:
      return 0;
  }
}

// Lee una trama. 0 si no llego nada dentro de firstMs.
static size_t readFrame(uint8_t* buf, size_t max, uint32_t firstMs) {
  uint32_t t0 = millis();
  while (!port.available()) {
    if (millis() - t0 >= firstMs) return 0;
    vTaskDelay(1);
  }
  size_t n = 0;
  uint32_t last = micros();
  for (;;) {
    while (port.available()) {
      int c = port.read();
      if (n < max) buf[n++] = (uint8_t)c;
      last = micros();
    }
    size_t exp = rtuExpectedLen(buf, n, false);
    if (exp && n >= exp) break;
    if (n >= max) break;
    if (micros() - last > s_gapUs) break;
    vTaskDelay(1);
  }
  return n;
}

RtuResult rtuTransact(const uint8_t* req, size_t reqLen,
                      uint8_t* resp, size_t& respLen, size_t respMax,
                      uint32_t& latencyMs, uint16_t timeoutMs, int8_t retries) {
  respLen = 0;
  latencyMs = 0;
  if (reqLen < 2 || reqLen > RTU_MAX_ADU - 2) return RTU_BADFRAME;
  if (!timeoutMs) timeoutMs = cfg.respTimeoutMs;

  uint8_t tx[RTU_MAX_ADU];
  memcpy(tx, req, reqLen);
  uint16_t crc = rtuCrc(tx, reqLen);
  tx[reqLen]     = crc & 0xFF;
  tx[reqLen + 1] = crc >> 8;
  size_t txLen = reqLen + 2;
  bool bcast = (req[0] == 0);
  uint8_t maxTry = bcast ? 0 : (retries < 0 ? cfg.retries : (uint8_t)retries);

  RtuResult res = RTU_TIMEOUT;
  uint8_t rx[RTU_MAX_ADU];

  for (uint8_t attempt = 0; attempt <= maxTry; attempt++) {
    // turnaround: silencio minimo entre transacciones del bus
    while (millis() - s_lastEndMs < cfg.turnaroundMs) vTaskDelay(1);
    while (port.available()) port.read();          // descarta basura previa

    port.write(tx, txLen);
    port.flush();                                  // espera a que salga el ultimo bit
    gwTrace(TR_RTU_TX, tx, txLen, attempt ? "reintento" : nullptr);
    uint32_t tSent = millis();

    if (bcast) {
      vTaskDelay(pdMS_TO_TICKS(cfg.bcastDelayMs));
      s_lastEndMs = millis();
      return RTU_BCAST;
    }

    size_t n = readFrame(rx, sizeof(rx), timeoutMs);
    s_lastEndMs = millis();
    latencyMs = s_lastEndMs - tSent;
    if (!n) { res = RTU_TIMEOUT; gwTrace(TR_EVENT, nullptr, 0, "timeout"); continue; }
    gwTrace(TR_RTU_RX, rx, n, nullptr);

    if (n < 4) { res = RTU_BADFRAME; continue; }
    uint16_t c = rtuCrc(rx, n - 2);
    if ((c & 0xFF) != rx[n - 2] || (c >> 8) != rx[n - 1]) { res = RTU_CRC; gwTrace(TR_EVENT, nullptr, 0, "CRC invalido"); continue; }
    if (rx[0] != req[0] || (rx[1] & 0x7F) != req[1]) { res = RTU_BADFRAME; gwTrace(TR_EVENT, nullptr, 0, "trama ajena"); continue; }

    respLen = min(n - 2, respMax);
    memcpy(resp, rx, respLen);
    return RTU_OK;
  }
  return res;
}

const char* rtuResultStr(RtuResult r) {
  switch (r) {
    case RTU_OK:       return "OK";
    case RTU_TIMEOUT:  return "timeout";
    case RTU_CRC:      return "CRC";
    case RTU_BADFRAME: return "trama invalida";
    case RTU_BCAST:    return "broadcast";
  }
  return "?";
}
