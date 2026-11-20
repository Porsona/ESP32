#include "TouchCalibrationStore.h"
#include <Preferences.h>
#include <cstring>

namespace touchcal {
bool load(CalibrationRecord& record) {
  Preferences nvs;
  if (!nvs.begin("cyd-touch",true)) return false;
  CalibrationRecord candidate{};
  const bool read=nvs.getBytesLength("affine")==sizeof(candidate) &&
                  nvs.getBytes("affine",&candidate,sizeof(candidate))==sizeof(candidate);
  nvs.end();
  if (!read || !validRecord(candidate)) return false;
  record=candidate;
  return true;
}

bool save(const CalibrationRecord& record) {
  if (!validRecord(record)) return false;
  Preferences nvs;
  if (!nvs.begin("cyd-touch",false)) return false;
  // One versioned blob, only after successful calibration + independent checks.
  const bool written=nvs.putBytes("affine",&record,sizeof(record))==sizeof(record);
  nvs.end();
  CalibrationRecord readback{};
  return written && load(readback) && std::memcmp(&record,&readback,sizeof(record))==0;
}
}  // namespace touchcal
