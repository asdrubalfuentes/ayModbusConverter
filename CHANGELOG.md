# Changelog

## 1.1.0 - 2026-09-28

Mejoras Aysafi sobre la base estándar.

- Segundo puerto serie (P2, UART1: RS-232 con MAX3232 o RS-485 autodireccional). Cada puerto
  atiende un rango de Unit ID con offset; broadcast a todos los puertos.
- Núcleo nuevo: una tarea por puerto con su cola de peticiones; la tarea TCP ya no se bloquea
  mientras un puerto espera una respuesta.
- Estados de transmisión OK / FALLAS / DESCONECTADO / SIN TRÁFICO por puerto, esclavo y global,
  más LAN DESCONECTADA: página de estado en el OLED (parpadea y despierta la pantalla), banner en
  el portal y MQTT.
- Esclavos caídos: tras N timeouts se responde 0x0B al instante y se reintenta en segundo plano.
- Asistente de puesta en marcha (5 pasos, ficha imprimible) que se abre solo en un equipo nuevo.
- Detección automática de baudios/paridad (activa y pasiva) y exploración de equipos
  (identificación FC43 y rangos de registros).
- Modo caché/agente con tabla de sondeo, aprendizaje del polling del SCADA y reglas de solo lectura.
- MQTT (estado con LWT, datos de la tabla de sondeo, comandos) y API REST `/api/read`,
  `/api/write`, `/api/tags` con clave de API.
- Monitor de tráfico descargable como `.pcapng` para Wireshark.
- Cambio de configuración: `ser` pasa a `ports[]`; el mapeo global de Unit ID pasa a cada puerto.

## 1.0.0 - 2026-09-28

Primera versión.

- Servidor Modbus TCP transparente hacia Modbus RTU por RS-485 (WT32-ETH01).
- Modbus TCP (MBAP) o RTU sobre TCP; hasta 8 clientes; inactividad, keep-alive y lista blanca de IP.
- Unit ID transparente, esclavo fijo o mapeo por rango + offset; Unit ID 0 como broadcast.
- Serie: baudios/bits/paridad/parada; timeout, reintentos, fin de trama t3.5, pausas, retardo inicial.
- LAN con DHCP o IP fija (máscara, gateway, DNS); WiFi solo como AP con portal cautivo.
- Web: panel en vivo, configuración, monitor de tramas, lectura/escritura directa, escaneo del bus,
  respaldo/restauración, OTA web y desde GitHub Releases.
- OLED 128x64 con páginas de estado; botón para página, AP y valores de fábrica.
- Configuración guardada en NVS como JSON: los campos nuevos no borran la configuración en campo.
