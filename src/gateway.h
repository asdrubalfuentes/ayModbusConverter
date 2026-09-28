#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Pasarela transparente: servidor Modbus TCP (o RTU sobre TCP) -> bus RTU.
//
// Corre en su propia tarea FreeRTOS. Atiende hasta cfg.maxClients conexiones
// en round-robin (una peticion por cliente por vuelta, el bus es half-duplex)
// y reenvia cada PDU al esclavo RTU; la respuesta vuelve sin modificar con la
// cabecera MBAP (Transaction ID / Unit ID) del cliente original.
// ---------------------------------------------------------------------------

// tipos de traza del monitor de trafico
enum TraceKind : uint8_t { TR_TCP_RX = 0, TR_RTU_TX, TR_RTU_RX, TR_TCP_TX, TR_EVENT };

struct GwStats {
  uint32_t tcpReq;        // peticiones recibidas por TCP
  uint32_t rtuOk;         // respuestas validas del bus
  uint32_t rtuExc;        // de ellas, excepciones Modbus del esclavo
  uint32_t rtuTimeout;
  uint32_t rtuCrc;
  uint32_t rtuBad;
  uint32_t bcast;
  uint32_t gwExc;         // excepciones generadas por la pasarela (0x0A/0x0B)
  uint32_t rejected;      // conexiones rechazadas (lista blanca / cupo)
  uint32_t connTotal;
  uint32_t bytesTcpRx, bytesTcpTx;
  uint32_t lastLatencyMs;
  uint32_t lastActMs;     // millis() de la ultima transaccion (LED/OLED)
};

extern GwStats gwStats;

void gwBegin();                 // crea la tarea (llamar tras rtuBegin)
uint8_t gwClientCount();
bool    gwListening();

void gwTrace(TraceKind k, const uint8_t* d, size_t n, const char* note);
void gwMonitorEnable(bool en);
bool gwMonitorEnabled();
void gwMonitorClear();
void gwStatsReset();

void gwStatusJson(JsonObject o);                 // contadores + clientes + esclavos
uint32_t gwMonitorJson(JsonArray a, uint32_t since);  // devuelve el ultimo seq

// Escaneo del bus (lo ejecuta la tarea de la pasarela entre peticiones)
bool gwScanStart(uint8_t from, uint8_t to, uint16_t timeoutMs);
void gwScanStop();
void gwScanJson(JsonObject o);
