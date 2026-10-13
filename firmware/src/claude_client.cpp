#include "claude_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "text_utils.h"

namespace {

const char SYSTEM_PROMPT[] =
    "Eres el asistente integrado en una calculadora de bolsillo con una pantalla pequeña "
    "(unas 40 columnas de texto). Responde en español y en texto plano: sin Markdown, "
    "sin LaTeX, sin tablas y sin emojis. Escribe * para multiplicar, / para dividir y ^ "
    "para potencias. Sé breve: 120 palabras como máximo, en frases cortas.";

bool waitForWifi(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    delay(100);
  }
  return true;
}

String buildBody(const String& prompt) {
  JsonDocument doc;
  doc["model"] = CLAUDE_MODEL;
  doc["max_tokens"] = CLAUDE_MAX_TOKENS;
  doc["output_config"]["effort"] = CLAUDE_EFFORT;
  // Si el modelo declina la consulta, la API la reintenta sola en otro modelo
  // (requiere el header anthropic-beta de abajo).
  doc["fallbacks"] = "default";
  doc["system"] = SYSTEM_PROMPT;
  JsonObject msg = doc["messages"].add<JsonObject>();
  msg["role"] = "user";
  msg["content"] = prompt;
  String body;
  serializeJson(doc, body);
  return body;
}

ClaudeReply fail(const String& message) {
  Serial.println("[claude] " + message);
  return {false, String(utf8ToAscii(message.c_str()).c_str())};
}

}  // namespace

ClaudeReply askClaude(const String& prompt) {
  if (!waitForWifi(10000)) return fail("Sin WiFi. Revisa WIFI_SSID y WIFI_PASSWORD en secrets.h.");

  WiFiClientSecure client;
#ifdef ANTHROPIC_ROOT_CA
  client.setCACert(ANTHROPIC_ROOT_CA);
#else
  client.setInsecure();  // solo para la prueba: va cifrado, pero no se verifica el servidor
#endif

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(CLAUDE_TIMEOUT_MS);
  if (!http.begin(client, CLAUDE_URL)) return fail("No se pudo abrir la conexion.");
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", ANTHROPIC_API_KEY);
  http.addHeader("anthropic-version", "2023-06-01");
  http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");

  const int status = http.POST(buildBody(prompt));
  if (status <= 0) {
    const String reason = HTTPClient::errorToString(status);
    http.end();
    return fail("Error de red: " + reason);
  }
  const String payload = http.getString();
  http.end();

  // Solo nos interesan estos campos de la respuesta.
  JsonDocument filter;
  filter["content"][0]["type"] = true;
  filter["content"][0]["text"] = true;
  filter["stop_reason"] = true;
  filter["error"]["message"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
    return fail("Respuesta ilegible (HTTP " + String(status) + ").");
  }

  if (status != 200) {
    const char* detail = doc["error"]["message"] | "sin detalle";
    return fail("Error HTTP " + String(status) + ": " + detail);
  }

  const char* stop = doc["stop_reason"] | "";
  if (strcmp(stop, "refusal") == 0) {
    return fail("Claude no respondio a esta consulta. Prueba con otra opcion.");
  }

  // La respuesta puede traer bloques "thinking" (vacíos) y "fallback"; solo se muestran los "text".
  std::string text;
  for (JsonObject block : doc["content"].as<JsonArray>()) {
    if (strcmp(block["type"] | "", "text") == 0) text += block["text"] | "";
  }
  if (text.empty()) return fail("Claude devolvio una respuesta vacia.");
  if (strcmp(stop, "max_tokens") == 0) text += " [...]";

  return {true, String(utf8ToAscii(text).c_str())};
}
