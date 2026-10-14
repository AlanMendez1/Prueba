// CalculadoraIA — asignar el nombre de cada tecla de la Casio desde el Monitor Serie.
// Placa: ESP32S3 Dev Module · USB CDC On Boot: Disabled · Puerto: conector COM del DevKit
// Monitor Serie a 115200 (cualquier final de línea sirve).
//
// 1. Presiona una tecla de la Casio: aparece su par de pines (por ejemplo IO15 + IO41).
// 2. Escribe el NÚMERO de la lista (o un nombre propio) y Enter: queda asignada.
// 3. Si la tecla ya tiene nombre, se muestra. Para cambiarlo, escribe otro.
// Lo asignado se guarda en la memoria del ESP32: no se pierde al reiniciar ni al desconectar.
//
// Comandos (escríbelos y Enter):
//   lista        muestra la lista de teclas de la fx-CG50 y cuáles ya asignaste
//   faltan       muestra las teclas que todavía no tienen pines
//   tabla        imprime la tabla final para copiarla y pegarla (o pasármela)
//   borrar todo  empieza de cero

#include <Preferences.h>

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

// Teclas de la fx-CG50, de arriba hacia abajo y de izquierda a derecha.
const char* const KEY_NAMES[] = {
  "F1", "F2", "F3", "F4", "F5", "F6",                         //  1-6
  "SHIFT", "OPTN", "VARS", "MENU",                            //  7-10
  "IZQUIERDA", "ARRIBA", "ABAJO", "DERECHA",                  // 11-14
  "ALPHA", "x^2", "^", "EXIT",                                // 15-18
  "X,theta,T", "log", "ln", "sin", "cos", "tan",              // 19-24
  "fraccion", "S<>D", "(", ")", "coma", "->",                 // 25-30
  "7", "8", "9", "DEL", "AC/ON",                              // 31-35
  "4", "5", "6", "x", "/",                                    // 36-40
  "1", "2", "3", "+", "-",                                    // 41-45
  "0", ".", "x10^x", "(-)", "EXE",                            // 46-50
};
const int KEY_COUNT = sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]);

const uint32_t SCAN_MS = 5;     // cada cuánto se revisa el teclado
const int STABLE_SCANS = 6;     // lecturas iguales seguidas para aceptar (6 x 5 ms = 30 ms)
const uint32_t SETTLE_US = 30;  // espera a que el pin baje antes de leer
const uint32_t LINE_TIMEOUT_MS = 300;  // por si el Monitor Serie no manda Enter

struct Mapping {
  uint8_t a, b;  // GPIO, a < b
  char name[16];
};
const int MAX_MAP = 80;
Mapping maps[MAX_MAP];
int mapCount = 0;
Preferences prefs;

bool fixedPair[N][N];  // pines unidos sin tocar nada (VCC/GND de la Casio): se ignoran
int hitA = -1, hitB = -1;
int lastA = -1, lastB = -1;  // GPIO de la última tecla presionada
String line;
uint32_t lastCharMs = 0;

// ---------------------------------------------------------------------------------------
// Teclado
// ---------------------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------------------
// Asignaciones (guardadas en la memoria del ESP32)
// ---------------------------------------------------------------------------------------

int findByPins(int a, int b) {
  for (int i = 0; i < mapCount; i++) {
    if (maps[i].a == a && maps[i].b == b) return i;
  }
  return -1;
}

int findByName(const char* name) {
  for (int i = 0; i < mapCount; i++) {
    if (strcasecmp(maps[i].name, name) == 0) return i;
  }
  return -1;
}

void removeAt(int i) {
  for (int k = i; k < mapCount - 1; k++) maps[k] = maps[k + 1];
  mapCount--;
}

void save() {
  prefs.putInt("count", mapCount);
  if (mapCount > 0) prefs.putBytes("maps", maps, sizeof(Mapping) * mapCount);
}

void load() {
  mapCount = prefs.getInt("count", 0);
  if (mapCount < 0 || mapCount > MAX_MAP) mapCount = 0;
  if (mapCount > 0 && prefs.getBytes("maps", maps, sizeof(Mapping) * mapCount) != sizeof(Mapping) * mapCount) {
    mapCount = 0;
  }
}

void printList() {
  Serial.println();
  Serial.println("---- Teclas de la fx-CG50 (escribe el numero) ----");
  for (int i = 0; i < KEY_COUNT; i++) {
    const int m = findByName(KEY_NAMES[i]);
    char pins[16] = "-";
    if (m >= 0) snprintf(pins, sizeof(pins), "IO%d+IO%d", maps[m].a, maps[m].b);
    Serial.printf("%2d %-10s %-12s", i + 1, KEY_NAMES[i], pins);
    if (i % 3 == 2 || i == KEY_COUNT - 1) Serial.println();
  }
  Serial.printf("Asignadas: %d\n\n", mapCount);
}

