#pragma once
#include <stddef.h>
#include "ProgressService.h"

namespace deskos {
constexpr uint32_t FOCUS_DURATION_MS=25UL*60UL*1000UL;
constexpr uint32_t BREAK_DURATION_MS=5UL*60UL*1000UL;
constexpr uint32_t FOCUS_CHECKPOINT_MS=60000;
constexpr uint32_t COMPLETION_RETRY_MS=5000;
enum class FocusState : uint8_t { Idle, Running, Paused, Completed };
enum class FocusMode : uint8_t { Focus, Break };
struct FocusRecord {
  uint32_t magic;
  uint16_t version,bytes;
  FocusMode mode;
  FocusState state;
  uint8_t reserved[2];
  uint32_t remainingMs;
  ProgressData progress;
  uint32_t generation,checksum;
};
static_assert(sizeof(FocusRecord)==40,"Change Focus storage version when record layout changes");
static_assert(FOCUS_DURATION_MS==1500000 && BREAK_DURATION_MS==300000,"Production durations must stay 25/5 minutes");
uint32_t focusDuration(FocusMode mode);
uint32_t focusChecksum(const FocusRecord& record);
void sealFocusRecord(FocusRecord& record);
bool validFocusRecord(const FocusRecord& record);
FocusRecord defaultFocusRecord();
const char* focusStateName(FocusState state);
const char* focusModeName(FocusMode mode);
}  // namespace deskos
