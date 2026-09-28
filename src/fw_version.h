#pragma once

#define PRODUCT_NAME   "ayModbusConverter"
#define GITHUB_OWNER   "asdrubalfuentes"
#define GITHUB_REPO    "ayModbusConverter"

// Version semver (X.Y.Z) del canal OTA (GitHub Releases). El CI la sobreescribe
// desde el tag vX.Y.Z; sin CI vale este literal.
//   1.0.0  primera version: pasarela Modbus TCP <-> RTU, portal web, OLED, OTA
#ifdef FW_VERSION_OVERRIDE
#  define FW_SEMVER FW_VERSION_OVERRIDE
#else
#  define FW_SEMVER "1.0.0"
#endif
