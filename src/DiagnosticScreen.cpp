#include "DiagnosticScreen.h"

#include <cstdio>

namespace {
constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t LOG_INTERVAL_MS = 5000;
constexpr uint32_t SOAK_DURATION_MS = 15UL * 60UL * 1000UL;
constexpr uint16_t COLORS[] = {
  TFT_RED, TFT_GREEN, TFT_BLUE, TFT_BLACK, TFT_WHITE
};
const char* const COLOR_LABELS[] = {"RED", "GREEN", "BLUE", "BLACK", "WHITE"};
}  // namespace

DiagnosticScreen::DiagnosticScreen(DisplayHAL& display, const char* firmwareVersion)
    : display_(display), firmwareVersion_(firmwareVersion) {}

void DiagnosticScreen::begin(uint32_t now) {
  if (started_) return;
  started_ = true;
  startedAt_ = lastSampleAt_ = lastLogAt_ = now;
  sampleMemory();
  renderPage(now);
  logStatus("BEGIN", now);
}

void DiagnosticScreen::show(uint32_t now) {
  if (!started_) { begin(now); return; }
  sampleMemory();
  lastSampleAt_ = now;
  renderPage(now);
  logStatus("RESUME", now);
}

void DiagnosticScreen::nextTest(uint32_t now) {
  if (!started_) return;
  test_ = static_cast<Test>((static_cast<uint8_t>(test_) + 1) %
                            static_cast<uint8_t>(Test::Count));
  sampleMemory();
  lastSampleAt_ = now;
  renderPage(now);
  logStatus("TEST_CHANGE", now);
}

void DiagnosticScreen::update(uint32_t now) {
  if (!started_) return;
  if (now - lastSampleAt_ >= SAMPLE_INTERVAL_MS) {
    lastSampleAt_ = now;
    sampleMemory();
    const uint32_t startedAtUs = micros();
    // Only repaint live regions, preserving the geometry border and corners.
    if (test_ == Test::System) drawSystemInfo(now);
    drawFooter(now);
    recordRender(startedAtUs);
  }
  if (now - lastLogAt_ >= LOG_INTERVAL_MS) {
    lastLogAt_ = now;
    logStatus("PERIODIC", now);
  }
}

const char* DiagnosticScreen::testName() const {
  switch (test_) {
    case Test::Color: return "COLORS";
    case Test::Font: return "FONT";
    case Test::Geometry: return "GEOMETRY";
    case Test::System: return "SYSTEM";
    default: return "UNKNOWN";
  }
}

const char* DiagnosticScreen::soakState(uint32_t now) const {
  // Elapsed time alone cannot prove visual correctness or an acceptable heap trend.
  return now - startedAt_ >= SOAK_DURATION_MS ? "ELAPSED_PENDING_REVIEW" : "RUNNING";
}

void DiagnosticScreen::sampleMemory() {
  // Both APIs use MALLOC_CAP_INTERNAL in the installed Arduino ESP32 2.0.11.
  freeHeap_ = ESP.getFreeHeap();
  largestFreeBlock_ = ESP.getMaxAllocHeap();
  if (!baselineSet_) {
    baselineSet_ = true;
    baselineHeap_ = minimumFreeHeap_ = freeHeap_;
    minimumLargestBlock_ = largestFreeBlock_;
  }
  if (freeHeap_ < minimumFreeHeap_) minimumFreeHeap_ = freeHeap_;
  if (largestFreeBlock_ < minimumLargestBlock_) minimumLargestBlock_ = largestFreeBlock_;
}

void DiagnosticScreen::recordRender(uint32_t startedAtUs) {
  renderUs_ = micros() - startedAtUs;
  if (renderUs_ > peakRenderUs_) peakRenderUs_ = renderUs_;
  ++drawCount_;
}

void DiagnosticScreen::renderPage(uint32_t now) {
  const uint32_t startedAtUs = micros();
  auto& tft = display_.graphics();
  tft.fillScreen(TFT_BLACK);
  drawHeader();
  switch (test_) {
    case Test::Color: drawColors(); break;
    case Test::Font: drawFonts(); break;
    case Test::Geometry: drawGeometry(); break;
    case Test::System: drawSystemInfo(now); break;
    default: break;
  }
  drawFooter(now);
  recordRender(startedAtUs);
}

void DiagnosticScreen::drawHeader() {
  auto& tft = display_.graphics();
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("CYD DESKOS M0.1B", 16, 14);
  char line[48];
  snprintf(line, sizeof(line), "%u/4 %s TEST", static_cast<unsigned>(test_) + 1,
           testName());
  tft.setTextSize(1);
  tft.drawString(line, 16, 36);
}

void DiagnosticScreen::drawColors() {
  auto& tft = display_.graphics();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("RGB / BLACK / WHITE - INVERT_ON", 10, 60);
  for (int i = 0; i < 5; ++i) {
    const int x = 10 + i * 62;
    tft.drawRect(x, 82, 52, 88, TFT_WHITE);
    tft.fillRect(x + 1, 83, 50, 86, COLORS[i]);
    tft.drawString(COLOR_LABELS[i], x, 182);
  }
}