void printMissing() {
  Serial.print("\nFaltan: ");
  int missing = 0;
  for (int i = 0; i < KEY_COUNT; i++) {
    if (findByName(KEY_NAMES[i]) < 0) {
      Serial.printf("%s%d %s", missing ? ", " : "", i + 1, KEY_NAMES[i]);
      missing++;
    }
  }
  if (missing == 0) Serial.print("ninguna");
  Serial.printf("\n(%d teclas)\n\n", missing);
}

void printTable() {
  Serial.println("\n// ---- copia desde aqui ----");
  Serial.println("const KeyName KEYS[] = {");
  for (int i = 0; i < mapCount; i++) {
    Serial.printf("  {\"IO%d\", \"IO%d\", \"%s\"},\n", maps[i].a, maps[i].b, maps[i].name);
  }
  Serial.println("  {nullptr, nullptr, nullptr},");
  Serial.println("};");
  Serial.println("// ---- hasta aqui ----\n");
}

void assign(String text) {
  if (lastA < 0) {
    Serial.println("Primero presiona una tecla de la Casio.");
    return;
  }
  char name[16];
  bool isNumber = text.length() > 0;
  for (size_t i = 0; i < text.length(); i++) isNumber = isNumber && isDigit(text[i]);
  if (isNumber) {
    const int n = text.toInt();
    if (n < 1 || n > KEY_COUNT) {
      Serial.printf("No hay tecla %d en la lista (1 a %d). Escribe 'lista' para verla.\n", n, KEY_COUNT);
      return;
    }
    strlcpy(name, KEY_NAMES[n - 1], sizeof(name));
  } else {
    strlcpy(name, text.c_str(), sizeof(name));
  }

  // Un nombre solo puede estar en una tecla: si ya estaba en otra, se mueve.
  const int other = findByName(name);
  if (other >= 0 && !(maps[other].a == lastA && maps[other].b == lastB)) {
    Serial.printf("(\"%s\" estaba en IO%d + IO%d: lo muevo a esta tecla)\n", name, maps[other].a, maps[other].b);
    removeAt(other);
  }
  int m = findByPins(lastA, lastB);
  if (m < 0) {
    if (mapCount >= MAX_MAP) {
      Serial.println("Ya no caben mas teclas. Usa 'borrar todo'.");
      return;
    }
    m = mapCount++;
    maps[m].a = lastA;
    maps[m].b = lastB;
  }
  strlcpy(maps[m].name, name, sizeof(maps[m].name));
  save();
  Serial.printf("Guardado: IO%d + IO%d = %s   (llevas %d de %d)\n\n", lastA, lastB, name, mapCount, KEY_COUNT);
}

void handleLine(String text) {
  text.trim();
  if (text.length() == 0) return;
  String cmd = text;
  cmd.toLowerCase();
  if (cmd == "lista" || cmd == "?") {
    printList();
  } else if (cmd == "faltan") {
    printMissing();
  } else if (cmd == "tabla") {
    printTable();
  } else if (cmd == "borrar todo") {
    mapCount = 0;
    save();
    Serial.println("Borrado. Empieza de nuevo.\n");
  } else {
    assign(text);
  }
}

void readSerial() {
  while (Serial.available()) {
    const char c = Serial.read();
    lastCharMs = millis();
    if (c == '\r' || c == '\n') {
      handleLine(line);
      line = "";
    } else if (line.length() < 40) {
      line += c;
    }
  }
  if (line.length() > 0 && millis() - lastCharMs > LINE_TIMEOUT_MS) {
    handleLine(line);
    line = "";
  }
}

// ---------------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < N; i++) pinMode(PINS[i], INPUT_PULLUP);  // todos ENTRADA
  prefs.begin("casio", false);
  load();

  Serial.println();
  Serial.println("=== Asignar teclas de la Casio ===");
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
  printList();
  Serial.println("Presiona una tecla y escribe su numero de la lista (o un nombre) + Enter.");
  Serial.println("Comandos: lista, faltan, tabla, borrar todo\n");
}

void loop() {
  static int lastRaw = -2;  // -1 = nada presionado
  static int stable = 0;
  static int shown = -1;
  static uint32_t lastScan = 0;

  readSerial();
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
  if (raw < 0) return;  // tecla soltada

  lastA = min(PINS[raw / N], PINS[raw % N]);
  lastB = max(PINS[raw / N], PINS[raw % N]);
  const int m = findByPins(lastA, lastB);
  if (m >= 0) {
    Serial.printf(">> %-10s (IO%d + IO%d)   [escribe otro numero/nombre para cambiarla]\n", maps[m].name, lastA, lastB);
  } else {
    Serial.printf(">> NUEVA: IO%d + IO%d  ->  escribe su numero (1-%d) o un nombre y Enter\n", lastA, lastB, KEY_COUNT);
  }
}
