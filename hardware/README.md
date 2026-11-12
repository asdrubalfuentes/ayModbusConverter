# Portadora MINI — ayModbusConverter

Rev C · KiCad 8 · PCB base para la tarjeta de desarrollo **WT32-ETH01 V1.4**

Un solo puerto RS-485 y la LAN del módulo. Sin OLED, sin RS-232, sin tramo óptico.
Configuración por consola serie y por WiFi (AP con portal cautivo).

---

## 1. Qué es esta placa

Es una **base**: la WT32-ETH01 no se suelda, se enchufa sobre dos zócalos hembra de 13
posiciones. La portadora solo aporta lo que el módulo no trae:

1. Alimentación 24 VDC → 5 V con protección de entrada.
2. Transceptor RS-485 no aislado con terminación, polarización y protección.
3. Botón CFG, LED de actividad y cabecera de consola/programación.

Hacia el exterior hay **dos borneras y nada más**:

| Conector | Vías | Señales |
|---|:-:|---|
| J1 | 2 | 24 VDC: `+` y `−` |
| J2 | 3 | RS-485: `A`, `B`, `GND` |

La LAN sale por el RJ45 del propio módulo. J3 es una cabecera interna de 2,54 mm, no un
conector de campo.

---

## 2. Pinaje del módulo

