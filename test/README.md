# Banco de pruebas - ayModbusConverter

## 1. Materiales

| # | Elemento | Uso | Nota |
|---|---|---|---|
| 1 | WT32-ETH01 v1.4 | equipo bajo prueba | |
| 1 | Adaptador USB-TTL 3,3 V (CP2102, CH340, FT232) | primera programación y consola de log | con salida 3V3 o 5V |
| 1 | Módulo transceptor RS-485 **3,3 V** (MAX3485, SP3485, "TTL to RS485" 3V3) | puerto RS-485 del conversor | ver §2.2 si es de 5 V o autodireccional |
| 1 | OLED SSD1306 0,96" 128x64 **I2C** (4 pines) | pantalla | dirección 0x3C |
| 1 | Pulsador NA | botón CFG | |
| 1 | LED + resistencia 330 Ω a 1 kΩ | actividad Modbus | opcional |
| 1 | Fuente 5 V ≥ 1 A | alimentación | |
| 1 | **Adaptador USB-RS485** para el PC (CH340/FT232 con bornes A/B) | el PC hace de esclavo(s) RTU con `rtu_slave_sim.py` | imprescindible para el banco automático |
| 2 | Resistencias 120 Ω | terminación del bus en los dos extremos | |
| – | Par trenzado (UTP sirve), cables dupont, protoboard | | |
| 1 | Cable de red + switch/router con DHCP | LAN | o directo al PC con IP fija |
| 1 | *Opcional:* equipo Modbus RTU real (sensor XY-MD02, medidor Eastron SDM120/SDM630, variador) | prueba de campo | XY-MD02: 9600 8N1, ID 1 |
| 1 | *Opcional:* analizador lógico 8 canales 24 MHz | ver DE, t3.5 y tiempos reales en el bus | |

## 2. Cableado pin a pin

### 2.1 WT32-ETH01: pines usados

| Serigrafía WT32 | GPIO | Va a | Señal |
|---|---|---|---|
| **5V** | – | fuente +5 V | alimentación (o 3V3, nunca ambas) |
| **GND** | – | GND común de todo | referencia |
| **3V3** | – | VCC del transceptor 3,3 V y del OLED | salida 3,3 V del regulador de la placa |
| **RXD** (IO5) | 5 | RO del transceptor | RS-485 → ESP32 |
| **TXD** (IO17) | 17 | DI del transceptor | ESP32 → RS-485 |
| **485_EN** (IO33) | 33 | DE + /RE del transceptor (puenteados) | dirección, alto = transmitir (lo maneja la UART por hardware) |
| **IO14** | 14 | SDA del OLED | I2C datos |
| **IO15** | 15 | SCL del OLED | I2C reloj |
| **CFG** (IO32) | 32 | pulsador → GND | botón (pull-up interno) |
| **IO2** | 2 | resistencia → LED → GND | LED de actividad (opcional) |
| **TX0** (IO1) | 1 | RX del USB-TTL | programación / log 115200 |
| **RX0** (IO3) | 3 | TX del USB-TTL | programación / log |
| **IO0** | 0 | GND **solo para programar** | modo descarga; después se deja libre |
| **EN** | – | GND un instante | reset |

**No conectar nada** a IO0 (en funcionamiento), 16, 18, 19, 21, 22, 23, 25, 26, 27: los usa la PHY Ethernet. IO12 debe estar en bajo al arrancar. IO35, IO36 e IO39 quedan libres (solo entrada).

### 2.2 Transceptor RS-485

Módulo MAX3485/SP3485 de 3,3 V con pines RO, RE, DE, DI:

| Módulo | Conectar a |
|---|---|
| VCC | 3V3 del WT32 |
| GND | GND |
| RO | RXD (IO5) |
| DI | TXD (IO17) |
| DE y RE (puenteados) | 485_EN (IO33) |
| A | A / D+ / "485+" del equipo esclavo |
| B | B / D- / "485-" del equipo esclavo |

Variantes habituales:

