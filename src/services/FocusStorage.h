#pragma once
#include "FocusModel.h"

namespace deskos {
enum class FocusLoadResult { Missing, Loaded, Invalid, IoError };
class IFocusStorage {
 public:
  virtual ~IFocusStorage() = default;
  virtual FocusLoadResult load(FocusRecord& record) = 0;
  virtual bool save(const FocusRecord& record) = 0;
};
class IFocusClock {
 public:
  virtual ~IFocusClock() = default;
  virtual uint32_t nowMs() const = 0;
};
}  // namespace deskos
