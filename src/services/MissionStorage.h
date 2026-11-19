#pragma once
#include "MissionModel.h"

namespace deskos {
class IMissionStorage {
 public:
  virtual ~IMissionStorage() = default;
  virtual MissionLoadResult load(MissionRecord& record) = 0;
  // True only after the write is committed and read back identically.
  virtual bool save(const MissionRecord& record) = 0;
};

// "FocusCompleted" source. lastCompletedSessionId() is the Focus session ID of
// the latest *committed* +25 XP reward; it only increases while storage is sane.
class IFocusCompletionSource {
 public:
  virtual ~IFocusCompletionSource() = default;
  virtual bool available() const = 0;
  virtual uint32_t lastCompletedSessionId() const = 0;
};

// M5 hook: a TimeService/NTP adapter returns a local calendar day key
// (e.g. days since epoch). M2 has no implementation, so days are manual only.
class IDaySource {
 public:
  virtual ~IDaySource() = default;
  virtual bool calendarDay(uint32_t& dayKey) const = 0;
};
}  // namespace deskos
