#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Configuracion persistente.
//
// Se guarda en NVS como JSON (no como blob binario del struct): agregar o
// quitar campos en futuras versiones NO borra la configuracion de los equipos
// en campo; los campos nuevos toman su valor de fabrica y el resto se conserva.
// El mismo JSON es el formato de la API web y del respaldo/restauracion.
// ---------------------------------------------------------------------------

// Funcionamiento base: servidor Modbus TCP TRANSPARENTE. Cada peticion de un
// cliente TCP (SCADA/HMI/PLC) se enruta por Unit ID a un puerto serie y la
// respuesta del esclavo vuelve sin tocar. Opcionalmente el conversor sondea
// por su cuenta (modo cache/agente) y responde las lecturas desde memoria.

#define NUM_PORTS       2   // 0 = RS-485 (UART2)   1 = RS-232 / RS-485 autodireccional (UART1)

// Protocolo del lado TCP
#define TCP_MBTCP       0   // Modbus TCP estandar (cabecera MBAP)
#define TCP_RTU_OVER    1   // Modbus RTU sobre TCP (trama RTU con CRC encapsulada)

// Tratamiento del Unit ID
#define UID_TRANSPARENT 0   // se enruta por los rangos de Unit ID de cada puerto
#define UID_FIXED       1   // todas las peticiones van al esclavo fijo del puerto 1

// WiFi = SOLO punto de acceso con portal cautivo de configuracion.
// El servicio Modbus TCP va por la LAN (Ethernet).
#define AP_AUTO         0   // solo si no hay LAN a los 30 s, o con el boton
#define AP_ALWAYS       1
#define AP_BUTTON       2   // solo con el boton (3 s)

#define MAX_ALLOW       4
#define MAX_TCP_CLIENTS 8
#define MAX_POLLS       24
#define MAX_RO          8

struct IpCfg {
  bool     dhcp;
  uint32_t ip, mask, gw, dns1, dns2;    // formato IPAddress (uint32 little-endian)
};

struct AllowRule {
  uint32_t ip, mask;
};

struct PortCfg {
  bool     en;
  char     name[16];
  uint32_t baud;
  uint8_t  dataBits;                   // 7 / 8
  char     parity;                     // 'N' 'E' 'O'
  uint8_t  stopBits;                   // 1 / 2
  uint16_t respTimeoutMs;              // espera de la respuesta del esclavo
  uint16_t charTimeoutMs;              // silencio de fin de trama; 0 = auto (t3.5)
  uint16_t turnaroundMs;               // pausa minima entre peticiones al bus
  uint16_t bcastDelayMs;               // pausa tras un broadcast (Unit ID 0)
  uint8_t  retries;                    // reintentos ante timeout / CRC
  // enrutamiento: Unit ID TCP [uidFrom..uidTo] -> este puerto, esclavo = UID + uidOffset
  uint8_t  uidFrom, uidTo;
  int16_t  uidOffset;
  // esclavos caidos: tras offlineAfter timeouts seguidos se responde 0x0B al
  // instante (sin ocupar el bus) y se reintenta cada offlineRetryS
  uint8_t  offlineAfter;               // 0 = desactivado
  uint16_t offlineRetryS;
  // cache / agente
  bool     cacheEn;                    // responder lecturas desde la tabla de sondeo
  uint16_t cacheMaxAgeMs;              // antiguedad maxima aceptable de un dato
};

// Entrada de la tabla de sondeo (modo cache/agente, MQTT, API REST /api/tags)
struct PollEntry {
  bool     en;
  uint8_t  port;                       // 0..NUM_PORTS-1
  uint8_t  uid;                        // esclavo RTU real
  uint8_t  fc;                         // 1..4
  uint16_t addr, qty;
  uint16_t periodMs;
  bool     mqtt;                       // publicar por MQTT
  char     name[20];
};

// Regla de solo lectura: escrituras a esos esclavos/direcciones -> excepcion 0x01
struct RoRule {
  bool     en;
  uint8_t  port;
  uint8_t  uidFrom, uidTo;
  uint16_t addrFrom, addrTo;
};

struct AppConfig {
  // --- sistema ---
  char     devName[32];
  char     adminUser[16];
  char     adminPass[32];
  char     apiKey[40];                 // alternativa a HTTP Basic para /api/read|write|tags (vacia = off)
  bool     wizardDone;                 // el asistente de puesta en marcha ya se completo
  bool     oledEn;
  bool     oledFlip;
  uint8_t  oledSleepMin;               // 0 = siempre encendida
  uint8_t  oledContrast;               // 0..255
  bool     otaAuto;                    // buscar actualizacion en GitHub al arrancar
  char     ntpServer[48];
  char     tz[48];                     // TZ POSIX

  // --- red LAN (servicio) ---
  char     hostname[32];
  bool     mdns;
  IpCfg    eth;
  // --- WiFi: punto de acceso + portal cautivo (configuracion) ---
  uint8_t  apMode;                     // AP_AUTO / AP_ALWAYS / AP_BUTTON
  char     apSsid[33];
  char     apPass[65];
  uint8_t  apChannel;

  // --- puertos serie ---
  PortCfg  port[NUM_PORTS];
  uint16_t initDelayMs;                // retardo tras el arranque antes de atender (esclavos lentos)

  // --- servidor Modbus TCP ---
  uint8_t  tcpProto;                   // TCP_MBTCP / TCP_RTU_OVER
  uint16_t tcpPort;                    // puerto del servidor (502)
  uint8_t  maxClients;                 // 1..MAX_TCP_CLIENTS
  uint16_t idleTimeoutS;               // cierra conexiones inactivas; 0 = nunca
  bool     sendExceptions;             // responder 0x0A/0x0B ante fallos de la pasarela
  uint8_t  uidMode;                    // UID_TRANSPARENT / UID_FIXED
  uint8_t  fixedUid;                   // destino con UID_FIXED y para Unit ID 255
  bool     uid0Broadcast;              // Unit ID 0 = broadcast (true) o -> fixedUid (false)
  uint16_t tcpAliveS;                  // TCP keep-alive: detecta clientes caidos; 0 = off
  bool     allowEn;                    // lista blanca de clientes
  AllowRule allow[MAX_ALLOW];

  // --- sondeo / cache / solo lectura ---
  PollEntry poll[MAX_POLLS];
  RoRule    ro[MAX_RO];

  // --- MQTT ---
  bool     mqttEn;
  char     mqttHost[64];
  uint16_t mqttPort;
  bool     mqttTls;
  char     mqttUser[32];
  char     mqttPass[64];
  char     mqttBase[64];               // raiz de topicos (vacio = aysafi/<host>)
  uint16_t mqttStatusS;                // periodo del topico de estado
  bool     mqttRetain;
  bool     mqttCmd;                    // aceptar lecturas/escrituras por <base>/cmd
};

extern AppConfig cfg;

void cfgFactory();
void cfgLoad();
bool cfgSave();
void cfgErase();

// secrets=false -> las claves se envian vacias (API);  true -> respaldo completo
void cfgToJson(JsonDocument& doc, bool secrets);
// Mezcla parcial: solo aplica las claves presentes. Una clave vacia NO borra
// la clave guardada (la UI nunca recibe las claves reales).
void cfgFromJson(JsonVariantConst root);

String   ipStr(uint32_t v);
uint32_t strIp(const char* s);
String   chipId();                     // 6 hex (ultimos 3 bytes de la MAC)
bool     cfgDefaultPassword();
String   mqttBaseTopic();
uint32_t serialConfigOf(uint8_t dataBits, char parity, uint8_t stopBits);
