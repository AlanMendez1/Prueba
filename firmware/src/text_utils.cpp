#include "text_utils.h"

#include <stdint.h>

namespace {

const char* asciiFor(uint32_t cp) {
  switch (cp) {
    case 0xE1: case 0xE0: case 0xE4: case 0xE2: return "a";
    case 0xE9: case 0xE8: case 0xEB: case 0xEA: return "e";
    case 0xED: case 0xEC: case 0xEF: case 0xEE: return "i";
    case 0xF3: case 0xF2: case 0xF6: case 0xF4: return "o";
    case 0xFA: case 0xF9: case 0xFC: case 0xFB: return "u";
    case 0xC1: case 0xC0: case 0xC4: case 0xC2: return "A";
    case 0xC9: case 0xC8: case 0xCB: case 0xCA: return "E";
    case 0xCD: case 0xCC: case 0xCF: case 0xCE: return "I";
    case 0xD3: case 0xD2: case 0xD6: case 0xD4: return "O";
    case 0xDA: case 0xD9: case 0xDC: case 0xDB: return "U";
    case 0xF1: return "n";
    case 0xD1: return "N";
    case 0xE7: return "c";
    case 0xC7: return "C";
    case 0xBF: case 0xA1: return "";  // ¿ ¡
    case 0xD7: return "x";            // ×
    case 0xF7: return "/";            // ÷
    case 0xB7: case 0x22C5: return "*";
    case 0xB2: return "^2";
    case 0xB3: return "^3";
    case 0xB0: return " grados";
    case 0xBD: return "1/2";
    case 0xA0: case 0x202F: return " ";
    case 0x2212: case 0x2013: case 0x2014: case 0x2022: return "-";
    case 0x2018: case 0x2019: return "'";
    case 0x201C: case 0x201D: case 0xAB: case 0xBB: return "\"";
    case 0x2026: return "...";
    case 0x2192: return "->";
    case 0x2248: return "~";
    case 0x2260: return "!=";
    case 0x2264: return "<=";
    case 0x2265: return ">=";
    case 0x221A: return "raiz";
    case 0x221E: return "infinito";
    case 0x03C0: return "pi";
    default: return "?";
  }
}

}  // namespace

std::string utf8ToAscii(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  size_t i = 0;
  while (i < in.size()) {
    const unsigned char c = in[i];
    if (c < 0x80) {
      if (c == '\t') {
        out += ' ';
      } else if (c >= 0x20 || c == '\n') {
        out += static_cast<char>(c);
      }
      i++;
      continue;
    }

    uint32_t cp;
    size_t len;
    if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      len = 2;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      len = 3;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      len = 4;
    } else {
      out += '?';
      i++;
      continue;
    }
    if (i + len > in.size()) {
      out += '?';
      break;
    }
    bool valid = true;
    for (size_t k = 1; k < len; k++) {
      const unsigned char cc = in[i + k];
      if ((cc & 0xC0) != 0x80) {
        valid = false;
        break;
      }
      cp = (cp << 6) | (cc & 0x3F);
    }
    if (!valid) {
      out += '?';
      i++;
      continue;
    }
    out += asciiFor(cp);
    i += len;
  }
  return out;
}
