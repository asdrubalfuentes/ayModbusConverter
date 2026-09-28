#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Puente MQTT (opcional). Topicos bajo <base> (por defecto aysafi/<host>):
//   <base>/status          estado del equipo (retenido; LWT {"online":false})
//                          se publica cada mqttStatusS y al cambiar un estado
//   <base>/data/<nombre>   valores de cada entrada de sondeo marcada "MQTT"
//   <base>/cmd             (si mqttCmd) {"id":..,"port":1,"uid":1,"fc":3,"addr":0,"qty":10}
//                          o escrituras fc 5/6/15/16 con "value"/"values"
//   <base>/cmd/resp        respuesta a cada comando
// ---------------------------------------------------------------------------
void   mqttBegin();
void   mqttLoop();
bool   mqttConnected();
String mqttStateStr();
