#include "mb_util.h"

size_t mbBuildPdu(JsonVariantConst q, uint8_t* pdu, const char** err) {
  uint8_t  fc   = q["fc"]   | 3;
  uint16_t addr = q["addr"] | 0;
  uint16_t qty  = q["qty"]  | 1;
  size_t n = 0;
  pdu[n++] = fc;
  pdu[n++] = addr >> 8; pdu[n++] = addr & 0xFF;
  switch (fc) {
    case 1: case 2: case 3: case 4:
      qty = constrain(qty, 1, (fc <= 2) ? 2000 : 125);
      pdu[n++] = qty >> 8; pdu[n++] = qty & 0xFF;
      return n;
    case 5: {
      bool v = (q["value"] | 0) != 0;
      pdu[n++] = v ? 0xFF : 0x00; pdu[n++] = 0x00;
      return n;
    }
    case 6: {
      uint16_t v = (uint16_t)(q["value"] | 0);
      pdu[n++] = v >> 8; pdu[n++] = v & 0xFF;
      return n;
    }
    case 15: {
      JsonArrayConst vals = q["values"];
      uint16_t c = constrain((int)vals.size(), 1, 1968);
      uint8_t bc = (c + 7) / 8;
      pdu[n++] = c >> 8; pdu[n++] = c & 0xFF; pdu[n++] = bc;
      memset(pdu + n, 0, bc);
      for (uint16_t i = 0; i < c; i++) if ((vals[i] | 0) != 0) pdu[n + (i >> 3)] |= 1 << (i & 7);
      return n + bc;
    }
    case 16: {
      JsonArrayConst vals = q["values"];
      uint16_t c = constrain((int)vals.size(), 1, 123);
      pdu[n++] = c >> 8; pdu[n++] = c & 0xFF; pdu[n++] = c * 2;
      for (uint16_t i = 0; i < c; i++) {
        uint16_t v = (uint16_t)(vals[i] | 0);
        pdu[n++] = v >> 8; pdu[n++] = v & 0xFF;
      }
      return n;
    }
    default:
      if (err) *err = "funcion no soportada (1-6, 15, 16)";
      return 0;
  }
}

void mbResultJson(const GwResult& r, bool completed, uint8_t fc, uint16_t qty, JsonObject d) {
  bool ok = completed && !r.gwExc && (r.res == RTU_OK || r.res == RTU_BCAST);
  d["port"] = r.port + 1;
  d["lat"] = r.lat;
  if (r.cached) { d["cached"] = true; d["age"] = r.ageMs; }
  if (!completed)      d["result"] = "sin respuesta a tiempo";
  else if (r.gwExc)    d["result"] = r.gwExc == 0x01 ? "escritura bloqueada (solo lectura)" : "sin ruta / puerto ocupado";
  else                 d["result"] = rtuResultStr(r.res);
  if (r.gwExc) d["exception"] = r.gwExc;

  char hex[RTU_MAX_ADU * 3 + 1]; size_t p = 0;
  for (size_t i = 0; i < r.len; i++) p += snprintf(hex + p, sizeof(hex) - p, i ? " %02X" : "%02X", r.resp[i]);
  hex[p] = 0;
  if (r.len) d["raw"] = hex;

  if (ok && r.res == RTU_OK && r.len >= 2) {
    if (r.resp[1] & 0x80) {
      ok = false;
      d["exception"] = r.resp[2];
    } else if (fc <= 4 && r.len >= 3) {
      JsonArray v = d["values"].to<JsonArray>();
      uint8_t bc = r.resp[2];
      if (fc <= 2) {
        for (uint16_t i = 0; i < qty && (i >> 3) < bc; i++) v.add((r.resp[3 + (i >> 3)] >> (i & 7)) & 1);
      } else {
        for (uint16_t i = 0; i + 1 < bc && 4 + i < r.len; i += 2) v.add((uint16_t)((r.resp[3 + i] << 8) | r.resp[4 + i]));
      }
    }
  }
  d["ok"] = ok;
}
