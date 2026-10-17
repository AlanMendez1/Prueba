// CalculadoraIA — saber qué punto T de la placa de la Casio va a qué pin del ESP32.
// Placa: ESP32S3 Dev Module · USB CDC On Boot: Disabled · Monitor Serie a 115200.
//
// Cómo se usa: conecta un cable a un pin GND del ESP32 y con la otra punta toca un punto T
// (los circulitos de cobre T701, T703...). El pin del ESP32 unido a ese punto baja a LOW y
// aparece en el Monitor Serie, por ejemplo:  "Tocando: IO17".

const uint8_t PINS[] = {
  // lado derecho del DevKit
  4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14,
  // lado izquierdo del DevKit
  1, 2, 42, 41, 40, 39, 38, 48, 47, 21, 20, 19,
};
const int N = sizeof(PINS) / sizeof(PINS[0]);
const int STABLE_SCANS = 4;  // lecturas iguales seguidas (4 x 5 ms = 20 ms)

uint64_t ignored = 0;  // pines que ya estaban en LOW al arrancar

uint64_t readLowPins() {
  uint64_t mask = 0;
  for (int i = 0; i < N; i++) {
    if (digitalRead(PINS[i]) == LOW) mask |= (uint64_t)1 << i;
  }
  return mask & ~ignored;
}

void printPins(uint64_t mask) {
  bool first = true;
  for (int i = 0; i < N; i++) {
    if (mask & ((uint64_t)1 << i)) {
      Serial.printf("%sIO%d", first ? "" : " + ", PINS[i]);
      first = false;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < N; i++) pinMode(PINS[i], INPUT_PULLUP);
  delay(50);
  ignored = readLowPins();

  Serial.println("\n=== Buscar puntos T de la Casio ===");
  if (ignored) {
    Serial.print("Ya estaban en LOW (se ignoran): ");
    printPins(ignored);
    Serial.println();
  }
  Serial.println("Toca cada punto T con un cable conectado a GND del ESP32.\n");
}

void loop() {
  static uint64_t last = 0, shown = 0;
  static int stable = 0;
  delay(5);

  const uint64_t now = readLowPins();
  if (now == last) {
    if (stable < STABLE_SCANS) stable++;
  } else {
    last = now;
    stable = 0;
  }
  if (stable != STABLE_SCANS || now == shown) return;
  shown = now;
  if (now == 0) return;  // soltaste el cable

  Serial.print("Tocando: ");
  printPins(now);
  Serial.println();
}
