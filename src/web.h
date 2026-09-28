#pragma once
#include <Arduino.h>

// Servidor web de configuracion (puerto 80, en todas las interfaces).
// UI embebida + API REST JSON protegida con HTTP Basic (usuario/clave de cfg).
void webBegin();
void webLoop();

// Peticiones que la web deja para el loop principal
extern volatile uint32_t webRebootAt;     // millis() en que reiniciar (0 = no)
extern volatile uint8_t  webOtaReq;       // 1 = buscar, 2 = buscar e instalar (GitHub)
extern String            webOtaState;     // texto de estado del OTA de GitHub
extern String            webOtaLatest;
