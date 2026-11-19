#pragma once

#include <Arduino.h>
#include "DisplayHAL.h"

class DiagnosticScreen {
 public:
  DiagnosticScreen(DisplayHAL& display, const char* firmwareVersion);
  void begin(uint32_t now);
  void show(uint32_t now);
  void nextTest(uint32_t now);
  void update(uint32_t now);

 private:
  enum class Test : uint8_t { Color, Font, Geometry, System, Count };

  const char* testName() const;
  const char* soakState(uint32_t now) const;
  void sampleMemory();
  void renderPage(uint32_t now);
  void recordRender(uint32_t startedAtUs);
  void drawHeader();
  void drawColors();
  void drawFonts();
  void drawGeometry();
  void drawSystemInfo(uint32_t now);
  void drawFooter(uint32_t now);
  void logStatus(const char* event, uint32_t now) const;

  DisplayHAL& display_;
  const char* firmwareVersion_;
  Test test_ = Test::Color;
  bool started_ = false;
  bool baselineSet_ = false;
  uint32_t startedAt_ = 0;
  uint32_t lastSampleAt_ = 0;
  uint32_t lastLogAt_ = 0;
  uint32_t freeHeap_ = 0;
  uint32_t largestFreeBlock_ = 0;
  uint32_t baselineHeap_ = 0;
  uint32_t minimumFreeHeap_ = 0;
  uint32_t minimumLargestBlock_ = 0;
  uint32_t renderUs_ = 0;
  uint32_t peakRenderUs_ = 0;
  uint32_t drawCount_ = 0;
};
