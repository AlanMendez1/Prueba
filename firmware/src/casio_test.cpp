// Prueba del teclado ORIGINAL de la Casio (conector CN701 de la placa de teclas).
// Solo se compila en el entorno casio_test:  pio run -e casio_test -t upload
//
// No hace falta saber cuáles líneas son filas y cuáles columnas: el programa pone en LOW
// una línea a la vez y mira cuál de las demás baja. Cada tecla une dos líneas, así que cada
// tecla aparece como un par ("T710 + T719") con su propio símbolo en la pantalla.
//
// Para ponerle nombre a una tecla: presiónala, copia la línea que sale en el monitor serie
// (por ejemplo  {"T710", "T719", "?"},) dentro de KEYS (más abajo) y cambia "?" por el nombre.

#ifdef CASIO_KEY_TEST

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>

namespace {

// ---------------------------------------------------------------------------------------
// 1) CABLES: línea del conector de la Casio -> pin del ESP32.
//    "name" es lo que aparece en la pantalla; "cn" es el número de pin del conector (hoja).
// ---------------------------------------------------------------------------------------
struct Line {
  uint8_t gpio;
  const char* name;
  uint8_t cn;
};

const Line LINES[] = {
    {4, "T710", 4},   {5, "T703", 5},   {6, "T706", 6},   {7, "T716", 7},
    {15, "T707", 8},  {16, "P9", 9},    {17, "P10", 10},  {18, "T719", 11},
    {8, "T708", 12},  {9, "T711", 13},  {1, "T712", 14},  {2, "T713", 15},
    {42, "T714", 16}, {41, "T705", 17}, {40, "T704", 18}, {39, "T709", 19},
    {47, "VCC", 2},  // por si alguna tecla (por ejemplo AC/ON) va a VCC
};
constexpr int N = sizeof(LINES) / sizeof(LINES[0]);
constexpr int GND = N;    // los pines GND de la Casio van al GND del ESP32
constexpr int NODES = N + 1;
constexpr int PAIRS = NODES * (NODES - 1) / 2;

// ---------------------------------------------------------------------------------------
// 2) NOMBRES DE LAS TECLAS: agrega aquí cada tecla que identifiques.
//    El orden de las dos líneas no importa. Usa solo letras sin tilde, números y signos.
// ---------------------------------------------------------------------------------------
struct KeyName {
  const char* a;
  const char* b;
  const char* label;
};

const KeyName KEYS[] = {
    // {"T710", "T719", "SHIFT"},   <- ejemplo: quita las // y cambia los valores
    {nullptr, nullptr, nullptr},  // no borres esta línea
};

// ---------------------------------------------------------------------------------------

constexpr uint32_t SCAN_MS = 5;    // cada cuánto se revisa el teclado
constexpr int STABLE_SCANS = 4;    // lecturas iguales seguidas para aceptar (antirrebote)
constexpr uint32_t SETTLE_US = 30; // espera a que la línea baje antes de leer

TFT_eSPI tft;
bool deadLine[NODES];         // línea pegada a GND desde el arranque: se ignora
bool fixedPair[PAIRS];        // par unido sin tocar nada: se ignora
bool seen[PAIRS];             // teclas distintas encontradas en esta sesión
int seenCount = 0;

int pairIndex(int a, int b) {
  if (a > b) std::swap(a, b);
  return a * (2 * NODES - a - 1) / 2 + (b - a - 1);
}

const char* nodeName(int i) { return i == GND ? "GND" : LINES[i].name; }

// Símbolo automático distinto para cada par: ASCII 33..126 y luego 128.. de la fuente GLCD.
uint16_t autoSymbol(int idx) { return idx < 94 ? 33 + idx : 128 + (idx - 94); }

const char* userLabel(int a, int b) {
  for (const KeyName* k = KEYS; k->a != nullptr; k++) {
    if ((strcmp(k->a, nodeName(a)) == 0 && strcmp(k->b, nodeName(b)) == 0) ||
        (strcmp(k->a, nodeName(b)) == 0 && strcmp(k->b, nodeName(a)) == 0)) {
      return k->label;
    }
  }
  return nullptr;
}

// Recorre el teclado y llama a found(a, b) por cada par unido. Si found devuelve true, para.
template <typename F>
void scan(F found) {
  // Teclas que unen una línea con GND: se ven sin poner ninguna línea en LOW.
  for (int j = 0; j < N; j++) {
    if (!deadLine[j] && digitalRead(LINES[j].gpio) == LOW && found(j, GND)) return;
  }
  for (int i = 0; i < N; i++) {
    if (deadLine[i]) continue;
    pinMode(LINES[i].gpio, OUTPUT);
    digitalWrite(LINES[i].gpio, LOW);
    delayMicroseconds(SETTLE_US);
    bool stop = false;
    for (int j = 0; j < N && !stop; j++) {
      if (j == i || deadLine[j]) continue;
      if (digitalRead(LINES[j].gpio) == LOW) stop = found(i, j);
    }
    pinMode(LINES[i].gpio, INPUT_PULLUP);
    delayMicroseconds(SETTLE_US);
    if (stop) return;
  }
}

// Al arrancar (sin tocar teclas) aprende qué está unido de fábrica para no confundirlo.
void calibrate() {
  for (int round = 0; round < 3; round++) {
    for (int j = 0; j < N; j++) {
      if (digitalRead(LINES[j].gpio) == LOW) deadLine[j] = true;
    }
    scan([](int a, int b) {
      fixedPair[pairIndex(a, b)] = true;
      return false;
    });
    delay(10);
  }
  for (int j = 0; j < N; j++) {
    if (deadLine[j]) Serial.printf("[calibrar] %s (IO%d) esta unida a GND: se ignora\n", nodeName(j), LINES[j].gpio);
  }
  for (int a = 0; a < NODES; a++) {
    for (int b = a + 1; b < NODES; b++) {
      if (fixedPair[pairIndex(a, b)]) Serial.printf("[calibrar] %s y %s estan unidas sin tocar nada: se ignoran\n", nodeName(a), nodeName(b));
    }
  }
}

// Devuelve el primer par presionado, o -1 si no hay ninguna tecla.
int readKey(int& a, int& b) {
  int key = -1;
  scan([&](int x, int y) {
    const int idx = pairIndex(x, y);
    if (fixedPair[idx]) return false;
    a = std::min(x, y);
    b = std::max(x, y);
    key = idx;
    return true;
  });
  return key;
}

void drawFrame() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, tft.width(), 20, TFT_NAVY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("PRUEBA TECLADO CASIO", 6, 10, 2);
}

