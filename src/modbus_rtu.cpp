#include "modbus_rtu.h"
#include "config.h"
#include "board.h"
#include "gateway.h"

RtuPort rtuPort[NUM_PORTS] = {
  RtuPort(0, Serial2, PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_DE),
  RtuPort(1, Serial1, PIN_P2_RX, PIN_P2_TX, PIN_P2_DE),
};

void RtuPort::begin() {
  const PortCfg& c = cfg.port[idx_];
  if (!c.en) { close(); return; }
  open(c.baud, c.dataBits, c.parity, c.stopBits, c.charTimeoutMs);
}

void RtuPort::open(uint32_t baud, uint8_t dataBits, char parity, uint8_t stopBits, uint16_t charTimeoutMs) {
  if (!baud) baud = 9600;
  // bits por caracter: start + datos + paridad + stop
  uint32_t bits = 1 + dataBits + (parity == 'N' ? 0 : 1) + stopBits;
  uint32_t charUs = (bits * 1000000UL) / baud;
  if (charTimeoutMs) {
    gapUs_ = (uint32_t)charTimeoutMs * 1000UL;
  } else {
    // t3.5 del estandar (1750 us fijo por encima de 19200) con margen para la
    // latencia del driver de la UART y equipos que no respetan el silencio.
    uint32_t t35 = (baud > 19200) ? 1750 : (charUs * 35) / 10;
    gapUs_ = max<uint32_t>(t35 + charUs, 2000);
  }

  if (open_) ser_.end();
  ser_.setRxBufferSize(1024);
  ser_.begin(baud, serialConfigOf(dataBits, parity, stopBits), rx_, tx_);
  if (de_ >= 0) {
    ser_.setPins(rx_, tx_, -1, de_);
    ser_.setMode(UART_MODE_RS485_HALF_DUPLEX);   // DE = RTS, activo durante TX
  }
  ser_.setRxTimeout(1);   // entrega los bytes al driver tras 1 caracter de silencio
  while (ser_.available()) ser_.read();
  baud_ = baud;
  open_ = true;
}

void RtuPort::close() {
  if (open_) ser_.end();
  open_ = false;
}

uint16_t rtuCrc(const uint8_t* d, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= *d++;
    for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

bool rtuCrcOk(const uint8_t* d, size_t n) {
  if (n < 4) return false;
  uint16_t c = rtuCrc(d, n - 2);
  return (c & 0xFF) == d[n - 2] && (c >> 8) == d[n - 1];
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
      case 0x2B:
        return 7;                                  // MEI 0x0E: uid fc 0E code obj crc crc
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
      return 0;                                    // 0x2B y otras: fin por silencio
  }
}

size_t RtuPort::readFrame(uint8_t* buf, size_t max, uint32_t firstMs, bool request) {
  uint32_t t0 = millis();
  while (!ser_.available()) {
    if (millis() - t0 >= firstMs) return 0;
    vTaskDelay(1);
  }
  size_t n = 0;
  uint32_t last = micros();
  for (;;) {
    while (ser_.available()) {
      int c = ser_.read();
      if (n < max) buf[n++] = (uint8_t)c;
      last = micros();
    }
    size_t exp = rtuExpectedLen(buf, n, request);
    if (exp && n >= exp) break;
    if (n >= max) break;
    if (micros() - last > gapUs_) break;
    vTaskDelay(1);
  }
  return n;
}

size_t RtuPort::sniff(uint8_t* buf, size_t max, uint32_t waitMs) {
  if (!open_) return 0;
  // en escucha pasiva no se sabe si es peticion o respuesta: se corta por silencio
  uint32_t t0 = millis();
  while (!ser_.available()) {
    if (millis() - t0 >= waitMs) return 0;
    vTaskDelay(1);
  }
  size_t n = 0;
  uint32_t last = micros();
  for (;;) {
    while (ser_.available()) {
      int c = ser_.read();
      if (n < max) buf[n++] = (uint8_t)c;
      last = micros();
    }
    if (n >= max || micros() - last > gapUs_) break;
    vTaskDelay(1);
  }
  return n;
}

RtuResult RtuPort::transact(const uint8_t* req, size_t reqLen, uint8_t* resp, size_t& respLen,
                            size_t respMax, uint32_t& latencyMs, uint16_t timeoutMs, int8_t retries) {
  const PortCfg& c = cfg.port[idx_];
  respLen = 0;
  latencyMs = 0;
  if (!open_ || reqLen < 2 || reqLen > RTU_MAX_ADU - 2) return RTU_BADFRAME;
  if (!timeoutMs) timeoutMs = c.respTimeoutMs;

  uint8_t tx[RTU_MAX_ADU];
  memcpy(tx, req, reqLen);
  uint16_t crc = rtuCrc(tx, reqLen);
  tx[reqLen]     = crc & 0xFF;
  tx[reqLen + 1] = crc >> 8;
  size_t txLen = reqLen + 2;
  bool bcast = (req[0] == 0);
  uint8_t maxTry = bcast ? 0 : (retries < 0 ? c.retries : (uint8_t)retries);

  RtuResult res = RTU_TIMEOUT;
  uint8_t rx[RTU_MAX_ADU];

  for (uint8_t attempt = 0; attempt <= maxTry; attempt++) {
    // turnaround: silencio minimo entre transacciones del bus
    while (millis() - lastEndMs_ < c.turnaroundMs) vTaskDelay(1);
    while (ser_.available()) ser_.read();          // descarta basura previa

    ser_.write(tx, txLen);
    ser_.flush();                                  // espera a que salga el ultimo bit
    gwTrace(TR_RTU_TX, idx_, tx, txLen, attempt ? "reintento" : nullptr);
    uint32_t tSent = millis();

    if (bcast) {
      vTaskDelay(pdMS_TO_TICKS(c.bcastDelayMs));
      lastEndMs_ = millis();
      return RTU_BCAST;
    }

    size_t n = readFrame(rx, sizeof(rx), timeoutMs, false);
    lastEndMs_ = millis();
    latencyMs = lastEndMs_ - tSent;
    if (!n) { res = RTU_TIMEOUT; gwTrace(TR_EVENT, idx_, nullptr, 0, "timeout"); continue; }
    gwTrace(TR_RTU_RX, idx_, rx, n, nullptr);

    if (n < 4) { res = RTU_BADFRAME; continue; }
    if (!rtuCrcOk(rx, n)) { res = RTU_CRC; gwTrace(TR_EVENT, idx_, nullptr, 0, "CRC invalido"); continue; }
    if (rx[0] != req[0] || (rx[1] & 0x7F) != req[1]) { res = RTU_BADFRAME; gwTrace(TR_EVENT, idx_, nullptr, 0, "trama ajena"); continue; }

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
    case RTU_OFFLINE:  return "esclavo caido";
  }
  return "?";
}
