// CalculadoraIA — prototipo mínimo.
// Modo normal: calculadora (+ - x / y decimales). F1: menú de preguntas a Claude
// sobre la operación actual. F2: vuelve a la calculadora.

#include <Arduino.h>
#include <WiFi.h>

#include "calculator.h"
#include "claude_client.h"
#include "config.h"
#include "keypad.h"
#include "ui.h"

namespace {

struct AiOption {
  const char* label;   // lo que se ve en el menú (ASCII)
  const char* prompt;  // lo que se envía, seguido de la operación
};

const AiOption AI_OPTIONS[] = {
    {"Explicar paso a paso", "Explica paso a paso cómo se resuelve esta operación: "},
    {"Verificar el resultado",
     "Comprueba si este resultado es correcto. Si no lo es, da el correcto y explica el error: "},
    {"Otra forma de resolverlo",
     "Muestra otra forma de resolver esta operación, por ejemplo un atajo de cálculo mental: "},
    {"Dato curioso del resultado",
     "Cuenta un dato curioso o una propiedad matemática del resultado de esta operación: "},
};
constexpr int AI_COUNT = sizeof(AI_OPTIONS) / sizeof(AI_OPTIONS[0]);

enum class Mode { Calc, AiMenu, AiAnswer };

Mode mode = Mode::Calc;
Calculator calculator;
Keypad keypad;
Ui ui;
int menuSelected = 0;
std::string aiContext;

void showCalc(const char* notice = nullptr) {
  mode = Mode::Calc;
  ui.showCalc(calculator, notice);
}

void showMenu() {
  static const char* labels[AI_COUNT];
  for (int i = 0; i < AI_COUNT; i++) labels[i] = AI_OPTIONS[i].label;
  mode = Mode::AiMenu;
  ui.showMenu(aiContext, labels, AI_COUNT, menuSelected);
}

void askSelected() {
  ui.showWaiting();
  const String prompt = String(AI_OPTIONS[menuSelected].prompt) + aiContext.c_str();
  Serial.println("[claude] " + prompt);
  const ClaudeReply reply = askClaude(prompt);
  ui.setAnswer(reply.text);
  mode = Mode::AiAnswer;
  ui.showAnswer();
}

void handleCalc(Key k) {
  if (k == Key::F1) {
    aiContext = calculator.context();
    if (aiContext.empty()) {
      showCalc("Escribe una operacion y luego F1");
      return;
    }
    menuSelected = 0;
    showMenu();
    return;
  }
  if (k == Key::F2) return;  // ya estamos en la calculadora
  calculator.press(k);
  showCalc();
}

void handleMenu(Key k) {
  switch (k) {
    case Key::Up:
      menuSelected = (menuSelected + AI_COUNT - 1) % AI_COUNT;
      showMenu();
      break;
    case Key::Down:
      menuSelected = (menuSelected + 1) % AI_COUNT;
      showMenu();
      break;
    case Key::Eq:
      askSelected();
      break;
    case Key::F2:
    case Key::AC:
      showCalc();
      break;
    default:
      break;
  }
}

void handleAnswer(Key k) {
  switch (k) {
    case Key::Up:
      if (ui.scrollAnswer(-1)) ui.showAnswer();
      break;
    case Key::Down:
      if (ui.scrollAnswer(+1)) ui.showAnswer();
      break;
    case Key::AC:
    case Key::F1:
      showMenu();
      break;
    case Key::F2:
      showCalc();
      break;
    default:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  keypad.begin();
  ui.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // conecta en segundo plano
  showCalc();
}

void loop() {
  ui.setWifi(WiFi.status() == WL_CONNECTED);

  const Key k = keypad.poll();
  if (k == Key::None) return;

  switch (mode) {
    case Mode::Calc: handleCalc(k); break;
    case Mode::AiMenu: handleMenu(k); break;
    case Mode::AiAnswer: handleAnswer(k); break;
  }
}
