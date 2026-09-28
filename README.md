# ayModbusConverter

**Conversor Modbus TCP ⇄ Modbus RTU (RS-485 y RS-232)** de Aysafi, basado en la placa **WT32-ETH01** (ESP32 + Ethernet LAN8720), con pantalla OLED de 0.96" y configuración web con asistente.

La base es un **servidor Modbus TCP transparente**: cada petición que llega por la LAN se enruta por Unit ID a uno de los dos puertos serie y la respuesta del esclavo vuelve sin tocar. El mapa de memoria es el del equipo RTU conectado. Sobre esa base estándar, Aysafi agrega estados de transmisión visibles, asistente con detección automática, protección del bus (esclavos caídos, caché, solo lectura) e integración con MQTT y API REST.

```
                                                    ┌── P1 RS-485 ──► esclavos (Unit ID 1-100)
 SCADA / HMI / PLC ──LAN (Modbus TCP :502)──► ayModbusConverter
 nube Aysafi ◄── MQTT / API REST ──┘                └── P2 RS-232 ──► equipo (Unit ID 101 → ID 1)
                                        ▲
                teléfono / PC ──WiFi AP (portal cautivo, solo configuración)
```

## Características

### Base estándar

| Área | Detalle |
|---|---|
| LAN (servicio) | Ethernet 10/100. DHCP o IP fija con máscara, puerta de enlace, DNS 1 y 2. Nombre de host, mDNS (`<host>.local`) |
| WiFi | **Solo** punto de acceso con **portal cautivo** para configurar. No transporta tráfico Modbus |
| Modbus TCP | Servidor en :502, hasta 8 clientes, cierre por inactividad, TCP keep-alive, lista blanca de IP/redes. Modbus TCP (MBAP) o RTU sobre TCP |
| Puertos serie | P1 RS-485 (DE/RE por hardware) y P2 RS-232 (o RS-485 autodireccional). 1200 a 230400 bps, 7/8 bits, N/E/O, 1/2 bits de parada |
| Unit ID | Cada puerto atiende un rango de Unit ID con offset. Esclavo fijo opcional. Unit ID 0 = broadcast a todos los puertos |
| Temporización | Timeout, reintentos, fin de trama (t3.5 automático o manual), pausa entre peticiones, pausa tras broadcast, retardo inicial |
| Excepciones | 0x0B (el esclavo no responde), 0x0A (ruta no disponible), 0x01 (solo lectura); desactivables |
| Sistema | Usuario y clave (HTTP Basic), NTP/zona horaria, respaldo y restauración (JSON), valores de fábrica, OTA web y desde GitHub Releases |

### Mejoras Aysafi

| Mejora | Qué hace |
|---|---|
| **Estados de transmisión** | **OK / FALLAS / DESCONECTADO / SIN TRÁFICO** por puerto, por esclavo y global, más **LAN DESCONECTADA**. Se ven en el OLED (página de estado grande que parpadea ante fallas y se enciende sola al cambiar), en el banner del portal y por MQTT |
| **Asistente de puesta en marcha** | Se abre solo en un equipo nuevo: nombre y clave, red LAN, puerto serie con **detección automática**, búsqueda e identificación de equipos, y **ficha de instalación imprimible** |
| **Detección automática del puerto** | Encuentra baudios y paridad. *Activa:* consulta los Unit ID indicados en 24 combinaciones. *Pasiva:* solo escucha el tráfico de otro maestro, sin transmitir |
| **Exploración de equipos** | Lee fabricante, modelo y versión (FC43, si el equipo la soporta) y descubre qué rangos de registros existen, por bloques |
| **Esclavos caídos** | Tras N timeouts seguidos el esclavo queda "caído": se responde 0x0B al instante (sin frenar al resto del bus) y se lo reintenta en segundo plano |
| **Modo caché / agente** | El conversor sondea una tabla de lecturas y responde desde memoria: varios SCADA no saturan un bus lento. Las escrituras invalidan la caché y van directo al esclavo |
| **Aprendizaje del polling** | Anota qué lecturas hace el SCADA y cada cuánto, y las pasa a la tabla de sondeo con un clic |
| **Solo lectura** | Bloquea escrituras a esclavos y rangos de direcciones elegidos (excepción 0x01) |
| **MQTT** | Estado con LWT, valores de la tabla de sondeo y comandos de lectura/escritura (`<base>/cmd`). TLS, por defecto hacia `emqx.aysafi.com` |
| **API REST** | `/api/read`, `/api/write`, `/api/tags` en JSON, con clave de API (`X-API-Key`) o HTTP Basic |
| **Captura para Wireshark** | El monitor de tráfico se descarga como `.pcapng` (RTU y MBAP) |

