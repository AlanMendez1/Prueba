#pragma once
#include <stdint.h>

// Las 24 teclas del prototipo mínimo (matriz 5x5, una celda libre).
enum class Key : uint8_t {
  None,
  D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
  Add, Sub, Mul, Div,
  Dot, Eq, AC, Del,
  Up, Down, Left, Right,
  F1, F2,
};

inline bool isDigitKey(Key k) { return k >= Key::D0 && k <= Key::D9; }

inline char digitChar(Key k) {
  return static_cast<char>('0' + (static_cast<uint8_t>(k) - static_cast<uint8_t>(Key::D0)));
}
