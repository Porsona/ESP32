#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Small fixed-size settings/cache records (Pet, Window, Weather). Each record
// type carries its own magic/version/size/CRC and is validated by its owner.
enum class BlobLoad : uint8_t { Missing, Loaded, Invalid, IoError };
class IBlobStore {
 public:
  virtual ~IBlobStore() = default;
  // Loaded only when exactly `size` bytes were read; Invalid for size/type mismatch.
  virtual BlobLoad load(const char* ns,const char* key,void* out,size_t size) = 0;
  // True only after the write committed and an independent read-back matched.
  virtual bool save(const char* ns,const char* key,const void* data,size_t size) = 0;
};
}  // namespace deskos
