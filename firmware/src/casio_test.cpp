// Prueba del teclado ORIGINAL de la Casio (conector CN701 de la placa de teclas).
// Solo se compila en el entorno casio_test:  pio run -e casio_test -t upload
//
// No hace falta saber cuáles líneas son filas y cuáles columnas: el programa pone en LOW
// una línea a la vez y mira cuál de las demás baja. Cada tecla une dos líneas, así que cada
// tecla aparece como un par ("IO5 + IO18") con su propio símbolo en la pantalla.
//
// Para ponerle nombre a una tecla: presiónala, copia la línea que sale en el monitor serie
// (por ejemplo  {"IO5", "IO18", "?"},) dentro de KEYS (más abajo) y cambia "?" por el nombre.

#ifdef CASIO_KEY_TEST

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>

namespace {

// ---------------------------------------------------------------------------------------
// 1) CABLES: el cable plano de la Casio va directo a estos pines del DevKit.
//    Cada línea se llama como su pin (IO5, IO18...). Si algún pin de la lista no tiene
//    cable, no pasa nada: simplemente nunca aparece.
//    NUNCA uses 0, 45, 35, 36 ni 37 (arranque y memoria PSRAM de la placa).
// ---------------------------------------------------------------------------------------
struct Line {
  uint8_t gpio;
  const char* name;
};

const Line LINES[] = {
  // lado derecho del DevKit (el de los pines 4 a 13)
  {5, "IO5"}, {6, "IO6"}, {7, "IO7"}, {15, "IO15"}, {16, "IO16"},
  {17, "IO17"}, {18, "IO18"}, {8, "IO8"}, {3, "IO3"}, {46, "IO46"},
  // lado izquierdo del DevKit (el de los pines 1 a 21)
  {1, "IO1"}, {2, "IO2"}, {42, "IO42"}, {41, "IO41"}, {40, "IO40"}, {39, "IO39"},
  {38, "IO38"}, {48, "IO48"}, {47, "IO47"}, {21, "IO21"}, {20, "IO20"}, {19, "IO19"},
};

constexpr int N = sizeof(LINES) / sizeof(LINES[0]);
constexpr int GND = N;    // por si algún día conectas un GND de la Casio al GND del ESP32
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
    // {"IO5", "IO18", "SHIFT"},   <- ejemplo: quita las // y cambia los valores
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

// Símbolo automático distinto para cada par: la fuente básica tiene 253 dibujos distintos
// (33..127, 128..254 y 1..31), uno por cada par posible.
uint16_t autoSymbol(int idx) {
  if (idx < 95) return 33 + idx;
  if (idx < 222) return 128 + (idx - 95);
  return 1 + (idx - 222);
}

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

  tft.setTextColor(label ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
  tft.drawString(label ? "(nombre tuyo)" : "(sin nombre: agregala en KEYS)", w / 2, 185, 2);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[casio] prueba del teclado de la Casio");
  tft.init();
  tft.setRotation(1);
  tft.setAttribute(CP437_SWITCH, true);
  // Después de tft.init(): el pin 21 (retroiluminación) ahora es una línea del teclado.
  for (int i = 0; i < N; i++) pinMode(LINES[i].gpio, INPUT_PULLUP);
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
