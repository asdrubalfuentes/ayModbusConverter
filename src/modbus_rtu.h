#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Puerto serie Modbus RTU (maestro).
//
// Una instancia por puerto fisico. Cada puerto lo usa UNA sola tarea (la de
// ese puerto en gateway.cpp), asi que no lleva candados.
//
// La direccion del transceptor (DE//RE) la maneja el hardware de la UART en
// modo RS485 half-duplex (RTS = pin DE), sin retardos por software.
// El fin de trama se detecta por longitud esperada segun el codigo de funcion
// (respuesta inmediata) y, para funciones desconocidas, por silencio t3.5.
// ---------------------------------------------------------------------------

#define RTU_MAX_ADU 256

enum RtuResult : uint8_t {
  RTU_OK = 0,
  RTU_TIMEOUT,        // el esclavo no respondio
  RTU_CRC,            // respuesta con CRC invalido
  RTU_BADFRAME,       // respuesta demasiado corta / Unit ID o funcion no coinciden
  RTU_BCAST,          // broadcast enviado (sin respuesta por definicion)
  RTU_OFFLINE,        // no se envio: esclavo marcado como caido (respuesta inmediata)
};

class RtuPort {
 public:
  RtuPort(uint8_t idx, HardwareSerial& ser, int8_t rx, int8_t tx, int8_t de)
      : idx_(idx), ser_(ser), rx_(rx), tx_(tx), de_(de) {}

  // Abre el puerto con la configuracion de cfg.port[idx]
  void begin();
  // Reabre con otros parametros (autodeteccion). charTimeoutMs 0 = auto.
  void open(uint32_t baud, uint8_t dataBits, char parity, uint8_t stopBits, uint16_t charTimeoutMs = 0);
  void close();
  bool isOpen() const { return open_; }

  // Transaccion completa. req/resp: [uid][fc][datos] SIN CRC.
  // timeoutMs = 0 / retries = -1 -> valores de cfg.port[idx].
  RtuResult transact(const uint8_t* req, size_t reqLen, uint8_t* resp, size_t& respLen,
                     size_t respMax, uint32_t& latencyMs, uint16_t timeoutMs = 0, int8_t retries = -1);

  // Escucha pasiva: devuelve una trama completa (con CRC) o 0 si no hubo nada en waitMs
  size_t sniff(uint8_t* buf, size_t max, uint32_t waitMs);

  uint32_t gapUs() const { return gapUs_; }
  uint8_t  index() const { return idx_; }
  uint32_t curBaud() const { return baud_; }

 private:
  size_t readFrame(uint8_t* buf, size_t max, uint32_t firstMs, bool request);

  uint8_t         idx_;
  HardwareSerial& ser_;
  int8_t          rx_, tx_, de_;
  bool            open_ = false;
  uint32_t        baud_ = 0;
  uint32_t        gapUs_ = 2000;
  uint32_t        lastEndMs_ = 0;
};

extern RtuPort rtuPort[];

uint16_t    rtuCrc(const uint8_t* d, size_t n);
bool        rtuCrcOk(const uint8_t* d, size_t n);   // n incluye el CRC
size_t      rtuExpectedLen(const uint8_t* b, size_t n, bool request);
const char* rtuResultStr(RtuResult r);
