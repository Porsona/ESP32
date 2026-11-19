#include "BlobStoreNVS.h"
#include <Arduino.h>
#include <Preferences.h>
#include <nvs.h>
#include <cstring>

namespace deskos {
namespace { constexpr size_t MAX_BLOB=512; }

BlobLoad BlobStoreNVS::load(const char* ns,const char* key,void* out,size_t size) {
  nvs_handle_t handle;
  const esp_err_t opened=nvs_open(ns,NVS_READONLY,&handle);
  if (opened==ESP_ERR_NVS_NOT_FOUND) return BlobLoad::Missing;
  if (opened!=ESP_OK) return BlobLoad::IoError;
  size_t bytes=0;
  const esp_err_t query=nvs_get_blob(handle,key,nullptr,&bytes);
  BlobLoad result=BlobLoad::IoError;
  if (query==ESP_ERR_NVS_NOT_FOUND) result=BlobLoad::Missing;
  else if (query==ESP_ERR_NVS_TYPE_MISMATCH || (query==ESP_OK && bytes!=size)) result=BlobLoad::Invalid;
  else if (query==ESP_OK && nvs_get_blob(handle,key,out,&bytes)==ESP_OK && bytes==size) result=BlobLoad::Loaded;
  nvs_close(handle);
  return result;
}

bool BlobStoreNVS::save(const char* ns,const char* key,const void* data,size_t size) {
  if (size==0 || size>MAX_BLOB) return false;
  Preferences nvs;
  if (!nvs.begin(ns,false)) { Serial.printf("BLOB_STORE: OPEN_WRITE_FAIL ns=%s\n",ns); return false; }
  const bool written=nvs.putBytes(key,data,size)==size;
  nvs.end();
  uint8_t readback[MAX_BLOB];
  const bool verified=written && load(ns,key,readback,size)==BlobLoad::Loaded &&
                      std::memcmp(readback,data,size)==0;
  Serial.printf("BLOB_STORE: %s ns=%s key=%s bytes=%u\n",verified?"SAVED":"WRITE_OR_READBACK_FAIL",
                ns,key,static_cast<unsigned>(size));
  return verified;
}
}  // namespace deskos
