# ayModbusConverter

**Conversor transparente Modbus TCP ⇄ Modbus RTU (RS-485)** de Aysafi, basado en la placa **WT32-ETH01** (ESP32 + Ethernet LAN8720), con pantalla OLED de 0.96" y configuración web.

El equipo es un **servidor Modbus TCP transparente**: cada petición que llega por la LAN se reenvía al bus RS-485 y la respuesta del esclavo vuelve sin tocar. El conversor no guarda registros propios, así que el mapa de memoria es el del equipo RTU conectado. Para el SCADA/HMI/PLC, el esclavo RTU aparece como si fuera un dispositivo Modbus TCP.

```
 SCADA / HMI / PLC ──LAN (Modbus TCP :502)──►  ayModbusConverter  ──RS-485 (Modbus RTU)──►  esclavos 1..247
                                                      ▲
                          teléfono / PC ──WiFi AP (portal cautivo, solo configuración)
```

## Características

| Área | Detalle |
|---|---|
| LAN (servicio) | Ethernet 10/100. DHCP o IP fija con máscara, puerta de enlace, DNS 1 y 2. Nombre de host, mDNS (`<host>.local`) |
| WiFi | **Solo** punto de acceso con **portal cautivo** para configurar. No transporta tráfico Modbus. Modo siempre activo, automático (sin LAN a los 30 s) o con el botón |
| Modbus TCP | Servidor en :502 (configurable), hasta 8 clientes simultáneos atendidos por turno (round-robin), cierre por inactividad, TCP keep-alive, lista blanca de IP/redes |
| Protocolo TCP | Modbus TCP (MBAP) o Modbus RTU sobre TCP |
| Unit ID | Transparente, esclavo fijo o mapeo por rango + offset. Unit ID 0 como broadcast. Unit ID 255 va al esclavo fijo |
| RS-485 | 1200 a 230400 bps, 7/8 bits, N/E/O, 1/2 bits de parada. DE/RE por hardware de la UART (sin retardos por software) |
| Temporización | Timeout de respuesta, reintentos, fin de trama (t3.5 automático o manual), pausa entre peticiones, pausa tras broadcast, retardo inicial |
| Excepciones | La pasarela responde 0x0B (el esclavo no responde) y 0x0A (ruta no disponible); se puede desactivar |
| Diagnóstico | Panel en vivo, estadísticas por esclavo, clientes conectados, monitor de tramas con decodificación y exportación a CSV, lectura/escritura directa (FC 1-6, 16), escaneo del bus |
| Sistema | Usuario y clave de administrador (HTTP Basic), NTP/zona horaria, respaldo y restauración de la configuración (JSON), valores de fábrica |
| Firmware | OTA subiendo el `.bin` desde la web y OTA desde GitHub Releases (verificación SHA-256) |
| OLED | 4 páginas: red, pasarela, tráfico y sistema. Indicador de actividad Modbus. Se apaga sola para no quemar la pantalla |

## Parámetros estándar de la industria

La configuración sigue los parámetros que tienen en común los conversores comerciales más usados (Moxa MGate MB3180/MB3170, USR-TCP232-410S/USR-M100, Waveshare RS485-to-ETH, Elfin EE11/EW11), con los mismos nombres y comportamiento, para que un integrador lo reconozca de inmediato. Las mejoras propias de Aysafi se agregarán después, encima de esta base.

| Parámetro estándar | Moxa | USR | Waveshare/Elfin | ayModbusConverter |
|---|:-:|:-:|:-:|---|
| IP fija / DHCP, máscara, gateway, DNS | ✓ | ✓ | ✓ | Red → LAN |
| Baudios, bits de datos, paridad, bits de parada | ✓ | ✓ | ✓ | Serie RS-485 |
| Puerto TCP local (502) | ✓ | ✓ | ✓ | Modbus TCP → Puerto |
| Máximo de conexiones | ✓ | ✓ | ✓ | Conexiones simultáneas (1-8) |
| Modbus TCP ⇄ RTU / RTU sobre TCP | ✓ | ✓ | ✓ | Protocolo |
| Timeout de respuesta | ✓ | ✓ | ✓ | Serie → Timeout de respuesta |
| Timeout entre caracteres / fin de trama | ✓ | ✓ | ✓ | Fin de trama (0 = t3.5) |
| Retardo entre tramas | ✓ | – | ✓ | Pausa entre peticiones |
| Reintentos | ✓ | – | – | Reintentos |
| Retardo inicial | ✓ | – | – | Retardo inicial |
| Excepción de pasarela (0x0A/0x0B) | ✓ | ✓ | – | Responder excepciones |
| Mapeo de Unit ID (rango + offset) | ✓ | – | – | Unit ID → Mapeo por rango |
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
| RS-485 RX | 5 | RXD / IO5 | RO del transceptor |
| RS-485 TX | 17 | TXD / IO17 | DI del transceptor |
| RS-485 DE//RE | 33 | 485_EN | DE y /RE unidos |
| OLED SDA | 14 | IO14 | SDA del SSD1306 (0x3C) |
| OLED SCL | 15 | IO15 | SCL del SSD1306 |
| Botón | 32 | CFG | pulsador a GND (pull-up interno) |
| LED actividad | 2 | IO2 | LED + 1 kΩ a GND (opcional) |
| Programación | 1 / 3 | TX0 / RX0 | adaptador USB-TTL 3V3 |

