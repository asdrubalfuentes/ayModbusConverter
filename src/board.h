#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin map - WT32-ETH01 (v1.4)
//
// Ocupados por la PHY LAN8720 (NO usar): GPIO0 (reloj 50 MHz), 16 (power),
// 18 (MDIO), 19, 21, 22, 23 (MDC), 25, 26, 27.  GPIO1/3 = UART0 (programacion
// y consola de log).  GPIO12 es strapping (debe estar en bajo al arrancar).
// ---------------------------------------------------------------------------

// Ethernet (LAN8720)
#define ETH_PHY_ADDR_WT32    1
#define ETH_PHY_POWER_WT32   16
#define ETH_PHY_MDC_WT32     23
#define ETH_PHY_MDIO_WT32    18

// RS-485 (UART2) -> transceptor MAX485/MAX3485/SP3485
#define PIN_RS485_RX   5     // serigrafia "RXD/IO5"   <- RO
#define PIN_RS485_TX   17    // serigrafia "TXD/IO17"  -> DI
#define PIN_RS485_DE   33    // serigrafia "485_EN"    -> DE + /RE  (UART RTS, half-duplex HW)

// OLED SSD1306 128x64 0.96" I2C
#define PIN_OLED_SDA   14
#define PIN_OLED_SCL   15
#define OLED_I2C_ADDR  0x3C

// Boton de usuario (a GND, pull-up interno). Serigrafia "CFG".
//   pulso corto  -> siguiente pagina del OLED / despierta la pantalla
//   3 s          -> fuerza el punto de acceso WiFi de configuracion
//   10 s         -> restaurar valores de fabrica
#define PIN_BUTTON     32

// LED de actividad Modbus (opcional, activo en alto). -1 para deshabilitar.
#define PIN_LED_ACT    2
