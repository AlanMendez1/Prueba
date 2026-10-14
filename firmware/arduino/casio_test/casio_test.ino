// CalculadoraIA — prueba del teclado ORIGINAL de la Casio (versión para Arduino IDE).
// Es el mismo programa que firmware/src/casio_test.cpp, pero con la librería de Adafruit
// para la pantalla (se configura aquí mismo, sin editar archivos de la librería).
//
// Librería: Herramientas > Administrar bibliotecas > "Adafruit ST7735 and ST7789 Library"
//           (cuando pregunte, instala también sus dependencias: Adafruit GFX y BusIO).
// Placa:    ESP32S3 Dev Module · USB CDC On Boot: Disabled · Flash Size: 16MB
//           PSRAM: OPI PSRAM · Puerto: el del conector COM del DevKit
//
// No hace falta saber cuáles líneas son filas y cuáles columnas: el programa pone en LOW
// una línea a la vez y mira cuál de las demás baja. Cada tecla une dos líneas, así que cada
// tecla aparece como un par ("IO5 + IO18") con su propio símbolo en la pantalla.
//
// Para ponerle nombre a una tecla: presiónala, copia la línea que sale en el monitor serie
// (por ejemplo  {"IO5", "IO18", "?"},) dentro de KEYS (más abajo) y cambia "?" por el nombre.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ---------------------------------------------------------------------------------------
// Pantalla ST7789 (mismos pines que la calculadora)
// ---------------------------------------------------------------------------------------
#define TFT_CS 10
#define TFT_DC 13
#define TFT_RST 14
#define TFT_MOSI 11
#define TFT_SCLK 12
// LED de la pantalla -> 3V3 (el pin 21 ahora es del teclado)

const uint16_t C_BLACK = 0x0000;
const uint16_t C_WHITE = 0xFFFF;
const uint16_t C_NAVY = 0x000F;
const uint16_t C_YELLOW = 0xFFE0;
const uint16_t C_GREEN = 0x07E0;
const uint16_t C_ORANGE = 0xFDA0;
const uint16_t C_GREY = 0xC618;

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
  { 5, "IO5" }, { 6, "IO6" }, { 7, "IO7" }, { 15, "IO15" }, { 16, "IO16" },
  { 17, "IO17" }, { 18, "IO18" }, { 8, "IO8" }, { 3, "IO3" }, { 46, "IO46" },
  // lado izquierdo del DevKit (el de los pines 1 a 21)
  { 1, "IO1" }, { 2, "IO2" }, { 42, "IO42" }, { 41, "IO41" }, { 40, "IO40" }, { 39, "IO39" },
  { 38, "IO38" }, { 48, "IO48" }, { 47, "IO47" }, { 21, "IO21" }, { 20, "IO20" }, { 19, "IO19" },
};

const int N = sizeof(LINES) / sizeof(LINES[0]);
const int GND = N;  // por si algún día conectas un GND de la Casio al GND del ESP32
const int NODES = N + 1;
const int PAIRS = NODES * (NODES - 1) / 2;

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
  { nullptr, nullptr, nullptr },  // no borres esta línea
};

// ---------------------------------------------------------------------------------------

const uint32_t SCAN_MS = 5;     // cada cuánto se revisa el teclado
const int STABLE_SCANS = 4;     // lecturas iguales seguidas para aceptar (antirrebote)
const uint32_t SETTLE_US = 30;  // espera a que la línea baje antes de leer

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
bool deadLine[NODES];  // línea pegada a GND desde el arranque: se ignora
bool fixedPair[PAIRS];  // par unido sin tocar nada: se ignora
bool seen[PAIRS];       // teclas distintas encontradas en esta sesión
int seenCount = 0;
int hitA = 0, hitB = 0, hitKey = -1;  // resultado de la última lectura

int pairIndex(int a, int b) {
  if (a > b) {
    int t = a;
    a = b;
    b = t;
  }
  return a * (2 * NODES - a - 1) / 2 + (b - a - 1);
}

const char* nodeName(int i) {
  return i == GND ? "GND" : LINES[i].name;
}

// Símbolo automático distinto para cada par: la fuente básica tiene 253 dibujos distintos
// (33..127, 128..254 y 1..31), uno por cada par posible.
unsigned char autoSymbol(int idx) {
  if (idx < 95) return 33 + idx;
  if (idx < 222) return 128 + (idx - 95);
  return 1 + (idx - 222);
}

