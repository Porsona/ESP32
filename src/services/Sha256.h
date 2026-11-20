#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// FIPS 180-4 SHA-256 and RFC 2104 HMAC-SHA256. Portable C++ so the same code
// runs on the ESP32 and in host tests (verified against FIPS/RFC 4231 vectors).
class Sha256 {
 public:
  Sha256() { reset(); }
  void reset();
  void update(const void* data,size_t length);
  void finish(uint8_t digest[32]);
 private:
  void block(const uint8_t* chunk);
  uint32_t state_[8];
  uint64_t bits_;
  uint8_t buffer_[64];
  size_t used_;
};
void hmacSha256(const uint8_t* key,size_t keyLength,const void* data,size_t length,uint8_t mac[32]);
// Constant-time comparison (no early exit on the first differing byte).
bool equalConstantTime(const uint8_t* a,const uint8_t* b,size_t length);
}  // namespace deskos
