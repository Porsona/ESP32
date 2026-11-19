#include "FocusStorageNVS.h"
#include <Arduino.h>
#include <Preferences.h>
#include <nvs.h>
#include <cstring>

namespace deskos {
namespace { const char* const NS="deskos-focus"; const char* const KEY="state"; }
FocusLoadResult FocusStorageNVS::load(FocusRecord& record) {
  nvs_handle_t handle;
  const esp_err_t opened=nvs_open(NS,NVS_READONLY,&handle);
  if (opened==ESP_ERR_NVS_NOT_FOUND) return FocusLoadResult::Missing;
  if (opened!=ESP_OK) return FocusLoadResult::IoError;
  FocusRecord candidate{};
  size_t bytes=0;
  const esp_err_t query=nvs_get_blob(handle,KEY,nullptr,&bytes);
  FocusLoadResult result=FocusLoadResult::IoError;
  if (query==ESP_ERR_NVS_NOT_FOUND) result=FocusLoadResult::Missing;
  else if (query==ESP_ERR_NVS_TYPE_MISMATCH || (query==ESP_OK && bytes!=sizeof(candidate)))
    result=FocusLoadResult::Invalid;
  else if (query==ESP_OK) {
    const esp_err_t read=nvs_get_blob(handle,KEY,&candidate,&bytes);
    if (read==ESP_OK && bytes==sizeof(candidate)) {
      result=validFocusRecord(candidate)?FocusLoadResult::Loaded:FocusLoadResult::Invalid;
      if (result==FocusLoadResult::Loaded) record=candidate;
    }
  }
  nvs_close(handle); return result;
}
bool FocusStorageNVS::save(const FocusRecord& record) {
  if (!validFocusRecord(record)) { Serial.println("FOCUS_STORE: INVALID_CANDIDATE"); return false; }
  Preferences nvs;
  if (!nvs.begin(NS,false)) { Serial.println("FOCUS_STORE: OPEN_WRITE_FAIL"); return false; }
  const bool written=nvs.putBytes(KEY,&record,sizeof(record))==sizeof(record);
  nvs.end();
  FocusRecord readback{};
  const bool verified=written && load(readback)==FocusLoadResult::Loaded &&
                      std::memcmp(&record,&readback,sizeof(record))==0;
  Serial.printf("FOCUS_STORE: %s generation=%lu\n",verified?"SAVED":"WRITE_OR_READBACK_FAIL",
                static_cast<unsigned long>(record.generation));
  return verified;
}
}  // namespace deskos