- **Módulo MAX485 de 5 V** (el azul común): VCC a 5V. DI y DE aceptan los 3,3 V del ESP32. **RO sale a 5 V**, así que usa un divisor 1 kΩ (serie) / 2 kΩ (a GND) hacia IO5.
- **Módulo "TTL to RS485" autodireccional** (sin pin DE, con pines TXD/RXD/VCC/GND): la serigrafía es desde el punto de vista del módulo. **TXD del módulo → IO5 y RXD del módulo → IO17.** IO33 queda sin conectar; el firmware funciona igual.
- Si A/B se ven invertidos (timeouts en todo), intercambia A y B: algunos fabricantes etiquetan al revés.

### 2.3 Bus RS-485

```
 WT32 + MAX3485            USB-RS485 del PC              equipo real (opcional)
   A ───────────────┬──────────── A ──────────────┬──────────── A
   B ───────────────┼──────────── B ──────────────┼──────────── B
  GND ──────────────┼─────────── GND ─────────────┼─────────── GND/COM
                  [120Ω]                        [120Ω]
            (extremo 1: A-B)               (extremo 2: A-B)
```

- 120 Ω entre A y B **solo en los dos extremos físicos** del cable. Muchos módulos ya traen una (R0 = 120 Ω): usa como máximo dos en todo el bus.
- En el banco (menos de 2 m) funciona sin terminación, pero conviene probar como en campo.
- Une las masas (GND) si los equipos no comparten fuente.

### 2.4 OLED SSD1306 I2C

| OLED | Conectar a |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | IO14 |
| SCL | IO15 |

Si tu módulo es 0x3D (puente en el reverso), cambia `OLED_I2C_ADDR` en `src/board.h`. Los módulos traen pull-ups de I2C, no hace falta agregar resistencias.

### 2.5 Programación (USB-TTL)

| USB-TTL | WT32 |
|---|---|
| TX | RX0 (IO3) |
| RX | TX0 (IO1) |
| GND | GND |
| 5V (o 3V3) | 5V (o 3V3) |

Para cargar: IO0 a GND → pulso EN a GND → `pio run -t upload` → retira el puente de IO0 → pulso EN. Monitor: `pio device monitor` (115200).

## 3. Ambientes de prueba

| Ambiente | Qué se prueba | Hardware | Herramienta |
|---|---|---|---|
| **A. UI sin placa** | interfaz web, formularios, validaciones | ninguno | `python test/webmock/mock.py` |
| **B. Banco automático** | todo el camino TCP → RTU → TCP con datos verificables | WT32 + transceptor + USB-RS485 en el PC | `rtu_slave_sim.py` + `gw_test.py` |
| **C. Equipo real** | compatibilidad con un esclavo comercial y un cliente SCADA | B + equipo RTU real | web del conversor (Diagnóstico), `gw_test.py --only`, Modbus Poll / QModMaster |
| **D. Duración** | fugas de memoria, reconexiones, estabilidad de 24 h | B | `gw_test.py --soak 86400` |

### 3.1 Ambiente B: banco automático (un solo PC)

```
PC ──LAN──► WT32 (Modbus TCP :502) ──RS-485──► USB-RS485 ──► PC (rtu_slave_sim.py emula los IDs 1 y 2)
```

1. En el conversor: serie **9600 8N1**, Unit ID transparente, 0 = broadcast, excepciones activas, 4 conexiones (valores de fábrica).
2. Terminal 1, esclavos simulados:
   ```bash
   pip install -r test/tools/requirements.txt
   python test/tools/rtu_slave_sim.py --port COM7 --baud 9600 --ids 1,2
   ```
3. Terminal 2, batería:
   ```bash
   python test/tools/gw_test.py --host 192.168.1.50 --ids 1,2 --missing 9
   ```
