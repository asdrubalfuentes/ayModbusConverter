#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Red:
//   - LAN (Ethernet LAN8720): interfaz de SERVICIO. DHCP o IP fija con
//     mascara, puerta de enlace y DNS. Por aqui se sirve Modbus TCP y la web.
//   - WiFi: SOLO punto de acceso con portal cautivo de configuracion (DNS
//     comodin -> la web se abre sola al conectarse desde el telefono/PC).
// ---------------------------------------------------------------------------

void   netBegin();
void   netLoop();

bool   netEthLink();          // cable conectado
bool   netEthUp();            // LAN con IP
bool   netApUp();
bool   netAnyUp();            // LAN con IP o AP activo
void   netStartAp();          // fuerza el AP (boton 3 s)

String netEthIp();
String netEthMask();
String netEthGw();
String netEthDns();
String netEthMac();
int    netEthSpeed();         // Mbps (0 sin enlace)
bool   netEthFullDuplex();
String netApIp();
uint8_t netApStations();
String netPrimaryIp();

bool   netIsApClient(uint32_t ip);   // la peticion viene de la red del AP
bool   netTimeOk();
