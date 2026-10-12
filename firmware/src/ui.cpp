#include "ui.h"

namespace {

constexpr int HEADER_H = 20;
constexpr int FOOTER_H = 18;
constexpr int MARGIN = 8;
constexpr int LINE_H = 16;
constexpr uint8_t FONT_SMALL = 2;  // 16 px, ASCII
constexpr uint8_t FONT_BIG = 4;    // 26 px, ASCII
constexpr int BIG_H = 26;
constexpr uint16_t BAR_BG = TFT_NAVY;
constexpr uint16_t SEL_BG = TFT_DARKCYAN;

// La fuente no tiene "×": se muestra '*' como 'x'.
std::string pretty(const std::string& expr) {
  std::string out = expr;
  for (char& c : out) {
    if (c == '*') c = 'x';
  }
  return out;
}

}  // namespace

void Ui::begin() {
  tft_.init();  // también enciende la retroiluminación (TFT_BL)
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);
}

void Ui::setWifi(bool connected) {
  if (connected == wifi_) return;
  wifi_ = connected;
  drawWifi();
}

void Ui::showCalc(const Calculator& calc, const char* notice) {
  header("CALC");
  clearBody();
  const int w = tft_.width();
  const int maxW = w - 2 * MARGIN;
  const std::string shown = pretty(calc.expr());
  const size_t cursor = calc.cursor();

  // Ventana horizontal: se desplaza lo justo para que el cursor siga visible.
  size_t start = 0;
  while (start < cursor && tft_.textWidth(shown.substr(start, cursor - start).c_str(), FONT_BIG) > maxW) {
    start++;
  }
  size_t end = start;
  while (end < shown.size() &&
         tft_.textWidth(shown.substr(start, end + 1 - start).c_str(), FONT_BIG) <= maxW) {
    end++;
  }

  const int y = HEADER_H + 16;
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString(shown.substr(start, end - start).c_str(), MARGIN, y, FONT_BIG);
  const int cx = MARGIN + tft_.textWidth(shown.substr(start, cursor - start).c_str(), FONT_BIG);
  tft_.drawFastVLine(cx, y - 2, BIG_H + 2, TFT_YELLOW);
  if (start > 0) tft_.fillTriangle(1, y + 13, 5, y + 8, 5, y + 18, TFT_DARKGREY);
  if (end < shown.size()) tft_.fillTriangle(w - 2, y + 13, w - 6, y + 8, w - 6, y + 18, TFT_DARKGREY);

  const std::string& result = calc.result();
  if (!result.empty()) {
    const uint8_t size = tft_.textWidth(result.c_str(), FONT_BIG) * 2 <= maxW ? 2 : 1;
    tft_.setTextSize(size);
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextColor(calc.hasError() ? TFT_RED : TFT_GREEN, TFT_BLACK);
    tft_.drawString(result.c_str(), w - MARGIN, y + BIG_H + 24, FONT_BIG);
    tft_.setTextSize(1);
  }

  if (notice) {
    tft_.setTextDatum(BL_DATUM);
    tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft_.drawString(notice, MARGIN, tft_.height() - FOOTER_H - 6, FONT_SMALL);
  }
  footer("F1 IA   ^ v historial   < > cursor");
}

void Ui::showMenu(const std::string& context, const char* const* items, int count, int selected) {
  header("IA");
  clearBody();
  const int w = tft_.width();
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.drawString("Preguntar a Claude sobre:", MARGIN, HEADER_H + 6, FONT_SMALL);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString(fit(pretty(context), w - 2 * MARGIN), MARGIN, HEADER_H + 24, FONT_SMALL);

  constexpr int ROW_H = 26;
  int y = HEADER_H + 52;
  for (int i = 0; i < count; i++) {
    const bool sel = i == selected;
    if (sel) tft_.fillRoundRect(4, y, w - 8, ROW_H - 2, 4, SEL_BG);
    tft_.setTextColor(TFT_WHITE, sel ? SEL_BG : TFT_BLACK);
    tft_.drawString(items[i], MARGIN + 6, y + 4, FONT_SMALL);
    y += ROW_H;
  }
  footer("^ v elegir   = enviar   F2 salir");
}

void Ui::showWaiting() {
  header("IA");
  clearBody();
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.drawString("Consultando a Claude...", tft_.width() / 2, tft_.height() / 2, FONT_SMALL);
  footer("Espera unos segundos");
}

void Ui::setAnswer(const String& text) {
  lines_ = wrap(text, tft_.width() - 2 * MARGIN);
  while (!lines_.empty() && lines_.back().length() == 0) lines_.pop_back();
  top_ = 0;
}

