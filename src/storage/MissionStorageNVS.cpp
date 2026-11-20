#include "MissionStorageNVS.h"
#include <Arduino.h>
#include <Preferences.h>
#include <nvs.h>
#include <cstring>

namespace deskos {
namespace {
const char* const NS="deskos-quest";
const char* const KEYS[2]={"slot0","slot1"};

SlotStatus readSlot(nvs_handle_t handle,const char* key,MissionRecord& out) {
  size_t bytes=0;
  const esp_err_t query=nvs_get_blob(handle,key,nullptr,&bytes);
  if (query==ESP_ERR_NVS_NOT_FOUND) return SlotStatus::Missing;
  if (query==ESP_ERR_NVS_TYPE_MISMATCH || (query==ESP_OK && bytes!=sizeof(MissionRecord))) return SlotStatus::Invalid;
  if (query!=ESP_OK) return SlotStatus::IoError;
  MissionRecord candidate;
  std::memset(&candidate,0,sizeof(candidate));
  if (nvs_get_blob(handle,key,&candidate,&bytes)!=ESP_OK || bytes!=sizeof(candidate)) return SlotStatus::IoError;
  if (!validMissionRecord(candidate)) return SlotStatus::Invalid;
  out=candidate;
  return SlotStatus::Valid;
}

// Reads one slot back after a write, independent of the A/B choice.
bool readBack(const char* key,MissionRecord& out) {
  nvs_handle_t handle;
  if (nvs_open(NS,NVS_READONLY,&handle)!=ESP_OK) return false;
  const bool ok=readSlot(handle,key,out)==SlotStatus::Valid;
  nvs_close(handle);
  return ok;
}
}  // namespace

MissionLoadResult MissionStorageNVS::load(MissionRecord& record) {
  nvs_handle_t handle;
  const esp_err_t opened=nvs_open(NS,NVS_READONLY,&handle);
  if (opened==ESP_ERR_NVS_NOT_FOUND) { target_=0; return MissionLoadResult::Missing; }
  if (opened!=ESP_OK) return MissionLoadResult::IoError;
  SlotRead slots[2];
  for (uint8_t i=0;i<2;++i) {
    std::memset(&slots[i].record,0,sizeof(slots[i].record));
    slots[i].status=readSlot(handle,KEYS[i],slots[i].record);
  }
  nvs_close(handle);
  uint8_t index=0;
  const MissionLoadResult result=selectMissionSlot(slots,record,index);
  if (result==MissionLoadResult::Loaded || result==MissionLoadResult::Recovered) target_=index^1;
  else target_=slots[0].status==SlotStatus::Missing?0:slots[1].status==SlotStatus::Missing?1:0;
  return result;
}

bool MissionStorageNVS::save(const MissionRecord& record) {
  if (!validMissionRecord(record)) { Serial.println("MISSION_STORE: INVALID_CANDIDATE"); return false; }
  Preferences nvs;
  if (!nvs.begin(NS,false)) { Serial.println("MISSION_STORE: OPEN_WRITE_FAIL"); return false; }
  const uint8_t slot=target_;
  const bool written=nvs.putBytes(KEYS[slot],&record,sizeof(record))==sizeof(record);
  nvs.end();
  MissionRecord readback;
  const bool verified=written && readBack(KEYS[slot],readback) &&
                      std::memcmp(&record,&readback,sizeof(record))==0;
  // Flip only after verification; a failed slot is retried, the good one stays.
  if (verified) target_=slot^1;
  Serial.printf("MISSION_STORE: %s slot=%u generation=%lu\n",verified?"SAVED":"WRITE_OR_READBACK_FAIL",
                static_cast<unsigned>(slot),static_cast<unsigned long>(record.generation));
  return verified;
}
}  // namespace deskos