void drawFooter() {
  const int y = tft.height() - 18;
  tft.fillRect(0, y, tft.width(), 18, TFT_NAVY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  char text[48];
  snprintf(text, sizeof(text), "Teclas distintas: %d", seenCount);
  tft.drawString(text, 6, y + 9, 2);
}

void showKey(int a, int b, int idx) {
  const int w = tft.width();
  tft.fillRect(0, 20, w, tft.height() - 38, TFT_BLACK);

  const char* label = userLabel(a, b);
  if (label != nullptr) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(strlen(label) <= 5 ? 2 : 1);
    tft.drawString(label, w / 2, 80, 4);
    tft.setTextSize(1);
  } else {
    constexpr int size = 9;  // 6x8 px por 9 = 54x72 px
    tft.drawChar(w / 2 - 3 * size, 80 - 4 * size, autoSymbol(idx), TFT_YELLOW, TFT_BLACK, size);
  }

  char text[64];
  snprintf(text, sizeof(text), "%s + %s", nodeName(a), nodeName(b));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(text, w / 2, 150, 4);

  const int gpioA = LINES[a].gpio;
  if (b == GND) {
    snprintf(text, sizeof(text), "pin %d  |  IO%d + GND", LINES[a].cn, gpioA);
  } else {
    snprintf(text, sizeof(text), "pines %d y %d  |  IO%d + IO%d", LINES[a].cn, LINES[b].cn, gpioA, LINES[b].gpio);
  }
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(text, w / 2, 182, 2);
  tft.setTextColor(label ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
  tft.drawString(label ? "(nombre tuyo)" : "(sin nombre: agregala en KEYS)", w / 2, 202, 2);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[casio] prueba del teclado de la Casio");
  for (int i = 0; i < N; i++) pinMode(LINES[i].gpio, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);
  drawFrame();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("No toques el teclado...", tft.width() / 2, 110, 4);
  delay(500);
  calibrate();

  drawFrame();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Presiona una tecla", tft.width() / 2, 110, 4);
  drawFooter();
  Serial.println("[casio] listo: presiona las teclas una por una");
}

void loop() {
  static int lastRaw = -1;
  static int stable = 0;
  static int shown = -1;
  static uint32_t lastScan = 0;

  if (millis() - lastScan < SCAN_MS) return;
  lastScan = millis();

  int a = 0, b = 0;
  const int key = readKey(a, b);
  if (key == lastRaw) {
    if (stable < STABLE_SCANS) stable++;
  } else {
    lastRaw = key;
    stable = 0;
  }
  if (stable != STABLE_SCANS || key == shown) return;
  shown = key;
  if (key < 0) return;  // tecla soltada

  if (!seen[key]) {
    seen[key] = true;
    seenCount++;
    drawFooter();
  }
  showKey(a, b, key);
  const char* label = userLabel(a, b);
  Serial.printf("[tecla] %-5s + %-5s  ->  {\"%s\", \"%s\", \"%s\"},\n", nodeName(a), nodeName(b), nodeName(a),
                nodeName(b), label ? label : "?");
}

#endif  // CASIO_KEY_TEST
