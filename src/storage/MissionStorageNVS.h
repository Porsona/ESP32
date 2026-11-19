#pragma once
#include "../services/MissionStorage.h"

namespace deskos {
// Namespace "deskos-quest", keys "slot0"/"slot1". Does not touch
// "cyd-touch" (calibration) or "deskos-focus" (timer + Focus XP).
class MissionStorageNVS : public IMissionStorage {
 public:
  MissionLoadResult load(MissionRecord& record) override;
  bool save(const MissionRecord& record) override;
  uint8_t nextSlot() const { return target_; }
 private:
  uint8_t target_=0;  // Slot that does NOT hold the last good record.
};
}  // namespace deskos
