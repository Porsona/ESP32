#pragma once
#include "../services/BlobStore.h"

namespace deskos {
// Shares the default NVS partition; never erases or touches other namespaces.
class BlobStoreNVS : public IBlobStore {
 public:
  BlobLoad load(const char* ns,const char* key,void* out,size_t size) override;
  bool save(const char* ns,const char* key,const void* data,size_t size) override;
};
}  // namespace deskos
