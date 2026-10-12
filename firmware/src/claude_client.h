#pragma once
#include <Arduino.h>

struct ClaudeReply {
  bool ok;
  String text;  // respuesta (o mensaje de error) ya convertida a ASCII para la pantalla
};

// Envía una pregunta a Claude por la Messages API. Bloquea hasta tener respuesta.
ClaudeReply askClaude(const String& prompt);