## Parámetros estándar de la industria

La base sigue los parámetros que tienen en común los conversores comerciales más usados (Moxa MGate MB3180/MB3280, USR-TCP232-410S/USR-M100, Waveshare RS485-to-ETH, Elfin EE11/EW11), con los mismos nombres y comportamiento.

| Parámetro estándar | Moxa | USR | Waveshare/Elfin | ayModbusConverter |
|---|:-:|:-:|:-:|---|
| IP fija / DHCP, máscara, gateway, DNS | ✓ | ✓ | ✓ | Red → LAN |
| Baudios, bits de datos, paridad, bits de parada | ✓ | ✓ | ✓ | Puertos serie |
| Puerto TCP local (502) | ✓ | ✓ | ✓ | Modbus TCP → Puerto |
| Máximo de conexiones | ✓ | ✓ | ✓ | Conexiones simultáneas (1-8) |
| Modbus TCP ⇄ RTU / RTU sobre TCP | ✓ | ✓ | ✓ | Protocolo |
| Timeout de respuesta | ✓ | ✓ | ✓ | Puertos serie → Timeout |
| Timeout entre caracteres / fin de trama | ✓ | ✓ | ✓ | Fin de trama (0 = t3.5) |
| Retardo entre tramas | ✓ | – | ✓ | Pausa entre peticiones |
| Reintentos | ✓ | – | – | Reintentos |
| Retardo inicial | ✓ | – | – | Modbus TCP → Retardo inicial |
| Excepción de pasarela (0x0A/0x0B) | ✓ | ✓ | – | Responder excepciones |
| Mapeo de Unit ID por puerto (rango + offset) | ✓ | – | – | Puertos serie → Enrutamiento |
| TCP alive check | ✓ | ✓ | ✓ | TCP alive check (s) |
| Cierre por inactividad | ✓ | ✓ | ✓ | Cierre por inactividad (s) |
| Lista de IP permitidas | ✓ | – | – | Control de acceso |
| Monitor de tráfico / diagnóstico | ✓ | ✓ | – | Diagnóstico |
| Actualización de firmware web | ✓ | ✓ | ✓ | Sistema → Firmware |
| Respaldo / restauración de la configuración | ✓ | ✓ | – | Sistema → Respaldo |
| Contraseña de administrador | ✓ | ✓ | ✓ | Sistema → Acceso |

## Hardware

### Conexiones (WT32-ETH01 v1.4)

| Función | GPIO | Serigrafía | Conectar a |
|---|---|---|---|
| P1 RS-485 RX | 5 | RXD / IO5 | RO del transceptor RS-485 |
| P1 RS-485 TX | 17 | TXD / IO17 | DI del transceptor RS-485 |
| P1 RS-485 DE//RE | 33 | 485_EN | DE y /RE unidos |
| P2 RS-232 RX | 35 | IO35 | R1OUT del MAX3232 |
| P2 RS-232 TX | 4 | IO4 | T1IN del MAX3232 |
| OLED SDA | 14 | IO14 | SDA del SSD1306 (0x3C) |
| OLED SCL | 15 | IO15 | SCL del SSD1306 |
| Botón | 32 | CFG | pulsador a GND (pull-up interno) |
| LED actividad | 2 | IO2 | LED + 1 kΩ a GND (opcional) |
| Programación | 1 / 3 | TX0 / RX0 | adaptador USB-TTL 3V3 |

