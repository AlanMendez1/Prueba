#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#include <string>
#include <vector>

#include "calculator.h"

// Pantallas del prototipo. Cada show*() redibuja la pantalla completa.
class Ui {
 public:
  void begin();
  void setWifi(bool connected);  // solo redibuja el indicador si cambió

  void showCalc(const Calculator& calc, const char* notice = nullptr);
  void showMenu(const std::string& context, const char* const* items, int count, int selected);
  void showWaiting();

  void setAnswer(const String& text);
  void showAnswer();
  bool scrollAnswer(int delta);  // true si cambió la posición

 private:
  void header(const char* mode);
  void footer(const char* hint);
  void drawWifi();
  void clearBody();
  int answerRows();
  String fit(const std::string& text, int maxWidth);
  std::vector<String> wrap(const String& text, int maxWidth);

  TFT_eSPI tft_;
  bool wifi_ = false;
  std::vector<String> lines_;
  int top_ = 0;
};
