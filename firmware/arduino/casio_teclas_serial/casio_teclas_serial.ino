// CalculadoraIA — identificar las teclas de la Casio SOLO por el Monitor Serie (sin pantalla).
// Placa: ESP32S3 Dev Module · USB CDC On Boot: Disabled · Puerto: conector COM del DevKit
// Monitor Serie a 115200.
//
// Cómo funciona: todos los pines del cable plano empiezan como ENTRADA con pull-up (en HIGH).
// Uno por uno, cada pin pasa a SALIDA en LOW y se leen todos los demás: si alguno baja, hay
// una tecla uniendo esos dos pines. La lectura tiene que repetirse igual varias veces
// seguidas (antirrebote) antes de mostrarse, así cada pulsación sale una sola vez.

// ---------------------------------------------------------------------------------------
// Pines del DevKit donde puede haber un cable de la Casio. Si un pin no tiene cable, no pasa
// nada: nunca aparece. NUNCA pongas aquí 0, 45, 35, 36, 37, 43 ni 44.
// ---------------------------------------------------------------------------------------
const uint8_t PINS[] = {
  // lado derecho del DevKit
  4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14,
  // lado izquierdo del DevKit
  1, 2, 42, 41, 40, 39, 38, 48, 47, 21, 20, 19,
};
const int N = sizeof(PINS) / sizeof(PINS[0]);

const uint32_t SCAN_MS = 5;     // cada cuánto se revisa el teclado
const int STABLE_SCANS = 6;     // lecturas iguales seguidas para aceptar (6 x 5 ms = 30 ms)
const uint32_t SETTLE_US = 30;  // espera a que el pin baje antes de leer

bool fixedPair[N][N];  // pines unidos sin tocar nada (VCC/GND de la Casio): se ignoran
int keyNumber[N][N];   // número que se le da a cada tecla nueva (0 = todavía no vista)
int keysFound = 0;
int hitA = -1, hitB = -1;

// Busca el primer par de pines unidos que no sea de fábrica. Devuelve true si encontró uno.
bool scanKeys(bool learnFixed) {
  for (int i = 0; i < N; i++) {
    pinMode(PINS[i], OUTPUT);  // este pin pasa a SALIDA en LOW
    digitalWrite(PINS[i], LOW);
    delayMicroseconds(SETTLE_US);
    for (int j = 0; j < N; j++) {
      if (j == i || digitalRead(PINS[j]) == HIGH) continue;
      const int a = min(i, j), b = max(i, j);
      if (learnFixed) {
        fixedPair[a][b] = true;
      } else if (!fixedPair[a][b]) {
        pinMode(PINS[i], INPUT_PULLUP);
        hitA = a;
        hitB = b;
        return true;
      }
    }
    pinMode(PINS[i], INPUT_PULLUP);  // vuelve a ENTRADA
    delayMicroseconds(SETTLE_US);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < N; i++) pinMode(PINS[i], INPUT_PULLUP);  // todos ENTRADA

  Serial.println();
  Serial.println("=== Prueba de teclas de la Casio ===");
  Serial.println("No toques el teclado: revisando que pines vienen unidos de fabrica...");
  delay(1000);
  for (int round = 0; round < 3; round++) {
    scanKeys(true);
    delay(10);
  }
  for (int a = 0; a < N; a++) {
    for (int b = a + 1; b < N; b++) {
      if (fixedPair[a][b]) Serial.printf("  IO%d y IO%d estan unidos siempre (se ignoran)\n", PINS[a], PINS[b]);
    }
  }
  Serial.println("Listo. Presiona las teclas UNA POR UNA.");
  Serial.println();
}

void loop() {
  static int lastRaw = -2;  // -1 = nada presionado
  static int stable = 0;
  static int shown = -1;
  static uint32_t lastScan = 0;

  if (millis() - lastScan < SCAN_MS) return;
  lastScan = millis();

  const int raw = scanKeys(false) ? hitA * N + hitB : -1;
  if (raw == lastRaw) {
    if (stable < STABLE_SCANS) stable++;
  } else {
    lastRaw = raw;
    stable = 0;
  }
  if (stable != STABLE_SCANS || raw == shown) return;  // todavía no está estable
  shown = raw;

  if (raw < 0) {
    Serial.println("        (soltada)");
    return;
  }
  const int a = raw / N, b = raw % N;
  const bool isNew = keyNumber[a][b] == 0;
  if (isNew) keyNumber[a][b] = ++keysFound;
  Serial.printf("Tecla #%-3d IO%-2d + IO%-2d  %s  ->  {\"IO%d\", \"IO%d\", \"?\"},\n", keyNumber[a][b], PINS[a],
                PINS[b], isNew ? "NUEVA " : "repite", PINS[a], PINS[b]);
}