Verificado contra el datasheet V1.4 y la referencia comunitaria
[egnor/wt32-eth01](https://github.com/egnor/wt32-eth01). La asignación de GPIO sale de
`src/board.h` del firmware: **no la cambies sin cambiar el firmware**.

| Pin | Señal | Uso en esta placa |
|:-:|---|---|
| 4 | `ESP32_EN` | Reset, con C8 de 10 µF |
| 5 | `IO32` | Botón CFG |
| 6 | `IO33` | DE + RE̅ del MAX485 |
| 7 | `IO5` | RO del MAX485 (por el divisor) |
| 8 | `IO17` | DI del MAX485 |
| 12 | `5V` | Alimentación |
| 15 | `IO2` | LED de actividad |
| 24 | `IO0` | Boot, a J3 |
| 25 / 26 | `IO3` / `IO1` | Consola UART0, a J3 |
| 3, 10 | `3V3` | **Salida** del regulador del módulo |
| 2, 9, 11, 14, 23 | `GND` | Masa |

### Tres trampas del módulo

**Pin 1 no es el EN del ESP32.** Es `CLK_EN`, el habilitador del oscilador de 50 MHz del
PHY. El EN del micro es el pin 4. Confundirlos deja la placa sin arrancar o sin Ethernet.
Aquí el pin 1 queda sin conectar: el firmware habilita el oscilador por IO16, que no sale
al conector.

**Alimentar por 5 V o por 3,3 V, nunca por los dos.** El regulador del módulo es un
AMS1117 de dudosa procedencia sin protección térmica real; por encima de unos 6 V se
quema. Por eso el K7805 entrega 5 V exactos al pin 12 y los pines 3 y 10 se usan solo
como salida.

**Arranque con rampa lenta.** La red R-C de EN del módulo es de 1 ms. Si la fuente de
24 V sube despacio, el ESP32 puede quedarse colgado al encender. **C8 de 10 µF** lleva el
retardo a unos 100 ms y lo resuelve. Es el componente que evita fallos intermitentes de
arranque en campo, y no está en ningún diseño de referencia del módulo.

### GPIO que no se pueden usar

IO16 (alimentación del PHY), IO18 (MDIO), IO23 (MDC), IO19, IO21, IO22, IO25, IO26 e IO27
los ocupa el PHY y ni siquiera salen al conector. IO0 comparte el reloj de 50 MHz: a GND
solo para grabar. IO12 debe estar en bajo al arrancar. IO2 debe quedar flotante o en bajo
al arrancar, cosa que el LED con su resistencia a GND cumple. IO35, IO36 e IO39 son solo
entrada y **no tienen pull-up interno**, razón por la que el botón va en IO32.

---

## 3. Bloque RS-485

### Selección de tensión — JP1

| JP1 | Transceptor | R2 | R3 |
|---|---|---|---|
| 1-2 (5 V) | MAX485ESA | 1 k | 2 k |
| 2-3 (3,3 V) | MAX3485 / SP3485 | 0 Ω | sin poblar |

El MAX485 a 5 V acepta los 3,3 V del ESP32 en DI y DE, pero su salida RO llega a 5 V y
destruiría la entrada del micro. El divisor R2/R3 la baja a 3,33 V. Con un MAX3485 (mismo
pinout SOIC-8) el divisor desaparece.

### Terminación de fin de línea

**R4 de 120 Ω**, habilitada con el puente **JP2**. Ciérralo solo si esta tarjeta es un
extremo físico del segmento. Con el BW500 a pocos metros y 9600 bps puede quedar abierto.

### Polarización y protección

- **R5 / R6 de 560 Ω** fijan el estado de reposo del bus. Una sola vez por segmento: si
  el BW500 o el PLC ya polarizan, retíralas.
- **R7 / R8 de 10 Ω** antipulso limitan la corriente de falla hacia el transceptor.
- **D4 (SM712)** es una TVS asimétrica de −7/+12 V, el recorte exacto del rango de modo
  común de RS-485. No la sustituyas por una bidireccional simétrica genérica.
- **R10 de 100 Ω** va entre el borne `GND` de J2 y la masa de la placa. Sin aislamiento
  galvánico, las masas de la tarjeta y del BW500 quedan unidas por el cable; R10 limita la
  corriente de lazo de tierra sin perder la referencia de modo común.

> Al no haber aislamiento, ambos equipos deben compartir barra de tierra, idealmente la
> misma cabina. Si el integrador está en otro tablero con tierra distinta, R10 no basta y
> hay que volver a un transceptor aislado tipo ADM2587E.

---

## 4. Conectores

**J2 — RS-485 al BW500 (Port 2)**

| Pin | Señal |
|:-:|---|
| 1 | A (+) |
| 2 | B (−) |
| 3 | GND (común, por R10) |

**J3 — consola y primera grabación** (adaptador USB-TTL de 3,3 V)

| Pin | Señal | Al adaptador |
|:-:|---|---|
| 1 | 3V3 | referencia, **no alimentar por aquí** |
| 2 | TXD0 | RX del adaptador |
| 3 | RXD0 | TX del adaptador |
| 4 | IO0 | a GND para modo descarga |
| 5 | EN | pulso a GND para reset |
| 6 | GND | GND |

Secuencia: alimentar por 24 V, puentear pin 4 a GND, pulsar reset, lanzar el flasheo,
retirar el puente y resetear. Después de la primera carga, las siguientes van por OTA.

---

## 5. Interfaz local

Sin OLED, el diagnóstico en planta queda en:

| Elemento | Función |
|---|---|
| D3 verde | Alimentación presente |
| D5 ámbar (IO2) | Actividad Modbus |
| SW1 corta | Siguiente página / despertar |
| SW1 3 s | Levanta el AP WiFi de configuración |
| SW1 10 s | Restaura valores de fábrica |
| RJ45 del módulo | LED de enlace y velocidad propios |

La configuración completa se hace por el AP `ayModbus-XXXXXX` con portal cautivo, o por la
consola serie de J3 a 115200 8N1.

---

## 6. Archivos

| Archivo | Contenido |
|---|---|
| `aymc_carrier_min.kicad_pro` | Proyecto KiCad 8 |
| `aymc_carrier_min.kicad_sch` | Esquemático, hoja A3 |
| `aymc.kicad_sym` | Símbolos propios |
| `aymc.pretty/WT32-ETH01_2x13_THT.kicad_mod` | Huella del módulo |
| `sym-lib-table`, `fp-lib-table` | Tablas de biblioteca del proyecto |
| `netlist.csv` | 25 redes |
| `bom.csv` | Lista de materiales |

### Sobre la huella

2×13 pines, paso 2,54 mm, filas separadas **22,86 mm**, pin 1 cuadrado. El contorno de
26 × 60 mm es **indicativo**: el RJ45 sobresale por el extremo de los pines 1 y 26.
Antes de cerrar el PCB, contrasta la posición del conector con el modelo STEP de la
referencia comunitaria y deja hueco para el cuerpo del RJ45.

Los símbolos y la huella de esta carpeta son originales. El repositorio de referencia no
publica licencia, así que la geometría se tomó como dato físico medido y no se copió
ningún archivo suyo.

---

## 7. Puesta en marcha

1. Sin el módulo puesto, alimenta y mide **5,0 V** en el pin 12 del zócalo. Solo entonces
   enchufa la WT32-ETH01.
2. Verifica que D3 enciende y que el RJ45 marca enlace contra un switch.
3. Con el bus conectado, mide en reposo entre A y B: entre 200 mV y 1 V. Si marca 0 V, no
   hay polarización en ningún punto del segmento.
4. Lee el registro 40.064 del BW500: debe devolver **2**. Si no responde, invierte A y B.
5. Lee Rate desde el PLC con `modpoll` antes de programar el bloque `MB_CLIENT`.
6. Prueba de robustez: desconecta J2 con el PLC leyendo. El estado del esclavo debe pasar
   a caído y responder excepción 0x0B sin que caiga la conexión TCP.

---

## 8. Parámetros del BW500 (Port 2, RS-485)

Los parámetros de comunicación llevan índice de puerto (`-02` para el RS-485).

| Parámetro | Valor | Significado |
|---|:-:|---|
| `P770-02` | 3 | Modbus RTU |
| `P771-02` | 1…247 | Dirección del esclavo |
| `P772-02` | 3 | Baudios (el manual documenta 9600; confírmalo en tu edición) |
| `P773-02` | 0 | Sin paridad |
| `P774-02` | 8 | Bits de datos |
| `P775-02` | 1 | Bit de parada |
| `P778-02` | 0 | Sin módem |
| `P799` | 1 | Control de programación por Modbus |

Registros de proceso, todos de 32 bits sobre dos registros: Rate en 41.010, Load en
41.012, Speed en 41.014, Total 1 en 41.016, Total 2 en 41.018 y Device State en 41.020.
El registro 40.064 devuelve 2 y sirve de heartbeat. Offset de cable: `41010 − 40001 =
1009` (0x03F1) con función 03.
