#include "keypad.h"

#include <Arduino.h>

#include "config.h"

namespace {

constexpr uint32_t SCAN_PERIOD_MS = 5;
constexpr uint32_t DEBOUNCE_MS = 20;
constexpr uint32_t REPEAT_DELAY_MS = 450;
constexpr uint32_t REPEAT_RATE_MS = 120;

// Cableado lógico fila/columna (ver README). R4C4 queda libre.
const Key KEYMAP[KEY_ROWS][KEY_COLS] = {
    {Key::D7, Key::D8, Key::D9, Key::Div, Key::AC},
    {Key::D4, Key::D5, Key::D6, Key::Mul, Key::Del},
    {Key::D1, Key::D2, Key::D3, Key::Sub, Key::F1},
    {Key::D0, Key::Dot, Key::Eq, Key::Add, Key::F2},
    {Key::Left, Key::Up, Key::Down, Key::Right, Key::None},
};

bool repeats(Key k) {
  return k == Key::Up || k == Key::Down || k == Key::Left || k == Key::Right || k == Key::Del;
}

}  // namespace

void Keypad::begin() {
  for (uint8_t pin : COL_PINS) pinMode(pin, INPUT_PULLUP);
  for (uint8_t pin : ROW_PINS) pinMode(pin, INPUT);  // alta impedancia salvo al escanear
}

// Devuelve la primera tecla presionada; el prototipo no necesita combinaciones.
Key Keypad::scan() {
  Key found = Key::None;
  for (uint8_t r = 0; r < KEY_ROWS && found == Key::None; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(10);
    for (uint8_t c = 0; c < KEY_COLS; c++) {
      if (digitalRead(COL_PINS[c]) == LOW && KEYMAP[r][c] != Key::None) {
        found = KEYMAP[r][c];
        break;
      }
    }
    pinMode(ROW_PINS[r], INPUT);
  }
  return found;
}

Key Keypad::poll() {
  const uint32_t now = millis();
  if (now - lastScan_ < SCAN_PERIOD_MS) return Key::None;
  lastScan_ = now;

  const Key raw = scan();
  if (raw != candidate_) {
    candidate_ = raw;
    candidateSince_ = now;
    return Key::None;
  }
  if (now - candidateSince_ < DEBOUNCE_MS) return Key::None;

  if (candidate_ != stable_) {
    stable_ = candidate_;
    nextRepeat_ = now + REPEAT_DELAY_MS;
    return stable_;  // Key::None al soltar
  }
  if (stable_ != Key::None && repeats(stable_) && static_cast<int32_t>(now - nextRepeat_) >= 0) {
    nextRepeat_ = now + REPEAT_RATE_MS;
    return stable_;
  }
  return Key::None;
}