void Ui::showAnswer() {
  header("IA");
  clearBody();
  const int rows = answerRows();
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int r = 0; r < rows && top_ + r < static_cast<int>(lines_.size()); r++) {
    tft_.drawString(lines_[top_ + r], MARGIN, HEADER_H + 4 + r * LINE_H, FONT_SMALL);
  }
  footer("^ v mover   AC menu   F2 calc");

  // Posición dentro de la respuesta, a la derecha del pie.
  if (static_cast<int>(lines_.size()) > rows) {
    const int last = min(top_ + rows, static_cast<int>(lines_.size()));
    const String pos = String(last) + "/" + String(lines_.size());
    tft_.setTextDatum(MR_DATUM);
    tft_.setTextColor(TFT_LIGHTGREY, BAR_BG);
    tft_.drawString(pos, tft_.width() - 4, tft_.height() - FOOTER_H / 2, FONT_SMALL);
  }
}

bool Ui::scrollAnswer(int delta) {
  const int maxTop = max(0, static_cast<int>(lines_.size()) - answerRows());
  const int next = constrain(top_ + delta, 0, maxTop);
  if (next == top_) return false;
  top_ = next;
  return true;
}

void Ui::header(const char* mode) {
  tft_.fillRect(0, 0, tft_.width(), HEADER_H, BAR_BG);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(TFT_WHITE, BAR_BG);
  tft_.drawString(mode, 4, HEADER_H / 2, FONT_SMALL);
  drawWifi();
}

void Ui::footer(const char* hint) {
  const int y = tft_.height() - FOOTER_H;
  tft_.fillRect(0, y, tft_.width(), FOOTER_H, BAR_BG);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(TFT_LIGHTGREY, BAR_BG);
  tft_.drawString(hint, 4, y + FOOTER_H / 2, FONT_SMALL);
}

void Ui::drawWifi() {
  const char* label = wifi_ ? "WiFi" : "sin WiFi";
  const int areaW = tft_.textWidth("sin WiFi", FONT_SMALL) + 8;
  tft_.fillRect(tft_.width() - areaW, 0, areaW, HEADER_H, BAR_BG);
  tft_.setTextDatum(MR_DATUM);
  tft_.setTextColor(wifi_ ? TFT_GREEN : TFT_ORANGE, BAR_BG);
  tft_.drawString(label, tft_.width() - 4, HEADER_H / 2, FONT_SMALL);
}

void Ui::clearBody() {
  tft_.fillRect(0, HEADER_H, tft_.width(), tft_.height() - HEADER_H - FOOTER_H, TFT_BLACK);
}

int Ui::answerRows() {
  return (tft_.height() - HEADER_H - FOOTER_H - 8) / LINE_H;
}

String Ui::fit(const std::string& text, int maxWidth) {
  String s(text.c_str());
  if (tft_.textWidth(s, FONT_SMALL) <= maxWidth) return s;
  while (s.length() > 0 && tft_.textWidth(s + "...", FONT_SMALL) > maxWidth) s.remove(s.length() - 1);
  return s + "...";
}

// Corta en palabras según el ancho real de la fuente; respeta los saltos de línea.
std::vector<String> Ui::wrap(const String& text, int maxWidth) {
  std::vector<String> out;
  int start = 0;
  const int len = text.length();
  while (start <= len) {
    int nl = text.indexOf('\n', start);
    if (nl < 0) nl = len;
    const String para = text.substring(start, nl);
    start = nl + 1;

    String line;
    int i = 0;
    while (i < static_cast<int>(para.length())) {
      int sp = para.indexOf(' ', i);
      if (sp < 0) sp = para.length();
      String word = para.substring(i, sp);
      i = sp + 1;
      if (word.length() == 0) continue;

      const String candidate = line.length() ? line + " " + word : word;
      if (tft_.textWidth(candidate, FONT_SMALL) <= maxWidth) {
        line = candidate;
        continue;
      }
      if (line.length()) out.push_back(line);
      // Palabra más ancha que la pantalla: se corta.
      while (tft_.textWidth(word, FONT_SMALL) > maxWidth) {
        int n = word.length();
        while (n > 1 && tft_.textWidth(word.substring(0, n), FONT_SMALL) > maxWidth) n--;
        out.push_back(word.substring(0, n));
        word = word.substring(n);
      }
      line = word;
    }
    out.push_back(line);  // las líneas vacías separan párrafos
  }
  return out;
}
