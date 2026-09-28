#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Maestro Modbus RTU sobre RS-485 (UART2).
//
// La direccion del transceptor (DE//RE) la maneja el hardware de la UART en
// modo RS485 half-duplex (RTS = PIN_RS485_DE), sin retardos por software.
// El fin de trama se detecta por longitud esperada segun el codigo de funcion
// (respuesta inmediata) y, para funciones desconocidas, por silencio t3.5.
//
// Todas las funciones son de un solo hilo: el acceso concurrente (tarea de la
// pasarela + herramientas de diagnostico de la web) se serializa con
// rtuLock()/rtuUnlock().
// ---------------------------------------------------------------------------

#define RTU_MAX_ADU 256

enum RtuResult : uint8_t {
  RTU_OK = 0,
  RTU_TIMEOUT,        // el esclavo no respondio
  RTU_CRC,            // respuesta con CRC invalido
  RTU_BADFRAME,       // respuesta demasiado corta / Unit ID o funcion no coinciden
  RTU_BCAST,          // broadcast enviado (sin respuesta por definicion)
};

void     rtuBegin();                 // abre UART2 con la configuracion de cfg
uint16_t rtuCrc(const uint8_t* d, size_t n);
uint32_t rtuGapUs();                 // silencio de fin de trama efectivo

bool     rtuLock(uint32_t waitMs);
void     rtuUnlock();

// Transaccion completa (con reintentos y turnaround de cfg).
//   req: [uid][fc][datos...]  SIN CRC.   resp: idem (sin CRC).
// latencyMs: tiempo desde el fin del envio hasta el fin de la respuesta.
// timeoutMs = 0 / retries = -1 -> valores de cfg.
RtuResult rtuTransact(const uint8_t* req, size_t reqLen,
                      uint8_t* resp, size_t& respLen, size_t respMax,
                      uint32_t& latencyMs, uint16_t timeoutMs = 0, int8_t retries = -1);

// Longitud esperada de una trama RTU (con CRC) a partir de sus primeros bytes.
// 0 = aun no se puede saber / funcion desconocida.
size_t   rtuExpectedLen(const uint8_t* b, size_t n, bool request);

const char* rtuResultStr(RtuResult r);
