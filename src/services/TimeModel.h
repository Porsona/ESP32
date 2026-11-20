#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Asia/Ho_Chi_Minh is UTC+7 all year (no DST). The ESP32 side also sets the
// POSIX TZ "<+07>-7" for libc; day/hour logic below is pure and host-tested.
constexpr int32_t LOCAL_UTC_OFFSET_S=7*3600;
constexpr char LOCAL_POSIX_TZ[]="<+07>-7";
constexpr uint32_t MIN_VALID_EPOCH=1735689600UL;  // 2025-01-01: anything earlier is "not synced".
inline bool plausibleEpoch(uint32_t epoch) { return epoch>=MIN_VALID_EPOCH; }
inline uint32_t localDayKey(uint32_t epoch) { return static_cast<uint32_t>((static_cast<uint64_t>(epoch)+LOCAL_UTC_OFFSET_S)/86400ULL); }
inline uint8_t localHour(uint32_t epoch) { return static_cast<uint8_t>(((static_cast<uint64_t>(epoch)+LOCAL_UTC_OFFSET_S)%86400ULL)/3600ULL); }
inline uint8_t localMinute(uint32_t epoch) { return static_cast<uint8_t>(((static_cast<uint64_t>(epoch)+LOCAL_UTC_OFFSET_S)%3600ULL)/60ULL); }

class ITimeSource {
 public:
  virtual ~ITimeSource() = default;
  virtual bool valid() const = 0;      // Synced from NTP in this boot.
  virtual uint32_t epoch() const = 0;  // UTC seconds; meaningful only when valid().
};
}  // namespace deskos
