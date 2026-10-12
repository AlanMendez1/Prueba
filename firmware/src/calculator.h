#pragma once
#include <string>
#include <vector>

#include "keys.h"

// Evaluación aritmética básica: + - * / con precedencia, signo unario y decimales.
namespace calc {

enum class Error { None, Syntax, DivByZero, Overflow };

struct Result {
  double value;
  Error error;
};

Result evaluate(const std::string& expr);
std::string format(double value);
const char* errorText(Error error);

}  // namespace calc

// Estado del modo calculadora: expresión editable con cursor, resultado e historial.
class Calculator {
 public:
  static constexpr size_t kMaxExpr = 64;
  static constexpr size_t kMaxHistory = 8;

  // Procesa una tecla del modo normal (F1/F2 los maneja main).
  void press(Key k);

  const std::string& expr() const { return expr_; }
  size_t cursor() const { return cursor_; }
  const std::string& result() const { return result_; }  // vacío si no hay resultado
  bool hasError() const { return error_; }

  // Texto para Claude: "expresión = resultado"; vacío si no hay expresión.
  std::string context() const;

 private:
  void insert(char c);
  void evaluateNow();
  void recall(int direction);
  void clearResult();

  std::string expr_;
  size_t cursor_ = 0;
  std::string result_;
  bool error_ = false;
  bool justEvaluated_ = false;
  std::string lastAnswer_;  // último resultado válido, para encadenar operaciones
  std::vector<std::string> history_;
  int historyPos_ = -1;  // -1 = expresión nueva; 0 = la más reciente del historial
};