void DiagnosticScreen::drawFonts() {
  auto& tft = display_.graphics();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("CYD DESKOS", 12, 58);
  tft.drawString("ABC 123", 12, 86);
  tft.setTextSize(1);
  tft.drawString("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 12, 116);
  tft.drawString("abcdefghijklmnopqrstuvwxyz 0123456789", 12, 132);
  tft.drawFastHLine(12, 170, 296, TFT_WHITE);
  tft.drawFastVLine(12, 146, 28, TFT_WHITE);
  tft.drawFastVLine(160, 146, 28, TFT_WHITE);
  tft.drawFastVLine(307, 146, 28, TFT_WHITE);
  tft.drawString("LEFT", 12, 154);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("CENTER", 160, 154);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("RIGHT", 307, 154);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("!@#$%^&*()_+-=[]{};:,.?/", 12, 182);
  tft.drawString("Font 1 / size 1 and 2", 12, 194);
}

void DiagnosticScreen::drawGeometry() {
  auto& tft = display_.graphics();
  const int w = tft.width();
  const int h = tft.height();
  tft.drawRect(0, 0, w, h, TFT_WHITE);
  // Markers sit inside the outer border; footer repaint cannot overwrite them.
  tft.drawFastHLine(2, 2, 12, TFT_RED);
  tft.drawFastVLine(2, 2, 12, TFT_RED);
  tft.drawFastHLine(w - 14, 2, 12, TFT_GREEN);
  tft.drawFastVLine(w - 3, 2, 12, TFT_GREEN);
  tft.drawFastHLine(2, h - 3, 12, TFT_BLUE);
  tft.drawFastVLine(2, h - 15, 12, TFT_BLUE);
  tft.drawFastHLine(w - 14, h - 3, 12, TFT_WHITE);
  tft.drawFastVLine(w - 3, h - 15, 12, TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("TL (0,0)", 16, 52);
  tft.drawString("TR (319,0)", 242, 52);
  tft.drawString("BL", 8, 196);
  tft.drawString("BR", 296, 196);
  const int cx = w / 2;
  const int cy = h / 2;
  tft.drawFastHLine(cx - 25, cy, 51, TFT_WHITE);
  tft.drawFastVLine(cx, cy - 25, 51, TFT_WHITE);
  tft.drawCircle(cx, cy, 12, TFT_WHITE);
  char line[48];
  snprintf(line, sizeof(line), "CENTER (%d,%d)", cx, cy);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(line, cx, 154);
  snprintf(line, sizeof(line), "%dx%d LANDSCAPE", w, h);
  tft.drawString(line, cx, 180);
  tft.setTextDatum(TL_DATUM);
}

void DiagnosticScreen::drawSystemInfo(uint32_t now) {
  auto& tft = display_.graphics();
  tft.fillRect(8, 48, 304, 164, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  char line[72];
  snprintf(line, sizeof(line), "Free: %lu B", static_cast<unsigned long>(freeHeap_));
  tft.drawString(line, 10, 54);
  snprintf(line, sizeof(line), "Largest: %lu B", static_cast<unsigned long>(largestFreeBlock_));
  tft.drawString(line, 10, 74);
  snprintf(line, sizeof(line), "Min free: %lu B", static_cast<unsigned long>(minimumFreeHeap_));
  tft.drawString(line, 10, 94);
  snprintf(line, sizeof(line), "Uptime: %lu s", static_cast<unsigned long>(now / 1000));
  tft.drawString(line, 10, 114);
  // The displayed render time is the preceding completed draw, not this one.
  snprintf(line, sizeof(line), "Prev draw: %lu us", static_cast<unsigned long>(renderUs_));
  tft.drawString(line, 10, 134);
  snprintf(line, sizeof(line), "Peak draw: %lu us", static_cast<unsigned long>(peakRenderUs_));
  tft.drawString(line, 10, 154);
  snprintf(line, sizeof(line), "Draws: %lu", static_cast<unsigned long>(drawCount_));
  tft.drawString(line, 10, 174);
  tft.setTextSize(1);
  snprintf(line, sizeof(line), "Base heap: %lu B | Trend: PENDING",
           static_cast<unsigned long>(baselineHeap_));
  tft.drawString(line, 10, 198);
}

void DiagnosticScreen::drawFooter(uint32_t now) {
  auto& tft = display_.graphics();
  // Leave x=0/319 and the bottom corner L markers intact in Geometry Test.
  tft.fillRect(16, 216, 288, 22, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  char line[72];
  snprintf(line, sizeof(line), "Up %lus | 15m: %s",
           static_cast<unsigned long>(now / 1000),
           now - startedAt_ >= SOAK_DURATION_MS ? "ELAPSED" : "RUNNING");
  tft.drawString(line, 20, 218);
  tft.drawString("TAP: NEXT | HW CHECK: PENDING", 20, 230);
}

void DiagnosticScreen::logStatus(const char* event, uint32_t now) const {
  Serial.printf("DIAG: event=%s firmware=%s test=%s size=%dx%d uptime_ms=%lu "
                "free_heap=%lu largest_free_block=%lu baseline_heap=%lu heap_delta=%ld "
                "min_free_heap=%lu min_largest_block=%lu render_us=%lu peak_render_us=%lu "
                "draw_count=%lu soak=%s\n",
                event, firmwareVersion_, testName(), display_.graphics().width(),
                display_.graphics().height(), static_cast<unsigned long>(now),
                static_cast<unsigned long>(freeHeap_), static_cast<unsigned long>(largestFreeBlock_),
                static_cast<unsigned long>(baselineHeap_),
                static_cast<long>(freeHeap_) - static_cast<long>(baselineHeap_),
                static_cast<unsigned long>(minimumFreeHeap_),
                static_cast<unsigned long>(minimumLargestBlock_),
                static_cast<unsigned long>(renderUs_), static_cast<unsigned long>(peakRenderUs_),
                static_cast<unsigned long>(drawCount_), soakState(now));
}
