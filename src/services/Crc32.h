#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Same reflected CRC-32 (0xEDB88320) used by the M1 Focus/calibration records.
inline uint32_t crc32Bytes(const void* data,size_t length) {
  const auto* bytes=static_cast<const uint8_t*>(data);
  uint32_t crc=0xFFFFFFFFUL;
  for (size_t i=0;i<length;++i) {
    crc^=bytes[i];
    for (int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xEDB88320UL:0);
  }
  return ~crc;
}
}  // namespace deskos
