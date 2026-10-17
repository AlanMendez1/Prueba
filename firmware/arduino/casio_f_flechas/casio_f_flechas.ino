// CalculadoraIA — prueba de F1-F6 y las 4 flechas de la Casio (Monitor Serie a 115200).
// Placa: ESP32S3 Dev Module · USB CDC On Boot: Disabled.
// Presiona las 10 teclas: cada una aparece al presionarla y al final dice cuáles faltan.

struct Key {
  uint8_t row, col;
  const char* name;
};

// Filas: IO17 (F1-F6), IO40 (← ↑), IO16 (↓ →). Columnas: IO46, IO41, IO42, IO7, IO3, IO21.
const Key KEYS[] = {
  {17, 46, "F1"}, {17, 41, "F2"}, {17, 42, "F3"}, {17, 7, "F4"}, {17, 3, "F5"}, {17, 21, "F6"},
  {40, 3, "IZQUIERDA"}, {40, 21, "ARRIBA"}, {16, 3, "ABAJO"}, {16, 21, "DERECHA"},
};
const int KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);
const uint8_t ROWS[] = {17, 40, 16};
const uint8_t COLS[] = {46, 41, 42, 7, 3, 21};

const int STABLE_SCANS = 6;  // 6 x 5 ms = 30 ms de antirrebote
bool seen[KEY_COUNT];
int seenCount = 0;

// Devuelve el número de la tecla presionada, o -1 si no hay ninguna.
int readKey() {
  int found = -1;
  for (uint8_t row : ROWS) {
    pinMode(row, OUTPUT);
    digitalWrite(row, LOW);
    delayMicroseconds(30);
    for (int k = 0; k < KEY_COUNT && found < 0; k++) {
      if (KEYS[k].row == row && digitalRead(KEYS[k].col) == LOW) found = k;
    }
    pinMode(row, INPUT_PULLUP);
    delayMicroseconds(30);
    if (found >= 0) break;
  }
  return found;
}

void printMissing() {
  Serial.print("   Faltan: ");
  for (int k = 0; k < KEY_COUNT; k++) {
    if (!seen[k]) Serial.printf("%s ", KEYS[k].name);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  for (uint8_t p : ROWS) pinMode(p, INPUT_PULLUP);
  for (uint8_t p : COLS) pinMode(p, INPUT_PULLUP);
  Serial.println("\n=== Prueba F1-F6 y flechas ===");
  Serial.println("Presiona las 10 teclas una por una.");
  printMissing();
}

void loop() {
  static int lastRaw = -2, stable = 0, shown = -1;
  delay(5);
  const int key = readKey();
  if (key == lastRaw) {
    if (stable < STABLE_SCANS) stable++;
  } else {
    lastRaw = key;
    stable = 0;
  }
  if (stable != STABLE_SCANS || key == shown) return;
  shown = key;
  if (key < 0) return;

  Serial.printf("OK  %-10s (IO%d + IO%d)\n", KEYS[key].name, KEYS[key].row, KEYS[key].col);
  if (!seen[key]) {
    seen[key] = true;
    seenCount++;
    if (seenCount == KEY_COUNT) {
      Serial.println("\n*** Las 10 teclas funcionan ***\n");
    } else {
      printMissing();
    }
  }
}
