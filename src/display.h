#pragma once
#include <Arduino.h>

// OLED SSD1306 128x64 (I2C). Paginas de estado rotativas con el boton.
void dispBegin();                  // init + splash con el logo aysafi
void dispLoop();                   // refresco (~4 Hz) y apagado por inactividad
void dispNextPage();
void dispWake();
// Mensaje a pantalla completa durante holdMs (0 = hasta el proximo mensaje/clear)
void dispMessage(const char* title, const char* l1, const char* l2 = nullptr,
                 const char* l3 = nullptr, uint32_t holdMs = 3000);
void dispClearMessage();
void dispProgress(const char* title, const char* detail, int pct);   // dibuja ya (bloqueante)
