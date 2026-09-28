# Changelog

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
