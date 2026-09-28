#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "modbus_rtu.h"

// ---------------------------------------------------------------------------
// Pasarela Modbus TCP -> puertos RTU.
//
// Tareas:
//   - TCP: acepta hasta cfg.maxClients conexiones, arma cada peticion como un
//     "trabajo" y la encola en el puerto que corresponde segun el Unit ID. Un
//     cliente tiene como maximo un trabajo en curso (el orden de las respuestas
//     se mantiene); los demas clientes siguen atendidos mientras tanto.
//   - una por puerto serie: ejecuta los trabajos de su cola, sondea la tabla de
//     cache cuando esta libre y corre las herramientas (escaneo, autodeteccion,
//     exploracion de registros).
// La web, la API REST y MQTT usan la misma cola via gwExecute().
// ---------------------------------------------------------------------------

enum TraceKind : uint8_t { TR_TCP_RX = 0, TR_RTU_TX, TR_RTU_RX, TR_TCP_TX, TR_EVENT };

// Estado de transmision (OLED / portal / MQTT)
enum TxState : uint8_t { TXS_IDLE = 0, TXS_OK, TXS_FAULT, TXS_DOWN, TXS_OFF };
const char* txStateStr(TxState s);        // "SIN TRAFICO" "OK" "FALLAS" "DESCONECTADO" "APAGADO"

struct GwStats {
  uint32_t tcpReq;        // peticiones recibidas (TCP + API + MQTT)
  uint32_t rtuOk;         // respuestas validas del bus
  uint32_t rtuExc;        // de ellas, excepciones Modbus del esclavo
  uint32_t rtuTimeout;
  uint32_t rtuCrc;
  uint32_t rtuBad;
  uint32_t bcast;
  uint32_t gwExc;         // excepciones generadas por la pasarela (0x01/0x0A/0x0B)
  uint32_t fastFail;      // respuestas inmediatas por esclavo caido
  uint32_t cacheHits;     // lecturas respondidas desde la cache
  uint32_t polls;         // lecturas de sondeo propias
  uint32_t rejected;      // conexiones rechazadas (lista blanca / cupo)
  uint32_t connTotal;
  uint32_t bytesTcpRx, bytesTcpTx;
  uint32_t lastLatencyMs;
  uint32_t lastActMs;     // millis() de la ultima transaccion (LED/OLED)
};
extern GwStats gwStats;

void    gwBegin();                         // crea las tareas (llamar tras rtuPort[i].begin())
uint8_t gwClientCount();
bool    gwListening();

// ---- estados ----
TxState gwPortState(uint8_t p);
TxState gwGlobalState();                   // peor estado entre los puertos habilitados
struct SlaveBrief { uint8_t port, uid; TxState st; };
int     gwSlaveList(SlaveBrief* out, int max);

// ---- ejecucion para la web / API / MQTT (bloqueante) ----
struct GwResult {
  RtuResult res;
  uint8_t   gwExc;                         // !=0: la pasarela respondio esta excepcion
  uint8_t   resp[RTU_MAX_ADU];             // [uid][fc][datos] sin CRC
  size_t    len;
  uint32_t  lat;
  bool      cached;
  uint32_t  ageMs;
  uint8_t   port;
};
// port < 0: enruta por Unit ID como una peticion TCP.  port >= 0: directo a ese puerto/esclavo.
bool gwExecute(int port, uint8_t uid, const uint8_t* pdu, size_t pduLen, GwResult& r,
               uint32_t waitMs, bool useCache);

// ---- monitor / estadisticas ----
void     gwTrace(TraceKind k, uint8_t port, const uint8_t* d, size_t n, const char* note);
void     gwMonitorEnable(bool en);
bool     gwMonitorEnabled();
void     gwMonitorClear();
void     gwStatsReset();
void     gwStatusJson(JsonObject o);
uint32_t gwMonitorJson(JsonArray a, uint32_t since);
void     gwPcap(Print& out);               // monitor como captura .pcap (Wireshark)

// ---- herramientas (una a la vez; las ejecuta la tarea del puerto) ----
bool gwScanStart(uint8_t port, uint8_t from, uint8_t to, uint16_t timeoutMs);
bool gwDetectStart(uint8_t port, bool passive, const char* ids);
bool gwDiscoverStart(uint8_t port, uint8_t uid, uint16_t step, uint16_t maxAddr);
void gwToolStop();
void gwToolJson(JsonObject o);
bool gwDetectApply();                      // guarda en cfg lo detectado y reabre el puerto
void gwReopenPort(uint8_t p);              // reabre el puerto con cfg.port[p] (sin reiniciar)

// ---- cache / aprendizaje ----
void gwTagsJson(JsonArray a);              // tabla de sondeo con sus valores
bool gwTagValues(int idx, JsonObject o);
void gwLearnEnable(bool en);
bool gwLearnEnabled();
void gwLearnClear();
void gwLearnJson(JsonArray a);
uint32_t gwPollUpdatedMask(bool clear);    // bits de entradas de sondeo actualizadas (MQTT)