const char* userLabel(int a, int b) {
  for (const KeyName* k = KEYS; k->a != nullptr; k++) {
    if ((strcmp(k->a, nodeName(a)) == 0 && strcmp(k->b, nodeName(b)) == 0) || (strcmp(k->a, nodeName(b)) == 0 && strcmp(k->b, nodeName(a)) == 0)) {
      return k->label;
    }
  }
  return nullptr;
}

// Recorre el teclado y llama a found(a, b) por cada par unido. Si found devuelve true, para.
typedef bool (*PairFn)(int, int);

void scan(PairFn found) {
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

bool markFixed(int a, int b) {
  fixedPair[pairIndex(a, b)] = true;
  return false;  // seguir buscando
}

bool takeFirst(int a, int b) {
  const int idx = pairIndex(a, b);
  if (fixedPair[idx]) return false;
  hitA = min(a, b);
  hitB = max(a, b);
  hitKey = idx;
  return true;
}

// Al arrancar (sin tocar teclas) aprende qué está unido de fábrica para no confundirlo.
void calibrate() {
  for (int round = 0; round < 3; round++) {
    for (int j = 0; j < N; j++) {
      if (digitalRead(LINES[j].gpio) == LOW) deadLine[j] = true;
    }
    scan(markFixed);
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

// Devuelve el primer par presionado (en hitA, hitB), o -1 si no hay ninguna tecla.
int readKey() {
  hitKey = -1;
  scan(takeFirst);
  return hitKey;
}

// Texto centrado con la fuente básica (cada letra mide 6x8 px por "size").
void centerText(const char* s, int yCenter, uint8_t size, uint16_t color) {
  const int w = strlen(s) * 6 * size;
  tft.setTextSize(size);
  tft.setTextColor(color, C_BLACK);
  tft.setCursor((tft.width() - w) / 2, yCenter - 4 * size);
  tft.print(s);
}

void drawFrame() {
  tft.fillScreen(C_BLACK);
  tft.fillRect(0, 0, tft.width(), 20, C_NAVY);
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_NAVY);
  tft.setCursor(6, 2);
  tft.print("PRUEBA TECLADO CASIO");
}

void drawFooter() {
  const int y = tft.height() - 18;
  tft.fillRect(0, y, tft.width(), 18, C_NAVY);
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_NAVY);
  tft.setCursor(6, y + 1);
  tft.printf("Teclas distintas: %d", seenCount);
}

void showKey(int a, int b, int idx) {
  const int w = tft.width();
  tft.fillRect(0, 20, w, tft.height() - 38, C_BLACK);

  const char* label = userLabel(a, b);
  if (label != nullptr) {
    const int len = strlen(label);
    centerText(label, 70, len <= 6 ? 4 : (len <= 13 ? 3 : 2), C_GREEN);
  } else {
    const int size = 9;  // 6x8 px por 9 = 54x72 px
    tft.drawChar(w / 2 - 3 * size, 70 - 4 * size, autoSymbol(idx), C_YELLOW, C_BLACK, size);
  }

  char text[48];
  snprintf(text, sizeof(text), "%s + %s", nodeName(a), nodeName(b));
  centerText(text, 130, 3, C_WHITE);

  centerText(label ? "nombre tuyo" : "sin nombre todavia", 180, 2, label ? C_GREEN : C_ORANGE);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[casio] prueba del teclado de la Casio");
  for (int i = 0; i < N; i++) pinMode(LINES[i].gpio, INPUT_PULLUP);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.setSPISpeed(20000000);  // cables dupont: 20 MHz
  tft.init(240, 320);
  tft.invertDisplay(false);  // esta pantalla no usa inversión de color
  tft.setRotation(1);        // horizontal
  tft.cp437(true);
  uint8_t madctl = 0x40 | 0x20 | 0x08;  // horizontal + orden de color BGR (como en la calculadora)
  tft.sendCommand(ST77XX_MADCTL, &madctl, 1);

  drawFrame();
  centerText("No toques el teclado...", 110, 2, C_WHITE);
  delay(500);
  calibrate();

  drawFrame();
  centerText("Presiona una tecla", 110, 2, C_WHITE);
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

  const int key = readKey();
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
  showKey(hitA, hitB, key);
  const char* label = userLabel(hitA, hitB);
  Serial.printf("[tecla] %-5s + %-5s  ->  {\"%s\", \"%s\", \"%s\"},\n", nodeName(hitA), nodeName(hitB),
                nodeName(hitA), nodeName(hitB), label ? label : "?");
}
