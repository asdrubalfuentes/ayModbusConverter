#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "gateway.h"

// Utilidades comunes a la web, la API REST y MQTT.

// Arma la PDU desde {fc, addr, qty, value, values}. FC 1-6, 15, 16.
// Devuelve el largo (0 = error, *err con el motivo).
size_t mbBuildPdu(JsonVariantConst q, uint8_t* pdu, const char** err);

// Traduce el resultado a JSON: ok, result, lat, cached, age, raw, exception, values.
void mbResultJson(const GwResult& r, bool completed, uint8_t fc, uint16_t qty, JsonObject d);
