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
// tecla aparece como un par ("T710 + T719") con su propio símbolo en la pantalla.
//
// Para ponerle nombre a una tecla: presiónala, copia la línea que sale en el monitor serie
// (por ejemplo  {"T710", "T719", "?"},) dentro de KEYS (más abajo) y cambia "?" por el nombre.

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
#define TFT_BL 21

const uint16_t C_BLACK = 0x0000;
const uint16_t C_WHITE = 0xFFFF;
const uint16_t C_NAVY = 0x000F;
const uint16_t C_YELLOW = 0xFFE0;
const uint16_t C_GREEN = 0x07E0;
const uint16_t C_ORANGE = 0xFDA0;
const uint16_t C_GREY = 0xC618;

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
  { 4, "T710", 4 },   { 5, "T703", 5 },   { 6, "T706", 6 },   { 7, "T716", 7 },
  { 15, "T707", 8 },  { 16, "P9", 9 },    { 17, "P10", 10 },  { 18, "T719", 11 },
  { 8, "T708", 12 },  { 9, "T711", 13 },  { 1, "T712", 14 },  { 2, "T713", 15 },
  { 42, "T714", 16 }, { 41, "T705", 17 }, { 40, "T704", 18 }, { 39, "T709", 19 },
  { 47, "VCC", 2 },  // por si alguna tecla (por ejemplo AC/ON) va a VCC
};
const int N = sizeof(LINES) / sizeof(LINES[0]);
const int GND = N;  // los pines GND de la Casio van al GND del ESP32
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
  // {"T710", "T719", "SHIFT"},   <- ejemplo: quita las // y cambia los valores
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

// Símbolo automático distinto para cada par: ASCII 33..126 y luego 128.. de la fuente.
unsigned char autoSymbol(int idx) {
  return idx < 94 ? 33 + idx : 128 + (idx - 94);
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

  if (b == GND) {
    snprintf(text, sizeof(text), "IO%d + GND", LINES[a].gpio);
    centerText(text, 162, 2, C_GREY);
    snprintf(text, sizeof(text), "pin %d", LINES[a].cn);
  } else {
    snprintf(text, sizeof(text), "IO%d + IO%d", LINES[a].gpio, LINES[b].gpio);
    centerText(text, 162, 2, C_GREY);
    snprintf(text, sizeof(text), "pines %d y %d", LINES[a].cn, LINES[b].cn);
  }
  centerText(text, 184, 2, C_GREY);
  centerText(label ? "nombre tuyo" : "sin nombre todavia", 206, 2, label ? C_GREEN : C_ORANGE);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[casio] prueba del teclado de la Casio");
  for (int i = 0; i < N; i++) pinMode(LINES[i].gpio, INPUT_PULLUP);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.setSPISpeed(20000000);  // cables dupont: 20 MHz
  tft.init(240, 320);
  tft.invertDisplay(false);  // esta pantalla no usa inversión de color
  tft.setRotation(1);        // horizontal
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