- P1: transceptor MAX3485 / SP3485 (3,3 V). Con un MAX485 de 5 V, pon un divisor en RO.
- P2: MAX3232 a 3,3 V. T1OUT → RX del equipo, R1IN ← TX del equipo, GND común (en un DB9 estándar: pin 2 RX, 3 TX, 5 GND; si no hay comunicación, cruza 2 y 3). También acepta un módulo RS-485 **autodireccional**: no queda ningún GPIO de salida libre para un DE.
- Termina el bus RS-485 con 120 Ω en ambos extremos. Usa par trenzado con malla y referencia GND común.
- No uses los GPIO 0, 16, 18, 19, 21, 22, 23, 25, 26 ni 27: los usa la PHY Ethernet. El GPIO 12 debe estar en bajo al arrancar.

Detalle pin a pin, variantes de módulos y banco de pruebas: [test/README.md](test/README.md).

### Botón

| Pulsación | Acción |
|---|---|
| Corta | Siguiente página del OLED / despierta la pantalla |
| 3 s | Activa el punto de acceso de configuración |
| 10 s | Restaura los valores de fábrica y reinicia |

### Páginas del OLED

| # | Página | Contenido |
|---|---|---|
| 0 | Estado | OK / FALLAS / DESCONECTADO / SIN LAN en grande (parpadea ante fallas) y estado de cada puerto |
| 1 | Red | IP de la LAN, velocidad, AP y portal |
| 2 | Puertos | formato serie, rango de Unit ID y caché de cada puerto; clientes TCP |
| 3 | Esclavos | estado de cada esclavo visto |
| 4 | Tráfico | peticiones, OK, timeouts, CRC, excepciones, caché, latencia |
| 5 | Sistema | versión, tiempo encendido, memoria, MAC |

Un cambio de estado (cable de red, puerto que empieza a fallar o se recupera) enciende la pantalla y salta a la página de estado.

## Primer arranque

1. Conecta la LAN (DHCP por defecto) o el WiFi **`ayModbus-XXXXXX`** (clave `aysafi1234`; el portal se abre solo).
2. Entra a la IP que muestra el OLED o a `http://aymc-xxxxxx.local`. Usuario **admin**, clave **admin**.
3. El **asistente** se abre solo: nombre y clave, red, puerto serie (con *Detectar automáticamente*), búsqueda de equipos y resumen para imprimir. Al finalizar, el equipo se reinicia.

Desde el portal WiFi (red WPA2) no se pide usuario ni clave, porque los navegadores de portal cautivo de los teléfonos no manejan bien HTTP Basic. Si la clave del AP está vacía, se pide igual.

## Valores de fábrica

| Parámetro | Valor |
|---|---|
| LAN | DHCP, host `aymc-<id>` |
| AP | siempre activo, `ayModbus-<ID>` / `aysafi1234`, canal 6 |
| P1 RS-485 | habilitado, 9600 8N1, timeout 1000 ms, Unit ID 1-247, caído tras 3 timeouts (reintento 10 s) |
| P2 RS-232 | deshabilitado, 9600 8N1, Unit ID 101-147 con offset -100. Al habilitarlo, la web ajusta P1 a 1-100 |
| Modbus TCP | :502, MBAP, 4 clientes, inactividad 300 s, keep-alive 60 s, excepciones sí |
| Caché, MQTT, API key | desactivados |
| Acceso web | admin / admin |

## Compilar y programar

```bash
pio run
```

La WT32-ETH01 no tiene USB. Para la primera carga, conecta un adaptador USB-TTL de 3,3 V a TX0/RX0/GND, pon **IO0 a GND**, reinicia (EN) y ejecuta:

```bash
pio run -t upload
```

Las cargas siguientes se hacen por OTA desde **Sistema → Firmware** (`.pio/build/wt32-eth01/firmware.bin`).

Para probar la interfaz web sin la placa (usa la UI embebida y simula la API, herramientas incluidas):

```bash
python test/webmock/mock.py --wizard
```

## Publicar una versión (OTA GitHub)

