#pragma once
#include <Arduino.h>
#include "../services/FocusStorage.h"

namespace deskos {
class FocusClock : public IFocusClock {
 public:
  uint32_t nowMs() const override { return millis(); }
};
}  // namespace deskos
