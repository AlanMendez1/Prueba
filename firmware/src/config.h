#pragma once
#include <stdint.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Falta src/secrets.h: copia src/secrets.h.example como src/secrets.h y completa WiFi y API key"
#endif

// ---- Teclado: matriz 5x5 directa en GPIO (sin MCP23017) ----
// Filas = salidas (se ponen en LOW de a una); columnas = entradas con pull-up interno.
// Los pines de la TFT están en platformio.ini (flags de TFT_eSPI).
constexpr uint8_t KEY_ROWS = 5;
constexpr uint8_t KEY_COLS = 5;
constexpr uint8_t ROW_PINS[KEY_ROWS] = {4, 5, 6, 7, 15};
constexpr uint8_t COL_PINS[KEY_COLS] = {16, 17, 18, 8, 9};

// ---- Claude (Messages API) ----
constexpr const char* CLAUDE_URL = "https://api.anthropic.com/v1/messages";
constexpr const char* CLAUDE_MODEL = "claude-haiku-5-5";
constexpr const char* CLAUDE_EFFORT = "low";  // respuestas rápidas y baratas
constexpr int CLAUDE_MAX_TOKENS = 1024;       // incluye el razonamiento interno del modelo
constexpr uint16_t CLAUDE_TIMEOUT_MS = 45000;