1. Anota el cambio en `CHANGELOG.md` y sube `FW_SEMVER` en `src/fw_version.h`.
2. Etiqueta sobre `main`: `git tag v1.1.0 && git push origin v1.1.0`.
3. El workflow `.github/workflows/release.yml` compila y publica `firmware.bin`, `version.txt` y `firmware.sha256` como Release *latest*.
4. Los equipos con salida a internet lo instalan con **Sistema → Instalar desde GitHub**, o solos al arrancar si está activado.

El repositorio debe ser **público** para que los equipos descarguen los Releases sin token.

## MQTT

Raíz de tópicos por defecto: `aysafi/<host>`.

| Tópico | Dirección | Contenido |
|---|---|---|
| `<base>/status` | publica (retenido) | `{online, name, fw, ip, lan, state, ports[], slaves[], stats}`; cada N s y al cambiar un estado. LWT `{"online":false}` |
| `<base>/data/<nombre>` | publica | valores de cada lectura de la tabla de sondeo marcada *MQTT*: `{v:[...], q, age, uid, fc, addr, ts}` |
| `<base>/cmd` | suscribe (opcional) | `{"id":7,"uid":1,"fc":3,"addr":0,"qty":10}` o escritura `{"uid":1,"fc":6,"addr":10,"value":123}` (`port` opcional) |
| `<base>/cmd/resp` | publica | `{id, ok, result, values / exception, lat}` |

## API REST

Todas las rutas usan HTTP Basic, salvo que la petición llegue desde el AP. Las marcadas con * aceptan también la cabecera `X-API-Key` (o `?key=`).

| Método | Ruta | Uso |
|---|---|---|
| GET | `/api/status` | Estado global y por puerto/esclavo, red, contadores, clientes, MQTT |
| GET | `/api/read`* | `?uid=1&fc=3&addr=0&qty=10[&port=1][&cache=0]` lectura (enruta por Unit ID si no hay `port`) |
| POST | `/api/write`* | `{uid, fc(5/6/15/16), addr, value / values[, port]}` |
| GET | `/api/tags`* | Tabla de sondeo con sus últimos valores |
| GET / POST | `/api/config` | Leer / guardar la configuración (JSON parcial; reinicia) |
| POST | `/api/port/apply` | `{port, cfg:{...}}` aplica un puerto sin reiniciar |
| GET / POST | `/api/tool` | Herramientas: `{kind:"scan"|"detect"|"discover"|"apply"|"stop", port, ...}` y su progreso |
| GET / POST | `/api/learn` | Aprendizaje del polling: resultados / `{en, clear}` |
| GET / POST | `/api/monitor` | Tramas desde `?since=N` / `{en, clear}` |
| GET | `/api/pcap` | Monitor como captura `.pcapng` |
| POST | `/api/test` | `{port, uid, fc, addr, qty, value, values}` lectura/escritura directa |
| GET | `/api/backup` · POST `/api/restore` | Respaldo / restauración (reinicia) |
| POST | `/api/stats/reset`, `/api/reboot`, `/api/factory`, `/api/wizard/done` | Mantenimiento |
| POST | `/api/fw` · `/api/ota` | Firmware por archivo / desde GitHub |

## Estructura

```
src/
  main.cpp          arranque, botón, OTA GitHub
  config.*          configuración en NVS como JSON (sobrevive a cambios de versión)
  net.*             LAN (LAN8720) + AP con portal cautivo, mDNS, NTP
  modbus_rtu.*      puerto RTU (clase por puerto): CRC, fin de trama, reintentos, escucha pasiva
  gateway.*         tareas TCP y por puerto, colas, enrutamiento, salud/estados, caché,
                    aprendizaje, solo lectura, escaneo/autodetección/exploración, monitor y pcap
  mb_util.*         armado de peticiones y respuestas JSON (web, REST, MQTT)
  mqtt_bridge.*     puente MQTT
  web.*, web_ui.h   API REST + interfaz web embebida (con asistente)
  display.*         OLED SSD1306 (páginas de estado)
  ota_update.*      OTA desde GitHub Releases
test/webmock/       servidor para probar la UI sin la placa
test/tools/         simulador de esclavos RTU + batería de pruebas automática
```
