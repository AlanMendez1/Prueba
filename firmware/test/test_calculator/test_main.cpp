// Pruebas de la lógica sin hardware:  pio test -e native
#include <unity.h>

#include <string>

#include "calculator.h"
#include "text_utils.h"

void setUp() {}
void tearDown() {}

namespace {

// Teclea una secuencia: dígitos, + - * / . =, C=AC, D=DEL, < > flechas, ^ v arriba/abajo.
void type(Calculator& c, const char* keys) {
  for (const char* p = keys; *p; p++) {
    Key k = Key::None;
    switch (*p) {
      case '+': k = Key::Add; break;
      case '-': k = Key::Sub; break;
      case '*': k = Key::Mul; break;
      case '/': k = Key::Div; break;
      case '.': k = Key::Dot; break;
      case '=': k = Key::Eq; break;
      case 'C': k = Key::AC; break;
      case 'D': k = Key::Del; break;
      case '<': k = Key::Left; break;
      case '>': k = Key::Right; break;
      case '^': k = Key::Up; break;
      case 'v': k = Key::Down; break;
      default:
        if (*p >= '0' && *p <= '9') k = static_cast<Key>(static_cast<int>(Key::D0) + (*p - '0'));
    }
    c.press(k);
  }
}

std::string eval(const char* expr) {
  const calc::Result r = calc::evaluate(expr);
  return r.error == calc::Error::None ? calc::format(r.value) : calc::errorText(r.error);
}

}  // namespace

void test_precedence_and_signs() {
  TEST_ASSERT_EQUAL_STRING("14", eval("2+3*4").c_str());
  TEST_ASSERT_EQUAL_STRING("2.5", eval("10/4").c_str());
  TEST_ASSERT_EQUAL_STRING("6", eval("-3*-2").c_str());
  TEST_ASSERT_EQUAL_STRING("2", eval("1.5+.5").c_str());
  TEST_ASSERT_EQUAL_STRING("0.3", eval("0.1+0.2").c_str());
  TEST_ASSERT_EQUAL_STRING("0", eval("0.1+0.2-0.3").c_str());
  TEST_ASSERT_EQUAL_STRING("1e+15", eval("1e+15").c_str());
}

void test_errors() {
  TEST_ASSERT_EQUAL_STRING("Division por cero", eval("5/0").c_str());
  TEST_ASSERT_EQUAL_STRING("Division por cero", eval("1+8/-0").c_str());
  TEST_ASSERT_EQUAL_STRING("Error de sintaxis", eval("5+").c_str());
  TEST_ASSERT_EQUAL_STRING("Error de sintaxis", eval("1.2.3").c_str());
  TEST_ASSERT_EQUAL_STRING("Error de sintaxis", eval("*3").c_str());
  TEST_ASSERT_EQUAL_STRING("Error de sintaxis", eval("").c_str());
  TEST_ASSERT_EQUAL_STRING("Error de sintaxis", eval(".").c_str());
}

void test_chaining_after_equals() {
  Calculator c;
  type(c, "12+3=");
  TEST_ASSERT_EQUAL_STRING("15", c.result().c_str());
  type(c, "*2=");
  TEST_ASSERT_EQUAL_STRING("15*2", c.expr().c_str());
  TEST_ASSERT_EQUAL_STRING("30", c.result().c_str());
  type(c, "7");
  TEST_ASSERT_EQUAL_STRING("7", c.expr().c_str());
  TEST_ASSERT_EQUAL_STRING("", c.result().c_str());
}

void test_cursor_and_delete() {
  Calculator c;
  type(c, "123<D");
  TEST_ASSERT_EQUAL_STRING("13", c.expr().c_str());
  TEST_ASSERT_EQUAL_UINT32(1, c.cursor());
  type(c, "9");
  TEST_ASSERT_EQUAL_STRING("193", c.expr().c_str());
  type(c, "<<<<D>>>>");
  TEST_ASSERT_EQUAL_STRING("193", c.expr().c_str());
  TEST_ASSERT_EQUAL_UINT32(3, c.cursor());
}

void test_history() {
  Calculator c;
  type(c, "1+1=C2+2=C");
  type(c, "^");
  TEST_ASSERT_EQUAL_STRING("2+2", c.expr().c_str());
  type(c, "^");
  TEST_ASSERT_EQUAL_STRING("1+1", c.expr().c_str());
  type(c, "^");
  TEST_ASSERT_EQUAL_STRING("1+1", c.expr().c_str());
  type(c, "v");
  TEST_ASSERT_EQUAL_STRING("2+2", c.expr().c_str());
  type(c, "v");
  TEST_ASSERT_EQUAL_STRING("", c.expr().c_str());
}

void test_error_keeps_editing() {
  Calculator c;
  type(c, "5/0=");
  TEST_ASSERT_TRUE(c.hasError());
  TEST_ASSERT_EQUAL_STRING("Division por cero", c.result().c_str());
  type(c, "D2=");
  TEST_ASSERT_FALSE(c.hasError());
  TEST_ASSERT_EQUAL_STRING("2.5", c.result().c_str());
}

void test_context_for_claude() {
  Calculator c;
  TEST_ASSERT_EQUAL_STRING("", c.context().c_str());
  type(c, "6*7");
  TEST_ASSERT_EQUAL_STRING("6*7 = 42", c.context().c_str());
  type(c, "=");
  TEST_ASSERT_EQUAL_STRING("6*7 = 42", c.context().c_str());
  type(c, "C8/");
  TEST_ASSERT_EQUAL_STRING("8/ (Error de sintaxis)", c.context().c_str());
}

void test_utf8_to_ascii() {
  TEST_ASSERT_EQUAL_STRING("Cuanto es 3x4? Nandu, 8/2 -> 4...",
                           utf8ToAscii("¿Cuánto es 3×4? Ñandú, 8÷2 → 4…").c_str());
  TEST_ASSERT_EQUAL_STRING("x^2 \"si\"", utf8ToAscii("x² “sí”").c_str());
  TEST_ASSERT_EQUAL_STRING("a?b", utf8ToAscii("a\xF0\x9F\x98\x80" "b").c_str());  // emoji
  TEST_ASSERT_EQUAL_STRING("a?", utf8ToAscii("a\xC3").c_str());                  // UTF-8 cortado
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_precedence_and_signs);
  RUN_TEST(test_errors);
  RUN_TEST(test_chaining_after_equals);
  RUN_TEST(test_cursor_and_delete);
  RUN_TEST(test_history);
  RUN_TEST(test_error_keeps_editing);
  RUN_TEST(test_context_for_claude);
  RUN_TEST(test_utf8_to_ascii);
  return UNITY_END();
}
