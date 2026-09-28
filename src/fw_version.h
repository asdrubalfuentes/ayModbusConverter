#pragma once

#define PRODUCT_NAME   "ayModbusConverter"
#define GITHUB_OWNER   "asdrubalfuentes"
#define GITHUB_REPO    "ayModbusConverter"

// Version semver (X.Y.Z) del canal OTA (GitHub Releases). El CI la sobreescribe
// desde el tag vX.Y.Z; sin CI vale este literal.
//   1.0.0  primera version: pasarela Modbus TCP <-> RTU, portal web, OLED, OTA
//   1.1.0  mejoras Aysafi: 2 puertos (RS-485 + RS-232) enrutados por Unit ID, estados de
//          transmision (OLED/portal/MQTT), esclavos caidos con respuesta inmediata,
//          asistente, autodeteccion serie, exploracion de equipos, cache/agente con
//          aprendizaje, solo lectura, MQTT, API REST, captura .pcapng
#ifdef FW_VERSION_OVERRIDE
#  define FW_SEMVER FW_VERSION_OVERRIDE
#else
#  define FW_SEMVER "1.1.0"
#endif
