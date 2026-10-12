#pragma once
#include "keys.h"

// Escaneo de la matriz 5x5 con antirrebote y autorepetición (flechas y DEL).
class Keypad {
 public:
  void begin();
  // Devuelve una pulsación nueva (o una repetición) o Key::None. Llamar en cada loop().
  Key poll();

 private:
  Key scan();

  Key stable_ = Key::None;
  Key candidate_ = Key::None;
  uint32_t candidateSince_ = 0;
  uint32_t lastScan_ = 0;
  uint32_t nextRepeat_ = 0;
};