4. Se repite con otros formatos: cambia el conversor y el simulador por igual (`--baud 19200 --parity E`, `--baud 115200`, `--stopbits 2`).
5. Timeout y reintentos: `rtu_slave_sim.py --ids 1,2 --delay 2:1500` con timeout del conversor en 1000 ms → el ID 2 debe dar 0x0B. Con timeout en 2000 ms → debe responder.
6. RTU sobre TCP: cambia el protocolo en la web y corre `gw_test.py --rtu-over-tcp`.

Qué verifica `gw_test.py` (`--list` para verlas):

| Prueba | Criterio |
|---|---|
| FC01/02/03/04/05/06/15/16 | cada valor coincide con el modelo del simulador |
| 125 registros | trama máxima de 255 bytes |
| Excepción del esclavo | la 0x02 llega intacta al cliente |
| Esclavo inexistente | la pasarela responde 0x0B en ≈ timeout |
| TID / Unit ID | se devuelven exactamente los del cliente |
| Encadenadas / fragmentadas | 5 peticiones en un segmento; una petición partida en 2 |
| MBAP inválido | el conversor cierra la conexión |
| Broadcast | Unit ID 0 escribe en todos los esclavos y no responde |
| Concurrencia | N clientes, ninguna respuesta cruzada |
| Límite de conexiones | la conexión N+1 se rechaza |
| Latencia | min/media/p95/max extremo a extremo |
| API | `/api/status` responde y el servidor escucha |

Latencia esperada a 9600 bps con FC03 de 10 registros: unos 35 a 45 ms (≈ 8 + 25 bytes en el aire + pausas).

### 3.2 Checklist manual

| # | Prueba | Resultado esperado |
|---|---|---|
| 1 | Arranque con cable de red y DHCP | OLED: logo, luego IP; `E` en la cabecera |
| 2 | Desconectar / reconectar el cable | OLED "ETH sin cable"; vuelve la IP; los clientes reconectan |
| 3 | IP fija desde la web | reinicia con la IP nueva; ping y web OK; DNS resuelve (buscar OTA) |
| 4 | Teléfono al WiFi `ayModbus-XXXX` | el portal se abre solo |
| 5 | Botón corto / 3 s / 10 s | página siguiente / AP activo / fábrica |
| 6 | OLED se apaga tras N minutos | el botón la despierta |
| 7 | Diagnóstico → Escanear 1..10 | encuentra los IDs 1 y 2 |
| 8 | Monitor de tramas durante `gw_test.py` | TCP> / >RTU / RTU> / >TCP coherentes |
| 9 | Subir firmware .bin desde la web | barra de progreso, reinicio, versión nueva |
| 10 | Respaldo → fábrica → restaurar | la configuración vuelve completa |
| 11 | Cliente fuera de la lista blanca | conexión rechazada, contador "rechazadas" sube |
| 12 | Cortar la alimentación con tráfico | arranca solo y vuelve a atender |

## 4. Herramientas existentes: ¿sirven?

| Herramienta | Qué sirve | Limitación para este producto |
|---|---|---|
| `modbusMaster` (Flask, pestaña Master) | hablar **directo** con un esclavo RTU por el USB-RS485, para comprobar el equipo real **sin** el conversor | solo cliente serie: no habla Modbus TCP; solo FC01-06 |
| `modbusMaster` (pestaña Slave) | – | no abre el puerto: solo genera datos en memoria y **no responde en el bus** |
| `modbusMaster` (PLC-SIM / Mapa A) | – | atados al mapa de registros de la orquestación LoRa |
| `nodeIO_master/test/tools` (mb_dump, mb_watch) | ya usan pymodbus y aceptan `--tcp` | decodifican el mapa del gateway LoRa, no son genéricos |
| **Modbus Poll / QModMaster / mbpoll** (terceros) | cliente Modbus TCP genérico tipo SCADA | manual, sin verificación automática |

Por eso el banco usa `test/tools/`: `rtu_slave_sim.py` es un esclavo RTU real y multi-ID, y `gw_test.py` verifica el conversor de punta a punta. Ambos usan pymodbus 3.5, como las demás herramientas Aysafi.
