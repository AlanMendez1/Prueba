#pragma once
#include <string>

// Las fuentes de TFT_eSPI son solo ASCII: convierte el UTF-8 de la respuesta
// (acentos, ñ, ×, ÷, comillas tipográficas...) a ASCII legible. Lo desconocido → '?'.
std::string utf8ToAscii(const std::string& in);
