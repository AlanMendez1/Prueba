#include "calculator.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace calc {
namespace {

// Gramática:  expr := term (('+'|'-') term)*
//             term := factor (('*'|'/') factor)*
//             factor := ('+'|'-') factor | number
struct Parser {
  explicit Parser(const std::string& text) : s(text) {}

  const std::string& s;
  size_t i = 0;
  Error err = Error::None;

  bool peek(char c) const { return i < s.size() && s[i] == c; }

  double expression() {
    double v = term();
    while (err == Error::None && (peek('+') || peek('-'))) {
      const char op = s[i++];
      const double r = term();
      v = (op == '+') ? v + r : v - r;
    }
    return v;
  }

  double term() {
    double v = factor();
    while (err == Error::None && (peek('*') || peek('/'))) {
      const char op = s[i++];
      const double r = factor();
      if (err != Error::None) break;
      if (op == '*') {
        v *= r;
      } else if (r == 0) {
        err = Error::DivByZero;
      } else {
        v /= r;
      }
    }
    return v;
  }

  double factor() {
    if (peek('-')) {
      i++;
      return -factor();
    }
    if (peek('+')) {
      i++;
      return factor();
    }
    return number();
  }

  // Dígitos con un punto opcional; acepta exponente ("1e+15") para poder
  // encadenar sobre un resultado grande mostrado en notación científica.
  double number() {
    const size_t start = i;
    bool digits = false;
    bool dot = false;
    while (i < s.size()) {
      const char c = s[i];
      if (std::isdigit(static_cast<unsigned char>(c))) {
        digits = true;
      } else if (c == '.' && !dot) {
        dot = true;
      } else {
        break;
      }
      i++;
    }
    if (!digits) {
      err = Error::Syntax;
      return 0;
    }
    if (peek('e') || peek('E')) {
      size_t j = i + 1;
      if (j < s.size() && (s[j] == '+' || s[j] == '-')) j++;
      size_t k = j;
      while (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) k++;
      if (k == j) {
        err = Error::Syntax;
        return 0;
      }
      i = k;
    }
    return std::strtod(s.substr(start, i - start).c_str(), nullptr);
  }
};

}  // namespace

Result evaluate(const std::string& expr) {
  Parser p(expr);
  const double v = p.expression();
  if (p.err == Error::None && p.i != expr.size()) p.err = Error::Syntax;
  if (p.err == Error::None && !std::isfinite(v)) p.err = Error::Overflow;
  return {p.err == Error::None ? v : 0, p.err};
}

std::string format(double value) {
  // Redondea a cero el ruido de coma flotante (p. ej. 0.1+0.2-0.3) y evita "-0".
  if (std::fabs(value) < 1e-12) value = 0;
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.10g", value);
  return buf;
}

const char* errorText(Error error) {
  switch (error) {
    case Error::Syntax: return "Error de sintaxis";
    case Error::DivByZero: return "Division por cero";
    case Error::Overflow: return "Desbordamiento";
    case Error::None: break;
  }
  return "";
}

}  // namespace calc

namespace {

char opChar(Key k) {
  switch (k) {
    case Key::Add: return '+';
    case Key::Sub: return '-';
    case Key::Mul: return '*';
    case Key::Div: return '/';
    default: return 0;
  }
}

}  // namespace

void Calculator::press(Key k) {
  switch (k) {
    case Key::AC:
      expr_.clear();
      cursor_ = 0;
      historyPos_ = -1;
      clearResult();
      return;
    case Key::Eq:
      evaluateNow();
      return;
    case Key::Up:
      recall(+1);
      return;
    case Key::Down:
      recall(-1);
      return;
    case Key::Left:
      if (cursor_ > 0) cursor_--;
      justEvaluated_ = false;
      return;
    case Key::Right:
      if (cursor_ < expr_.size()) cursor_++;
      justEvaluated_ = false;
      return;
    case Key::Del:
      if (cursor_ > 0) {
        expr_.erase(cursor_ - 1, 1);
        cursor_--;
      }
      clearResult();
      return;
    default:
      break;
  }

  const char op = opChar(k);
  const char c = isDigitKey(k) ? digitChar(k) : (k == Key::Dot ? '.' : op);
  if (!c) return;

  if (justEvaluated_) {
    // Después de "=": un operador sigue sobre el resultado; un número empieza de cero.
    expr_ = op ? lastAnswer_ : std::string();
    cursor_ = expr_.size();
  }
  clearResult();
  insert(c);
}

std::string Calculator::context() const {
  if (expr_.empty()) return "";
  if (!result_.empty() && !error_) return expr_ + " = " + result_;
  const calc::Result r = calc::evaluate(expr_);
  if (r.error == calc::Error::None) return expr_ + " = " + calc::format(r.value);
  return expr_ + " (" + calc::errorText(r.error) + ")";
}

void Calculator::insert(char c) {
  if (expr_.size() >= kMaxExpr) return;
  expr_.insert(cursor_, 1, c);
  cursor_++;
}

void Calculator::evaluateNow() {
  if (expr_.empty()) return;
  const calc::Result r = calc::evaluate(expr_);
  cursor_ = expr_.size();
  if (r.error != calc::Error::None) {
    result_ = calc::errorText(r.error);
    error_ = true;
    justEvaluated_ = false;  // se queda editando para corregir
    return;
  }
  result_ = calc::format(r.value);
  error_ = false;
  justEvaluated_ = true;
  lastAnswer_ = result_;
  if (history_.empty() || history_.back() != expr_) {
    history_.push_back(expr_);
    if (history_.size() > kMaxHistory) history_.erase(history_.begin());
  }
  historyPos_ = -1;
}

void Calculator::recall(int direction) {
  if (history_.empty()) return;
  const int n = static_cast<int>(history_.size());
  int pos = historyPos_ + direction;
  if (pos < -1) pos = -1;
  if (pos >= n) pos = n - 1;
  historyPos_ = pos;
  expr_ = pos < 0 ? std::string() : history_[n - 1 - pos];
  cursor_ = expr_.size();
  clearResult();
}

void Calculator::clearResult() {
  result_.clear();
  error_ = false;
  justEvaluated_ = false;
}