- Transceptor: MAX3485 / SP3485 (3,3 V). Con un MAX485 de 5 V, pon un divisor en RO.
- Termina el bus con 120 Ω en ambos extremos. Usa par trenzado con malla y referencia GND común.
- No uses los GPIO 0, 16, 18, 19, 21, 22, 23, 25, 26 ni 27: los usa la PHY Ethernet. El GPIO 12 debe estar en bajo al arrancar.
- Alimentación: 5 V por el pin 5V, o 3,3 V por el pin 3V3 (no las dos a la vez).

### Botón

| Pulsación | Acción |
|---|---|
| Corta | Siguiente página del OLED / despierta la pantalla |
| 3 s | Activa el punto de acceso de configuración |
| 10 s | Restaura los valores de fábrica y reinicia |

## Primer arranque

1. Conecta la LAN. Por defecto el equipo pide IP por **DHCP**, y el OLED la muestra en la página de red.
2. O conéctate al WiFi **`ayModbus-XXXXXX`** (clave `aysafi1234`). El portal se abre solo; si no, entra a **http://192.168.4.1**.
3. Entra por la LAN a `http://<ip>` o `http://aymc-xxxxxx.local`. Usuario **admin**, clave **admin**. El panel avisa hasta que cambies la clave.
4. Configura **Red** (IP fija recomendada), **Serie RS-485** (igual que los esclavos) y **Modbus TCP**. Guarda: el equipo se reinicia.
5. En **Diagnóstico → Escanear el bus** comprueba qué esclavos responden.

Por seguridad, desde el portal WiFi (red WPA2) no se pide usuario ni clave, porque los navegadores de portal cautivo de los teléfonos no manejan bien HTTP Basic. Si la clave del AP está vacía, se pide igual.

## Valores de fábrica

| Parámetro | Valor |
|---|---|
| LAN | DHCP, host `aymc-<id>` |
| AP | siempre activo, `ayModbus-<ID>` / `aysafi1234`, canal 6 |
| Serie | 9600 8N1, timeout 1000 ms, fin de trama automático, pausa 20 ms, 0 reintentos |
| Modbus TCP | :502, Modbus TCP, 4 clientes, inactividad 300 s, keep-alive 60 s, excepciones sí |
| Unit ID | transparente, 0 = broadcast, fijo = 1 |
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

Para probar la interfaz web sin la placa (usa la UI embebida y simula la API):

```bash
python test/webmock/mock.py
```

## Publicar una versión (OTA GitHub)

1. Anota el cambio en `CHANGELOG.md` y sube `FW_SEMVER` en `src/fw_version.h`.
2. Etiqueta sobre `main`: `git tag v1.0.1 && git push origin v1.0.1`.
3. El workflow `.github/workflows/release.yml` compila y publica `firmware.bin`, `version.txt` y `firmware.sha256` como Release *latest*.
4. Los equipos con salida a internet lo instalan con **Sistema → Instalar desde GitHub**, o solos al arrancar si está activado.

El repositorio debe ser **público** para que los equipos descarguen los Releases sin token.

## API REST

Todas las rutas usan HTTP Basic, salvo que la petición llegue desde el AP.

| Método | Ruta | Uso |
|---|---|---|
| GET | `/api/status` | Estado, red, contadores, clientes y esclavos |
| GET / POST | `/api/config` | Leer / guardar la configuración (JSON parcial; reinicia) |
| GET | `/api/backup` | Descargar respaldo completo |
| POST | `/api/restore` | Restaurar un respaldo (reinicia) |
| GET / POST | `/api/monitor` | Tramas desde `?since=N` / `{en, clear}` |
| POST | `/api/test` | `{uid, fc, addr, qty, value, values}` lectura/escritura directa |
| GET / POST | `/api/scan` | Estado / `{from, to, timeout}` o `{stop:true}` |
| POST | `/api/stats/reset` | Reiniciar contadores |
| POST | `/api/fw` | Subir firmware (multipart) |
| POST | `/api/ota` | `{apply:false}` buscar / `{apply:true}` instalar desde GitHub |
| POST | `/api/reboot`, `/api/factory` | Reiniciar / valores de fábrica |

## Estructura

```
src/
  main.cpp         arranque, botón, OTA GitHub
  config.*         configuración en NVS como JSON (sobrevive a cambios de versión)
  net.*            LAN (LAN8720) + AP con portal cautivo, mDNS, NTP
  modbus_rtu.*     maestro RTU: CRC, detección de fin de trama, reintentos
  gateway.*        servidor TCP multi-cliente, mapeo de Unit ID, estadísticas, monitor, escaneo
  web.*, web_ui.h  API REST + interfaz web embebida
  display.*        OLED SSD1306 (páginas de estado)
  ota_update.*     OTA desde GitHub Releases
test/webmock/      servidor para probar la UI sin la placa
test/tools/        simulador de esclavos RTU + bateria de pruebas automatica
```

Banco de pruebas, materiales y cableado detallado: [test/README.md](test/README.md).

## Próximos pasos: mejoras Aysafi

Espacio reservado para las funciones propias que se agregarán sobre la base estándar.
